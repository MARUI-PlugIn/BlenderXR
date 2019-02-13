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

/** \file blender/vr/intern/vr_widget_insetfaces.cpp
*   \ingroup vr
*
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_insetfaces.h"
#include "vr_widget_transform.h"

#include "vr_draw.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_unit.h"

#include "DEG_depsgraph.h"

#include "DNA_mesh_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_numinput.h"
#include "ED_undo.h"

#include "GPU_immediate.h"
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

/* Sensitivity multiplier for interactions. */
#define WIDGET_INSETFACES_SENSITIVITY 3.0f

#include "vr_util.h"

/***********************************************************************************************//**
 * \class									Widget_InsetFaces
 ***************************************************************************************************
 * Interaction widget for the Inset Faces tool.
 *
 **************************************************************************************************/
Widget_InsetFaces Widget_InsetFaces::obj;

Coord3Df Widget_InsetFaces::p0;
Coord3Df Widget_InsetFaces::p1;
Coord3Df Widget_InsetFaces::p0_b;
Coord3Df Widget_InsetFaces::p1_b;
VR_Side Widget_InsetFaces::cursor_side;

float Widget_InsetFaces::thickness(0.01f);
float Widget_InsetFaces::depth(0.0f);

bool Widget_InsetFaces::use_individual(false);
bool Widget_InsetFaces::use_boundary(true);
bool Widget_InsetFaces::use_even_offset(true);
bool Widget_InsetFaces::use_relative_offset(false);
bool Widget_InsetFaces::use_outset(false);

/* Dummy op */
static wmOperator inset_dummy_op;

/* From editmesh_inset.c */
typedef struct {
	BMEditMesh *em;
	BMBackup mesh_backup;
} InsetObjectStore;

typedef struct {
	float old_thickness;
	float old_depth;
	bool modify_depth;
	float initial_length;
	float pixel_size;  /* use when mouse input is interpreted as spatial distance */
	bool is_modal;
	bool shift;
	float shift_amount;
	NumInput num_input;

	InsetObjectStore *ob_store;
	uint              ob_store_len;

	/* modal only */
	float mcenter[2];
	void *draw_handle_pixel;
	short gizmo_flag;
} InsetData;

static bool edbm_inset_init(bContext *C, wmOperator *op, const bool is_modal)
{
	InsetData *opdata;
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);

	//if (is_modal) {
		Widget_InsetFaces::thickness = 0.01f;
		Widget_InsetFaces::depth = 0.0f;
	//}

	op->customdata = opdata = (InsetData*)MEM_mallocN(sizeof(InsetData), "inset_operator_data");
	uint objects_used_len = 0;

	{
		uint ob_store_len = 0;
		static ObjectsInModeParams params = { OB_MODE_EDIT, true };
		Object **objects = BKE_view_layer_array_from_objects_in_mode_params(view_layer, CTX_wm_view3d(C), &ob_store_len, &params);

		opdata->ob_store = (InsetObjectStore*)MEM_malloc_arrayN(ob_store_len, sizeof(*opdata->ob_store), __func__);
		for (uint ob_index = 0; ob_index < ob_store_len; ob_index++) {
			Object *obedit = objects[ob_index];
			BMEditMesh *em = BKE_editmesh_from_object(obedit);
			if (em->bm->totvertsel > 0) {
				opdata->ob_store[objects_used_len].em = em;
				objects_used_len++;
			}
		}
		MEM_freeN(objects);
		opdata->ob_store_len = objects_used_len;
	}

	opdata->old_thickness = 0.01f;
	opdata->old_depth = 0.0f;
	opdata->modify_depth = false;
	opdata->shift = false;
	opdata->shift_amount = 0.0f;
	opdata->is_modal = is_modal;

	initNumInput(&opdata->num_input);
	opdata->num_input.idx_max = 1; /* Two elements. */
	opdata->num_input.unit_sys = scene->unit.system;
	opdata->num_input.unit_type[0] = B_UNIT_LENGTH;
	opdata->num_input.unit_type[1] = B_UNIT_LENGTH;

	//if (is_modal) {
		View3D *v3d = CTX_wm_view3d(C);
		for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
			opdata->ob_store[ob_index].mesh_backup = EDBM_redo_state_store(opdata->ob_store[ob_index].em);
		}
		G.moving = G_TRANSFORM_EDIT;
		if (v3d) {
			opdata->gizmo_flag = v3d->gizmo_flag;
			v3d->gizmo_flag = V3D_GIZMO_HIDE;
		}
	//}

	return true;
}

