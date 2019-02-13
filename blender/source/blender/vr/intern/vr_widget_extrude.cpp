/*
* ***** BEGIN GPL LICENSE BLOCK *****
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The Original Code is Copyright (C) 2018 by Blender Foundation.
* All rights reserved.
*
* Contributor(s): MARUI-PlugIn
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/vr/intern/vr_widget_extrude.cpp
*   \ingroup vr
* 
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_extrude.h"
#include "vr_widget_transform.h"

#include "vr_math.h"
#include "vr_draw.h"

#include "BLI_math.h"
#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"

#include "DEG_depsgraph.h"

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"

#include "ED_mesh.h"
#include "ED_undo.h"

#include "GPU_batch_presets.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "MEM_guardedalloc.h"
#include "mesh_intern.h"

#include "WM_api.h"
#include "WM_types.h"

/* Multiplier for one and two-handed scaling transformations. */
#define WIDGET_TRANSFORM_SCALING_SENSITIVITY 0.5f

/* Precision multipliers. */
#define WIDGET_TRANSFORM_TRANS_PRECISION 0.1f
#define WIDGET_TRANSFORM_ROT_PRECISION (PI/36.0f)
#define WIDGET_TRANSFORM_SCALE_PRECISION 0.005f

#include "vr_util.h"

/***********************************************************************************************//**
 * \class									Widget_Extrude
 ***************************************************************************************************
 * Interaction widget for the Extrude tool.
 *
 **************************************************************************************************/
Widget_Extrude Widget_Extrude::obj;

Widget_Extrude::ExtrudeMode Widget_Extrude::extrude_mode(Widget_Extrude::EXTRUDEMODE_REGION);
bool Widget_Extrude::extrude(false);
bool Widget_Extrude::flip_normals(false);
//bool Widget_Extrude::offset_even(true);
bool Widget_Extrude::transform(false);

/* Dummy op to pass to EDBM_op_init() and EDBM_op_finish() */
static wmOperator extrude_dummy_op;

/* From editmesh_extrude.c */
static void edbm_extrude_edge_exclude_mirror(
	Object *obedit, BMEditMesh *em,
	const char hflag,
	BMOperator *op, BMOpSlot *slot_edges_exclude)
{
	BMesh *bm = em->bm;
	ModifierData *md;

	/* If a mirror modifier with clipping is on, we need to adjust some
	 * of the cases above to handle edges on the line of symmetry.
	 */
	for (md = (ModifierData*)obedit->modifiers.first; md; md = md->next) {
		if ((md->type == eModifierType_Mirror) && (md->mode & eModifierMode_Realtime)) {
			MirrorModifierData *mmd = (MirrorModifierData *)md;

			if (mmd->flag & MOD_MIR_CLIPPING) {
				BMIter iter;
				BMEdge *edge;

				float mtx[4][4];
				if (mmd->mirror_ob) {
					float imtx[4][4];
					invert_m4_m4(imtx, mmd->mirror_ob->obmat);
					mul_m4_m4m4(mtx, imtx, obedit->obmat);
				}

				BM_ITER_MESH(edge, &iter, bm, BM_EDGES_OF_MESH) {
					if (BM_elem_flag_test(edge, hflag) &&
						BM_edge_is_boundary(edge) &&
						BM_elem_flag_test(edge->l->f, hflag))
					{
						float co1[3], co2[3];

						copy_v3_v3(co1, edge->v1->co);
						copy_v3_v3(co2, edge->v2->co);

						if (mmd->mirror_ob) {
							mul_v3_m4v3(co1, mtx, co1);
							mul_v3_m4v3(co2, mtx, co2);
						}

						if (mmd->flag & MOD_MIR_AXIS_X) {
							if ((fabsf(co1[0]) < mmd->tolerance) &&
								(fabsf(co2[0]) < mmd->tolerance))
							{
								BMO_slot_map_empty_insert(op, slot_edges_exclude, edge);
							}
						}
						if (mmd->flag & MOD_MIR_AXIS_Y) {
							if ((fabsf(co1[1]) < mmd->tolerance) &&
								(fabsf(co2[1]) < mmd->tolerance))
							{
								BMO_slot_map_empty_insert(op, slot_edges_exclude, edge);
							}
						}
						if (mmd->flag & MOD_MIR_AXIS_Z) {
							if ((fabsf(co1[2]) < mmd->tolerance) &&
								(fabsf(co2[2]) < mmd->tolerance))
							{
								BMO_slot_map_empty_insert(op, slot_edges_exclude, edge);
							}
						}
					}
				}
			}
		}
	}
}

static bool edbm_extrude_verts_indiv(BMEditMesh *em, const char hflag)
{
	BMOperator bmop;

	EDBM_op_init(
		em, &bmop, &extrude_dummy_op,
		"extrude_vert_indiv verts=%hv use_select_history=%b",
		hflag, true);

	/* deselect original verts */
	BMO_slot_buffer_hflag_disable(em->bm, bmop.slots_in, "verts", BM_VERT, BM_ELEM_SELECT, true);

	BMO_op_exec(em->bm, &bmop);
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "verts.out", BM_VERT, BM_ELEM_SELECT, true);

	if (!EDBM_op_finish(em, &bmop, &extrude_dummy_op, true)) {
		return false;
	}

	return true;
}

static bool edbm_extrude_edges_indiv(BMEditMesh *em, const char hflag, const bool use_normal_flip)
{
	BMesh *bm = em->bm;
	BMOperator bmop;

	EDBM_op_init(
		em, &bmop, &extrude_dummy_op,
		"extrude_edge_only edges=%he use_normal_flip=%b use_select_history=%b",
		hflag, use_normal_flip, true);

	/* deselect original verts */
	BM_SELECT_HISTORY_BACKUP(bm);
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	BM_SELECT_HISTORY_RESTORE(bm);

	BMO_op_exec(em->bm, &bmop);
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "geom.out", BM_VERT | BM_EDGE, BM_ELEM_SELECT, true);

	if (!EDBM_op_finish(em, &bmop, &extrude_dummy_op, true)) {
		return false;
	}

	return true;
}

static bool edbm_extrude_discrete_faces(BMEditMesh *em, const char hflag)
{
	BMOIter siter;
	BMIter liter;
	BMFace *f;
	BMLoop *l;
	BMOperator bmop;

	EDBM_op_init(
		em, &bmop, &extrude_dummy_op,
		"extrude_discrete_faces faces=%hf use_select_history=%b",
		hflag, true);

	/* deselect original verts */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	BMO_op_exec(em->bm, &bmop);

	BMO_ITER(f, &siter, bmop.slots_out, "faces.out", BM_FACE) {
		BM_face_select_set(em->bm, f, true);

		/* set face vertex normals to face normal */
		BM_ITER_ELEM(l, &liter, f, BM_LOOPS_OF_FACE) {
			copy_v3_v3(l->v->no, f->no);
		}
	}

	if (!EDBM_op_finish(em, &bmop, &extrude_dummy_op, true)) {
		return false;
	}

	return true;
}

static char edbm_extrude_htype_from_em_select(BMEditMesh *em)
{
	char htype = BM_ALL_NOLOOP;

	if (em->selectmode & SCE_SELECT_VERTEX) {
		/* pass */
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		htype &= ~BM_VERT;
	}
	else {
		htype &= ~(BM_VERT | BM_EDGE);
	}

	if (em->bm->totedgesel == 0) {
		htype &= ~(BM_EDGE | BM_FACE);
	}
	else if (em->bm->totfacesel == 0) {
		htype &= ~BM_FACE;
	}

	return htype;
}

static bool edbm_extrude_ex(
	Object *obedit, BMEditMesh *em,
	char htype, const char hflag,
	const bool use_normal_flip,
	const bool use_mirror,
	const bool use_select_history)
{
	BMesh *bm = em->bm;
	BMOIter siter;
	BMOperator extop;
	BMElem *ele;

	/* needed to remove the faces left behind */
	if (htype & BM_FACE) {
		htype |= BM_EDGE;
	}

	BMO_op_init(bm, &extop, BMO_FLAG_DEFAULTS, "extrude_face_region");
	BMO_slot_bool_set(extop.slots_in, "use_normal_flip", use_normal_flip);
	BMO_slot_bool_set(extop.slots_in, "use_select_history", use_select_history);
	BMO_slot_buffer_from_enabled_hflag(bm, &extop, extop.slots_in, "geom", htype, hflag);

	if (use_mirror) {
		BMOpSlot *slot_edges_exclude;
		slot_edges_exclude = BMO_slot_get(extop.slots_in, "edges_exclude");

		edbm_extrude_edge_exclude_mirror(obedit, em, hflag, &extop, slot_edges_exclude);
	}

	BM_SELECT_HISTORY_BACKUP(bm);
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	BM_SELECT_HISTORY_RESTORE(bm);

	BMO_op_exec(bm, &extop);

	BMO_ITER(ele, &siter, extop.slots_out, "geom.out", BM_ALL_NOLOOP) {
		BM_elem_select_set(bm, ele, true);
	}

	BMO_op_finish(bm, &extop);

	return true;
}

