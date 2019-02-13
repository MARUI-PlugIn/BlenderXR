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

/** \file blender/vr/intern/vr_widget_loopcut.cpp
*   \ingroup vr
*
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_loopcut.h"
#include "vr_widget_transform.h"

#include "vr_math.h"
#include "vr_draw.h"

#include "BLI_alloca.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_linklist_stack.h"
#include "BLI_string.h"
#include "BLI_utildefines_stack.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_bvh.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_unit.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DNA_mesh_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_numinput.h"
#include "ED_undo.h"

#include "GPU_immediate.h"
#include "GPU_state.h"
#include "GPU_matrix.h"

#include "MEM_guardedalloc.h"
#include "mesh_intern.h"

#include "transform.h"

#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

/* Multiplier for one and two-handed scaling transformations. */
#define WIDGET_TRANSFORM_SCALING_SENSITIVITY 0.5f

/* Precision multipliers. */
#define WIDGET_TRANSFORM_TRANS_PRECISION 0.1f
#define WIDGET_TRANSFORM_ROT_PRECISION (PI/36.0f)
#define WIDGET_TRANSFORM_SCALE_PRECISION 0.005f

/* Sensitivity multiplier for interactions. */
#define WIDGET_LOOPCUT_SENSITIVITY 3.0f

#include "vr_util.h"

/***********************************************************************************************//**
 * \class									Widget_LoopCut
 ***************************************************************************************************
 * Interaction widget for the Loop Cut tool.
 *
 **************************************************************************************************/
Widget_LoopCut Widget_LoopCut::obj;

Coord3Df Widget_LoopCut::p0;
Coord3Df Widget_LoopCut::p1;
Coord3Df Widget_LoopCut::p0_b;
Coord3Df Widget_LoopCut::p1_b;
bool Widget_LoopCut::selection_empty(true);

bool Widget_LoopCut::edge_slide(false);

int Widget_LoopCut::object_index(0);
int Widget_LoopCut::edge_index(0);

float Widget_LoopCut::percent(0.0f);
int Widget_LoopCut::cuts(1);
bool Widget_LoopCut::double_side(true);
bool Widget_LoopCut::even(false);
bool Widget_LoopCut::flipped(false);
bool Widget_LoopCut::clamp(true);

/* TransInfo struct for edge slide operation. */
static TransInfo loopcut_info;

/* Dummy op */
static wmOperator loopcut_dummy_op;

/* From edimesh_preselect_edgering.c */
struct EditMesh_PreSelEdgeRing {
	float(*edges)[2][3];
	int     edges_len;

	float(*verts)[3];
	int     verts_len;
};

/* From editmesh_loopcut.c */
#define SUBD_SMOOTH_MAX 4.0f
#define SUBD_CUTS_MAX 500

/* ringsel operator */

/* struct for properties used while drawing */
typedef struct RingSelOpData {
	ARegion *ar;        /* region that ringsel was activated in */
	void *draw_handle;  /* for drawing preview loop */

	struct EditMesh_PreSelEdgeRing *presel_edgering;

	ViewContext vc;

	Depsgraph *depsgraph;

	Object **objects;
	uint     objects_len;

	/* These values switch objects based on the object under the cursor. */
	uint ob_index;
	Object *ob;
	BMEditMesh *em;
	BMEdge *eed;

	NumInput num;

	bool extend;
	bool do_cut;

	float cuts;         /* cuts as float so smooth mouse pan works in small increments */
	float smoothness;
} RingSelOpData;

static void edgering_select(RingSelOpData *lcd)
{
	if (!lcd->eed) {
		return;
	}

	if (!lcd->extend) {
		for (uint ob_index = 0; ob_index < lcd->objects_len; ob_index++) {
			Object *ob_iter = lcd->objects[ob_index];
			BMEditMesh *em = BKE_editmesh_from_object(ob_iter);
			EDBM_flag_disable_all(em, BM_ELEM_SELECT);
			DEG_id_tag_update((ID*)ob_iter->data, ID_RECALC_SELECT);
			WM_main_add_notifier(NC_GEOM | ND_SELECT, ob_iter->data);
		}
	}

	BMEditMesh *em = lcd->em;
	BMEdge *eed_start = lcd->eed;
	BMWalker walker;
	BMEdge *eed;
	BMW_init(&walker, em->bm, BMW_EDGERING,
		BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
		BMW_FLAG_TEST_HIDDEN,
		BMW_NIL_LAY);

	for (eed = (BMEdge*)BMW_begin(&walker, eed_start); eed; eed = (BMEdge*)BMW_step(&walker)) {
		BM_edge_select_set(em->bm, eed, true);
	}
	BMW_end(&walker);
}

static void ringsel_find_edge(RingSelOpData *lcd, const int previewlines)
{
	if (lcd->eed) {
		const float(*coords)[3] = NULL;
		{
			Mesh *me_eval = (Mesh *)DEG_get_evaluated_id(lcd->depsgraph, (ID*)lcd->ob->data);
			if (me_eval->runtime.edit_data) {
				coords = me_eval->runtime.edit_data->vertexCos;
			}
		}
		EDBM_preselect_edgering_update_from_edge(lcd->presel_edgering, lcd->em->bm, lcd->eed, previewlines, coords);
	}
	else {
		EDBM_preselect_edgering_clear(lcd->presel_edgering);
	}
}

/* called when modal loop selection gets set up... */
static int ringsel_init(bContext *C, wmOperator *op, bool do_cut)
{
	RingSelOpData *lcd;
	Scene *scene = CTX_data_scene(C);

	/* alloc new customdata */
	op->customdata = MEM_callocN(sizeof(RingSelOpData), "ringsel Modal Op Data");
	lcd = (RingSelOpData*)op->customdata;

	em_setup_viewcontext(C, &lcd->vc);

	lcd->depsgraph = CTX_data_depsgraph(C);

	/* assign the drawing handle for drawing preview line... */
	lcd->ar = CTX_wm_region(C);
	//lcd->draw_handle = ED_region_draw_cb_activate(lcd->ar->type, ringsel_draw, lcd, REGION_DRAW_POST_VIEW);
	lcd->presel_edgering = EDBM_preselect_edgering_create();
	/* Initialize once the cursor is over a mesh. */
	lcd->ob = NULL;
	lcd->em = NULL;
	/* TODO_XR */
	lcd->extend = do_cut ? false : true; //RNA_boolean_get(op->ptr, "extend");
	lcd->do_cut = do_cut;
	lcd->cuts = Widget_LoopCut::cuts;
	lcd->smoothness = 0.0f; //RNA_float_get(op->ptr, "smoothness");

	initNumInput(&lcd->num);
	lcd->num.idx_max = 1;
	lcd->num.val_flag[0] |= NUM_NO_NEGATIVE | NUM_NO_FRACTION;
	/* No specific flags for smoothness. */
	lcd->num.unit_sys = scene->unit.system;
	lcd->num.unit_type[0] = B_UNIT_NONE;
	lcd->num.unit_type[1] = B_UNIT_NONE;

	ED_region_tag_redraw(lcd->ar);

	return 1;
}

static void ringsel_finish(bContext *C, wmOperator *op)
{
	RingSelOpData *lcd = (RingSelOpData*)op->customdata;
	/* TODO_XR */
	const float smoothness = 0.0f; //RNA_float_get(op->ptr, "smoothness");
	const int smooth_falloff = 7; //RNA_enum_get(op->ptr, "falloff");
#ifdef BMW_EDGERING_NGON
	const bool use_only_quads = false;
#else
	const bool use_only_quads = false;
#endif

	if (lcd->eed) {
		BMEditMesh *em = lcd->em;
		BMVert *v_eed_orig[2] = { lcd->eed->v1, lcd->eed->v2 };

		edgering_select(lcd);

		if (lcd->do_cut) {
			const bool is_macro = (op->opm != NULL);
			/* a single edge (rare, but better support) */
			const bool is_single = (BM_edge_is_wire(lcd->eed));
			const int seltype = is_single ? SUBDIV_SELECT_INNER : SUBDIV_SELECT_LOOPCUT;

			/* Enable gridfill, so that intersecting loopcut works as one would expect.
			 * Note though that it will break edgeslide in this specific case.
			 * See [#31939]. */
			BM_mesh_esubdivide(em->bm, BM_ELEM_SELECT,
				smoothness, smooth_falloff, true,
				0.0f, 0.0f,
				Widget_LoopCut::cuts, seltype, SUBD_CORNER_PATH, 0, true,
				use_only_quads, 0);

			/* when used in a macro the tessfaces will be recalculated anyway,
			 * this is needed here because modifiers depend on updated tessellation, see T45920 */
			EDBM_update_generic(em, true, true);

			if (is_single) {
				/* de-select endpoints */
				BM_vert_select_set(em->bm, v_eed_orig[0], false);
				BM_vert_select_set(em->bm, v_eed_orig[1], false);

				EDBM_selectmode_flush_ex(lcd->em, SCE_SELECT_VERTEX);
			}
			/* we cant slide multiple edges in vertex select mode */
			else if (is_macro && (Widget_LoopCut::cuts > 1) && (em->selectmode & SCE_SELECT_VERTEX)) {
				EDBM_selectmode_disable(lcd->vc.scene, em, SCE_SELECT_VERTEX, SCE_SELECT_EDGE);
			}
			/* force edge slide to edge select mode in in face select mode */
			else if (EDBM_selectmode_disable(lcd->vc.scene, em, SCE_SELECT_FACE, SCE_SELECT_EDGE)) {
				/* pass, the change will flush selection */
			}
			else {
				/* else flush explicitly */
				EDBM_selectmode_flush(lcd->em);
			}
		}
		else {
			/* XXX Is this piece of code ever used now? Simple loop select is now
			 *     in editmesh_select.c (around line 1000)... */
			 /* sets as active, useful for other tools */
			if (em->selectmode & SCE_SELECT_VERTEX)
				BM_select_history_store(em->bm, lcd->eed->v1);  /* low priority TODO, get vertrex close to mouse */
			if (em->selectmode & SCE_SELECT_EDGE)
				BM_select_history_store(em->bm, lcd->eed);

			EDBM_selectmode_flush(lcd->em);
			DEG_id_tag_update((ID*)lcd->ob->data, ID_RECALC_SELECT);
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, lcd->ob->data);
		}
	}
}