static bool edbm_inset_calc(wmOperator *op)
{
	InsetData *opdata = (InsetData*)op->customdata;
	if (!opdata) {
		return false;
	}
	BMEditMesh *em;
	BMOperator bmop;
	bool changed = false;

	/* TODO_XR */
	const bool use_edge_rail = false; //RNA_boolean_get(op->ptr, "use_edge_rail");
	const bool use_select_inset = false; //RNA_boolean_get(op->ptr, "use_select_inset"); /* not passed onto the BMO */
	const bool use_interpolate = true; //RNA_boolean_get(op->ptr, "use_interpolate");

	for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
		em = opdata->ob_store[ob_index].em;

		//if (opdata->is_modal) {
			EDBM_redo_state_restore(opdata->ob_store[ob_index].mesh_backup, em, false);
		//}

		if (Widget_InsetFaces::use_individual) {
			EDBM_op_init(
				em, &bmop, op,
				"inset_individual faces=%hf use_even_offset=%b  use_relative_offset=%b "
				"use_interpolate=%b thickness=%f depth=%f",
				BM_ELEM_SELECT, Widget_InsetFaces::use_even_offset, Widget_InsetFaces::use_relative_offset, use_interpolate,
				Widget_InsetFaces::thickness, Widget_InsetFaces::depth);
		}
		else {
			EDBM_op_init(
				em, &bmop, op,
				"inset_region faces=%hf use_boundary=%b use_even_offset=%b use_relative_offset=%b "
				"use_interpolate=%b thickness=%f depth=%f use_outset=%b use_edge_rail=%b",
				BM_ELEM_SELECT, Widget_InsetFaces::use_boundary, Widget_InsetFaces::use_even_offset, Widget_InsetFaces::use_relative_offset, use_interpolate,
				Widget_InsetFaces::thickness, Widget_InsetFaces::depth, Widget_InsetFaces::use_outset, use_edge_rail);

			if (Widget_InsetFaces::use_outset) {
				BMO_slot_buffer_from_enabled_hflag(em->bm, &bmop, bmop.slots_in, "faces_exclude", BM_FACE, BM_ELEM_HIDDEN);
			}
		}
		BMO_op_exec(em->bm, &bmop);

		if (use_select_inset) {
			/* deselect original faces/verts */
			EDBM_flag_disable_all(em, BM_ELEM_SELECT);
			BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);
		}
		else {
			EDBM_flag_disable_all(em, BM_ELEM_SELECT);
			BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_in, "faces", BM_FACE, BM_ELEM_SELECT, true);
		}

		if (!EDBM_op_finish(em, &bmop, op, true)) {
			continue;
		}
		else {
			EDBM_update_generic(em, true, true);
			changed = true;
		}
	}
	return changed;
}

static void edbm_inset_exit(bContext *C, wmOperator *op)
{
	InsetData *opdata = (InsetData*)op->customdata;
	if (!opdata) {
		return;
	}

	ScrArea *sa = CTX_wm_area(C);
	if (sa) {
		ED_area_status_text(sa, NULL);
	}

	//if (opdata->is_modal) {
		View3D *v3d = CTX_wm_view3d(C);
		for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
			EDBM_redo_state_free(&opdata->ob_store[ob_index].mesh_backup, NULL, false);
		}
		if (v3d) {
			v3d->gizmo_flag = opdata->gizmo_flag;
		}
		G.moving = 0;
	//}

	MEM_SAFE_FREE(opdata->ob_store);
	MEM_SAFE_FREE(op->customdata);
	op->customdata = NULL;
}

static void edbm_inset_cancel(bContext *C, wmOperator *op)
{
	InsetData *opdata = (InsetData*)op->customdata;
	if (!opdata) {
		return;
	}
	//if (opdata->is_modal) {
	for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
		EDBM_redo_state_free(&opdata->ob_store[ob_index].mesh_backup, opdata->ob_store[ob_index].em, true);
		EDBM_update_generic(opdata->ob_store[ob_index].em, false, true);
	}
	//}

	edbm_inset_exit(C, op);

	/* need to force redisplay or we may still view the modified result */
	ED_region_tag_redraw(CTX_wm_region(C));
}