static bool edbm_extrude_mesh(Object *obedit, BMEditMesh *em, const bool use_normal_flip)
{
	const char htype = edbm_extrude_htype_from_em_select(em);
	enum { NONE = 0, ELEM_FLAG, VERT_ONLY, EDGE_ONLY } nr;
	bool changed = false;

	if (em->selectmode & SCE_SELECT_VERTEX) {
		if (em->bm->totvertsel == 0) nr = NONE;
		else if (em->bm->totvertsel == 1) nr = VERT_ONLY;
		else if (em->bm->totedgesel == 0) nr = VERT_ONLY;
		else                              nr = ELEM_FLAG;
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		if (em->bm->totedgesel == 0) nr = NONE;
		else if (em->bm->totfacesel == 0) nr = EDGE_ONLY;
		else                              nr = ELEM_FLAG;
	}
	else {
		if (em->bm->totfacesel == 0) nr = NONE;
		else                              nr = ELEM_FLAG;
	}

	switch (nr) {
	case NONE:
		return false;
	case ELEM_FLAG:
		changed = edbm_extrude_ex(obedit, em, htype, BM_ELEM_SELECT, use_normal_flip, true, true);
		break;
	case VERT_ONLY:
		changed = edbm_extrude_verts_indiv(em, BM_ELEM_SELECT);
		break;
	case EDGE_ONLY:
		changed = edbm_extrude_edges_indiv(em, BM_ELEM_SELECT, use_normal_flip);
		break;
	}

	if (changed) {
		return true;
	}
	else {
		return false;
	}
}

static int edbm_extrude_region_exec(bContext *C, bool use_normal_flip = false)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	uint objects_len = 0;
	static ObjectsInModeParams params = { OB_MODE_EDIT, true };
	Object **objects = BKE_view_layer_array_from_objects_in_mode_params(view_layer, CTX_wm_view3d(C), &objects_len, &params);

	for (uint ob_index = 0; ob_index < objects_len; ++ob_index) {
		Object *obedit = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		if (!em || em->bm->totvertsel == 0) {
			continue;
		}

		if (!edbm_extrude_mesh(obedit, em, use_normal_flip)) {
			continue;
		}
		/* This normally happens when pushing undo but modal operators
		 * like this one don't push undo data until after modal mode is
		 * done.*/

		EDBM_mesh_normals_update(em);

		EDBM_update_generic(em, true, true);
	}
	MEM_freeN(objects);
	
	return OPERATOR_FINISHED;
}

static int edbm_extrude_verts_exec(bContext *C)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	uint objects_len = 0;
	static ObjectsInModeParams params = { OB_MODE_EDIT, true };
	Object **objects = BKE_view_layer_array_from_objects_in_mode_params(view_layer, CTX_wm_view3d(C), &objects_len, &params);

	for (uint ob_index = 0; ob_index < objects_len; ++ob_index) {
		Object *obedit = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		if (!em || em->bm->totvertsel == 0) {
			continue;
		}

		edbm_extrude_verts_indiv(em, BM_ELEM_SELECT);

		EDBM_update_generic(em, true, true);
	}
	MEM_freeN(objects);

	return OPERATOR_FINISHED;
}

static int edbm_extrude_edges_exec(bContext *C, const bool use_normal_flip)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	uint objects_len = 0;
	static ObjectsInModeParams params = { OB_MODE_EDIT, true };
	Object **objects = BKE_view_layer_array_from_objects_in_mode_params(view_layer, CTX_wm_view3d(C), &objects_len, &params);

	for (uint ob_index = 0; ob_index < objects_len; ++ob_index) {
		Object *obedit = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		if (!em || em->bm->totedgesel == 0) {
			continue;
		}

		edbm_extrude_edges_indiv(em, BM_ELEM_SELECT, use_normal_flip);

		EDBM_update_generic(em, true, true);
	}
	MEM_freeN(objects);

	return OPERATOR_FINISHED;
}

static int edbm_extrude_faces_exec(bContext *C)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	uint objects_len = 0;
	static ObjectsInModeParams params = { OB_MODE_EDIT, true };
	Object **objects = BKE_view_layer_array_from_objects_in_mode_params(view_layer, CTX_wm_view3d(C), &objects_len, &params);

	for (uint ob_index = 0; ob_index < objects_len; ++ob_index) {
		Object *obedit = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		if (!em || em->bm->totfacesel == 0) {
			continue;
		}

		edbm_extrude_discrete_faces(em, BM_ELEM_SELECT);

		EDBM_update_generic(em, true, true);
	}
	MEM_freeN(objects);

	return OPERATOR_FINISHED;
}

static int edbm_extrude_indiv_exec(bContext *C, bool use_normal_flip)
{ 
	ToolSettings *ts = CTX_data_scene(C)->toolsettings;
	if (ts->selectmode & SCE_SELECT_VERTEX) {
		edbm_extrude_verts_exec(C);
	}
	else if (ts->selectmode & SCE_SELECT_EDGE) {
		edbm_extrude_edges_exec(C, use_normal_flip);
	}
	else if (ts->selectmode & SCE_SELECT_FACE) {
		edbm_extrude_faces_exec(C);
	}

	return OPERATOR_FINISHED;
}

bool Widget_Extrude::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Extrude::click(VR_UI::Cursor& c)
{
	const Mat44f& m = c.position.get();
	if (CTX_data_edit_object(vr_get_obj()->ctx)) {
		VR_Util::raycast_select_single_edit(*(Coord3Df*)m.m[3], VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
	}
	else {
		for (int i = 0; i < VR_SIDES; ++i) {
			Widget_Extrude::obj.do_render[i] = false;
		}
		return;
	}
	/* Update manipulator transform. */
	Widget_Transform::manipulator = true;
	Widget_Transform::omni = true;
	Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_OMNI;
	Widget_Transform::snap_mode = VR_UI::SNAPMODE_TRANSLATION;
	Widget_Transform::update_manipulator();

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Extrude::do_render[i] = true;
	}
}

