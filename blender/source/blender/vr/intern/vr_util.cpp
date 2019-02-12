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

/** \file blender/vr/intern/vr_util.cpp
*   \ingroup vr
* 
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_math.h"
#include "vr_ui.h"
#include "vr_util.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"

#include "ED_gpencil.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_select_utils.h"
#include "ED_undo.h"

#include "WM_api.h"
#include "WM_types.h"

/* From view3d_select.c */
void VR_Util::object_deselect_all_visible(ViewLayer *view_layer, View3D *v3d)
{
	for (Base *base = (Base*)view_layer->object_bases.first; base; base = base->next) {
		if (BASE_SELECTABLE(v3d, base)) {
			ED_object_base_select(base, BA_DESELECT);
		}
	}
}

/* From view3d_select.c */
void VR_Util::deselectall_except(ViewLayer *view_layer, Base *b)
{
	for (Base *base = (Base*)view_layer->object_bases.first; base; base = base->next) {
		if (base->flag & BASE_SELECTED) {
			if (b != base) {
				ED_object_base_select(base, BA_DESELECT);
			}
		}
	}
}

/* Adapted from ed_view3d_project__internal in view3d_project.c */
eV3DProjStatus VR_Util::view3d_project(const ARegion *ar,
	const float perspmat[4][4], const bool is_local,  /* normally hidden */
	const float co[3], float r_co[2], const eV3DProjTest flag)
{
	float vec4[4];

	/* check for bad flags */
	BLI_assert((flag & V3D_PROJ_TEST_ALL) == flag);

	if (flag & V3D_PROJ_TEST_CLIP_BB) {
		RegionView3D *rv3d = (RegionView3D*)ar->regiondata;
		if (rv3d->rflag & RV3D_CLIPPING) {
			if (ED_view3d_clipping_test(rv3d, co, is_local)) {
				return V3D_PROJ_RET_CLIP_BB;
			}
		}
	}

	copy_v3_v3(vec4, co);
	vec4[3] = 1.0;
	mul_m4_v4(perspmat, vec4);

	if (((flag & V3D_PROJ_TEST_CLIP_ZERO) == 0) || (fabsf(vec4[3]) > WIDGET_SELECT_RAYCAST_ZERO_CLIP)) {
		if (((flag & V3D_PROJ_TEST_CLIP_NEAR) == 0) || (vec4[3] > WIDGET_SELECT_RAYCAST_NEAR_CLIP)) {
			float& w_s = vec4[3];
			if (w_s == 0.0f) {
				w_s = 0.001f;
			}
			float& x_s = vec4[0];
			float& y_s = vec4[1];
			x_s /= w_s;
			y_s /= w_s;

			VR* vr = vr_get_obj();
			r_co[0] = (float)vr->tex_width * (x_s + 1.0f) / 2.0f;
			r_co[1] = (float)vr->tex_height * (1.0f - y_s) / 2.0f;

			/* check if the point is behind the view, we need to flip in this case */
			if (UNLIKELY((flag & V3D_PROJ_TEST_CLIP_NEAR) == 0) && (vec4[3] < 0.0f)) {
				negate_v2(r_co);
			}
		}
		else {
			return V3D_PROJ_RET_CLIP_NEAR;
		}
	}
	else {
		return V3D_PROJ_RET_CLIP_ZERO;
	}

	return V3D_PROJ_RET_OK;
}

void VR_Util::deselectall_edit(BMesh *bm, int mode)
{
	BMIter iter;

	switch (mode) {
	case 0: { /* Vertex */
		BMVert *eve;
		BM_ITER_MESH(eve, &iter, bm, BM_VERTS_OF_MESH) {
			if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
				BM_vert_select_set(bm, eve, 0);
			}
		}
		break;
	}
	case 1: { /* Edge */
		BMEdge *eed;
		BM_ITER_MESH(eed, &iter, bm, BM_EDGES_OF_MESH) {
			if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
				BM_edge_select_set(bm, eed, 0);
			}
		}
		break;
	}
	case 2: { /* Face */
		BMFace *efa;
		BM_ITER_MESH(efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
				BM_face_select_set(bm, efa, 0);
			}
		}
		break;
	}
	default: {
		break;
	}
	}
}