bool Widget_InsetFaces::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_InsetFaces::click(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		return;
	}

	const Mat44f& m = c.position.get();
	if (CTX_data_edit_object(vr_get_obj()->ctx)) {
		VR_Util::raycast_select_single_edit(*(Coord3Df*)m.m[3], VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
	}

	/* Update manipulators */
	Widget_Transform::update_manipulator();
}

void Widget_InsetFaces::drag_start(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		return;
	}

	if (c.bimanual) {
		return;
	}

	cursor_side = c.side;
	p1 = p0 = *(Coord3Df*)c.interaction_position.get(VR_SPACE_REAL).m[3];
	p1_b = p0_b = *(Coord3Df*)c.interaction_position.get(VR_SPACE_BLENDER).m[3];

	/* Execute inset operation */
	edbm_inset_init(C, &inset_dummy_op, false);

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_InsetFaces::obj.do_render[i] = true;
	}
}

void Widget_InsetFaces::drag_contd(VR_UI::Cursor& c)
{
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

	if (c.bimanual) {
		return;
	}

	p1 = *(Coord3Df*)c.position.get(VR_SPACE_REAL).m[3];
	p1_b = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];
	if (VR_UI::ctrl_key_get()) {
		depth = (p1 - p0).length() * WIDGET_INSETFACES_SENSITIVITY;
		if (VR_UI::shift_key_get()) {
			depth *= WIDGET_TRANSFORM_TRANS_PRECISION;
		}
	}
	else {
		thickness = (p1 - p0).length() * WIDGET_INSETFACES_SENSITIVITY;
		if (VR_UI::shift_key_get()) {
			thickness *= WIDGET_TRANSFORM_TRANS_PRECISION;
		}
	}

	/* Execute inset operation */
	if (!edbm_inset_calc(&inset_dummy_op)) {
		edbm_inset_cancel(C, &inset_dummy_op);
	}

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_InsetFaces::obj.do_render[i] = true;
	}
}

void Widget_InsetFaces::drag_stop(VR_UI::Cursor& c)
{
	if (c.bimanual) {
		return;
	}

	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		return;
	}

	/* Execute inset operation */
	p1 = *(Coord3Df*)c.position.get(VR_SPACE_REAL).m[3];
	p1_b = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];
	if (VR_UI::ctrl_key_get()) {
		depth = (p1 - p0).length() * WIDGET_INSETFACES_SENSITIVITY;
		if (VR_UI::shift_key_get()) {
			depth *= WIDGET_TRANSFORM_TRANS_PRECISION;
		}
	}
	else {
		thickness = (p1 - p0).length() * WIDGET_INSETFACES_SENSITIVITY;
		if (VR_UI::shift_key_get()) {
			thickness *= WIDGET_TRANSFORM_TRANS_PRECISION;
		}
	}
	/* Execute inset operation */
	edbm_inset_exit(C, &inset_dummy_op);

	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	EDBM_mesh_normals_update(em);
	Widget_Transform::update_manipulator();

	DEG_id_tag_update((ID*)obedit->data, ID_RECALC_GEOMETRY);
	WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
	ED_undo_push(C, "Inset Faces");

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_InsetFaces::obj.do_render[i] = false;
	}
}

void Widget_InsetFaces::render(VR_Side side)
{
	/* Render dashed line from center. */
	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	GPU_line_width(10.0f);

	static const float c_black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);
	immBeginAtMost(GPU_PRIM_LINES, 2);
	immUniformColor4fv(c_black);
	immUniform1f("dash_width", 6.0f);

	immVertex3fv(pos, (float*)&p0_b);
	immVertex3fv(pos, (float*)&p1_b);

	if (p0_b == p1_b) {
		/* cyclic */
		immVertex3fv(pos, (float*)&p0_b);
	}
	immEnd();
	immUnbindProgram();

	//Widget_InsetFaces::obj.do_render[side] = false;
}
