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

/** \file blender/vr/intern/vr_widget_bevel.cpp
*   \ingroup vr
* 
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_bevel.h"
#include "vr_widget_transform.h"

#include "vr_draw.h"

#include "BLI_math.h"
#include "BLI_linklist_stack.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_mesh.h"
#include "BKE_unit.h"

#include "DEG_depsgraph.h"

#include "DNA_mesh_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_space_api.h"
#include "ED_numinput.h"
#include "ED_undo.h"
#include "ED_view3d.h"

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

#include "vr_util.h"

/***********************************************************************************************//**
 * \class									Widget_Bevel
 ***************************************************************************************************
 * Interaction widget for the Bevel tool.
 *
 **************************************************************************************************/
Widget_Bevel Widget_Bevel::obj;

Coord3Df Widget_Bevel::p0;
Coord3Df Widget_Bevel::p1;
VR_Side Widget_Bevel::cursor_side;

float Widget_Bevel::offset(0.0f);
int Widget_Bevel::segments(1);
bool Widget_Bevel::vertex_only(false);

/* Dummy op */
static wmOperator bevel_dummy_op;

/* From editmesh_bevel.c */
#define PROFILE_HARD_MIN 0.0f
#define SEGMENTS_HARD_MAX 1000

#define OFFSET_VALUE 0
#define OFFSET_VALUE_PERCENT 1
#define PROFILE_VALUE 2
#define SEGMENTS_VALUE 3
#define NUM_VALUE_KINDS 4

static const char *value_rna_name[NUM_VALUE_KINDS] = { "offset", "offset", "profile", "segments" };
static const float value_clamp_min[NUM_VALUE_KINDS] = { 0.0f, 0.0f, PROFILE_HARD_MIN, 1.0f };
static const float value_clamp_max[NUM_VALUE_KINDS] = { 1e6, 100.0f, 1.0f, SEGMENTS_HARD_MAX };
static const float value_start[NUM_VALUE_KINDS] = { 0.0f, 0.0f, 0.5f, 1.0f };
static const float value_scale_per_inch[NUM_VALUE_KINDS] = { 0.0f, 100.0f, 1.0f, 4.0f };

typedef struct {
	BMEditMesh *em;
	BMBackup mesh_backup;
} BevelObjectStore;

typedef struct {
	float initial_length[NUM_VALUE_KINDS];
	float scale[NUM_VALUE_KINDS];
	NumInput num_input[NUM_VALUE_KINDS];
	float shift_value[NUM_VALUE_KINDS]; /* The current value when shift is pressed. Negative when shift not active. */
	bool is_modal;

	BevelObjectStore *ob_store;
	uint ob_store_len;

	/* modal only */
	float mcenter[2];
	void *draw_handle_pixel;
	short gizmo_flag;
	short value_mode;  /* Which value does mouse movement and numeric input affect? */
	float segments;     /* Segments as float so smooth mouse pan works in small increments */
} BevelData;

