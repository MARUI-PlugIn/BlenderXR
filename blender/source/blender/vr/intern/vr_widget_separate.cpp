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

/** \file blender/vr/intern/vr_widget_separate.cpp
*   \ingroup vr
*
* Main module for the VR widget UI.
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_separate.h"
#include "vr_widget_transform.h"

#include "vr_draw.h"

#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "DNA_mesh_types.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_undo.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "WM_types.h"

/***********************************************************************************************//**
 * \class                               Widget_Separate
 ***************************************************************************************************
 * Interaction widget for performing a 'separate' operation.
 *
 **************************************************************************************************/
Widget_Separate Widget_Separate::obj;

enum {
	MESH_SEPARATE_SELECTED = 0,
	MESH_SEPARATE_MATERIAL = 1,
	MESH_SEPARATE_LOOSE = 2,
};

int Widget_Separate::mode(MESH_SEPARATE_SELECTED);

/* Dummy op to pass to edbm_separate_exec() */
static wmOperator separate_dummy_op;

/* From editmesh_tools.c */
static Base *mesh_separate_tagged(Main *bmain, Scene *scene, ViewLayer *view_layer, Base *base_old, BMesh *bm_old)
{
	Base *base_new;
	Object *obedit = base_old->object;
	BMesh *bm_new;

	BMeshCreateParams params = { 0 };
	params.use_toolflags = true;

	bm_new = BM_mesh_create(
		&bm_mesh_allocsize_default,
		&params);
	BM_mesh_elem_toolflags_ensure(bm_new);  /* needed for 'duplicate' bmo */

	CustomData_copy(&bm_old->vdata, &bm_new->vdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm_old->edata, &bm_new->edata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm_old->ldata, &bm_new->ldata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm_old->pdata, &bm_new->pdata, CD_MASK_BMESH, CD_CALLOC, 0);

	CustomData_bmesh_init_pool(&bm_new->vdata, bm_mesh_allocsize_default.totvert, BM_VERT);
	CustomData_bmesh_init_pool(&bm_new->edata, bm_mesh_allocsize_default.totedge, BM_EDGE);
	CustomData_bmesh_init_pool(&bm_new->ldata, bm_mesh_allocsize_default.totloop, BM_LOOP);
	CustomData_bmesh_init_pool(&bm_new->pdata, bm_mesh_allocsize_default.totface, BM_FACE);

	base_new = ED_object_add_duplicate(bmain, scene, view_layer, base_old, USER_DUP_MESH);
	/* DAG_relations_tag_update(bmain); */ /* normally would call directly after but in this case delay recalc */
	assign_matarar(bmain, base_new->object, give_matarar(obedit), *give_totcolp(obedit)); /* new in 2.5 */

	ED_object_base_select(base_new, BA_SELECT);

	BMO_op_callf(bm_old, (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
		"duplicate geom=%hvef dest=%p", BM_ELEM_TAG, bm_new);
	BMO_op_callf(bm_old, (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
		"delete geom=%hvef context=%i", BM_ELEM_TAG, DEL_FACES);

	/* deselect loose data - this used to get deleted,
	 * we could de-select edges and verts only, but this turns out to be less complicated
	 * since de-selecting all skips selection flushing logic */
	BM_mesh_elem_hflag_disable_all(bm_old, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT, false);

	BM_mesh_normals_update(bm_new);

	BMeshToMeshParams mm_params = { 0 };
	BM_mesh_bm_to_me(bmain, bm_new, (Mesh*)base_new->object->data, &mm_params);

	BM_mesh_free(bm_new);
	((Mesh*)base_new->object->data)->edit_btmesh = NULL;

	return base_new;
}

static bool mesh_separate_selected(Main *bmain, Scene *scene, ViewLayer *view_layer, Base *base_old, BMesh *bm_old)
{
	/* we may have tags from previous operators */
	BM_mesh_elem_hflag_disable_all(bm_old, BM_FACE | BM_EDGE | BM_VERT, BM_ELEM_TAG, false);

	/* sel -> tag */
	BM_mesh_elem_hflag_enable_test(bm_old, BM_FACE | BM_EDGE | BM_VERT, BM_ELEM_TAG, true, false, BM_ELEM_SELECT);

	return (mesh_separate_tagged(bmain, scene, view_layer, base_old, bm_old) != NULL);
}

/* flush a hflag to from verts to edges/faces */
static void bm_mesh_hflag_flush_vert(BMesh *bm, const char hflag)
{
	BMEdge *e;
	BMLoop *l_iter;
	BMLoop *l_first;
	BMFace *f;

	BMIter eiter;
	BMIter fiter;

	bool ok;

	BM_ITER_MESH(e, &eiter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e->v1, hflag) &&
			BM_elem_flag_test(e->v2, hflag))
		{
			BM_elem_flag_enable(e, hflag);
		}
		else {
			BM_elem_flag_disable(e, hflag);
		}
	}
	BM_ITER_MESH(f, &fiter, bm, BM_FACES_OF_MESH) {
		ok = true;
		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			if (!BM_elem_flag_test(l_iter->v, hflag)) {
				ok = false;
				break;
			}
		} while ((l_iter = l_iter->next) != l_first);

		BM_elem_flag_set(f, hflag, ok);
	}
}