/* called when modal loop selection is done... */
static void ringsel_exit(bContext *UNUSED(C), wmOperator *op)
{
	RingSelOpData *lcd = (RingSelOpData*)op->customdata;

	/* deactivate the extra drawing stuff in 3D-View */
	ED_region_draw_cb_exit(lcd->ar->type, lcd->draw_handle);

	EDBM_preselect_edgering_destroy(lcd->presel_edgering);

	if (lcd->objects) {
		MEM_freeN(lcd->objects);
	}

	ED_region_tag_redraw(lcd->ar);

	/* free the custom data */
	MEM_freeN(lcd);
	op->customdata = NULL;
}

static void loopcut_update_edge(RingSelOpData *lcd, uint ob_index, BMEdge *e, const int previewlines)
{
	if (e != lcd->eed) {
		lcd->eed = e;
		lcd->ob = lcd->vc.obedit;
		lcd->ob_index = ob_index;
		lcd->em = lcd->vc.em;
		ringsel_find_edge(lcd, previewlines);
	}
	else if (e == NULL) {
		lcd->ob = NULL;
		lcd->em = NULL;
		lcd->ob_index = UINT_MAX;
	}
}

static void loopcut_mouse_move(RingSelOpData *lcd, const int previewlines)
{
	struct {
		Object *ob;
		BMEdge *eed;
		float dist;
		int ob_index;
	} best;
	best.ob = NULL;
	best.eed = NULL;
	best.dist = ED_view3d_select_dist_px();
	best.ob_index = 0;

	for (uint ob_index = 0; ob_index < lcd->objects_len; ob_index++) {
		Object *ob_iter = lcd->objects[ob_index];
		ED_view3d_viewcontext_init_object(&lcd->vc, ob_iter);
		BMEdge *eed_test = EDBM_edge_find_nearest_ex(&lcd->vc, &best.dist, NULL, false, false, NULL);
		if (eed_test) {
			best.ob = ob_iter;
			best.eed = eed_test;
			best.ob_index = ob_index;
		}
	}

	if (best.eed) {
		ED_view3d_viewcontext_init_object(&lcd->vc, best.ob);
	}

	loopcut_update_edge(lcd, best.ob_index, best.eed, previewlines);
}

static int loopcut_init(bContext *C, wmOperator *op, const wmEvent *event)
{
	const bool is_interactive = (event != NULL);

	/* Use for redo - intentionally wrap int to uint. */
	struct {
		uint ob_index;
		uint e_index;
	} exec_data;
	exec_data.ob_index = (uint)Widget_LoopCut::object_index;
	exec_data.e_index = (uint)Widget_LoopCut::edge_index;

	ViewLayer *view_layer = CTX_data_view_layer(C);

	uint objects_len;
	static ObjectsInModeParams params = { OB_MODE_EDIT, true };
	Object **objects = BKE_view_layer_array_from_objects_in_mode_params(view_layer, CTX_wm_view3d(C), &objects_len, &params);

	if (is_interactive) {
		for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
			Object *ob_iter = objects[ob_index];
			if (modifiers_isDeformedByLattice(ob_iter) || modifiers_isDeformedByArmature(ob_iter)) {
				//BKE_report(op->reports, RPT_WARNING, "Loop cut does not work well on deformed edit mesh display");
				break;
			}
		}
	}

	view3d_operator_needs_opengl(C);

	/* for re-execution, check edge index is in range before we setup ringsel */
	bool ok = true;
	if (is_interactive == false) {
		if (exec_data.ob_index >= objects_len) {
			return OPERATOR_CANCELLED;
			ok = false;
		}
		else {
			Object *ob_iter = objects[exec_data.ob_index];
			BMEditMesh *em = BKE_editmesh_from_object(ob_iter);
			if (exec_data.e_index >= em->bm->totedge) {
				ok = false;
			}
		}
	}

	if (!ok || !ringsel_init(C, op, true)) {
		MEM_freeN(objects);
		return OPERATOR_CANCELLED;
	}

	/* add a modal handler for this operator - handles loop selection */
	if (is_interactive) {
		op->flag |= OP_IS_MODAL_CURSOR_REGION;
		WM_event_add_modal_handler(C, op);
	}

	RingSelOpData *lcd = (RingSelOpData*)op->customdata;

	lcd->objects = objects;
	lcd->objects_len = objects_len;

	//if (is_interactive) {
		ARegion* ar = CTX_wm_region(C);
		RegionView3D *rv3d = (RegionView3D *)ar->regiondata;
		float projmat[4][4];
		mul_m4_m4m4(projmat, (float(*)[4])rv3d->winmat, (float(*)[4])rv3d->viewmat);
		float v1[3] = { Widget_LoopCut::p1.x, Widget_LoopCut::p1.y, Widget_LoopCut::p1.z };
		mul_project_m4_v3(projmat, v1);
		lcd->vc.mval[0] = (int)((ar->winx / 2.0f) + (ar->winx / 2.0f) * v1[0]);
		lcd->vc.mval[1] = (int)((ar->winy / 2.0f) + (ar->winy / 2.0f) * v1[1]);

		loopcut_mouse_move(lcd, is_interactive ? 1 : 0);
	/*}
	else {
		Object *ob_iter = objects[exec_data.ob_index];
		ED_view3d_viewcontext_init_object(&lcd->vc, ob_iter);

		BMEdge *e;
		BM_mesh_elem_table_ensure(lcd->vc.em->bm, BM_EDGE);
		e = BM_edge_at_index(lcd->vc.em->bm, exec_data.e_index);
		loopcut_update_edge(lcd, exec_data.ob_index, e, 0);
	}*/

#ifdef USE_LOOPSLIDE_HACK
	/* for use in macro so we can restore, HACK */
	/*{
		Scene *scene = CTX_data_scene(C);
		ToolSettings *settings = scene->toolsettings;
		const bool mesh_select_mode[3] = {
			(settings->selectmode & SCE_SELECT_VERTEX) != 0,
			(settings->selectmode & SCE_SELECT_EDGE) != 0,
			(settings->selectmode & SCE_SELECT_FACE) != 0,
		};

		RNA_boolean_set_array(op->ptr, "mesh_select_mode_init", mesh_select_mode);
	}*/
#endif

	if (is_interactive) {
		ED_workspace_status_text(C, IFACE_("Select a ring to be cut, use mouse-wheel or page-up/down for number of cuts, "
			"hold Alt for smooth"));
		return OPERATOR_RUNNING_MODAL;
	}
	else {
		ringsel_finish(C, op);
		ringsel_exit(C, op);
		return OPERATOR_FINISHED;
	}
}

/* Adapted from loopcut_init() in editmesh_loopcut.c */
static int ringsel_update(bContext *C, wmOperator *op)
{
	/* Use for redo - intentionally wrap int to uint. */
	struct {
		uint ob_index;
		uint e_index;
	} exec_data;
	exec_data.ob_index = (uint)Widget_LoopCut::object_index;
	exec_data.e_index = (uint)Widget_LoopCut::edge_index;

	ViewLayer *view_layer = CTX_data_view_layer(C);

	uint objects_len;
	static ObjectsInModeParams params = { OB_MODE_EDIT, true };
	Object **objects = BKE_view_layer_array_from_objects_in_mode_params(view_layer, CTX_wm_view3d(C), &objects_len, &params);

	view3d_operator_needs_opengl(C);

	/* for re-execution, check edge index is in range before we setup ringsel */
	bool ok = true;
	if (exec_data.ob_index >= objects_len) {
		return OPERATOR_CANCELLED;
		ok = false;
	}
	else {
		Object *ob_iter = objects[exec_data.ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(ob_iter);
		if (exec_data.e_index >= em->bm->totedge) {
			ok = false;
		}
	}

	if (!ok) {
		MEM_freeN(objects);
		return OPERATOR_CANCELLED;
	}

	RingSelOpData *lcd = (RingSelOpData*)op->customdata;

	lcd->objects = objects;
	lcd->objects_len = objects_len;

	ARegion* ar = CTX_wm_region(C);
	RegionView3D *rv3d = (RegionView3D *)ar->regiondata;
	float projmat[4][4];
	mul_m4_m4m4(projmat, (float(*)[4])rv3d->winmat, (float(*)[4])rv3d->viewmat);
	float v1[3] = { Widget_LoopCut::p1.x, Widget_LoopCut::p1.y, Widget_LoopCut::p1.z };
	mul_project_m4_v3(projmat, v1);
	lcd->vc.mval[0] = (int)((ar->winx / 2.0f) + (ar->winx / 2.0f) * v1[0]);
	lcd->vc.mval[1] = (int)((ar->winy / 2.0f) + (ar->winy / 2.0f) * v1[1]);

	loopcut_mouse_move(lcd, Widget_LoopCut::cuts);//1);

	return OPERATOR_RUNNING_MODAL;
}

/* From transform.c */
static void slide_origdata_init_flag(
	TransInfo *t, TransDataContainer *tc, SlideOrigData *sod)
{
	BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
	BMesh *bm = em->bm;
	const bool has_layer_math = CustomData_has_math(&bm->ldata);
	const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);

	if ((t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT) &&
		/* don't do this at all for non-basis shape keys, too easy to
		 * accidentally break uv maps or vertex colors then */
		(bm->shapenr <= 1) &&
		(has_layer_math || (cd_loop_mdisp_offset != -1)))
	{
		sod->use_origfaces = true;
		sod->cd_loop_mdisp_offset = cd_loop_mdisp_offset;
	}
	else {
		sod->use_origfaces = false;
		sod->cd_loop_mdisp_offset = -1;
	}
}