#define BLI_SMALLSTACK_PUSH_VR(var, data) \
{ \
	CHECK_TYPE_PAIR(data, _##var##_type); \
	if (_##var##_free) { \
		_##var##_temp = _##var##_free; \
		_##var##_free = _##var##_free->next; \
	} \
	else { \
		_##var##_temp = (LinkNode*)alloca(sizeof(LinkNode)); \
	} \
	_##var##_temp->next = _##var##_stack; \
	_##var##_temp->link = data; \
	_##var##_stack = _##var##_temp; \
	_BLI_SMALLSTACK_FAKEUSER(var); \
} (void)0

static void bevel_harden_normals(BMEditMesh *em, BMOperator *bmop, float face_strength)
{
	BKE_editmesh_lnorspace_update(em);
	BM_normals_loops_edges_tag(em->bm, true);
	const int cd_clnors_offset = CustomData_get_offset(&em->bm->ldata, CD_CUSTOMLOOPNORMAL);

	BMesh *bm = em->bm;
	BMFace *f;
	BMLoop *l, *l_cur, *l_first;
	BMIter fiter;

	BMOpSlot *nslot = BMO_slot_get(bmop->slots_out, "normals.out");		/* Per vertex normals depending on hn_mode */

	/* Similar functionality to bm_mesh_loops_calc_normals... Edges that can be smoothed are tagged */
	BM_ITER_MESH(f, &fiter, bm, BM_FACES_OF_MESH) {
		l_cur = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			if ((BM_elem_flag_test(l_cur->v, BM_ELEM_SELECT)) &&
				((!BM_elem_flag_test(l_cur->e, BM_ELEM_TAG)) ||
				(!BM_elem_flag_test(l_cur, BM_ELEM_TAG) && BM_loop_check_cyclic_smooth_fan(l_cur))))
			{
				/* Both adjacent loops are sharp, set clnor to face normal */
				if (!BM_elem_flag_test(l_cur->e, BM_ELEM_TAG) && !BM_elem_flag_test(l_cur->prev->e, BM_ELEM_TAG)) {
					const int loop_index = BM_elem_index_get(l_cur);
					short *clnors = (short*)BM_ELEM_CD_GET_VOID_P(l_cur, cd_clnors_offset);
					BKE_lnor_space_custom_normal_to_data(bm->lnor_spacearr->lspacearr[loop_index], f->no, clnors);
				}
				else {
					/* Find next corresponding sharp edge in this smooth fan */
					BMVert *v_pivot = l_cur->v;
					float *calc_n = (float*)BLI_ghash_lookup(nslot->data.ghash, v_pivot);

					BMEdge *e_next;
					const BMEdge *e_org = l_cur->e;
					BMLoop *lfan_pivot, *lfan_pivot_next;

					lfan_pivot = l_cur;
					e_next = lfan_pivot->e;
					BLI_SMALLSTACK_DECLARE(loops, BMLoop *);
					float cn_wght[3] = { 0.0f, 0.0f, 0.0f }, cn_unwght[3] = { 0.0f, 0.0f, 0.0f };

					/* Fan through current vert and accumulate normals and loops */
					while (true) {
						lfan_pivot_next = BM_vert_step_fan_loop(lfan_pivot, &e_next);
						if (lfan_pivot_next) {
							BLI_assert(lfan_pivot_next->v == v_pivot);
						}
						else {
							e_next = (lfan_pivot->e == e_next) ? lfan_pivot->prev->e : lfan_pivot->e;
						}

						BLI_SMALLSTACK_PUSH_VR(loops, lfan_pivot);
						float cur[3];
						mul_v3_v3fl(cur, lfan_pivot->f->no, BM_face_calc_area(lfan_pivot->f));
						add_v3_v3(cn_wght, cur);

						if (BM_elem_flag_test(lfan_pivot->f, BM_ELEM_SELECT))
							add_v3_v3(cn_unwght, cur);

						if (!BM_elem_flag_test(e_next, BM_ELEM_TAG) || (e_next == e_org)) {
							break;
						}
						lfan_pivot = lfan_pivot_next;
					}

					normalize_v3(cn_wght);
					normalize_v3(cn_unwght);
					if (calc_n) {
						mul_v3_fl(cn_wght, face_strength);
						mul_v3_fl(calc_n, 1.0f - face_strength);
						add_v3_v3(calc_n, cn_wght);
						normalize_v3(calc_n);
					}
					while ((l = (BMLoop*)BLI_SMALLSTACK_POP(loops))) {
						const int l_index = BM_elem_index_get(l);
						short *clnors = (short*)BM_ELEM_CD_GET_VOID_P(l, cd_clnors_offset);
						if (calc_n) {
							BKE_lnor_space_custom_normal_to_data(
								bm->lnor_spacearr->lspacearr[l_index], calc_n, clnors);
						}
						else {
							BKE_lnor_space_custom_normal_to_data(
								bm->lnor_spacearr->lspacearr[l_index], cn_unwght, clnors);
						}
					}
					BLI_ghash_remove(nslot->data.ghash, v_pivot, NULL, MEM_freeN);
				}
			}
		} while ((l_cur = l_cur->next) != l_first);
	}
}