/* Adapted from view3d_select.c */
void VR_Util::raycast_select_single_vertex(const Coord3Df& p, ViewContext *vc, bool extend, bool deselect)
{
	/* TODO_XR: Use rv3d->persmat of dominant eye. */
	bContext *C = vr_get_obj()->ctx;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = (RegionView3D*)ar->regiondata;
	float dist = ED_view3d_select_dist_px() * 1.3333f;
	int mval[2];
	VR_Side side = VR_UI::eye_dominance_get();
	VR_UI::get_pixel_coordinates(p, mval[0], mval[1], side);
	const float mval_fl[2] = { (float)mval[0], (float)mval[1] };
	float screen_co[2];
	bool is_inside = false;

	BM_mesh_elem_table_ensure(vc->em->bm, BM_VERT);
	BMVert *sv = NULL;

	const Mat44f& offset = *(Mat44f*)vc->obedit->obmat;
	static Coord3Df pos;
	BMVert *v;
	BMIter iter;
	BMesh *bm = vc->em->bm;
	BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
		if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
			VR_Math::multiply_mat44_coord3D(pos, offset, *(Coord3Df*)v->co);
			if (view3d_project(
				ar, rv3d->persmat, false, (float*)&pos, screen_co,
				(eV3DProjTest)(V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR)) == V3D_PROJ_RET_OK)
			{
				float dist_temp = len_manhattan_v2v2(mval_fl, screen_co);
				dist_temp += (dist * 0.1f); //10.0f
				if (dist_temp < dist) {
					dist = dist_temp;
					sv = v;
					is_inside = true;
				}
			}
		}
	}

	if (is_inside && sv) {
		const bool is_select = BM_elem_flag_test(sv, BM_ELEM_SELECT);
		const int sel_op_result = ED_select_op_action_deselected(deselect ? SEL_OP_SUB : SEL_OP_SET, is_select, is_inside);
		if (sel_op_result != -1) {
			if (!extend && !deselect) {
				deselectall_edit(vc->em->bm, 0);
			}
			BM_vert_select_set(vc->em->bm, sv, sel_op_result);

			DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
			ED_undo_push(C, "Select");
		}
	}
	else {
		if (!extend && !deselect) {
			/* Empty space -> deselect all */
			deselectall_edit(vc->em->bm, 0);

			DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
			ED_undo_push(C, "Select");
		}
	}
}

void VR_Util::raycast_select_single_edge(const Coord3Df& p, ViewContext *vc, bool extend, bool deselect)
{
	/* TODO_XR: Use rv3d->persmat of dominant eye. */
	bContext *C = vr_get_obj()->ctx;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = (RegionView3D*)ar->regiondata;
	float dist = ED_view3d_select_dist_px() * 1.3333f;
	int mval[2];
	VR_Side side = VR_UI::eye_dominance_get();
	VR_UI::get_pixel_coordinates(p, mval[0], mval[1], side);
	const float mval_fl[2] = { (float)mval[0], (float)mval[1] };
	float screen_co[2];
	float med_co[3];
	bool is_inside = false;

	BM_mesh_elem_table_ensure(vc->em->bm, BM_EDGE);
	BMEdge *se = NULL;

	const Mat44f& offset = *(Mat44f*)vc->obedit->obmat;
	static Coord3Df pos;
	BMEdge *e;
	BMIter iter;
	BMesh *bm = vc->em->bm;
	BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH) {
		if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
			*(Coord3Df*)med_co = (*(Coord3Df*)e->v1->co + *(Coord3Df*)e->v2->co) / 2.0f;
			VR_Math::multiply_mat44_coord3D(pos, offset, *(Coord3Df*)med_co);
			if (view3d_project(
				ar, rv3d->persmat, false, (float*)&pos, screen_co,
				(eV3DProjTest)(V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR)) == V3D_PROJ_RET_OK)
			{
				float dist_temp = len_manhattan_v2v2(mval_fl, screen_co);
				dist_temp += (dist * 0.1f); //10.0f
				if (dist_temp < dist) {
					dist = dist_temp;
					se = e;
					is_inside = true;
				}
			}
		}
	}

	if (is_inside && se) {
		const bool is_select = BM_elem_flag_test(se, BM_ELEM_SELECT);
		const int sel_op_result = ED_select_op_action_deselected(deselect ? SEL_OP_SUB : SEL_OP_SET, is_select, is_inside);
		if (sel_op_result != -1) {
			if (!extend && !deselect) {
				deselectall_edit(vc->em->bm, 1);
			}
			BM_edge_select_set(vc->em->bm, se, sel_op_result);

			DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
			ED_undo_push(C, "Select");
		}
	}
	else {
		if (!extend && !deselect) {
			/* Empty space -> deselect all */
			deselectall_edit(vc->em->bm, 1);

			DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
			ED_undo_push(C, "Select");
		}
	}
}