static void slide_origdata_init_data(
	TransDataContainer *tc, SlideOrigData *sod)
{
	if (sod->use_origfaces) {
		BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
		BMesh *bm = em->bm;

		sod->origfaces = BLI_ghash_ptr_new(__func__);
		BMeshCreateParams params = { 0 };
		params.use_toolflags = false;

		sod->bm_origfaces = BM_mesh_create(
			&bm_mesh_allocsize_default,
			&params);
		/* we need to have matching customdata */
		BM_mesh_copy_init_customdata(sod->bm_origfaces, bm, NULL);
	}
}

static void slide_origdata_create_data_vert(
	BMesh *bm, SlideOrigData *sod,
	TransDataGenericSlideVert *sv)
{
	BMIter liter;
	int j, l_num;
	float *loop_weights;

	/* copy face data */
	// BM_ITER_ELEM (l, &liter, sv->v, BM_LOOPS_OF_VERT) {
	BM_iter_init(&liter, bm, BM_LOOPS_OF_VERT, sv->v);
	l_num = liter.count;
	loop_weights = (float*)BLI_array_alloca(loop_weights, l_num);
	for (j = 0; j < l_num; j++) {
		BMLoop *l = (BMLoop*)BM_iter_step(&liter);
		BMLoop *l_prev, *l_next;
		void **val_p;
		if (!BLI_ghash_ensure_p(sod->origfaces, l->f, &val_p)) {
			BMFace *f_copy = BM_face_copy(sod->bm_origfaces, bm, l->f, true, true);
			*val_p = f_copy;
		}

		if ((l_prev = BM_loop_find_prev_nodouble(l, l->next, FLT_EPSILON)) &&
			(l_next = BM_loop_find_next_nodouble(l, l_prev, FLT_EPSILON)))
		{
			loop_weights[j] = angle_v3v3v3(l_prev->v->co, l->v->co, l_next->v->co);
		}
		else {
			loop_weights[j] = 0.0f;
		}

	}

	/* store cd_loop_groups */
	if (sod->layer_math_map_num && (l_num != 0)) {
		sv->cd_loop_groups = (LinkNode**)BLI_memarena_alloc(sod->arena, sod->layer_math_map_num * sizeof(void *));
		for (j = 0; j < sod->layer_math_map_num; j++) {
			const int layer_nr = sod->layer_math_map[j];
			sv->cd_loop_groups[j] = BM_vert_loop_groups_data_layer_create(bm, sv->v, layer_nr, loop_weights, sod->arena);
		}
	}
	else {
		sv->cd_loop_groups = NULL;
	}

	BLI_ghash_insert(sod->origverts, sv->v, sv);
}

static void slide_origdata_create_data(
	TransInfo *t, TransDataContainer *tc, SlideOrigData *sod,
	TransDataGenericSlideVert *sv_array, unsigned int v_stride, unsigned int v_num)
{
	if (sod->use_origfaces) {
		BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
		BMesh *bm = em->bm;
		unsigned int i;
		TransDataGenericSlideVert *sv;

		int layer_index_dst;
		int j;

		layer_index_dst = 0;

		if (CustomData_has_math(&bm->ldata)) {
			/* over alloc, only 'math' layers are indexed */
			sod->layer_math_map = (int*)MEM_mallocN(bm->ldata.totlayer * sizeof(int), __func__);
			for (j = 0; j < bm->ldata.totlayer; j++) {
				if (CustomData_layer_has_math(&bm->ldata, j)) {
					sod->layer_math_map[layer_index_dst++] = j;
				}
			}
			BLI_assert(layer_index_dst != 0);
		}

		sod->layer_math_map_num = layer_index_dst;

		sod->arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

		sod->origverts = BLI_ghash_ptr_new_ex(__func__, v_num);

		for (i = 0, sv = sv_array; i < v_num; i++, sv = (TransDataGenericSlideVert*)POINTER_OFFSET(sv, v_stride)) {
			slide_origdata_create_data_vert(bm, sod, sv);
		}

		if (tc->mirror.axis_flag) {
			TransData *td = tc->data;
			TransDataGenericSlideVert *sv_mirror;

			sod->sv_mirror = (TransDataGenericSlideVert*)MEM_callocN(sizeof(*sv_mirror) * tc->data_len, __func__);
			sod->totsv_mirror = tc->data_len;

			sv_mirror = sod->sv_mirror;

			for (i = 0; i < tc->data_len; i++, td++) {
				BMVert *eve = (BMVert*)td->extra;
				if (eve) {
					sv_mirror->v = eve;
					copy_v3_v3(sv_mirror->co_orig_3d, eve->co);

					slide_origdata_create_data_vert(bm, sod, sv_mirror);
					sv_mirror++;
				}
				else {
					sod->totsv_mirror--;
				}
			}

			if (sod->totsv_mirror == 0) {
				MEM_freeN(sod->sv_mirror);
				sod->sv_mirror = NULL;
			}
		}
	}
}

static void calcEdgeSlideCustomPoints(struct TransInfo *t)
{
	EdgeSlideData *sld = (EdgeSlideData*)TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data;

	setCustomPoints(t, &t->mouse, sld->mval_end, sld->mval_start);

	/* setCustomPoints isn't normally changing as the mouse moves,
	 * in this case apply mouse input immediately so we don't refresh
	 * with the value from the previous points */
	applyMouseInput(t, &t->mouse, t->mval, t->values);
}


static BMEdge *get_other_edge(BMVert *v, BMEdge *e)
{
	BMIter iter;
	BMEdge *e_iter;

	BM_ITER_ELEM(e_iter, &iter, v, BM_EDGES_OF_VERT) {
		if (BM_elem_flag_test(e_iter, BM_ELEM_SELECT) && e_iter != e) {
			return e_iter;
		}
	}

	return NULL;
}

/* interpoaltes along a line made up of 2 segments (used for edge slide) */
static void interp_line_v3_v3v3v3(float p[3], const float v1[3], const float v2[3], const float v3[3], float t)
{
	float t_mid, t_delta;

	/* could be pre-calculated */
	t_mid = line_point_factor_v3(v2, v1, v3);

	t_delta = t - t_mid;
	if (t_delta < 0.0f) {
		if (UNLIKELY(fabsf(t_mid) < FLT_EPSILON)) {
			copy_v3_v3(p, v2);
		}
		else {
			interp_v3_v3v3(p, v1, v2, t / t_mid);
		}
	}
	else {
		t = t - t_mid;
		t_mid = 1.0f - t_mid;

		if (UNLIKELY(fabsf(t_mid) < FLT_EPSILON)) {
			copy_v3_v3(p, v3);
		}
		else {
			interp_v3_v3v3(p, v2, v3, t / t_mid);
		}
	}
}

/**
 * Find the closest point on the ngon on the opposite side.
 * used to set the edge slide distance for ngons.
 */
static bool bm_loop_calc_opposite_co(BMLoop *l_tmp,
	const float plane_no[3],
	float r_co[3])
{
	/* skip adjacent edges */
	BMLoop *l_first = l_tmp->next;
	BMLoop *l_last = l_tmp->prev;
	BMLoop *l_iter;
	float dist = FLT_MAX;
	bool found = false;

	l_iter = l_first;
	do {
		float tvec[3];
		if (isect_line_plane_v3(tvec,
			l_iter->v->co, l_iter->next->v->co,
			l_tmp->v->co, plane_no))
		{
			const float fac = line_point_factor_v3(tvec, l_iter->v->co, l_iter->next->v->co);
			/* allow some overlap to avoid missing the intersection because of float precision */
			if ((fac > -FLT_EPSILON) && (fac < 1.0f + FLT_EPSILON)) {
				/* likelihood of multiple intersections per ngon is quite low,
				 * it would have to loop back on its self, but better support it
				 * so check for the closest opposite edge */
				const float tdist = len_v3v3(l_tmp->v->co, tvec);
				if (tdist < dist) {
					copy_v3_v3(r_co, tvec);
					dist = tdist;
					found = true;
				}
			}
		}
	} while ((l_iter = l_iter->next) != l_last);

	return found;
}