static bool edbm_bevel_init(bContext *C, wmOperator *op, const bool is_modal)
{
	Scene *scene = CTX_data_scene(C);
	BevelData *opdata;
	ViewLayer *view_layer = CTX_data_view_layer(C);
	float pixels_per_inch;
	int i;

	//if (is_modal) {
		Widget_Bevel::offset = 0.0f;
	//}

	op->customdata = opdata = (BevelData*)MEM_mallocN(sizeof(BevelData), "beveldata_mesh_operator");
	uint objects_used_len = 0;

	{
		uint ob_store_len = 0;
		static ObjectsInModeParams params = { OB_MODE_EDIT, true };
		Object **objects = BKE_view_layer_array_from_objects_in_mode_params(view_layer, CTX_wm_view3d(C), &ob_store_len, &params);

		opdata->ob_store = (BevelObjectStore*)MEM_malloc_arrayN(ob_store_len, sizeof(*opdata->ob_store), __func__);
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

	opdata->is_modal = is_modal;
	opdata->value_mode = OFFSET_VALUE;
	opdata->segments = (float)Widget_Bevel::segments;
	pixels_per_inch = U.dpi * U.pixelsize;

	for (i = 0; i < NUM_VALUE_KINDS; i++) {
		opdata->shift_value[i] = -1.0f;
		opdata->initial_length[i] = -1.0f;
		/* note: scale for OFFSET_VALUE will get overwritten in edbm_bevel_invoke */
		opdata->scale[i] = value_scale_per_inch[i] / pixels_per_inch;

		initNumInput(&opdata->num_input[i]);
		opdata->num_input[i].idx_max = 0;
		opdata->num_input[i].val_flag[0] |= NUM_NO_NEGATIVE;
		if (i == SEGMENTS_VALUE) {
			opdata->num_input[i].val_flag[0] |= NUM_NO_FRACTION | NUM_NO_ZERO;
		}
		if (i == OFFSET_VALUE) {
			opdata->num_input[i].unit_sys = scene->unit.system;
		}
		opdata->num_input[i].unit_type[0] = B_UNIT_NONE;  /* Not sure this is a factor or a unit? */
	}

	/* avoid the cost of allocating a bm copy */
	//if (is_modal) {
		View3D *v3d = CTX_wm_view3d(C);
		ARegion *ar = CTX_wm_region(C);

		for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
			opdata->ob_store[ob_index].mesh_backup = EDBM_redo_state_store(opdata->ob_store[ob_index].em);
		}
		opdata->draw_handle_pixel = ED_region_draw_cb_activate(ar->type, ED_region_draw_mouse_line_cb,
			opdata->mcenter, REGION_DRAW_POST_PIXEL);
		G.moving = G_TRANSFORM_EDIT;

		if (v3d) {
			opdata->gizmo_flag = v3d->gizmo_flag;
			v3d->gizmo_flag = V3D_GIZMO_HIDE;
		}
	//}

	return true;
}

static bool edbm_bevel_calc(wmOperator *op)
{
	BevelData *opdata = (BevelData*)op->customdata;
	if (!opdata) {
		return false;
	}
	BMEditMesh *em;
	BMOperator bmop;
	bool changed = false;

	/* TODO_XR */
	const int offset_type = 0; //RNA_enum_get(op->ptr, "offset_type");
	const float profile = 0.0f; //RNA_float_get(op->ptr, "profile");
	const bool clamp_overlap = false; //RNA_boolean_get(op->ptr, "clamp_overlap");
	int material = -1; //RNA_int_get(op->ptr, "material");
	const bool loop_slide = true; //RNA_boolean_get(op->ptr, "loop_slide");
	const bool mark_seam = false; //RNA_boolean_get(op->ptr, "mark_seam");
	const bool mark_sharp = false; //RNA_boolean_get(op->ptr, "mark_sharp");
	const bool harden_normals = false; //RNA_boolean_get(op->ptr, "harden_normals");
	const int face_strength_mode = 0; //RNA_enum_get(op->ptr, "face_strength_mode");
	const int miter_outer = 0; //RNA_enum_get(op->ptr, "miter_outer");
	const int miter_inner = 0; //RNA_enum_get(op->ptr, "miter_inner");
	const float spread = 0.1f; //RNA_float_get(op->ptr, "spread");

	for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
		em = opdata->ob_store[ob_index].em;

		/* revert to original mesh */
		//if (opdata->is_modal) {
			EDBM_redo_state_restore(opdata->ob_store[ob_index].mesh_backup, em, false);
		//}

		Mesh *me = (Mesh*)em->ob->data;

		if (harden_normals && !(me->flag & ME_AUTOSMOOTH)) {
			/* harden_normals only has a visible effect if autosmooth is on, so turn it on */
			me->flag |= ME_AUTOSMOOTH;
		}

		if (em->ob) {
			material = CLAMPIS(material, -1, em->ob->totcol - 1);
		}

		EDBM_op_init(
			em, &bmop, op,
			"bevel geom=%hev offset=%f segments=%i vertex_only=%b offset_type=%i profile=%f "
			"clamp_overlap=%b material=%i loop_slide=%b mark_seam=%b mark_sharp=%b "
			"harden_normals=%b face_strength_mode=%i "
			"miter_outer=%i miter_inner=%i spread=%f smoothresh=%f",
			BM_ELEM_SELECT, Widget_Bevel::offset, Widget_Bevel::segments, Widget_Bevel::vertex_only, offset_type, profile,
			clamp_overlap, material, loop_slide, mark_seam, mark_sharp, harden_normals, face_strength_mode,
			miter_outer, miter_inner, spread, me->smoothresh);

		BMO_op_exec(em->bm, &bmop);

		if (Widget_Bevel::offset != 0.0f) {
			/* not essential, but we may have some loose geometry that
			 * won't get bevel'd and better not leave it selected */
			EDBM_flag_disable_all(em, BM_ELEM_SELECT);
			BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);
		}

		/* no need to de-select existing geometry */
		if (!EDBM_op_finish(em, &bmop, op, true)) {
			continue;
		}

		EDBM_mesh_normals_update(em);

		EDBM_update_generic(em, true, true);
		changed = true;
	}
	return changed;
}