/**
 * Sets an object to a single material. from one of its slots.
 *
 * \note This could be used for split-by-material for non mesh types.
 * \note This could take material data from another object or args.
 */
static void mesh_separate_material_assign_mat_nr(Main *bmain, Object *ob, const short mat_nr)
{
	ID *obdata = (ID*)ob->data;

	Material ***matarar;
	const short *totcolp;

	totcolp = give_totcolp_id(obdata);
	matarar = give_matarar_id(obdata);

	if ((totcolp && matarar) == 0) {
		BLI_assert(0);
		return;
	}

	if (*totcolp) {
		Material *ma_ob;
		Material *ma_obdata;
		char matbit;

		if (mat_nr < ob->totcol) {
			ma_ob = ob->mat[mat_nr];
			matbit = ob->matbits[mat_nr];
		}
		else {
			ma_ob = NULL;
			matbit = 0;
		}

		if (mat_nr < *totcolp) {
			ma_obdata = (*matarar)[mat_nr];
		}
		else {
			ma_obdata = NULL;
		}

		BKE_material_clear_id(bmain, obdata, true);
		BKE_material_resize_object(bmain, ob, 1, true);
		BKE_material_resize_id(bmain, obdata, 1, true);

		ob->mat[0] = ma_ob;
		id_us_plus((ID *)ma_ob);
		ob->matbits[0] = matbit;
		(*matarar)[0] = ma_obdata;
		id_us_plus((ID *)ma_obdata);
	}
	else {
		BKE_material_clear_id(bmain, obdata, true);
		BKE_material_resize_object(bmain, ob, 0, true);
		BKE_material_resize_id(bmain, obdata, 0, true);
	}
}