/**
 * Given 2 edges and a loop, step over the loops
 * and calculate a direction to slide along.
 *
 * \param r_slide_vec: the direction to slide,
 * the length of the vector defines the slide distance.
 */
static BMLoop *get_next_loop(BMVert *v, BMLoop *l,
	BMEdge *e_prev, BMEdge *e_next, float r_slide_vec[3])
{
	BMLoop *l_first;
	float vec_accum[3] = { 0.0f, 0.0f, 0.0f };
	float vec_accum_len = 0.0f;
	int i = 0;

	BLI_assert(BM_edge_share_vert(e_prev, e_next) == v);
	BLI_assert(BM_vert_in_edge(l->e, v));

	l_first = l;
	do {
		l = BM_loop_other_edge_loop(l, v);

		if (l->e == e_next) {
			if (i) {
				normalize_v3_length(vec_accum, vec_accum_len / (float)i);
			}
			else {
				/* When there is no edge to slide along,
				 * we must slide along the vector defined by the face we're attach to */
				BMLoop *l_tmp = BM_face_vert_share_loop(l_first->f, v);

				BLI_assert(ELEM(l_tmp->e, e_prev, e_next) && ELEM(l_tmp->prev->e, e_prev, e_next));

				if (l_tmp->f->len == 4) {
					/* we could use code below, but in this case
					 * sliding diagonally across the quad works well */
					sub_v3_v3v3(vec_accum, l_tmp->next->next->v->co, v->co);
				}
				else {
					float tdir[3];
					BM_loop_calc_face_direction(l_tmp, tdir);
					cross_v3_v3v3(vec_accum, l_tmp->f->no, tdir);
#if 0
					/* rough guess, we can  do better! */
					normalize_v3_length(vec_accum, (BM_edge_calc_length(e_prev) + BM_edge_calc_length(e_next)) / 2.0f);
#else
					/* be clever, check the opposite ngon edge to slide into.
					 * this gives best results */
					{
						float tvec[3];
						float dist;

						if (bm_loop_calc_opposite_co(l_tmp, tdir, tvec)) {
							dist = len_v3v3(l_tmp->v->co, tvec);
						}
						else {
							dist = (BM_edge_calc_length(e_prev) + BM_edge_calc_length(e_next)) / 2.0f;
						}

						normalize_v3_length(vec_accum, dist);
					}
#endif
				}
			}

			copy_v3_v3(r_slide_vec, vec_accum);
			return l;
		}
		else {
			/* accumulate the normalized edge vector,
			 * normalize so some edges don't skew the result */
			float tvec[3];
			sub_v3_v3v3(tvec, BM_edge_other_vert(l->e, v)->co, v->co);
			vec_accum_len += normalize_v3(tvec);
			add_v3_v3(vec_accum, tvec);
			i += 1;
		}

		if (BM_loop_other_edge_loop(l, v)->e == e_next) {
			if (i) {
				normalize_v3_length(vec_accum, vec_accum_len / (float)i);
			}

			copy_v3_v3(r_slide_vec, vec_accum);
			return BM_loop_other_edge_loop(l, v);
		}

	} while ((l != l->radial_next) &&
		((l = l->radial_next) != l_first));

	if (i) {
		normalize_v3_length(vec_accum, vec_accum_len / (float)i);
	}

	copy_v3_v3(r_slide_vec, vec_accum);

	return NULL;
}

/**
 * Calculate screenspace `mval_start` / `mval_end`, optionally slide direction.
 */
static void calcEdgeSlide_mval_range(
	TransInfo *t, TransDataContainer *tc, EdgeSlideData *sld, const int *sv_table, const int loop_nr,
	const float mval[2], const bool use_occlude_geometry, const bool use_calc_direction)
{
	TransDataEdgeSlideVert *sv_array = sld->sv;
	BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
	BMesh *bm = em->bm;
	ARegion *ar = t->ar;
	View3D *v3d = NULL;
	RegionView3D *rv3d = NULL;
	float projectMat[4][4];
	BMBVHTree *bmbvh;

	/* only for use_calc_direction */
	float(*loop_dir)[3] = NULL, *loop_maxdist = NULL;

	float mval_start[2], mval_end[2];
	float mval_dir[3], dist_best_sq;
	BMIter iter;
	BMEdge *e;

	if (t->spacetype == SPACE_VIEW3D) {
		/* background mode support */
		v3d = (View3D*)(t->sa ? t->sa->spacedata.first : NULL);
		rv3d = (RegionView3D*)(t->ar ? t->ar->regiondata : NULL);
	}

	if (!rv3d) {
		/* ok, let's try to survive this */
		unit_m4(projectMat);
	}
	else {
		ED_view3d_ob_project_mat_get(rv3d, tc->obedit, projectMat);
	}

	if (use_occlude_geometry) {
		bmbvh = BKE_bmbvh_new_from_editmesh(em, BMBVH_RESPECT_HIDDEN, NULL, false);
	}
	else {
		bmbvh = NULL;
	}

	/* find mouse vectors, the global one, and one per loop in case we have
	 * multiple loops selected, in case they are oriented different */
	zero_v3(mval_dir);
	dist_best_sq = -1.0f;

	if (use_calc_direction) {
		loop_dir = (float(*)[3])MEM_callocN(sizeof(float[3]) * loop_nr, "sv loop_dir");
		loop_maxdist = (float*)MEM_mallocN(sizeof(float) * loop_nr, "sv loop_maxdist");
		copy_vn_fl(loop_maxdist, loop_nr, -1.0f);
	}

	BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
			int i;

			/* search cross edges for visible edge to the mouse cursor,
			 * then use the shared vertex to calculate screen vector*/
			for (i = 0; i < 2; i++) {
				BMIter iter_other;
				BMEdge *e_other;

				BMVert *v = i ? e->v1 : e->v2;
				BM_ITER_ELEM(e_other, &iter_other, v, BM_EDGES_OF_VERT) {
					/* screen-space coords */
					float sco_a[3], sco_b[3];
					float dist_sq;
					int j, l_nr;

					if (BM_elem_flag_test(e_other, BM_ELEM_SELECT))
						continue;

					/* This test is only relevant if object is not wire-drawn! See [#32068]. */
					if (use_occlude_geometry &&
						!BMBVH_EdgeVisible(bmbvh, e_other, t->depsgraph, ar, v3d, tc->obedit))
					{
						continue;
					}

					BLI_assert(sv_table[BM_elem_index_get(v)] != -1);
					j = sv_table[BM_elem_index_get(v)];

					if (sv_array[j].v_side[1]) {
						ED_view3d_project_float_v3_m4(ar, sv_array[j].v_side[1]->co, sco_b, projectMat);
					}
					else {
						add_v3_v3v3(sco_b, v->co, sv_array[j].dir_side[1]);
						ED_view3d_project_float_v3_m4(ar, sco_b, sco_b, projectMat);
					}

					if (sv_array[j].v_side[0]) {
						ED_view3d_project_float_v3_m4(ar, sv_array[j].v_side[0]->co, sco_a, projectMat);
					}
					else {
						add_v3_v3v3(sco_a, v->co, sv_array[j].dir_side[0]);
						ED_view3d_project_float_v3_m4(ar, sco_a, sco_a, projectMat);
					}

					/* global direction */
					dist_sq = dist_squared_to_line_segment_v2(mval, sco_b, sco_a);
					if ((dist_best_sq == -1.0f) ||
						/* intentionally use 2d size on 3d vector */
						(dist_sq < dist_best_sq && (len_squared_v2v2(sco_b, sco_a) > 0.1f)))
					{
						dist_best_sq = dist_sq;
						sub_v3_v3v3(mval_dir, sco_b, sco_a);
					}

					if (use_calc_direction) {
						/* per loop direction */
						l_nr = sv_array[j].loop_nr;
						if (loop_maxdist[l_nr] == -1.0f || dist_sq < loop_maxdist[l_nr]) {
							loop_maxdist[l_nr] = dist_sq;
							sub_v3_v3v3(loop_dir[l_nr], sco_b, sco_a);
						}
					}
				}
			}
		}
	}

	if (use_calc_direction) {
		int i;
		sv_array = sld->sv;
		for (i = 0; i < sld->totsv; i++, sv_array++) {
			/* switch a/b if loop direction is different from global direction */
			int l_nr = sv_array->loop_nr;
			if (dot_v3v3(loop_dir[l_nr], mval_dir) < 0.0f) {
				swap_v3_v3(sv_array->dir_side[0], sv_array->dir_side[1]);
				SWAP(BMVert *, sv_array->v_side[0], sv_array->v_side[1]);
			}
		}

		MEM_freeN(loop_dir);
		MEM_freeN(loop_maxdist);
	}

	/* possible all of the edge loops are pointing directly at the view */
	if (UNLIKELY(len_squared_v2(mval_dir) < 0.1f)) {
		mval_dir[0] = 0.0f;
		mval_dir[1] = 100.0f;
	}

	/* zero out start */
	zero_v2(mval_start);

	/* dir holds a vector along edge loop */
	copy_v2_v2(mval_end, mval_dir);
	mul_v2_fl(mval_end, 0.5f);

	sld->mval_start[0] = t->mval[0] + mval_start[0];
	sld->mval_start[1] = t->mval[1] + mval_start[1];

	sld->mval_end[0] = t->mval[0] + mval_end[0];
	sld->mval_end[1] = t->mval[1] + mval_end[1];

	if (bmbvh) {
		BKE_bmbvh_free(bmbvh);
	}
}