void Widget_Extrude::drag_start(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		for (int i = 0; i < VR_SIDES; ++i) {
			Widget_Extrude::obj.do_render[i] = false;
		}
		return;
	}

	/* If other hand is already dragging, don't change the current state of the Extrude/Transform tool. */
	if (c.bimanual) {
		return;
	}

	Widget_Transform::manipulator = true;
	Widget_Transform::omni = true;
	Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_OMNI;
	Widget_Transform::snap_mode = VR_UI::SNAPMODE_TRANSLATION;

	/* Test for manipulator selection and set constraints. */
	const Mat44f& m = c.interaction_position.get();
	extrude = false;
	Widget_Transform::raycast_select_manipulator(*(Coord3Df*)m.m[3], &extrude);
	if (extrude) {
		/* Manipulator extrude region was hit, do extrude operation. */
		switch (extrude_mode) {
		case EXTRUDEMODE_NORMALS: {
			edbm_extrude_region_exec(C, flip_normals);
			break;
		}
		case EXTRUDEMODE_INDIVIDUAL:
		case EXTRUDEMODE_REGION: 
		default: {
			edbm_extrude_indiv_exec(C, flip_normals);
			break;
		}
		}
	}

	/* Set transform/snapping modes based on constraints */
	memset(Widget_Transform::constraint_flag, 0, sizeof(int) * 3);
	if (Widget_Transform::constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
		switch (Widget_Transform::constraint_mode) {
		case VR_UI::CONSTRAINTMODE_TRANS_X: {
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_MOVE;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			Widget_Transform::constraint_flag[0] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_Y: {
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_MOVE;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			Widget_Transform::constraint_flag[1] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_Z: {
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_MOVE;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			Widget_Transform::constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_XY: {
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_MOVE;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			Widget_Transform::constraint_flag[0] = Widget_Transform::constraint_flag[1] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_YZ: {
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_MOVE;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			Widget_Transform::constraint_flag[1] = Widget_Transform::constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_ZX: {
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_MOVE;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			Widget_Transform::constraint_flag[0] = Widget_Transform::constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_X: {
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_ROTATE;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_ROTATION;
			Widget_Transform::constraint_flag[0] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_Y: {
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_ROTATE;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_ROTATION;
			Widget_Transform::constraint_flag[1] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_Z: {
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_ROTATE;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_ROTATION;
			Widget_Transform::constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_X: {
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_SCALE;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_SCALE;
			Widget_Transform::constraint_flag[0] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_Y: {
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_SCALE;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_SCALE;
			Widget_Transform::constraint_flag[1] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_Z: {
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_SCALE;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_SCALE;
			Widget_Transform::constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_XY: {
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_SCALE;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_SCALE;
			Widget_Transform::constraint_flag[0] = Widget_Transform::constraint_flag[1] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_YZ: {
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_SCALE;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_SCALE;
			Widget_Transform::constraint_flag[1] = Widget_Transform::constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_ZX: {
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_SCALE;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_SCALE;
			Widget_Transform::constraint_flag[0] = Widget_Transform::constraint_flag[2] = 1;
			break;
		}
		default: {
			break;
		}
		}
		memcpy(Widget_Transform::snap_flag, Widget_Transform::constraint_flag, sizeof(int) * 3);
	}
	else {
		memset(Widget_Transform::snap_flag, 1, sizeof(int) * 3);
	}

	/* Set up snapping positions vector */
	for (int i = 0; i < Widget_Transform::nonsnap_t.size(); ++i) {
		delete Widget_Transform::nonsnap_t[i];
	}
	Widget_Transform::nonsnap_t.clear();
	Widget_Transform::nonsnap_t.push_back(new Mat44f());
	Widget_Transform::snapped = false;

	/* Reset manipulator angles */
	memset(&Widget_Transform::manip_angle, 0, sizeof(float) * 9);
	/* Save original manipulator transformation */
	Widget_Transform::obmat_inv = (*(Mat44f*)obedit->obmat).inverse();
	Widget_Transform::manip_t_orig = Widget_Transform::manip_t * Widget_Transform::obmat_inv;

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Extrude::obj.do_render[i] = true;
	}

	//Widget_Transform::is_dragging = true;

	/* Call drag_contd() immediately? */
	Widget_Extrude::obj.drag_contd(c);
}

void Widget_Extrude::drag_contd(VR_UI::Cursor& c)
{
	if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_NONE && 
		!transform && 
		Widget_Transform::transform_mode != Widget_Transform::TRANSFORMMODE_SCALE) {
		/* Free transformation not allowed (except for center scale cube), so return. */
		return;
	}

	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		return;
	}
	ToolSettings *ts = NULL;
	BMesh *bm = NULL;
	if (obedit) {
		/* Edit mode */
		ts = CTX_data_scene(C)->toolsettings;
		if (!ts) {
			return;
		}
		if (obedit->type == OB_MESH) {
			bm = ((Mesh*)obedit->data)->edit_btmesh->bm;
			if (!bm) {
				return;
			}
		}
	}
	else {
		return;
	}

	static Mat44f curr;
	static Mat44f prev;
	/* Check if we're two-hand dragging */
	if (c.bimanual) {
		if (c.bimanual == VR_UI::Cursor::BIMANUAL_SECOND)
			return; /* calculations are only performed by first hand */

		const Mat44f& curr_h = VR_UI::cursor_position_get(VR_SPACE_BLENDER, c.side);
		const Mat44f& curr_o = VR_UI::cursor_position_get(VR_SPACE_BLENDER, (VR_Side)(1 - c.side));
		const Mat44f& prev_h = c.interaction_position.get(VR_SPACE_BLENDER);
		const Mat44f& prev_o = c.other_hand->interaction_position.get(VR_SPACE_BLENDER);

		/* Rotation
		/* x-axis is the base line between the two pointers */
		Coord3Df x_axis_prev(prev_h.m[3][0] - prev_o.m[3][0],
			prev_h.m[3][1] - prev_o.m[3][1],
			prev_h.m[3][2] - prev_o.m[3][2]);
		Coord3Df x_axis_curr(curr_h.m[3][0] - curr_o.m[3][0],
			curr_h.m[3][1] - curr_o.m[3][1],
			curr_h.m[3][2] - curr_o.m[3][2]);
		/* y-axis is the average of the pointers y-axis */
		Coord3Df y_axis_prev((prev_h.m[1][0] + prev_o.m[1][0]) / 2.0f,
			(prev_h.m[1][1] + prev_o.m[1][1]) / 2.0f,
			(prev_h.m[1][2] + prev_o.m[1][2]) / 2.0f);
		Coord3Df y_axis_curr((curr_h.m[1][0] + curr_o.m[1][0]) / 2.0f,
			(curr_h.m[1][1] + curr_o.m[1][1]) / 2.0f,
			(curr_h.m[1][2] + curr_o.m[1][2]) / 2.0f);

		/* z-axis is the cross product of the two */
		Coord3Df z_axis_prev = x_axis_prev ^ y_axis_prev;
		Coord3Df z_axis_curr = x_axis_curr ^ y_axis_curr;
		/* fix the y-axis to be orthogonal */
		y_axis_prev = z_axis_prev ^ x_axis_prev;
		y_axis_curr = z_axis_curr ^ x_axis_curr;
		/* normalize and apply */
		x_axis_prev.normalize_in_place();
		x_axis_curr.normalize_in_place();
		y_axis_prev.normalize_in_place();
		y_axis_curr.normalize_in_place();
		z_axis_prev.normalize_in_place();
		z_axis_curr.normalize_in_place();
		prev.m[0][0] = x_axis_prev.x;    prev.m[0][1] = x_axis_prev.y;    prev.m[0][2] = x_axis_prev.z;
		prev.m[1][0] = y_axis_prev.x;    prev.m[1][1] = y_axis_prev.y;    prev.m[1][2] = y_axis_prev.z;
		prev.m[2][0] = z_axis_prev.x;    prev.m[2][1] = z_axis_prev.y;    prev.m[2][2] = z_axis_prev.z;
		curr.m[0][0] = x_axis_curr.x;    curr.m[0][1] = x_axis_curr.y;    curr.m[0][2] = x_axis_curr.z;
		curr.m[1][0] = y_axis_curr.x;    curr.m[1][1] = y_axis_curr.y;    curr.m[1][2] = y_axis_curr.z;
		curr.m[2][0] = z_axis_curr.x;    curr.m[2][1] = z_axis_curr.y;    curr.m[2][2] = z_axis_curr.z;

		/* Translation: translation of the averaged pointer positions */
		prev.m[3][0] = (prev_h.m[3][0] + prev_o.m[3][0]) / 2.0f;    prev.m[3][1] = (prev_h.m[3][1] + prev_o.m[3][1]) / 2.0f;    prev.m[3][2] = (prev_h.m[3][2] + prev_o.m[3][2]) / 2.0f;	prev.m[3][3] = 1;
		curr.m[3][0] = (curr_h.m[3][0] + curr_o.m[3][0]) / 2.0f;    curr.m[3][1] = (curr_h.m[3][1] + curr_o.m[3][1]) / 2.0f;    curr.m[3][2] = (curr_h.m[3][2] + curr_o.m[3][2]) / 2.0f;	curr.m[3][3] = 1;

		if (Widget_Transform::transform_mode != Widget_Transform::TRANSFORMMODE_ROTATE) {
			/* Scaling: distance between pointers */
			float curr_s = sqrt(((curr_h.m[3][0] - curr_o.m[3][0])*(curr_h.m[3][0] - curr_o.m[3][0])) + ((curr_h.m[3][1]) - curr_o.m[3][1])*(curr_h.m[3][1] - curr_o.m[3][1])) + ((curr_h.m[3][2] - curr_o.m[3][2])*(curr_h.m[3][2] - curr_o.m[3][2]));
			float start_s = sqrt(((prev_h.m[3][0] - prev_o.m[3][0])*(prev_h.m[3][0] - prev_o.m[3][0])) + ((prev_h.m[3][1]) - prev_o.m[3][1])*(prev_h.m[3][1] - prev_o.m[3][1])) + ((prev_h.m[3][2] - prev_o.m[3][2])*(prev_h.m[3][2] - prev_o.m[3][2]));

			prev.m[0][0] *= start_s; prev.m[1][0] *= start_s; prev.m[2][0] *= start_s;
			prev.m[0][1] *= start_s; prev.m[1][1] *= start_s; prev.m[2][1] *= start_s;
			prev.m[0][2] *= start_s; prev.m[1][2] *= start_s; prev.m[2][2] *= start_s;

			curr.m[0][0] *= curr_s; curr.m[1][0] *= curr_s; curr.m[2][0] *= curr_s;
			curr.m[0][1] *= curr_s; curr.m[1][1] *= curr_s; curr.m[2][1] *= curr_s;
			curr.m[0][2] *= curr_s; curr.m[1][2] *= curr_s; curr.m[2][2] *= curr_s;
		}

		c.interaction_position.set(curr_h.m, VR_SPACE_BLENDER);
		c.other_hand->interaction_position.set(curr_o.m, VR_SPACE_BLENDER);
	}
	else { /* one-handed drag */
		curr = c.position.get(VR_SPACE_BLENDER);
		prev = c.interaction_position.get(VR_SPACE_BLENDER);
		c.interaction_position.set(curr.m, VR_SPACE_BLENDER);
	}

	curr = curr * Widget_Transform::obmat_inv;
	prev = prev * Widget_Transform::obmat_inv;

	/* Calculate delta based on transform mode. */
	static Mat44f delta;
	if (c.bimanual) {
		delta = prev.inverse() * curr;
	}
	else {
		switch (Widget_Transform::transform_mode) {
		case Widget_Transform::TRANSFORMMODE_MOVE: {
			delta = VR_Math::identity_f;
			*(Coord3Df*)delta.m[3] = *(Coord3Df*)curr.m[3] - *(Coord3Df*)prev.m[3];
			break;
		}
		case Widget_Transform::TRANSFORMMODE_SCALE: {
			delta = VR_Math::identity_f;
			if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_NONE) {
				/* Scaling based on distance from manipulator center. */
				static Coord3Df prev_d, curr_d;
				prev_d = *(Coord3Df*)prev.m[3] - *(Coord3Df*)Widget_Transform::manip_t.m[3];
				curr_d = *(Coord3Df*)curr.m[3] - *(Coord3Df*)Widget_Transform::manip_t.m[3];
				float p_len = prev_d.length();
				float s = (p_len == 0.0f) ? 1.0f : curr_d.length() / p_len;
				if (s > 1.0f) {
					s = 1.0f + (s - 1.0f) * WIDGET_TRANSFORM_SCALING_SENSITIVITY;
				}
				else if (s < 1.0f) {
					s = 1.0f - (1.0f - s) * WIDGET_TRANSFORM_SCALING_SENSITIVITY;
				}
				delta.m[0][0] = delta.m[1][1] = delta.m[2][2] = s;
			}
			else {
				*(Coord3Df*)delta.m[3] = *(Coord3Df*)curr.m[3] - *(Coord3Df*)prev.m[3];
				float s = (*(Coord3Df*)delta.m[3]).length();
				(*(Coord3Df*)delta.m[3]).normalize_in_place() *= s * WIDGET_TRANSFORM_SCALING_SENSITIVITY;
			}
			break;
		}
		case Widget_Transform::TRANSFORMMODE_ROTATE:
		case Widget_Transform::TRANSFORMMODE_OMNI:
		default: {
			delta = prev.inverse() * curr;
			break;
		}
		}
	}

	static Mat44f delta_orig;
	static float scale[3];
	static float eul[3];
	static float rot[3][3];
	static Coord3Df temp1, temp2;

	/* Precision */
	if (VR_UI::shift_key_get()) {
		/* Translation */
		for (int i = 0; i < 3; ++i) {
			scale[i] = (*(Coord3Df*)delta.m[i]).length();
		}
		*(Coord3Df*)delta.m[3] *= WIDGET_TRANSFORM_TRANS_PRECISION;
		/*for (int i = 0; i < 3; ++i) {
			delta.m[3][i] *= (scale[i] * WIDGET_TRANSFORM_TRANS_PRECISION);
		}*/

		/* Rotation */
		mat4_to_eul(eul, delta.m);
		for (int i = 0; i < 3; ++i) {
			eul[i] *= WIDGET_TRANSFORM_ROT_PRECISION;
		}
		eul_to_mat3(rot, eul);
		for (int i = 0; i < 3; ++i) {
			memcpy(delta.m[i], rot[i], sizeof(float) * 3);
		}

		/* Scale */
		for (int i = 0; i < 3; ++i) {
			if (scale[i] > 1.0001f) { /* Take numerical instability into account */
				*(Coord3Df*)delta.m[i] *= (1.0f + WIDGET_TRANSFORM_SCALE_PRECISION);
			}
			else if (scale[i] < 0.9999f) {
				*(Coord3Df*)delta.m[i] *= (1.0f - WIDGET_TRANSFORM_SCALE_PRECISION);
			}
		}
	}

	/* Constraints */
	bool constrain = false;
	if (Widget_Transform::constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
		delta_orig = delta;
		delta = VR_Math::identity_f;
		constrain = true;
	}

	/* Snapping */
	bool snap = false;
	if (VR_UI::ctrl_key_get()) {
		snap = true;
	}

	/* Constraints */
	if (constrain) {
		static float axis[3];
		static float angle;
		static Coord3Df temp3;
		switch (Widget_Transform::constraint_mode) {
		case VR_UI::CONSTRAINTMODE_TRANS_X: {
			if (extrude_mode == EXTRUDEMODE_NORMALS && extrude) {
				project_v3_v3v3(delta.m[3], delta_orig.m[3], Widget_Transform::manip_t.m[0]);
			}
			else {
				project_v3_v3v3(delta.m[3], delta_orig.m[3], Widget_Transform::manip_t_orig.m[0]);
			}
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_Y: {
			if (extrude_mode == EXTRUDEMODE_NORMALS && extrude) {
				project_v3_v3v3(delta.m[3], delta_orig.m[3], Widget_Transform::manip_t.m[1]);
			}
			else {
				project_v3_v3v3(delta.m[3], delta_orig.m[3], Widget_Transform::manip_t_orig.m[1]);
			}
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_Z: {
			if (extrude_mode == EXTRUDEMODE_NORMALS && extrude) {
				project_v3_v3v3(delta.m[3], delta_orig.m[3], Widget_Transform::manip_t.m[2]);
			}
			else {
				project_v3_v3v3(delta.m[3], delta_orig.m[3], Widget_Transform::manip_t_orig.m[2]);
			}
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_XY: {
			project_v3_v3v3(&temp1.x, delta_orig.m[3], Widget_Transform::manip_t_orig.m[0]);
			project_v3_v3v3(&temp2.x, delta_orig.m[3], Widget_Transform::manip_t_orig.m[1]);
			*(Coord3Df*)delta.m[3] = temp1 + temp2;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_YZ: {
			project_v3_v3v3(&temp1.x, delta_orig.m[3], Widget_Transform::manip_t_orig.m[1]);
			project_v3_v3v3(&temp2.x, delta_orig.m[3], Widget_Transform::manip_t_orig.m[2]);
			*(Coord3Df*)delta.m[3] = temp1 + temp2;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_ZX: {
			project_v3_v3v3(&temp1.x, delta_orig.m[3], Widget_Transform::manip_t_orig.m[0]);
			project_v3_v3v3(&temp2.x, delta_orig.m[3], Widget_Transform::manip_t_orig.m[2]);
			*(Coord3Df*)delta.m[3] = temp1 + temp2;
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_X: {
			mat4_to_axis_angle(axis, &angle, delta_orig.m);
			if ((*(Coord3Df*)axis) * (*(Coord3Df*)Widget_Transform::manip_t_orig.m[0]) < 0) {
				angle = -angle;
			}
			axis_angle_to_mat4(delta.m, Widget_Transform::manip_t_orig.m[0], angle);
			if (VR_UI::shift_key_get()) {
				Widget_Transform::manip_angle[Widget_Transform::transform_space].x += angle * WIDGET_TRANSFORM_ROT_PRECISION;
			}
			else {
				Widget_Transform::manip_angle[Widget_Transform::transform_space].x += angle;
			}
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_Y: {
			mat4_to_axis_angle(axis, &angle, delta_orig.m);
			if ((*(Coord3Df*)axis) * (*(Coord3Df*)Widget_Transform::manip_t_orig.m[1]) < 0) {
				angle = -angle;
			}
			axis_angle_to_mat4(delta.m, Widget_Transform::manip_t_orig.m[1], angle);
			if (VR_UI::shift_key_get()) {
				Widget_Transform::manip_angle[Widget_Transform::transform_space].y += angle * WIDGET_TRANSFORM_ROT_PRECISION;
			}
			else {
				Widget_Transform::manip_angle[Widget_Transform::transform_space].y += angle;
			}
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_Z: {
			mat4_to_axis_angle(axis, &angle, delta_orig.m);
			if ((*(Coord3Df*)axis) * (*(Coord3Df*)Widget_Transform::manip_t_orig.m[2]) < 0) {
				angle = -angle;
			}
			axis_angle_to_mat4(delta.m, Widget_Transform::manip_t_orig.m[2], angle);
			if (VR_UI::shift_key_get()) {
				Widget_Transform::manip_angle[Widget_Transform::transform_space].z += angle * WIDGET_TRANSFORM_ROT_PRECISION;
			}
			else {
				Widget_Transform::manip_angle[Widget_Transform::transform_space].z += angle;
			}
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_X: {
			float length;
			*(Coord3Df*)scale = (*(Coord3Df*)Widget_Transform::manip_t_orig.m[0]).normalize();
			if (c.bimanual) {
				length = -delta_orig.m[3][0];
			}
			else {
				project_v3_v3v3(&temp1.x, delta_orig.m[3], Widget_Transform::manip_t_orig.m[0]);
				length = temp1.length();
				temp2 = (*(Coord3Df*)delta_orig.m[3]).normalize();
				if (dot_v3v3((float*)&temp2, scale) < 0) {
					length = -length;
				}
			}
			for (int i = 0; i < 3; ++i) {
				delta.m[i][i] = 1.0f + fabsf(scale[i]) * length;
			}
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_Y: {
			float length;
			*(Coord3Df*)scale = (*(Coord3Df*)Widget_Transform::manip_t_orig.m[1]).normalize();
			if (c.bimanual) {
				length = -delta_orig.m[3][1];
			}
			else {
				project_v3_v3v3(&temp1.x, delta_orig.m[3], Widget_Transform::manip_t_orig.m[1]);
				length = temp1.length();
				temp2 = (*(Coord3Df*)delta_orig.m[3]).normalize();
				if (dot_v3v3((float*)&temp2, scale) < 0) {
					length = -length;
				}
			}
			for (int i = 0; i < 3; ++i) {
				delta.m[i][i] = 1.0f + fabsf(scale[i]) * length;
			}
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_Z: {
			float length;
			*(Coord3Df*)scale = (*(Coord3Df*)Widget_Transform::manip_t_orig.m[2]).normalize();
			if (c.bimanual) {
				length = -delta_orig.m[3][2];
			}
			else {
				project_v3_v3v3(&temp1.x, delta_orig.m[3], Widget_Transform::manip_t_orig.m[2]);
				length = temp1.length();
				temp2 = (*(Coord3Df*)delta_orig.m[3]).normalize();
				if (dot_v3v3((float*)&temp2, scale) < 0) {
					length = -length;
				}
			}
			for (int i = 0; i < 3; ++i) {
				delta.m[i][i] = 1.0f + fabsf(scale[i]) * length;
			}
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_XY: {
			float length;
			if (c.bimanual) {
				length = -(delta_orig.m[3][0] + delta_orig.m[3][1]) / 2.0f;
				*(Coord3Df*)scale = ((*(Coord3Df*)Widget_Transform::manip_t_orig.m[0]).normalize() + (*(Coord3Df*)Widget_Transform::manip_t_orig.m[1]).normalize()) / 2.0f;
			}
			else {
				project_v3_v3v3(&temp1.x, delta_orig.m[3], Widget_Transform::manip_t_orig.m[0]);
				length = temp1.length();
				(*(Coord3Df*)scale = (*(Coord3Df*)delta_orig.m[3]).normalize());
				temp1 = (*(Coord3Df*)Widget_Transform::manip_t_orig.m[0]).normalize();
				if (dot_v3v3((float*)&temp1, scale) < 0) {
					length = -length;
				}
				project_v3_v3v3(&temp3.x, delta_orig.m[3], Widget_Transform::manip_t_orig.m[1]);
				temp2 = (*(Coord3Df*)Widget_Transform::manip_t_orig.m[1]).normalize();
				if (dot_v3v3((float*)&temp2, scale) < 0) {
					length -= temp3.length();
				}
				else {
					length += temp3.length();
				}
				length /= 2.0f;
			}
			*(Coord3Df*)scale = (temp1 + temp2) / 2.0f;
			for (int i = 0; i < 3; ++i) {
				delta.m[i][i] = 1.0f + fabsf(scale[i]) * length;
			}
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_YZ: {
			float length;
			if (c.bimanual) {
				length = -(delta_orig.m[3][1] + delta_orig.m[3][2]) / 2.0f;
				*(Coord3Df*)scale = ((*(Coord3Df*)Widget_Transform::manip_t_orig.m[1]).normalize() + (*(Coord3Df*)Widget_Transform::manip_t_orig.m[2]).normalize()) / 2.0f;
			}
			else {
				project_v3_v3v3(&temp1.x, delta_orig.m[3], Widget_Transform::manip_t_orig.m[1]);
				length = temp1.length();
				(*(Coord3Df*)scale = (*(Coord3Df*)delta_orig.m[3]).normalize());
				temp1 = (*(Coord3Df*)Widget_Transform::manip_t_orig.m[1]).normalize();
				if (dot_v3v3((float*)&temp1, scale) < 0) {
					length = -length;
				}
				project_v3_v3v3(&temp3.x, delta_orig.m[3], Widget_Transform::manip_t_orig.m[2]);
				temp2 = (*(Coord3Df*)Widget_Transform::manip_t_orig.m[2]).normalize();
				if (dot_v3v3((float*)&temp2, scale) < 0) {
					length -= temp3.length();
				}
				else {
					length += temp3.length();
				}
				length /= 2.0f;
			}
			*(Coord3Df*)scale = (temp1 + temp2) / 2.0f;
			for (int i = 0; i < 3; ++i) {
				delta.m[i][i] = 1.0f + fabsf(scale[i]) * length;
			}
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_ZX: {
			float length;
			if (c.bimanual) {
				length = -(delta_orig.m[3][0] + delta_orig.m[3][2]) / 2.0f;
				*(Coord3Df*)scale = ((*(Coord3Df*)Widget_Transform::manip_t_orig.m[0]).normalize() + (*(Coord3Df*)Widget_Transform::manip_t_orig.m[2]).normalize()) / 2.0f;
			}
			else {
				project_v3_v3v3(&temp1.x, delta_orig.m[3], Widget_Transform::manip_t_orig.m[0]);
				length = temp1.length();
				(*(Coord3Df*)scale = (*(Coord3Df*)delta_orig.m[3]).normalize());
				temp1 = (*(Coord3Df*)Widget_Transform::manip_t_orig.m[0]).normalize();
				if (dot_v3v3((float*)&temp1, scale) < 0) {
					length = -length;
				}
				project_v3_v3v3(&temp3.x, delta_orig.m[3], Widget_Transform::manip_t_orig.m[2]);
				temp2 = (*(Coord3Df*)Widget_Transform::manip_t_orig.m[2]).normalize();
				if (dot_v3v3((float*)&temp2, scale) < 0) {
					length -= temp3.length();
				}
				else {
					length += temp3.length();
				}
				length /= 2.0f;
			}
			for (int i = 0; i < 3; ++i) {
				delta.m[i][i] = 1.0f + fabsf(scale[i]) * length;
			}
			break;
		}
		default: {
			break;
		}
		}
	}

	/* Snapping */
	static Mat44f m;
	if (snap) {
		Mat44f& nonsnap_m = *Widget_Transform::nonsnap_t[0];
		if (!Widget_Transform::snapped) {
			nonsnap_m = Widget_Transform::manip_t * Widget_Transform::obmat_inv;
			Widget_Transform::manip_t_snap = Widget_Transform::manip_t * Widget_Transform::obmat_inv;
		}
		else {
			m = nonsnap_m;
			nonsnap_m = m * delta;
		}
		static Mat44f manip_t_prev;
		manip_t_prev = Widget_Transform::manip_t_snap;

		/* Apply snapping. */
		float precision, iter_fac, val;
		for (int i = 0; i < 3; ++i) {
			scale[i] = (*(Coord3Df*)nonsnap_m.m[i]).length();
		}
		switch (Widget_Transform::snap_mode) {
		case VR_UI::SNAPMODE_TRANSLATION: {
			/* Translation */
			if (VR_UI::shift_key_get()) {
				precision = WIDGET_TRANSFORM_TRANS_PRECISION;
			}
			else {
				precision = 1.0f;
			}
			float *pos = (float*)nonsnap_m.m[3];
			for (int i = 0; i < 3; ++i, ++pos) {
				if (!Widget_Transform::snap_flag[i]) {
					continue;
				}
				iter_fac = precision * scale[i];
				val = roundf(*pos / iter_fac);
				Widget_Transform::manip_t_snap.m[3][i] = iter_fac * val;
			}
			switch (Widget_Transform::constraint_mode) {
			case VR_UI::CONSTRAINTMODE_TRANS_X: {
				temp1 = *(Coord3Df*)Widget_Transform::manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
				if (extrude_mode == EXTRUDEMODE_NORMALS && extrude) {
					project_v3_v3v3(&temp2.x, &temp1.x, Widget_Transform::manip_t.m[0]);
				}
				else {
					project_v3_v3v3(&temp2.x, &temp1.x, Widget_Transform::manip_t_orig.m[0]);
				}
				*(Coord3Df*)Widget_Transform::manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_Y: {
				temp1 = *(Coord3Df*)Widget_Transform::manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
				if (extrude_mode == EXTRUDEMODE_NORMALS && extrude) {
					project_v3_v3v3(&temp2.x, &temp1.x, Widget_Transform::manip_t.m[1]);
				}
				else {
					project_v3_v3v3(&temp2.x, &temp1.x, Widget_Transform::manip_t_orig.m[1]);
				}
				*(Coord3Df*)Widget_Transform::manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_Z: {
				temp1 = *(Coord3Df*)Widget_Transform::manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
				if (extrude_mode == EXTRUDEMODE_NORMALS && extrude) {
					project_v3_v3v3(&temp2.x, &temp1.x, Widget_Transform::manip_t.m[2]);
				}
				else {
					project_v3_v3v3(&temp2.x, &temp1.x, Widget_Transform::manip_t_orig.m[2]);
				}
				*(Coord3Df*)Widget_Transform::manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_XY: {
				temp1 = *(Coord3Df*)Widget_Transform::manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
				project_v3_v3v3(&temp2.x, &temp1.x, Widget_Transform::manip_t_orig.m[0]);
				*(Coord3Df*)Widget_Transform::manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
				project_v3_v3v3(&temp2.x, &temp1.x, Widget_Transform::manip_t_orig.m[1]);
				*(Coord3Df*)Widget_Transform::manip_t_snap.m[3] += temp2;
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_YZ: {
				temp1 = *(Coord3Df*)Widget_Transform::manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
				project_v3_v3v3(&temp2.x, &temp1.x, Widget_Transform::manip_t_orig.m[1]);
				*(Coord3Df*)Widget_Transform::manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
				project_v3_v3v3(&temp2.x, &temp1.x, Widget_Transform::manip_t_orig.m[2]);
				*(Coord3Df*)Widget_Transform::manip_t_snap.m[3] += temp2;
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_ZX: {
				temp1 = *(Coord3Df*)Widget_Transform::manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
				project_v3_v3v3(&temp2.x, &temp1.x, Widget_Transform::manip_t_orig.m[0]);
				*(Coord3Df*)Widget_Transform::manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
				project_v3_v3v3(&temp2.x, &temp1.x, Widget_Transform::manip_t_orig.m[2]);
				*(Coord3Df*)Widget_Transform::manip_t_snap.m[3] += temp2;
				break;
			}
			default: {
				/* TODO_XR: Local / normal translation snappping (no constraints) */
				break;
			}
			}
			break;
		}
		case VR_UI::SNAPMODE_ROTATION: {
			/* Rotation */
			if (VR_UI::shift_key_get()) {
				precision = PI / 180.0f;
			}
			else {
				precision = WIDGET_TRANSFORM_ROT_PRECISION;
			}
			/* TODO_XR: Local / normal rotation snapping (no constraints). */
			mat4_to_eul(eul, nonsnap_m.m);
			//static float eul_orig[3];
			//memcpy(eul_orig, eul, sizeof(float) * 3);
			for (int i = 0; i < 3; ++i) {
				if (!Widget_Transform::snap_flag[i]) {
					continue;
				}
				val = roundf(eul[i] / precision);
				eul[i] = precision * val;
			}
			eul_to_mat3(rot, eul);
			for (int i = 0; i < 3; ++i) {
				memcpy(Widget_Transform::manip_t_snap.m[i], rot[i], sizeof(float) * 3);
				*(Coord3Df*)Widget_Transform::manip_t_snap.m[i] *= scale[i];
			}
			/* Update manipulator angles. */
			/* TODO_XR */
			//for (int i = 0; i < 3; ++i) {
			//	if (!Widget_Transform::snap_flag[i]) {
			//		continue;
			//	}
			//	switch (i) {
			//	case 0: {
			//		float& m_angle = Widget_Transform::manip_angle[transform_space].x;
			//		m_angle += eul[i] - eul_orig[i];
			//		val = roundf(m_angle / precision);
			//		m_angle = precision * val;
			//		break;
			//	}
			//	case 1: {
			//		float& m_angle = Widget_Transform::manip_angle[transform_space].y;
			//		m_angle += eul[i] - eul_orig[i];
			//		val = roundf(m_angle / precision);
			//		m_angle = precision * val;
			//		break;
			//	}
			//	case 2: {
			//		float& m_angle = Widget_Transform::manip_angle[transform_space].z;
			//		m_angle += eul[i] - eul_orig[i];
			//		val = roundf(m_angle / precision);
			//		m_angle = precision * val;
			//		break;
			//	}
			//	}
			//}
			break;
		}
		case VR_UI::SNAPMODE_SCALE: {
			/* Scale */
			/* TODO_XR */
			//if (Widget_Transform::transform_space == VR_UI::TRANSFORMSPACE_GLOBAL && Widget_Transform::constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
			//	/* TODO_XR */
			//	break;
			//	/*for (int i = 0; i < 3; ++i) {
			//		if (snap_flag[i]) {
			//			continue;
			//		}
			//		(*(Coord3Df*)Widget_Transform::manip_t_snap.m[i]).normalize_in_place() *= (*(Coord3Df*)nonsnap_m.m[i]).length();
			//	}
			//	static Mat44f t;
			//	transpose_m4_m4(t.m, nonsnap_m.m);
			//	for (int i = 0; i < 3; ++i) {
			//		scale[i] = (*(Coord3Df*)t.m[i]).length();
			//	}*/
			//}
			//for (int i = 0; i < 3; ++i) {
			//	if (!snap_flag[i]) {
			//		continue;
			//	}
			//	if (VR_UI::shift_key_get()) {
			//		/* Snap scale based on the power of ten magnitude of the curent scale */
			//		precision = 0.1f * powf(10.0f, floor(log10(scale[i])));
			//	}
			//	else {
			//		precision = 0.5f * powf(10.0f, floor(log10(scale[i])));
			//	}
			//	val = roundf(scale[i] / precision);
			//	if (val == 0.0f) {
			//		val = 1.0f;
			//	}
			//	(*(Coord3Df*)Widget_Transform::manip_t_snap.m[i]).normalize_in_place() *= (precision * val);
			//}
			break;
		}
		default: {
			break;
		}
		}

		delta = manip_t_prev.inverse() * Widget_Transform::manip_t_snap;
		if (Widget_Transform::snap_mode == VR_UI::SNAPMODE_ROTATION) {
			memset(delta.m[3], 0, sizeof(float) * 3);
		}
	}
	else {
		/* Transform mode */
		switch (Widget_Transform::transform_mode) {
		case Widget_Transform::TRANSFORMMODE_MOVE: {
			for (int i = 0; i < 3; ++i) {
				memcpy(delta.m[i], VR_Math::identity_f.m[i], sizeof(float) * 3);
			}
			break;
		}
		case Widget_Transform::TRANSFORMMODE_ROTATE: {
			memset(delta.m[3], 0, sizeof(float) * 3);
			break;
		}
		case Widget_Transform::TRANSFORMMODE_SCALE: {
			memset(delta.m[3], 0, sizeof(float) * 3);
			break;
		}
		case Widget_Transform::TRANSFORMMODE_OMNI:
		default: {
			break;
		}
		}
	}

	/* Extrude mode */
	BMIter iter;
	if (extrude) {
		switch (extrude_mode) {
		case EXTRUDEMODE_NORMALS: {
			/* Extrude along each average normal with the magnitude of the current manipulator delta. */
			float mag = (*(Coord3Df*)delta.m[3]).length();
			temp1 = (*(Coord3Df*)delta.m[3]).normalize();
			temp2 = (*(Coord3Df*)Widget_Transform::manip_t.m[3]).normalize();
			if (temp1 * temp2 < 0) {
				mag = -mag;
			}

			if (ts->selectmode & SCE_SELECT_VERTEX) {
				BMVert *v;
				BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
					if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
						memcpy(delta.m[3], v->no, sizeof(float) * 3);
						(*(Coord3Df*)delta.m[3]) *= mag;

						float *co = v->co;
						memcpy((float*)&temp1, co, sizeof(float) * 3);
						mul_v3_m4v3(co, delta.m, (float*)&temp1);
					}
				}
			}
			else if (ts->selectmode & SCE_SELECT_EDGE) {
				BMEdge *e;
				BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH) {
					if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
						float *co1 = e->v1->co;
						float *co2 = e->v2->co;
						memcpy((float*)&temp1, co1, sizeof(float) * 3);
						memcpy((float*)&temp2, co2, sizeof(float) * 3);

						*(Coord3Df*)delta.m[3] = (*(Coord3Df*)e->v1->no + *(Coord3Df*)e->v2->no) / 2.0f;
						*(Coord3Df*)delta.m[3] *= mag;
						mul_v3_m4v3(co1, delta.m, (float*)&temp1);
						mul_v3_m4v3(co2, delta.m, (float*)&temp2);
					}
				}
			}
			else if (ts->selectmode & SCE_SELECT_FACE) {
				BMFace *f;
				BMLoop *l;
				BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH) {
					if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
						int len = f->len;
						l = f->l_first;
						for (int i = 0; i < len; ++i, l = l->next) {
							*(Coord3Df*)delta.m[3] += *(Coord3Df*)l->v->no;
						}
						*(Coord3Df*)delta.m[3] *= (mag / (float)len);

						l = f->l_first;
						for (int i = 0; i < len; ++i, l = l->next) {
							float *co = l->v->co;
							memcpy((float*)&temp1, co, sizeof(float) * 3);
							mul_v3_m4v3(co, delta.m, (float*)&temp1);
						}
					}
				}
			}
			break;
		}
		case EXTRUDEMODE_INDIVIDUAL: {
			/* Extrude along each average normal with the magnitude of the original manipulator delta. */
			float mag = (*(Coord3Df*)delta.m[3]).length();
			temp1 = (*(Coord3Df*)delta.m[3]).normalize();
			temp2 = (*(Coord3Df*)Widget_Transform::manip_t_orig.m[3]).normalize();
			if (temp1 * temp2 < 0) {
				mag = -mag;
			}

			if (ts->selectmode & SCE_SELECT_VERTEX) {
				BMVert *v;
				BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
					if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
						memcpy(delta.m[3], v->no, sizeof(float) * 3);
						(*(Coord3Df*)delta.m[3]) *= mag;

						float *co = v->co;
						memcpy((float*)&temp1, co, sizeof(float) * 3);
						mul_v3_m4v3(co, delta.m, (float*)&temp1);
					}
				}
			}
			else if (ts->selectmode & SCE_SELECT_EDGE) {
				BMEdge *e;
				BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH) {
					if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
						float *co1 = e->v1->co;
						float *co2 = e->v2->co;
						memcpy((float*)&temp1, co1, sizeof(float) * 3);
						memcpy((float*)&temp2, co2, sizeof(float) * 3);

						*(Coord3Df*)delta.m[3] = (*(Coord3Df*)e->v1->no + *(Coord3Df*)e->v2->no) / 2.0f;
						*(Coord3Df*)delta.m[3] *= mag;
						mul_v3_m4v3(co1, delta.m, (float*)&temp1);
						mul_v3_m4v3(co2, delta.m, (float*)&temp2);
					}
				}
			}
			else if (ts->selectmode & SCE_SELECT_FACE) {
				BMFace *f;
				BMLoop *l;
				BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH) {
					if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
						int len = f->len;
						l = f->l_first;
						for (int i = 0; i < len; ++i, l = l->next) {
							*(Coord3Df*)delta.m[3] += *(Coord3Df*)l->v->no;
						}
						*(Coord3Df*)delta.m[3] *= (mag / (float)len);

						l = f->l_first;
						for (int i = 0; i < len; ++i, l = l->next) {
							float *co = l->v->co;
							memcpy((float*)&temp1, co, sizeof(float) * 3);
							mul_v3_m4v3(co, delta.m, (float*)&temp1);
						}
					}
				}
			}
			break;
		}
		case EXTRUDEMODE_REGION:
		default: {
			if (ts->selectmode & SCE_SELECT_VERTEX) {
				BMVert *v;
				BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
					if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
						float *co = v->co;
						memcpy((float*)&temp1, co, sizeof(float) * 3);
						mul_v3_m4v3(co, delta.m, (float*)&temp1);
					}
				}
			}
			else if (ts->selectmode & SCE_SELECT_EDGE) {
				BMEdge *e;
				BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH) {
					if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
						float *co1 = e->v1->co;
						float *co2 = e->v2->co;
						memcpy((float*)&temp1, co1, sizeof(float) * 3);
						memcpy((float*)&temp2, co2, sizeof(float) * 3);
						mul_v3_m4v3(co1, delta.m, (float*)&temp1);
						mul_v3_m4v3(co2, delta.m, (float*)&temp2);
					}
				}
			}
			else if (ts->selectmode & SCE_SELECT_FACE) {
				BMFace *f;
				BMLoop *l;
				BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH) {
					if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
						l = f->l_first;
						for (int i = 0; i < f->len; ++i, l = l->next) {
							float *co = l->v->co;
							memcpy((float*)&temp1, co, sizeof(float) * 3);
							mul_v3_m4v3(co, delta.m, (float*)&temp1);
						}
					}
				}
			}
			break;
		}
		}
	}
	else {
		if (ts->selectmode & SCE_SELECT_VERTEX) {
			BMVert *v;
			BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
				if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
					float *co = v->co;
					memcpy((float*)&temp1, co, sizeof(float) * 3);
					mul_v3_m4v3(co, delta.m, (float*)&temp1);
				}
			}
		}
		else if (ts->selectmode & SCE_SELECT_EDGE) {
			BMEdge *e;
			BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH) {
				if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
					float *co1 = e->v1->co;
					float *co2 = e->v2->co;
					memcpy((float*)&temp1, co1, sizeof(float) * 3);
					memcpy((float*)&temp2, co2, sizeof(float) * 3);
					mul_v3_m4v3(co1, delta.m, (float*)&temp1);
					mul_v3_m4v3(co2, delta.m, (float*)&temp2);
				}
			}
		}
		else if (ts->selectmode & SCE_SELECT_FACE) {
			BMFace *f;
			BMLoop *l;
			BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH) {
				if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
					l = f->l_first;
					for (int i = 0; i < f->len; ++i, l = l->next) {
						float *co = l->v->co;
						memcpy((float*)&temp1, co, sizeof(float) * 3);
						mul_v3_m4v3(co, delta.m, (float*)&temp1);
					}
				}
			}
		}
	}
	
	/* Set recalc flags. */
	DEG_id_tag_update((ID*)obedit->data, 0);

	if (snap) {
		Widget_Transform::snapped = true;
	}
	else {
		Widget_Transform::snapped = false;
	}

	/* Update manipulator transform (also used when rendering constraints). */
	static VR_UI::TransformSpace prev_space = VR_UI::TRANSFORMSPACE_GLOBAL;
	if (prev_space != Widget_Transform::transform_space) {
		prev_space = Widget_Transform::transform_space;
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		EDBM_mesh_normals_update(em);
		Widget_Transform::update_manipulator();
		Widget_Transform::manip_t_orig = Widget_Transform::manip_t * (*(Mat44f*)obedit->obmat).inverse();
	}
	else {	
		if (extrude_mode == EXTRUDEMODE_NORMALS && extrude && (ts->selectmode & SCE_SELECT_FACE)) {
			BMEditMesh *em = BKE_editmesh_from_object(obedit);
			EDBM_mesh_normals_update(em);
		}
		/* Don't update manipulator transformation for rotations. */
		if (Widget_Transform::transform_mode != Widget_Transform::TRANSFORMMODE_ROTATE) {
			Widget_Transform::update_manipulator();
		}
	}

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Extrude::obj.do_render[i] = true;
	}

	Widget_Transform::is_dragging = true;
}

void Widget_Extrude::drag_stop(VR_UI::Cursor& c)
{
	/* Check if we're two-hand navi dragging */
	if (c.bimanual) {
		VR_UI::Cursor *other = c.other_hand;
		c.bimanual = VR_UI::Cursor::BIMANUAL_OFF;
		/* the other hand is still dragging - we're leaving a two-hand drag. */
		other->bimanual = VR_UI::Cursor::BIMANUAL_OFF;
		/* ALSO: the other hand should start one-hand manipulating from here: */
		c.other_hand->interaction_position.set(((Mat44f)VR_UI::cursor_position_get(VR_SPACE_REAL, other->side)).m, VR_SPACE_REAL);
		/* Calculations are only performed by the second hand. */
		return;
	}

	Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_OMNI;
	Widget_Transform::snap_mode = VR_UI::SNAPMODE_TRANSLATION;

	Widget_Transform::is_dragging = false;
	extrude = false;

	if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_NONE && !transform) {
		/* Free transformation not allowed, so return */
		return;
	}

	/* TODO_XR: Avoid doing this twice (already done in drag_start() */
	Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_NONE;
	memset(Widget_Transform::constraint_flag, 0, sizeof(int) * 3);
	memset(Widget_Transform::snap_flag, 1, sizeof(int) * 3);

	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		return;
	}
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	EDBM_mesh_normals_update(em);
	Widget_Transform::update_manipulator();

	DEG_id_tag_update((ID*)obedit->data, ID_RECALC_GEOMETRY);
	WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
	ED_undo_push(C, "Extrude");
}

void Widget_Extrude::render(VR_Side side) 
{
	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		//Widget_Extrude::obj.do_render[side] = false;
		return;
	}

	static float manip_length[3];
	for (int i = 0; i < 3; ++i) {
		manip_length[i] = Widget_Transform::manip_scale_factor * 2.0f;
	}
	static float clip_plane[4] = { 0.0f };

	if (Widget_Transform::omni && Widget_Transform::manipulator) {
		/* Dial and Gimbal */
		GPU_blend(true);
		GPU_matrix_push();
		GPU_matrix_mul(Widget_Transform::manip_t.m);
		GPU_polygon_smooth(false);
		if (Widget_Transform::transform_mode == Widget_Transform::TRANSFORMMODE_ROTATE) {
			switch (Widget_Transform::constraint_mode) {
			case VR_UI::CONSTRAINTMODE_ROT_X: {
				GPU_matrix_rotate_axis(-90.0f, 'Y');
				Widget_Transform::render_dial(PI / 4.0f, Widget_Transform::manip_angle[Widget_Transform::transform_space].x, 0.0f, manip_length[0] / 4.0f);
				if (VR_UI::ctrl_key_get()) {
					if (VR_UI::shift_key_get()) {
						Widget_Transform::render_incremental_angles(PI / 180.0f, 0.0f, manip_length[0] / 4.0f);
					}
					else {
						Widget_Transform::render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[0] / 4.0f);
					}
				}
				GPU_matrix_rotate_axis(90.0f, 'Y');
				break;
			}
			case VR_UI::CONSTRAINTMODE_ROT_Y: {
				GPU_matrix_rotate_axis(90.0f, 'X');
				Widget_Transform::render_dial(PI / 4.0f, Widget_Transform::manip_angle[Widget_Transform::transform_space].y, 0.0f, manip_length[1] / 4.0f);
				if (VR_UI::ctrl_key_get()) {
					if (VR_UI::shift_key_get()) {
						Widget_Transform::render_incremental_angles(PI / 180.0f, 0.0f, manip_length[1] / 4.0f);
					}
					else {
						Widget_Transform::render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[1] / 4.0f);
					}
				}
				GPU_matrix_rotate_axis(-90.0f, 'X');
				break;
			}
			case VR_UI::CONSTRAINTMODE_ROT_Z: {
				GPU_matrix_rotate_axis(-90.0f, 'Z');
				Widget_Transform::render_dial(-PI / 4.0f, -Widget_Transform::manip_angle[Widget_Transform::transform_space].z, 0.0f, manip_length[2] / 4.0f);
				if (VR_UI::ctrl_key_get()) {
					if (VR_UI::shift_key_get()) {
						Widget_Transform::render_incremental_angles(PI / 180.0f, 0.0f, manip_length[2] / 4.0f);
					}
					else {
						Widget_Transform::render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[2] / 4.0f);
					}
				}
				GPU_matrix_rotate_axis(90.0f, 'Z');
				break;
			}
			default: {
				break;
			}
			}
		}
		Widget_Transform::render_gimbal(manip_length, false, Widget_Transform::manip_t.m, clip_plane, 3 * PI / 2.0f, 0.0f);
		/* Extrude Ball and Arrow */
		*((Coord3Df*)manip_length) /= 2.0f;
		Widget_Transform::render_axes(manip_length, 3);
		Widget_Transform::render_axes(manip_length, 0);
		/* Box */
		*((Coord3Df*)manip_length) /= 2.0f;
		Widget_Transform::render_axes(manip_length, 1);
		/* Ball */
		Widget_Transform::render_axes(manip_length, 2);
		GPU_blend(false);
		GPU_matrix_pop();
		return;
	}

	switch (Widget_Transform::transform_mode) {
	case Widget_Transform::TRANSFORMMODE_OMNI: {
		/* Extrude Ball and Arrow */
		*((Coord3Df*)manip_length) /= 2.0f;
		GPU_matrix_push();
		GPU_matrix_mul(Widget_Transform::manip_t.m);
		GPU_blend(true);
		Widget_Transform::render_axes(manip_length, 3);
		Widget_Transform::render_axes(manip_length, 0);
		GPU_blend(false);
		GPU_matrix_pop();
		break;
	}
	case Widget_Transform::TRANSFORMMODE_MOVE: {
		/* Plane */
		GPU_matrix_push();
		GPU_matrix_mul(Widget_Transform::manip_t.m);
		GPU_blend(true);
		Widget_Transform::render_planes(manip_length);
		/* Extrude Ball and Arrow */
		*((Coord3Df*)manip_length) /= 2.0f;
		Widget_Transform::render_axes(manip_length, 3);
		Widget_Transform::render_axes(manip_length, 0);
		GPU_blend(false);
		GPU_matrix_pop();
		break;
	}
	case Widget_Transform::TRANSFORMMODE_ROTATE: {
		/* Dial and Gimbal */
		GPU_blend(true);
		GPU_matrix_push();
		GPU_matrix_mul(Widget_Transform::manip_t.m);
		GPU_polygon_smooth(false);
		switch (Widget_Transform::constraint_mode) {
		case VR_UI::CONSTRAINTMODE_ROT_X: {
			GPU_matrix_rotate_axis(-90.0f, 'Y');
			Widget_Transform::render_dial(PI / 4.0f, Widget_Transform::manip_angle[Widget_Transform::transform_space].x, 0.0f, manip_length[0] / 4.0f);
			if (VR_UI::ctrl_key_get()) {
				if (VR_UI::shift_key_get()) {
					Widget_Transform::render_incremental_angles(PI / 180.0f, 0.0f, manip_length[0] / 4.0f);
				}
				else {
					Widget_Transform::render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[0] / 4.0f);
				}
			}
			GPU_matrix_rotate_axis(90.0f, 'Y');
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_Y: {
			GPU_matrix_rotate_axis(90.0f, 'X');
			Widget_Transform::render_dial(PI / 4.0f, Widget_Transform::manip_angle[Widget_Transform::transform_space].y, 0.0f, manip_length[1] / 4.0f);
			if (VR_UI::ctrl_key_get()) {
				if (VR_UI::shift_key_get()) {
					Widget_Transform::render_incremental_angles(PI / 180.0f, 0.0f, manip_length[1] / 4.0f);
				}
				else {
					Widget_Transform::render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[1] / 4.0f);
				}
			}
			GPU_matrix_rotate_axis(-90.0f, 'X');
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_Z: {
			GPU_matrix_rotate_axis(-90.0f, 'Z');
			Widget_Transform::render_dial(-PI / 4.0f, -Widget_Transform::manip_angle[Widget_Transform::transform_space].z, 0.0f, manip_length[2] / 4.0f);
			if (VR_UI::ctrl_key_get()) {
				if (VR_UI::shift_key_get()) {
					Widget_Transform::render_incremental_angles(PI / 180.0f, 0.0f, manip_length[2] / 4.0f);
				}
				else {
					Widget_Transform::render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[2] / 4.0f);
				}
			}
			GPU_matrix_rotate_axis(90.0f, 'Z');
			break;
		}
		default: {
			break;
		}
		}
		if (!Widget_Transform::manipulator) {
			Widget_Transform::render_gimbal(manip_length, false, Widget_Transform::manip_t.m, clip_plane, 0.0f, 0.0f);
		}
		else {
			Widget_Transform::render_gimbal(manip_length, false, Widget_Transform::manip_t.m, clip_plane, 3 * PI / 2.0f, 0.0f);
		}
		/* Extrude Ball */
		*((Coord3Df*)manip_length) /= 2.0f;
		Widget_Transform::render_axes(manip_length, 3);
		/* Ball */
		*((Coord3Df*)manip_length) /= 2.0f;
		Widget_Transform::render_axes(manip_length, 2);
		GPU_blend(false);
		GPU_matrix_pop();
		break;
	}
	case Widget_Transform::TRANSFORMMODE_SCALE: {
		/* Plane */
		GPU_matrix_push();
		GPU_matrix_mul(Widget_Transform::manip_t.m);
		GPU_blend(true);
		Widget_Transform::render_planes(manip_length);
		/* Extrude Ball */
		*((Coord3Df*)manip_length) /= 2.0f;
		Widget_Transform::render_axes(manip_length, 3);
		/* Box */
		*((Coord3Df*)manip_length) /= 2.0f;
		Widget_Transform::render_axes(manip_length, 1);
		/* TODO_XR */
		static float zero[4][4] = { 0 };
		GPU_matrix_mul(zero);
		GPUBatch *sphere = GPU_batch_preset_sphere(0);
		GPU_batch_program_set_builtin(sphere, GPU_SHADER_3D_UNIFORM_COLOR);
		GPU_batch_draw(sphere);
		GPU_blend(false);
		GPU_matrix_pop();
		break;
	}
	default: {
		break;
	}
	}
}