void VR_Util::raycast_select_single_face(const Coord3Df& p, ViewContext *vc, bool extend, bool deselect)
{
	/* TODO_XR: Use rv3d->persmat of dominant eye. */
	bContext *C = vr_get_obj()->ctx;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = (RegionView3D*)ar->regiondata;
	float dist = ED_view3d_select_dist_px() * 1.3333f;
	int mval[2];
	VR_Side side = VR_UI::eye_dominance_get();
	VR_UI::get_pixel_coordinates(p, mval[0], mval[1], side);
	const float mval_fl[2] = { (float)mval[0], (float)mval[1] };
	float screen_co[2];
	bool is_inside = false;

	BM_mesh_elem_table_ensure(vc->em->bm, BM_FACE);
	BMFace *sf = NULL;

	const Mat44f& offset = *(Mat44f*)vc->obedit->obmat;
	static Coord3Df pos;
	static Coord3Df cent;
	BMFace *f;
	BMLoop *l;
	BMIter iter;
	BMesh *bm = vc->em->bm;
	BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH) {
		l = f->l_first;
		memset(&cent, 0, sizeof(float) * 3);
		for (int i = 0; i < f->len; ++i, l = l->next) {
			cent += *(Coord3Df*)l->v->co;
		}
		cent /= f->len;
		if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
			VR_Math::multiply_mat44_coord3D(pos, offset, cent);
			if (view3d_project(
				ar, rv3d->persmat, false, (float*)&pos, screen_co,
				(eV3DProjTest)(V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR)) == V3D_PROJ_RET_OK)
			{
				float dist_temp = len_manhattan_v2v2(mval_fl, screen_co);
				dist_temp += (dist * 0.1f); //10.0f
				if (dist_temp < dist) {
					dist = dist_temp;
					sf = f;
					is_inside = true;
				}
			}
		}
	}

	if (is_inside && sf) {
		const bool is_select = BM_elem_flag_test(sf, BM_ELEM_SELECT);
		const int sel_op_result = ED_select_op_action_deselected(deselect ? SEL_OP_SUB : SEL_OP_SET, is_select, is_inside);
		if (sel_op_result != -1) {
			if (!extend && !deselect) {
				deselectall_edit(vc->em->bm, 2);
			}
			BM_face_select_set(vc->em->bm, sf, sel_op_result);

			DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
			ED_undo_push(C, "Select");
		}
	}
	else {
		if (!extend && !deselect) {
			/* Empty space -> deselect all */
			deselectall_edit(vc->em->bm, 2);

			DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
			ED_undo_push(C, "Select");
		}
	}
}