static void calcEdgeSlide_even(
	TransInfo *t, TransDataContainer *tc, EdgeSlideData *sld, const float mval[2])
{
	TransDataEdgeSlideVert *sv = sld->sv;

	if (sld->totsv > 0) {
		ARegion *ar = t->ar;
		RegionView3D *rv3d = NULL;
		float projectMat[4][4];

		int i = 0;

		float v_proj[2];
		float dist_sq = 0;
		float dist_min_sq = FLT_MAX;

		if (t->spacetype == SPACE_VIEW3D) {
			/* background mode support */
			rv3d = (RegionView3D*)(t->ar ? t->ar->regiondata : NULL);
		}

		if (!rv3d) {
			/* ok, let's try to survive this */
			unit_m4(projectMat);
		}
		else {
			ED_view3d_ob_project_mat_get(rv3d, tc->obedit, projectMat);
		}

		for (i = 0; i < sld->totsv; i++, sv++) {
			/* Set length */
			sv->edge_len = len_v3v3(sv->dir_side[0], sv->dir_side[1]);

			ED_view3d_project_float_v2_m4(ar, sv->v->co, v_proj, projectMat);
			dist_sq = len_squared_v2v2(mval, v_proj);
			if (dist_sq < dist_min_sq) {
				dist_min_sq = dist_sq;
				sld->curr_sv_index = i;
			}
		}
	}
	else {
		sld->curr_sv_index = 0;
	}
}

static bool createEdgeSlideVerts_double_side(TransInfo *t, TransDataContainer *tc)
{
	BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
	BMesh *bm = em->bm;
	BMIter iter;
	BMEdge *e;
	BMVert *v;
	TransDataEdgeSlideVert *sv_array;
	int sv_tot;
	int *sv_table;  /* BMVert -> sv_array index */
	EdgeSlideData *sld = (EdgeSlideData*)MEM_callocN(sizeof(*sld), "sld");
	float mval[2] = { (float)t->mval[0], (float)t->mval[1] };
	int numsel, i, loop_nr;
	bool use_occlude_geometry = false;
	View3D *v3d = NULL;
	RegionView3D *rv3d = NULL;

	slide_origdata_init_flag(t, tc, &sld->orig_data);

	sld->curr_sv_index = 0;

	/*ensure valid selection*/
	BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
			BMIter iter2;
			numsel = 0;
			BM_ITER_ELEM(e, &iter2, v, BM_EDGES_OF_VERT) {
				if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
					/* BMESH_TODO: this is probably very evil,
					 * set v->e to a selected edge*/
					v->e = e;

					numsel++;
				}
			}

			if (numsel == 0 || numsel > 2) {
				MEM_freeN(sld);
				return false; /* invalid edge selection */
			}
		}
	}

	BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
			/* note, any edge with loops can work, but we won't get predictable results, so bail out */
			if (!BM_edge_is_manifold(e) && !BM_edge_is_boundary(e)) {
				/* can edges with at least once face user */
				MEM_freeN(sld);
				return false;
			}
		}
	}

	sv_table = (int*)MEM_mallocN(sizeof(*sv_table) * bm->totvert, __func__);

#define INDEX_UNSET   -1
#define INDEX_INVALID -2

	{
		int j = 0;
		BM_ITER_MESH_INDEX(v, &iter, bm, BM_VERTS_OF_MESH, i) {
			if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
				BM_elem_flag_enable(v, BM_ELEM_TAG);
				sv_table[i] = INDEX_UNSET;
				j += 1;
			}
			else {
				BM_elem_flag_disable(v, BM_ELEM_TAG);
				sv_table[i] = INDEX_INVALID;
			}
			BM_elem_index_set(v, i); /* set_inline */
		}
		bm->elem_index_dirty &= ~BM_VERT;

		if (!j) {
			MEM_freeN(sld);
			MEM_freeN(sv_table);
			return false;
		}
		sv_tot = j;
	}

	sv_array = (TransDataEdgeSlideVert*)MEM_callocN(sizeof(TransDataEdgeSlideVert) * sv_tot, "sv_array");
	loop_nr = 0;

	STACK_DECLARE(sv_array);
	STACK_INIT(sv_array, sv_tot);

	while (1) {
		float vec_a[3], vec_b[3];
		BMLoop *l_a, *l_b;
		BMLoop *l_a_prev, *l_b_prev;
		BMVert *v_first;
		/* If this succeeds call get_next_loop()
		 * which calculates the direction to slide based on clever checks.
		 *
		 * otherwise we simply use 'e_dir' as an edge-rail.
		 * (which is better when the attached edge is a boundary, see: T40422)
		 */
#define EDGESLIDE_VERT_IS_INNER(v, e_dir) \
		((BM_edge_is_boundary(e_dir) == false) && \
		 (BM_vert_edge_count_nonwire(v) == 2))

		v = NULL;
		BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(v, BM_ELEM_TAG))
				break;

		}

		if (!v)
			break;

		if (!v->e)
			continue;

		v_first = v;

		/*walk along the edge loop*/
		e = v->e;

		/*first, rewind*/
		do {
			e = get_other_edge(v, e);
			if (!e) {
				e = v->e;
				break;
			}

			if (!BM_elem_flag_test(BM_edge_other_vert(e, v), BM_ELEM_TAG))
				break;

			v = BM_edge_other_vert(e, v);
		} while (e != v_first->e);

		BM_elem_flag_disable(v, BM_ELEM_TAG);

		l_a = e->l;
		l_b = e->l->radial_next;

		/* regarding e_next, use get_next_loop()'s improved interpolation where possible */
		{
			BMEdge *e_next = get_other_edge(v, e);
			if (e_next) {
				get_next_loop(v, l_a, e, e_next, vec_a);
			}
			else {
				BMLoop *l_tmp = BM_loop_other_edge_loop(l_a, v);
				if (EDGESLIDE_VERT_IS_INNER(v, l_tmp->e)) {
					get_next_loop(v, l_a, e, l_tmp->e, vec_a);
				}
				else {
					sub_v3_v3v3(vec_a, BM_edge_other_vert(l_tmp->e, v)->co, v->co);
				}
			}
		}

		/* !BM_edge_is_boundary(e); */
		if (l_b != l_a) {
			BMEdge *e_next = get_other_edge(v, e);
			if (e_next) {
				get_next_loop(v, l_b, e, e_next, vec_b);
			}
			else {
				BMLoop *l_tmp = BM_loop_other_edge_loop(l_b, v);
				if (EDGESLIDE_VERT_IS_INNER(v, l_tmp->e)) {
					get_next_loop(v, l_b, e, l_tmp->e, vec_b);
				}
				else {
					sub_v3_v3v3(vec_b, BM_edge_other_vert(l_tmp->e, v)->co, v->co);
				}
			}
		}
		else {
			l_b = NULL;
		}

		l_a_prev = NULL;
		l_b_prev = NULL;