static bool mesh_separate_material(Main *bmain, Scene *scene, ViewLayer *view_layer, Base *base_old, BMesh *bm_old)
{
	BMFace *f_cmp, *f;
	BMIter iter;
	bool result = false;

	while ((f_cmp = (BMFace*)BM_iter_at_index(bm_old, BM_FACES_OF_MESH, NULL, 0))) {
		Base *base_new;
		const short mat_nr = f_cmp->mat_nr;
		int tot = 0;

		BM_mesh_elem_hflag_disable_all(bm_old, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

		BM_ITER_MESH(f, &iter, bm_old, BM_FACES_OF_MESH) {
			if (f->mat_nr == mat_nr) {
				BMLoop *l_iter;
				BMLoop *l_first;

				BM_elem_flag_enable(f, BM_ELEM_TAG);
				l_iter = l_first = BM_FACE_FIRST_LOOP(f);
				do {
					BM_elem_flag_enable(l_iter->v, BM_ELEM_TAG);
					BM_elem_flag_enable(l_iter->e, BM_ELEM_TAG);
				} while ((l_iter = l_iter->next) != l_first);

				tot++;
			}
		}

		/* leave the current object with some materials */
		if (tot == bm_old->totface) {
			mesh_separate_material_assign_mat_nr(bmain, base_old->object, mat_nr);

			/* since we're in editmode, must set faces here */
			BM_ITER_MESH(f, &iter, bm_old, BM_FACES_OF_MESH) {
				f->mat_nr = 0;
			}
			break;
		}

		/* Move selection into a separate object */
		base_new = mesh_separate_tagged(bmain, scene, view_layer, base_old, bm_old);
		if (base_new) {
			mesh_separate_material_assign_mat_nr(bmain, base_new->object, mat_nr);
		}

		result |= (base_new != NULL);
	}

	return result;
}

static bool mesh_separate_loose(Main *bmain, Scene *scene, ViewLayer *view_layer, Base *base_old, BMesh *bm_old)
{
	int i;
	BMEdge *e;
	BMVert *v_seed;
	BMWalker walker;
	bool result = false;
	int max_iter = bm_old->totvert;

	/* Clear all selected vertices */
	BM_mesh_elem_hflag_disable_all(bm_old, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

	/* A "while (true)" loop should work here as each iteration should
	 * select and remove at least one vertex and when all vertices
	 * are selected the loop will break out. But guard against bad
	 * behavior by limiting iterations to the number of vertices in the
	 * original mesh.*/
	for (i = 0; i < max_iter; i++) {
		int tot = 0;
		/* Get a seed vertex to start the walk */
		v_seed = (BMVert*)BM_iter_at_index(bm_old, BM_VERTS_OF_MESH, NULL, 0);

		/* No vertices available, can't do anything */
		if (v_seed == NULL) {
			break;
		}

		/* Select the seed explicitly, in case it has no edges */
		if (!BM_elem_flag_test(v_seed, BM_ELEM_TAG)) { BM_elem_flag_enable(v_seed, BM_ELEM_TAG); tot++; }

		/* Walk from the single vertex, selecting everything connected
		 * to it */
		BMW_init(&walker, bm_old, BMW_VERT_SHELL,
			BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
			BMW_FLAG_NOP,
			BMW_NIL_LAY);

		for (e = (BMEdge*)BMW_begin(&walker, v_seed); e; e = (BMEdge*)BMW_step(&walker)) {
			if (!BM_elem_flag_test(e->v1, BM_ELEM_TAG)) { BM_elem_flag_enable(e->v1, BM_ELEM_TAG); tot++; }
			if (!BM_elem_flag_test(e->v2, BM_ELEM_TAG)) { BM_elem_flag_enable(e->v2, BM_ELEM_TAG); tot++; }
		}
		BMW_end(&walker);

		if (bm_old->totvert == tot) {
			/* Every vertex selected, nothing to separate, work is done */
			break;
		}

		/* Flush the selection to get edge/face selections matching
		 * the vertex selection */
		bm_mesh_hflag_flush_vert(bm_old, BM_ELEM_TAG);

		/* Move selection into a separate object */
		result |= (mesh_separate_tagged(bmain, scene, view_layer, base_old, bm_old) != NULL);
	}

	return result;
}

static int edbm_separate_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	int retval = 0;

	if (ED_operator_editmesh(C)) {
		uint bases_len = 0;
		uint empty_selection_len = 0;
		static ObjectsInModeParams params = { OB_MODE_EDIT, true };
		Base **bases = BKE_view_layer_array_from_bases_in_mode_params(view_layer, CTX_wm_view3d(C), &bases_len, &params);

		for (uint bs_index = 0; bs_index < bases_len; bs_index++) {
			Base *base = bases[bs_index];
			BMEditMesh *em = BKE_editmesh_from_object(base->object);

			if (Widget_Separate::mode == 0) {
				if ((em->bm->totvertsel == 0) &&
					(em->bm->totedgesel == 0) &&
					(em->bm->totfacesel == 0))
				{
					/* when all objects has no selection */
					if (++empty_selection_len == bases_len) {
						BKE_report(op->reports, RPT_ERROR, "Nothing selected");
					}
					continue;
				}
			}

			/* editmode separate */
			switch (Widget_Separate::mode) {
			case MESH_SEPARATE_SELECTED:
				retval = mesh_separate_selected(bmain, scene, view_layer, base, em->bm);
				break;
			case MESH_SEPARATE_MATERIAL:
				retval = mesh_separate_material(bmain, scene, view_layer, base, em->bm);
				break;
			case MESH_SEPARATE_LOOSE:
				retval = mesh_separate_loose(bmain, scene, view_layer, base, em->bm);
				break;
			default:
				BLI_assert(0);
				break;
			}

			if (retval) {
				EDBM_update_generic(em, true, true);
			}
		}
		MEM_freeN(bases);
	}
	else {
		if (Widget_Separate::mode == MESH_SEPARATE_SELECTED) {
			BKE_report(op->reports, RPT_ERROR, "Selection not supported in object mode");
			return OPERATOR_CANCELLED;
		}

		/* object mode separate */
		ListBase ctx_data_list;                                               
		CollectionPointerLink *ctx_link;                                      
		CTX_data_selected_editable_bases(C, &ctx_data_list);                                 
		for (ctx_link = (CollectionPointerLink*)ctx_data_list.first; ctx_link; ctx_link = ctx_link->next)                                       
		{                                                                     
			Base *base_iter = (Base*)ctx_link->ptr.data;

			Object *ob = base_iter->object;
			if (ob->type == OB_MESH) {
				Mesh *me = (Mesh*)ob->data;
				if (!ID_IS_LINKED(me)) {
					BMesh *bm_old = NULL;
					int retval_iter = 0;

					BMeshCreateParams params = { 0 };
					params.use_toolflags = true;
					bm_old = BM_mesh_create(
						&bm_mesh_allocsize_default,
						&params);
					BMeshFromMeshParams mfm_params = { 0 };
					BM_mesh_bm_from_me(bm_old, me, &mfm_params);

					switch (Widget_Separate::mode) {
					case MESH_SEPARATE_MATERIAL:
						retval_iter = mesh_separate_material(bmain, scene, view_layer, base_iter, bm_old);
						break;
					case MESH_SEPARATE_LOOSE:
						retval_iter = mesh_separate_loose(bmain, scene, view_layer, base_iter, bm_old);
						break;
					default:
						BLI_assert(0);
						break;
					}

					BMeshToMeshParams mtm_params = { 0 };
					mtm_params.calc_object_remap = true;
					if (retval_iter) {
						BM_mesh_bm_to_me(
							bmain, bm_old, me,
							&mtm_params);

						DEG_id_tag_update(&me->id, ID_RECALC_GEOMETRY);
						WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);
					}

					BM_mesh_free(bm_old);

					retval |= retval_iter;
				}
			}
		}                                                               
		BLI_freelistN(&ctx_data_list);
	}

	if (retval) {
		/* delay depsgraph recalc until all objects are duplicated */
		DEG_relations_tag_update(bmain);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, NULL);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

bool Widget_Separate::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Separate::click(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	if (edbm_separate_exec(C, &separate_dummy_op) == OPERATOR_FINISHED) {
		/* Update manipulators */
		Widget_Transform::update_manipulator();

		ED_undo_push(C, "Separate");
	}
}

bool Widget_Separate::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_Separate::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
{
	if (touched) {
		const Mat44f& t_touched = m_widget_touched * t;
		VR_Draw::update_modelview_matrix(&t_touched, 0);
	}
	else {
		VR_Draw::update_modelview_matrix(&t, 0);
	}
	if (active) {
		VR_Draw::set_color(1.0f, 0.0f, 0.0f, 1.0f);
	}
	else {
		VR_Draw::set_color(1.0f, 1.0f, 1.0f, 1.0f);
	}
	VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::separate_tex);
}