void VR_Util::raycast_select_single_edit(
	const Coord3Df& p,
	bool extend,
	bool deselect,
	bool toggle,
	bool enumerate) 
{
	/* Adapted from do_mesh_box_select() in view3d_select.c */
	VR *vr = vr_get_obj();
	bContext *C = vr->ctx;
	ViewContext vc;

	/* setup view context */
	ED_view3d_viewcontext_init(C, &vc);
	ToolSettings *ts = vc.scene->toolsettings;
	Object *obedit = vc.obedit;
	if (obedit && BKE_object_is_in_editmode(obedit)) {
		ED_view3d_viewcontext_init_object(&vc, obedit);
		vc.em = BKE_editmesh_from_object(obedit);
		if (!vc.em) {
			return;
		}

		if (ts->selectmode & SCE_SELECT_VERTEX) {
			raycast_select_single_vertex(p, &vc, extend, deselect);
		}
		else if (ts->selectmode & SCE_SELECT_EDGE) {
			raycast_select_single_edge(p, &vc, extend, deselect);
		}
		else if (ts->selectmode & SCE_SELECT_FACE) {
			raycast_select_single_face(p, &vc, extend, deselect);
		}

		EDBM_selectmode_flush(vc.em);
	}
}

void VR_Util::raycast_select_single(
	const Coord3Df& p,
	bool extend,
	bool deselect,
	bool toggle,
	bool enumerate,
	bool object,
	bool obcenter)
{
	bContext *C = vr_get_obj()->ctx;

	ViewContext vc;
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	View3D *v3d = CTX_wm_view3d(C);
	Base *base, *startbase = NULL, *basact = NULL, *oldbasact = BASACT(view_layer);
	const eObjectMode object_mode = oldbasact ? (eObjectMode)oldbasact->object->mode : OB_MODE_OBJECT;
	bool is_obedit;
	float dist = ED_view3d_select_dist_px() * 1.3333f;
	//int hits;

	int mval[2];
	VR_Side side = VR_UI::eye_dominance_get();
	VR_UI::get_pixel_coordinates(p, mval[0], mval[1], side);
	const float mval_fl[2] = { (float)mval[0], (float)mval[1] };

	/* setup view context for argument to callbacks */
	ED_view3d_viewcontext_init(C, &vc);

	is_obedit = (vc.obedit != NULL);
	if (object) {
		/* signal for view3d_opengl_select to skip editmode objects */
		vc.obedit = NULL;
	}

	/* In pose mode we don't want to mess with object selection. */
	const bool is_pose_mode = (vc.obact && vc.obact->mode & OB_MODE_POSE);

	/* always start list from basact in wire mode */
	startbase = (Base*)FIRSTBASE(view_layer);
	if (BASACT(view_layer) && BASACT(view_layer)->next) startbase = BASACT(view_layer)->next;

	/* This block uses the control key to make the object selected by its center point rather than its contents */
	/* in editmode do not activate */
	if (obcenter) {
		/* note; shift+alt goes to group-flush-selecting */
		if (enumerate) {
			//basact = object_mouse_select_menu(C, &vc, NULL, 0, mval, toggle);
		}
		else {
			const int object_type_exclude_select = (
				vc.v3d->object_type_exclude_viewport | vc.v3d->object_type_exclude_select);
			base = startbase;
			while (base) {
				if (BASE_SELECTABLE(v3d, base) &&
					((object_type_exclude_select & (1 << base->object->type)) == 0))
				{
					float screen_co[2];
					/* TODO_XR: Use rv3d->persmat of dominant eye. */
					RegionView3D *rv3d = (RegionView3D*)ar->regiondata;
					if (view3d_project(
						ar, rv3d->persmat, false, base->object->obmat[3], screen_co,
						(eV3DProjTest)(V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR)) == V3D_PROJ_RET_OK)
					{
						float dist_temp = len_manhattan_v2v2(mval_fl, screen_co);
						if (base == BASACT(view_layer)) dist_temp += (dist * 0.1f); //10.0f
						if (dist_temp < dist) {
							dist = dist_temp;
							basact = base;
						}
					}
				}
				base = base->next;

				if (base == NULL) base = (Base*)FIRSTBASE(view_layer);
				if (base == startbase) break;
			}
		}
		if (scene->toolsettings->object_flag & SCE_OBJECT_MODE_LOCK) {
			if (is_obedit == false) {
				if (basact && !BKE_object_is_mode_compat(basact->object, object_mode)) {
					if (object_mode == OB_MODE_OBJECT) {
						struct Main *bmain = CTX_data_main(C);
						ED_object_mode_generic_exit(bmain, vc.depsgraph, scene, basact->object);
					}
					if (!BKE_object_is_mode_compat(basact->object, object_mode)) {
						basact = NULL;
					}
				}
			}
		}
	}
#if 0 /* TODO_XR */
	else {
		unsigned int buffer[MAXPICKBUF];
		bool do_nearest;

		// TIMEIT_START(select_time);

		/* if objects have posemode set, the bones are in the same selection buffer */
		const eV3DSelectObjectFilter select_filter = (
			(scene->toolsettings->object_flag & SCE_OBJECT_MODE_LOCK) ?
			VIEW3D_SELECT_FILTER_OBJECT_MODE_LOCK : VIEW3D_SELECT_FILTER_NOP);
		hits = mixed_bones_object_selectbuffer(
			&vc, buffer, mval,
			true, enumerate, select_filter,
			&do_nearest);

		// TIMEIT_END(select_time);

		if (hits > 0) {
			/* note: bundles are handling in the same way as bones */
			const bool has_bones = selectbuffer_has_bones(buffer, hits);

			/* note; shift+alt goes to group-flush-selecting */
			if (enumerate) {
				//basact = object_mouse_select_menu(C, &vc, buffer, hits, mval, toggle);
			}
			else {
				basact = mouse_select_eval_buffer(&vc, buffer, hits, startbase, has_bones, do_nearest);
			}

			if (scene->toolsettings->object_flag & SCE_OBJECT_MODE_LOCK) {
				if (is_obedit == false) {
					if (basact && !BKE_object_is_mode_compat(basact->object, object_mode)) {
						if (object_mode == OB_MODE_OBJECT) {
							struct Main *bmain = CTX_data_main(C);
							ED_object_mode_generic_exit(bmain, vc.depsgraph, scene, basact->object);
						}
						if (!BKE_object_is_mode_compat(basact->object, object_mode)) {
							basact = NULL;
						}
					}
				}
			}

			if (has_bones && basact) {
				if (basact->object->type == OB_CAMERA) {
					if (BASACT(view_layer) == basact) {
						int i, hitresult;
						bool changed = false;

						for (i = 0; i < hits; i++) {
							hitresult = buffer[3 + (i * 4)];

							/* if there's bundles in buffer select bundles first,
							 * so non-camera elements should be ignored in buffer */
							if (basact->object->select_color != (hitresult & 0xFFFF)) {
								continue;
							}

							/* index of bundle is 1<<16-based. if there's no "bone" index
							 * in height word, this buffer value belongs to camera. not to bundle */
							if (buffer[4 * i + 3] & 0xFFFF0000) {
								MovieClip *clip = BKE_object_movieclip_get(scene, basact->object, false);
								MovieTracking *tracking = &clip->tracking;
								ListBase *tracksbase;
								MovieTrackingTrack *track;

								track = BKE_tracking_track_get_indexed(&clip->tracking, hitresult >> 16, &tracksbase);

								if (TRACK_SELECTED(track) && extend) {
									changed = false;
									BKE_tracking_track_deselect(track, TRACK_AREA_ALL);
								}
								else {
									int oldsel = TRACK_SELECTED(track) ? 1 : 0;
									if (!extend)
										deselect_all_tracks(tracking);

									BKE_tracking_track_select(tracksbase, track, TRACK_AREA_ALL, extend);

									if (oldsel != (TRACK_SELECTED(track) ? 1 : 0))
										changed = true;
								}

								basact->flag |= BASE_SELECTED;
								BKE_scene_object_base_flag_sync_from_base(basact);

								DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
								DEG_id_tag_update(&clip->id, ID_RECALC_SELECT);
								WM_event_add_notifier(C, NC_MOVIECLIP | ND_SELECT, track);
								WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);

								break;
							}
						}

						if (!changed) {
							/* fallback to regular object selection if no new bundles were selected,
							 * allows to select object parented to reconstruction object */
							basact = mouse_select_eval_buffer(&vc, buffer, hits, startbase, 0, do_nearest);
						}
					}
				}
				else if (ED_armature_pose_select_pick_with_buffer(
					view_layer, basact, buffer, hits, extend, deselect, toggle, do_nearest))
				{
					/* then bone is found */

					/* we make the armature selected:
					 * not-selected active object in posemode won't work well for tools */
					basact->flag |= BASE_SELECTED;
					BKE_scene_object_base_flag_sync_from_base(basact);

					WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, basact->object);
					WM_event_add_notifier(C, NC_OBJECT | ND_BONE_ACTIVE, basact->object);

					/* in weightpaint, we use selected bone to select vertexgroup, so no switch to new active object */
					if (BASACT(view_layer) && (BASACT(view_layer)->object->mode & OB_MODE_WEIGHT_PAINT)) {
						/* prevent activating */
						basact = NULL;
					}

				}
				/* prevent bone selecting to pass on to object selecting */
				if (basact == BASACT(view_layer))
					basact = NULL;
			}
		}
	}