#define SV_FROM_VERT(v) ( \
		(sv_table[BM_elem_index_get(v)] == INDEX_UNSET) ? \
			((void)(sv_table[BM_elem_index_get(v)] = STACK_SIZE(sv_array)), STACK_PUSH_RET_PTR(sv_array)) : \
			(&sv_array[sv_table[BM_elem_index_get(v)]]))

		/*iterate over the loop*/
		v_first = v;
		do {
			bool l_a_ok_prev;
			bool l_b_ok_prev;
			TransDataEdgeSlideVert *sv;
			BMVert *v_prev;
			BMEdge *e_prev;

			/* XXX, 'sv' will initialize multiple times, this is suspicious. see [#34024] */
			BLI_assert(v != NULL);
			BLI_assert(sv_table[BM_elem_index_get(v)] != INDEX_INVALID);
			sv = SV_FROM_VERT(v);
			sv->v = v;
			copy_v3_v3(sv->v_co_orig, v->co);
			sv->loop_nr = loop_nr;

			if (l_a || l_a_prev) {
				BMLoop *l_tmp = BM_loop_other_edge_loop(l_a ? l_a : l_a_prev, v);
				sv->v_side[0] = BM_edge_other_vert(l_tmp->e, v);
				copy_v3_v3(sv->dir_side[0], vec_a);
			}

			if (l_b || l_b_prev) {
				BMLoop *l_tmp = BM_loop_other_edge_loop(l_b ? l_b : l_b_prev, v);
				sv->v_side[1] = BM_edge_other_vert(l_tmp->e, v);
				copy_v3_v3(sv->dir_side[1], vec_b);
			}

			v_prev = v;
			v = BM_edge_other_vert(e, v);

			e_prev = e;
			e = get_other_edge(v, e);

			if (!e) {
				BLI_assert(v != NULL);

				BLI_assert(sv_table[BM_elem_index_get(v)] != INDEX_INVALID);
				sv = SV_FROM_VERT(v);

				sv->v = v;
				copy_v3_v3(sv->v_co_orig, v->co);
				sv->loop_nr = loop_nr;

				if (l_a) {
					BMLoop *l_tmp = BM_loop_other_edge_loop(l_a, v);
					sv->v_side[0] = BM_edge_other_vert(l_tmp->e, v);
					if (EDGESLIDE_VERT_IS_INNER(v, l_tmp->e)) {
						get_next_loop(v, l_a, e_prev, l_tmp->e, sv->dir_side[0]);
					}
					else {
						sub_v3_v3v3(sv->dir_side[0], sv->v_side[0]->co, v->co);
					}
				}

				if (l_b) {
					BMLoop *l_tmp = BM_loop_other_edge_loop(l_b, v);
					sv->v_side[1] = BM_edge_other_vert(l_tmp->e, v);
					if (EDGESLIDE_VERT_IS_INNER(v, l_tmp->e)) {
						get_next_loop(v, l_b, e_prev, l_tmp->e, sv->dir_side[1]);
					}
					else {
						sub_v3_v3v3(sv->dir_side[1], sv->v_side[1]->co, v->co);
					}
				}

				BM_elem_flag_disable(v, BM_ELEM_TAG);
				BM_elem_flag_disable(v_prev, BM_ELEM_TAG);

				break;
			}
			l_a_ok_prev = (l_a != NULL);
			l_b_ok_prev = (l_b != NULL);

			l_a_prev = l_a;
			l_b_prev = l_b;

			if (l_a) {
				l_a = get_next_loop(v, l_a, e_prev, e, vec_a);
			}
			else {
				zero_v3(vec_a);
			}

			if (l_b) {
				l_b = get_next_loop(v, l_b, e_prev, e, vec_b);
			}
			else {
				zero_v3(vec_b);
			}


			if (l_a && l_b) {
				/* pass */
			}
			else {
				if (l_a || l_b) {
					/* find the opposite loop if it was missing previously */
					if (l_a == NULL && l_b && (l_b->radial_next != l_b)) l_a = l_b->radial_next;
					else if (l_b == NULL && l_a && (l_a->radial_next != l_a)) l_b = l_a->radial_next;
				}
				else if (e->l != NULL) {
					/* if there are non-contiguous faces, we can still recover the loops of the new edges faces */
					/* note!, the behavior in this case means edges may move in opposite directions,
					 * this could be made to work more usefully. */

					if (l_a_ok_prev) {
						l_a = e->l;
						l_b = (l_a->radial_next != l_a) ? l_a->radial_next : NULL;
					}
					else if (l_b_ok_prev) {
						l_b = e->l;
						l_a = (l_b->radial_next != l_b) ? l_b->radial_next : NULL;
					}
				}

				if (!l_a_ok_prev && l_a) {
					get_next_loop(v, l_a, e, e_prev, vec_a);
				}
				if (!l_b_ok_prev && l_b) {
					get_next_loop(v, l_b, e, e_prev, vec_b);
				}
			}

			BM_elem_flag_disable(v, BM_ELEM_TAG);
			BM_elem_flag_disable(v_prev, BM_ELEM_TAG);
		} while ((e != v_first->e) && (l_a || l_b));

#undef SV_FROM_VERT
#undef INDEX_UNSET
#undef INDEX_INVALID

		loop_nr++;

#undef EDGESLIDE_VERT_IS_INNER
	}

	/* EDBM_flag_disable_all(em, BM_ELEM_SELECT); */

	BLI_assert(STACK_SIZE(sv_array) == sv_tot);

	sld->sv = sv_array;
	sld->totsv = sv_tot;

	/* use for visibility checks */
	if (t->spacetype == SPACE_VIEW3D) {
		v3d = (View3D*)(t->sa ? t->sa->spacedata.first : NULL);
		rv3d = (RegionView3D*)(t->ar ? t->ar->regiondata : NULL);
		use_occlude_geometry = (v3d && TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->dt > OB_WIRE && v3d->shading.type > OB_WIRE);
	}

	calcEdgeSlide_mval_range(t, tc, sld, sv_table, loop_nr, mval, use_occlude_geometry, true);

	/* create copies of faces for customdata projection */
	bmesh_edit_begin(bm, BMO_OPTYPE_FLAG_UNTAN_MULTIRES);
	slide_origdata_init_data(tc, &sld->orig_data);
	slide_origdata_create_data(t, tc, &sld->orig_data, (TransDataGenericSlideVert *)sld->sv, sizeof(*sld->sv), sld->totsv);

	if (rv3d) {
		calcEdgeSlide_even(t, tc, sld, mval);
	}

	sld->em = em;

	tc->custom.mode.data = sld;

	MEM_freeN(sv_table);

	return true;
}

/**
 * A simple version of #createEdgeSlideVerts_double_side
 * Which assumes the longest unselected.
 */
static bool createEdgeSlideVerts_single_side(TransInfo *t, TransDataContainer *tc)
{
	BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
	BMesh *bm = em->bm;
	BMIter iter;
	BMEdge *e;
	TransDataEdgeSlideVert *sv_array;
	int sv_tot;
	int *sv_table;  /* BMVert -> sv_array index */
	EdgeSlideData *sld = (EdgeSlideData*)MEM_callocN(sizeof(*sld), "sld");
	float mval[2] = { (float)t->mval[0], (float)t->mval[1] };
	int loop_nr;
	bool use_occlude_geometry = false;
	View3D *v3d = NULL;
	RegionView3D *rv3d = NULL;

	if (t->spacetype == SPACE_VIEW3D) {
		/* background mode support */
		v3d = (View3D*)(t->sa ? t->sa->spacedata.first : NULL);
		rv3d = (RegionView3D*)(t->ar ? t->ar->regiondata : NULL);
	}

	slide_origdata_init_flag(t, tc, &sld->orig_data);

	sld->curr_sv_index = 0;
	/* ensure valid selection */
	{
		int i = 0, j = 0;
		BMVert *v;

		BM_ITER_MESH_INDEX(v, &iter, bm, BM_VERTS_OF_MESH, i) {
			if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
				float len_sq_max = -1.0f;
				BMIter iter2;
				BM_ITER_ELEM(e, &iter2, v, BM_EDGES_OF_VERT) {
					if (!BM_elem_flag_test(e, BM_ELEM_SELECT)) {
						float len_sq = BM_edge_calc_length_squared(e);
						if (len_sq > len_sq_max) {
							len_sq_max = len_sq;
							v->e = e;
						}
					}
				}

				if (len_sq_max != -1.0f) {
					j++;
				}
			}
			BM_elem_index_set(v, i); /* set_inline */
		}
		bm->elem_index_dirty &= ~BM_VERT;

		if (!j) {
			MEM_freeN(sld);
			return false;
		}

		sv_tot = j;
	}

	BLI_assert(sv_tot != 0);
	/* over alloc */
	sv_array = (TransDataEdgeSlideVert*)MEM_callocN(sizeof(TransDataEdgeSlideVert) * bm->totvertsel, "sv_array");

	/* same loop for all loops, weak but we dont connect loops in this case */
	loop_nr = 1;

	sv_table = (int*)MEM_mallocN(sizeof(*sv_table) * bm->totvert, __func__);

	{
		int i = 0, j = 0;
		BMVert *v;

		BM_ITER_MESH_INDEX(v, &iter, bm, BM_VERTS_OF_MESH, i) {
			sv_table[i] = -1;
			if ((v->e != NULL) && (BM_elem_flag_test(v, BM_ELEM_SELECT))) {
				if (BM_elem_flag_test(v->e, BM_ELEM_SELECT) == 0) {
					TransDataEdgeSlideVert *sv;
					sv = &sv_array[j];
					sv->v = v;
					copy_v3_v3(sv->v_co_orig, v->co);
					sv->v_side[0] = BM_edge_other_vert(v->e, v);
					sub_v3_v3v3(sv->dir_side[0], sv->v_side[0]->co, v->co);
					sv->loop_nr = 0;
					sv_table[i] = j;
					j += 1;
				}
			}
		}
	}

	/* check for wire vertices,
	 * interpolate the directions of wire verts between non-wire verts */
	if (sv_tot != bm->totvert) {
		const int sv_tot_nowire = sv_tot;
		TransDataEdgeSlideVert *sv_iter = sv_array;

		for (int i = 0; i < sv_tot_nowire; i++, sv_iter++) {
			BMIter eiter;
			BM_ITER_ELEM(e, &eiter, sv_iter->v, BM_EDGES_OF_VERT) {
				/* walk over wire */
				TransDataEdgeSlideVert *sv_end = NULL;
				BMEdge *e_step = e;
				BMVert *v = sv_iter->v;
				int j;

				j = sv_tot;

				while (1) {
					BMVert *v_other = BM_edge_other_vert(e_step, v);
					int endpoint = (
						(sv_table[BM_elem_index_get(v_other)] != -1) +
						(BM_vert_is_edge_pair(v_other) == false));

					if ((BM_elem_flag_test(e_step, BM_ELEM_SELECT) &&
						BM_elem_flag_test(v_other, BM_ELEM_SELECT)) &&
						(endpoint == 0))
					{
						/* scan down the list */
						TransDataEdgeSlideVert *sv;
						BLI_assert(sv_table[BM_elem_index_get(v_other)] == -1);
						sv_table[BM_elem_index_get(v_other)] = j;
						sv = &sv_array[j];
						sv->v = v_other;
						copy_v3_v3(sv->v_co_orig, v_other->co);
						copy_v3_v3(sv->dir_side[0], sv_iter->dir_side[0]);
						j++;

						/* advance! */
						v = v_other;
						e_step = BM_DISK_EDGE_NEXT(e_step, v_other);
					}
					else {
						if ((endpoint == 2) && (sv_tot != j)) {
							BLI_assert(BM_elem_index_get(v_other) != -1);
							sv_end = &sv_array[sv_table[BM_elem_index_get(v_other)]];
						}
						break;
					}
				}

				if (sv_end) {
					int sv_tot_prev = sv_tot;
					const float *co_src = sv_iter->v->co;
					const float *co_dst = sv_end->v->co;
					const float *dir_src = sv_iter->dir_side[0];
					const float *dir_dst = sv_end->dir_side[0];
					sv_tot = j;

					while (j-- != sv_tot_prev) {
						float factor;
						factor = line_point_factor_v3(sv_array[j].v->co, co_src, co_dst);
						interp_v3_v3v3(sv_array[j].dir_side[0], dir_src, dir_dst, factor);
					}
				}
			}
		}
	}

	/* EDBM_flag_disable_all(em, BM_ELEM_SELECT); */

	sld->sv = sv_array;
	sld->totsv = sv_tot;

	/* use for visibility checks */
	if (t->spacetype == SPACE_VIEW3D) {
		v3d = (View3D*)(t->sa ? t->sa->spacedata.first : NULL);
		rv3d = (RegionView3D*)(t->ar ? t->ar->regiondata : NULL);
		use_occlude_geometry = (v3d && TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->dt > OB_WIRE && v3d->shading.type > OB_WIRE);
	}

	calcEdgeSlide_mval_range(t, tc, sld, sv_table, loop_nr, mval, use_occlude_geometry, false);

	/* create copies of faces for customdata projection */
	bmesh_edit_begin(bm, BMO_OPTYPE_FLAG_UNTAN_MULTIRES);
	slide_origdata_init_data(tc, &sld->orig_data);
	slide_origdata_create_data(t, tc, &sld->orig_data, (TransDataGenericSlideVert *)sld->sv, sizeof(*sld->sv), sld->totsv);

	if (rv3d) {
		calcEdgeSlide_even(t, tc, sld, mval);
	}

	sld->em = em;

	tc->custom.mode.data = sld;

	MEM_freeN(sv_table);

	return true;
}