static void edbm_bevel_exit(bContext *C, wmOperator *op)
{
	BevelData *opdata = (BevelData*)op->customdata;
	if (!opdata) {
		return;
	}

	ScrArea *sa = CTX_wm_area(C);
	if (sa) {
		ED_area_status_text(sa, NULL);
	}

	//if (opdata->is_modal) {
		View3D *v3d = CTX_wm_view3d(C);
		ARegion *ar = CTX_wm_region(C);
		for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
			EDBM_redo_state_free(&opdata->ob_store[ob_index].mesh_backup, NULL, false);
		}
		ED_region_draw_cb_exit(ar->type, opdata->draw_handle_pixel);
		if (v3d) {
			v3d->gizmo_flag = opdata->gizmo_flag;
		}
		G.moving = 0;
	//}
	MEM_SAFE_FREE(opdata->ob_store);
	MEM_SAFE_FREE(op->customdata);
	op->customdata = NULL;
}

static void edbm_bevel_cancel(bContext *C, wmOperator *op)
{
	BevelData *opdata = (BevelData*)op->customdata;
	if (!opdata) {
		return;
	}
	//if (opdata->is_modal) {
		for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
			EDBM_redo_state_free(&opdata->ob_store[ob_index].mesh_backup, opdata->ob_store[ob_index].em, true);
			EDBM_update_generic(opdata->ob_store[ob_index].em, false, true);
		}
	//}

	edbm_bevel_exit(C, op);

	/* need to force redisplay or we may still view the modified result */
	ED_region_tag_redraw(CTX_wm_region(C));
}

bool Widget_Bevel::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Bevel::click(VR_UI::Cursor& c)
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

void Widget_Bevel::drag_start(VR_UI::Cursor& c)
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
	p1 = p0 = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];

	/* Execute bevel operation */
	edbm_bevel_init(C, &bevel_dummy_op, false);

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Bevel::obj.do_render[i] = true;
	}
}

void Widget_Bevel::drag_contd(VR_UI::Cursor& c)
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

	p1 = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];
	offset = (p1 - p0).length();
	if (VR_UI::shift_key_get()) {
		offset *= WIDGET_TRANSFORM_TRANS_PRECISION;
	}

	/* Execute bevel operation */
	if (!edbm_bevel_calc(&bevel_dummy_op)) {
		edbm_bevel_cancel(C, &bevel_dummy_op);
	}

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Bevel::obj.do_render[i] = true;
	}
}

void Widget_Bevel::drag_stop(VR_UI::Cursor& c)
{
	if (c.bimanual) {
		return;
	}

	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		return;
	}

	/* Execute bevel operation */
	p1 = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];
	offset = (p1 - p0).length();
	edbm_bevel_exit(C, &bevel_dummy_op);

	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	EDBM_mesh_normals_update(em);
	Widget_Transform::update_manipulator();

	DEG_id_tag_update((ID*)obedit->data, ID_RECALC_GEOMETRY);
	WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
	ED_undo_push(C, "Bevel");

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Bevel::obj.do_render[i] = false;
	}
}

void Widget_Bevel::render(VR_Side side)
{
	if (!Widget_Bevel::obj.do_render[side]) {
		return;
	}

	/* Render measurement (offset) text. */
	//const Mat44f& prior_model_matrix = VR_Draw::get_model_matrix();
	//static Mat44f m;
	//m = VR_UI::hmd_position_get(VR_SPACE_REAL);
	//const Mat44f& c = VR_UI::cursor_position_get(VR_SPACE_REAL, cursor_side);
	//memcpy(m.m[3], c.m[3], sizeof(float) * 3);
	//VR_Draw::update_modelview_matrix(&m, 0);

	VR_Draw::set_depth_test(false, false);
	VR_Draw::set_color(0.8f, 0.8f, 0.8f, 1.0f);

	//static std::string distance;
	//sprintf((char*)distance.data(), "%.3f", offset);
	//VR_Draw::render_string(distance.c_str(), 0.02f, 0.02f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.08f, 0.001f);
	
	//VR_Draw::set_depth_test(true, true);
	//VR_Draw::update_modelview_matrix(&prior_model_matrix, 0);

	/* Render dashed line from center. */
	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	GPU_line_width(10.0f);

	static const float c_black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);
	immBeginAtMost(GPU_PRIM_LINES, 2);
	immUniformColor4fv(c_black);
	immUniform1f("dash_width", 6.0f);

	immVertex3fv(pos, (float*)&p0);
	immVertex3fv(pos, (float*)&p1);

	if (p0 == p1) {
		/* cyclic */
		immVertex3fv(pos, (float*)&p0);
	}
	immEnd();
	immUnbindProgram();

	Widget_Bevel::obj.do_render[side] = false;
}