#endif

	if (scene->toolsettings->object_flag & SCE_OBJECT_MODE_LOCK) {
		/* Disallow switching modes,
		 * special exception for edit-mode - vertex-parent operator. */
		if (is_obedit == false) {
			if (oldbasact && basact) {
				if ((oldbasact->object->mode != basact->object->mode) &&
					(oldbasact->object->mode & basact->object->mode) == 0)
				{
					basact = NULL;
				}
			}
		}
	}

	/* so, do we have something selected? */
	if (basact) {
		if (vc.obedit) {
			/* only do select */
			deselectall_except(view_layer, basact);
			ED_object_base_select(basact, BA_SELECT);
		}
		/* also prevent making it active on mouse selection */
		else if (BASE_SELECTABLE(v3d, basact)) {
			if (extend) {
				ED_object_base_select(basact, BA_SELECT);
			}
			else if (deselect) {
				ED_object_base_select(basact, BA_DESELECT);
			}
			else if (toggle) {
				if (basact->flag & BASE_SELECTED) {
					if (basact == oldbasact) {
						ED_object_base_select(basact, BA_DESELECT);
					}
				}
				else {
					object_deselect_all_visible(view_layer, v3d);
					ED_object_base_select(basact, BA_SELECT);
				}
			}
			else {
				/* When enabled, this puts other objects out of multi pose-mode. */
				if (is_pose_mode == false) {
					deselectall_except(view_layer, basact);
					ED_object_base_select(basact, BA_SELECT);
				}
			}

			if ((oldbasact != basact) && (is_obedit == false)) {
				ED_object_base_activate(C, basact); /* adds notifier */
			}

			/* Set special modes for grease pencil
			   The grease pencil modes are not real modes, but a hack to make the interface
			   consistent, so need some tricks to keep UI synchronized */
			   // XXX: This stuff needs reviewing (Aligorith)
			if (false &&
				(((oldbasact) && oldbasact->object->type == OB_GPENCIL) ||
				(basact->object->type == OB_GPENCIL)))
			{
				/* set cursor */
				if (ELEM(basact->object->mode,
					OB_MODE_PAINT_GPENCIL,
					OB_MODE_SCULPT_GPENCIL,
					OB_MODE_WEIGHT_GPENCIL))
				{
					ED_gpencil_toggle_brush_cursor(C, true, NULL);
				}
				else {
					/* TODO: maybe is better use restore */
					ED_gpencil_toggle_brush_cursor(C, false, NULL);
				}
			}
		}

		DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
		ED_undo_push(C, "Select");
	}
	else {
		if (!extend && !deselect) {
			/* Empty space -> deselect all */
			object_deselect_all_visible(view_layer, v3d);
			DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
			WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
			ED_undo_push(C, "Select");
		}
	}
}