static void drawEdgeSlide(TransInfo *t)
{
	if ((t->mode == TFM_EDGE_SLIDE) && TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data) {
		const EdgeSlideParams *slp = (EdgeSlideParams*)t->custom.mode.data;
		EdgeSlideData *sld = (EdgeSlideData*)TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data;
		const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);

		/* Even mode */
		if ((slp->use_even == true) || (is_clamp == false)) {
			const float line_size = UI_GetThemeValuef(TH_OUTLINE_WIDTH) + 0.5f;

			GPU_depth_test(false);

			GPU_blend(true);
			GPU_blend_set_func_separate(GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

			GPU_matrix_push();
			GPU_matrix_mul(TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->obmat);

			uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

			immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

			if (slp->use_even == true) {
				float co_a[3], co_b[3], co_mark[3];
				TransDataEdgeSlideVert *curr_sv = &sld->sv[sld->curr_sv_index];
				const float fac = (slp->perc + 1.0f) / 2.0f;
				const float ctrl_size = UI_GetThemeValuef(TH_FACEDOT_SIZE) + 1.5f;
				const float guide_size = ctrl_size - 0.5f;
				const int alpha_shade = -30;

				add_v3_v3v3(co_a, curr_sv->v_co_orig, curr_sv->dir_side[0]);
				add_v3_v3v3(co_b, curr_sv->v_co_orig, curr_sv->dir_side[1]);

				GPU_line_width(line_size);
				immUniformThemeColorShadeAlpha(TH_EDGE_SELECT, 80, alpha_shade);
				immBeginAtMost(GPU_PRIM_LINES, 4);
				if (curr_sv->v_side[0]) {
					immVertex3fv(pos, curr_sv->v_side[0]->co);
					immVertex3fv(pos, curr_sv->v_co_orig);
				}
				if (curr_sv->v_side[1]) {
					immVertex3fv(pos, curr_sv->v_side[1]->co);
					immVertex3fv(pos, curr_sv->v_co_orig);
				}
				immEnd();

				immUniformThemeColorShadeAlpha(TH_SELECT, -30, alpha_shade);
				GPU_point_size(ctrl_size);
				immBegin(GPU_PRIM_POINTS, 1);
				if (slp->flipped) {
					if (curr_sv->v_side[1]) immVertex3fv(pos, curr_sv->v_side[1]->co);
				}
				else {
					if (curr_sv->v_side[0]) immVertex3fv(pos, curr_sv->v_side[0]->co);
				}
				immEnd();

				immUniformThemeColorShadeAlpha(TH_SELECT, 255, alpha_shade);
				GPU_point_size(guide_size);
				immBegin(GPU_PRIM_POINTS, 1);
				interp_line_v3_v3v3v3(co_mark, co_b, curr_sv->v_co_orig, co_a, fac);
				immVertex3fv(pos, co_mark);
				immEnd();
			}
			else {
				if (is_clamp == false) {
					const int side_index = sld->curr_side_unclamp;
					TransDataEdgeSlideVert *sv;
					int i;
					const int alpha_shade = -160;

					GPU_line_width(line_size);
					immUniformThemeColorShadeAlpha(TH_EDGE_SELECT, 80, alpha_shade);
					immBegin(GPU_PRIM_LINES, sld->totsv * 2);

					/* TODO(campbell): Loop over all verts  */
					sv = sld->sv;
					for (i = 0; i < sld->totsv; i++, sv++) {
						float a[3], b[3];

						if (!is_zero_v3(sv->dir_side[side_index])) {
							copy_v3_v3(a, sv->dir_side[side_index]);
						}
						else {
							copy_v3_v3(a, sv->dir_side[!side_index]);
						}

						mul_v3_fl(a, 100.0f);
						negate_v3_v3(b, a);
						add_v3_v3(a, sv->v_co_orig);
						add_v3_v3(b, sv->v_co_orig);

						immVertex3fv(pos, a);
						immVertex3fv(pos, b);
					}
					immEnd();
				}
				else {
					BLI_assert(0);
				}
			}

			immUnbindProgram();

			GPU_matrix_pop();

			GPU_blend(false);

			GPU_depth_test(true);
		}
	}
}

static void doEdgeSlide(TransInfo *t, float perc)
{
	EdgeSlideParams *slp = (EdgeSlideParams*)t->custom.mode.data;
	EdgeSlideData *sld_active = (EdgeSlideData*)TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data;

	slp->perc = perc;

	if (slp->use_even == false) {
		const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
		if (is_clamp) {
			const int side_index = (perc < 0.0f);
			const float perc_final = fabsf(perc);
			FOREACH_TRANS_DATA_CONTAINER(t, tc) {
				EdgeSlideData *sld = (EdgeSlideData*)tc->custom.mode.data;
				if (!sld) continue;
				TransDataEdgeSlideVert *sv = sld->sv;
				for (int i = 0; i < sld->totsv; i++, sv++) {
					madd_v3_v3v3fl(sv->v->co, sv->v_co_orig, sv->dir_side[side_index], perc_final);
				}
				sld->curr_side_unclamp = side_index;
			}
		}
		else {
			if (!sld_active) return;
			const float perc_init = fabsf(perc) * ((sld_active->curr_side_unclamp == (perc < 0.0f)) ? 1 : -1);
			const int side_index = sld_active->curr_side_unclamp;
			FOREACH_TRANS_DATA_CONTAINER(t, tc) {
				EdgeSlideData *sld = (EdgeSlideData*)tc->custom.mode.data;
				TransDataEdgeSlideVert *sv = sld->sv;
				for (int i = 0; i < sld->totsv; i++, sv++) {
					float dir_flip[3];
					float perc_final = perc_init;
					if (!is_zero_v3(sv->dir_side[side_index])) {
						copy_v3_v3(dir_flip, sv->dir_side[side_index]);
					}
					else {
						copy_v3_v3(dir_flip, sv->dir_side[!side_index]);
						perc_final *= -1;
					}
					madd_v3_v3v3fl(sv->v->co, sv->v_co_orig, dir_flip, perc_final);
				}
			}
		}
	}
	else {
		/**
		 * Implementation note, even mode ignores the starting positions and uses only the
		 * a/b verts, this could be changed/improved so the distance is still met but the verts are moved along
		 * their original path (which may not be straight), however how it works now is OK and matches 2.4x - Campbell
		 *
		 * \note len_v3v3(curr_sv->dir_side[0], curr_sv->dir_side[1])
		 * is the same as the distance between the original vert locations, same goes for the lines below.
		 */
		if (!sld_active) return;
		TransDataEdgeSlideVert *curr_sv = &sld_active->sv[sld_active->curr_sv_index];
		const float curr_length_perc = curr_sv->edge_len * (((slp->flipped ? perc : -perc) + 1.0f) / 2.0f);

		float co_a[3];
		float co_b[3];

		FOREACH_TRANS_DATA_CONTAINER(t, tc) {
			EdgeSlideData *sld = (EdgeSlideData*)tc->custom.mode.data;
			TransDataEdgeSlideVert *sv = sld->sv;
			for (int i = 0; i < sld->totsv; i++, sv++) {
				if (sv->edge_len > FLT_EPSILON) {
					const float fac = min_ff(sv->edge_len, curr_length_perc) / sv->edge_len;

					add_v3_v3v3(co_a, sv->v_co_orig, sv->dir_side[0]);
					add_v3_v3v3(co_b, sv->v_co_orig, sv->dir_side[1]);

					if (slp->flipped) {
						interp_line_v3_v3v3v3(sv->v->co, co_b, sv->v_co_orig, co_a, fac);
					}
					else {
						interp_line_v3_v3v3v3(sv->v->co, co_a, sv->v_co_orig, co_b, fac);
					}
				}
			}
		}
	}
}

static void initEdgeSlide_ex(TransInfo *t, bool use_double_side, bool use_even, bool flipped, bool use_clamp)
{
	EdgeSlideData *sld;
	bool ok = false;

	t->mode = TFM_EDGE_SLIDE;
	{
		EdgeSlideParams *slp = (EdgeSlideParams*)MEM_callocN(sizeof(*slp), __func__);
		slp->use_even = use_even;
		slp->flipped = flipped;
		/* happens to be best for single-sided */
		if (use_double_side == false) {
			slp->flipped = !flipped;
		}
		slp->perc = 0.0f;

		if (!use_clamp) {
			t->flag |= T_ALT_TRANSFORM;
		}

		t->custom.mode.data = slp;
		t->custom.mode.use_free = true;
	}

	if (use_double_side) {
		FOREACH_TRANS_DATA_CONTAINER(t, tc) {
			ok |= createEdgeSlideVerts_double_side(t, tc);
		}
	}
	else {
		FOREACH_TRANS_DATA_CONTAINER(t, tc) {
			ok |= createEdgeSlideVerts_single_side(t, tc);
		}
	}

	if (!ok) {
		t->state = TRANS_CANCEL;
		return;
	}

	FOREACH_TRANS_DATA_CONTAINER(t, tc) {
		sld = (EdgeSlideData*)tc->custom.mode.data;
		if (!sld) {
			continue;
		}
		tc->custom.mode.free_cb = freeEdgeSlideVerts;
	}

	/* set custom point first if you want value to be initialized by init */
	calcEdgeSlideCustomPoints(t);
	initMouseInputMode(t, &t->mouse, INPUT_CUSTOM_RATIO_FLIP);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_type[0] = B_UNIT_NONE;

	t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}

bool Widget_LoopCut::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_LoopCut::click(VR_UI::Cursor& c)
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

void Widget_LoopCut::drag_start(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		return;
	}

	if (c.bimanual) {
		return;
	}

	if (edge_slide) {
		/* Test for empty selection. */
		selection_empty = true;

		Scene *scene = CTX_data_scene(C);
		ToolSettings *ts = scene->toolsettings;
		BMesh *bm = ((Mesh*)obedit->data)->edit_btmesh->bm;
		if (!bm) {
			return;
		}
		BMIter iter;
		if (ts->selectmode & SCE_SELECT_VERTEX) {
			BMVert *v;
			BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
				if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
					selection_empty = false;
					break;
				}
			}
		}
		else if (ts->selectmode & SCE_SELECT_EDGE) {
			BMEdge *e;
			BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH) {
				if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
					selection_empty = false;
					break;
				}
			}
		}
		else if (ts->selectmode & SCE_SELECT_FACE) {
			BMFace *f;
			BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH) {
				if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
					selection_empty = false;
					break;
				}
			}
		}
		if (selection_empty) {
			return;
		}

		p1 = p0 = *(Coord3Df*)c.interaction_position.get(VR_SPACE_REAL).m[3];
		p1_b = p0_b = *(Coord3Df*)c.interaction_position.get(VR_SPACE_BLENDER).m[3];

		/* Execute edge slide operation */
		loopcut_info.context = C;
		loopcut_info.mode = TFM_EDGE_SLIDE;
		loopcut_info.state = TRANS_STARTING;
		unit_m3(loopcut_info.spacemtx);
		initTransInfo(C, &loopcut_info, NULL, NULL);
		initTransformOrientation(C, &loopcut_info);
		createTransData(C, &loopcut_info);

		initEdgeSlide_ex(&loopcut_info, Widget_LoopCut::double_side, Widget_LoopCut::even, Widget_LoopCut::flipped, Widget_LoopCut::clamp);

		/* Update manipulators */
		Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_NORMAL;
		Widget_Transform::update_manipulator();
	}
	else {
		p1 = p0 = *(Coord3Df*)c.interaction_position.get(VR_SPACE_BLENDER).m[3];
		/* Initialize ring selection */
		ringsel_init(C, &loopcut_dummy_op, false);
	}

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_LoopCut::obj.do_render[i] = true;
	}
}

void Widget_LoopCut::drag_contd(VR_UI::Cursor& c)
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

	if (edge_slide) {
		if (selection_empty) {
			return;
		}
		p1 = *(Coord3Df*)c.position.get(VR_SPACE_REAL).m[3];
		p1_b = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];
		Coord3Df v = p1 - p0;
		percent = v.length() * WIDGET_LOOPCUT_SENSITIVITY;
		if (VR_UI::shift_key_get()) {
			percent *= WIDGET_TRANSFORM_TRANS_PRECISION;
		}
		if (v * *(Coord3Df*)Widget_Transform::manip_t.m[2] < 0) {
			percent = -percent;
		}

		/* Execute edge slide operation */
		loopcut_info.state = TRANS_RUNNING;
		doEdgeSlide(&loopcut_info, percent);
		DEG_id_tag_update((ID*)obedit->data, 0);
	}
	else {
		p1 = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];
		/* Update ring selection */
		if (loopcut_dummy_op.customdata) {
			ringsel_update(C, &loopcut_dummy_op);
		}
	}
	
	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_LoopCut::obj.do_render[i] = true;
	}
}

void Widget_LoopCut::drag_stop(VR_UI::Cursor& c)
{
	if (c.bimanual) {
		return;
	}

	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		return;
	}

	if (edge_slide) {
		if (selection_empty) {
			return;
		}
		p1 = *(Coord3Df*)c.position.get(VR_SPACE_REAL).m[3];
		p1_b = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];
		Coord3Df v = p1 - p0;
		percent = v.length() * WIDGET_LOOPCUT_SENSITIVITY;
		if (VR_UI::shift_key_get()) {
			percent *= WIDGET_TRANSFORM_TRANS_PRECISION;
		}
		if (v * *(Coord3Df*)Widget_Transform::manip_t.m[2] < 0) {
			percent = -percent;
		}

		/* Finish edge slide operation */
		doEdgeSlide(&loopcut_info, percent);

		/* Free data. TODO_XR: Free all edge slide data. */
		loopcut_info.state = TRANS_CONFIRM;
		TransDataContainer *tc = loopcut_info.data_container;
		if (tc) {
			if (tc->custom.mode.data) {
				MEM_freeN(tc->custom.mode.data);
				tc->custom.mode.data = NULL;
			}
			if (tc->data) {
				MEM_freeN(tc->data);
				tc->data = NULL;
			}
		}
	}
	else {
		if (!loopcut_dummy_op.customdata) {
			return;
		}
		p1 = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];
		/* Finish ring selection */
		ringsel_finish(C, &loopcut_dummy_op);
		ringsel_exit(C, &loopcut_dummy_op);
		/* Execute loop cut operation */
		loopcut_init(C, &loopcut_dummy_op, NULL);
	}

	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	EDBM_mesh_normals_update(em);
	Widget_Transform::update_manipulator();

	DEG_id_tag_update((ID*)obedit->data, ID_RECALC_GEOMETRY);
	WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
	ED_undo_push(C, "Loop Cut");

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_LoopCut::obj.do_render[i] = false;
	}
}

static void render_arrow(float length)
{
	/* Adapted from arrow_draw_geom() in arrow3d_gizmo.c */
	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	bool unbind_shader = true;

	static float c_black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);
	immUniformColor4fv(c_black);
	immUniform1f("dash_width", 6.0f);

	/* Axis */
	GPU_line_width(1.0f);
	immBegin(GPU_PRIM_LINES, 2);
	immVertex3f(pos, 0.0f, 0.0f, -length);
	immVertex3f(pos, 0.0f, 0.0f, length);
	immEnd();

	/* Arrow */
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor4fv(c_black);
	GPU_matrix_push();

	float len = length * 0.1f;
	float width = length * 0.04f;
	
	GPU_matrix_translate_3f(0.0f, 0.0f, length);

	imm_draw_circle_fill_3d(pos, 0.0, 0.0, width, 8);
	imm_draw_cylinder_fill_3d(pos, width, 0.0, len, 8, 1);

	GPU_matrix_translate_3f(0.0f, 0.0f, -length);

	GPU_matrix_pop();

	if (unbind_shader) {
		immUnbindProgram();
	}
}

void Widget_LoopCut::render(VR_Side side)
{
	if (edge_slide) {
		/* Render edge slide */
		drawEdgeSlide(&loopcut_info);

		/* Render arrow */
		GPU_matrix_push();
		GPU_matrix_mul(Widget_Transform::manip_t.m);
		GPU_blend(true);
		render_arrow(Widget_Transform::manip_scale_factor);
		GPU_blend(false);
		GPU_matrix_pop();
	}
	else {
		/* Render preselection ring */
		RingSelOpData *lcd = (RingSelOpData*)loopcut_dummy_op.customdata;
		if (lcd) {
			EDBM_preselect_edgering_draw(lcd->presel_edgering, lcd->ob->obmat);
		}
	}

	Widget_LoopCut::obj.do_render[side] = false;
}
