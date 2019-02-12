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

/** \file blender/vr/intern/vr_widget_select.cpp
*   \ingroup vr
* 
* Main module for the VR widget UI.
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget.h"
#include "vr_widget_transform.h"
#include "vr_widget_select.h"

#include "vr_math.h"
#include "vr_draw.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"

#include "ED_gpencil.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_undo.h"

#include "WM_api.h"
#include "WM_types.h"

#include "vr_util.h"

/***********************************************************************************************//**
* \class                               Widget_Select
***************************************************************************************************
* Interaction widget for object selection in the default ray-casting mode
*
**************************************************************************************************/
Widget_Select Widget_Select::obj;

bool Widget_Select::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Select::click(VR_UI::Cursor& c)
{
	if (VR_UI::selection_mode == VR_UI::SELECTIONMODE_RAYCAST) {
		Raycast::obj.click(c);
	}
	else { /* SELECTIONMODE_PROXIMITY */
		Proximity::obj.click(c);
	}
}

void Widget_Select::drag_start(VR_UI::Cursor& c)
{
	if (VR_UI::selection_mode == VR_UI::SELECTIONMODE_RAYCAST) {
		Raycast::obj.drag_start(c);
	}
	else {
		Proximity::obj.drag_start(c);
	}

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Select::obj.do_render[i] = true;
	}
}

void Widget_Select::drag_contd(VR_UI::Cursor& c)
{
	if (VR_UI::selection_mode == VR_UI::SELECTIONMODE_RAYCAST) {
		Raycast::obj.drag_contd(c);
	}
	else {
		Proximity::obj.drag_contd(c);
	}

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Select::obj.do_render[i] = true;
	}
}

void Widget_Select::drag_stop(VR_UI::Cursor& c)
{
	if (VR_UI::selection_mode == VR_UI::SELECTIONMODE_RAYCAST) {
		Raycast::obj.drag_stop(c);
	}
	else {
		Proximity::obj.drag_stop(c);
	}
}

void Widget_Select::render(VR_Side side)
{
	if (VR_UI::selection_mode == VR_UI::SELECTIONMODE_RAYCAST) {
		Raycast::obj.render(side);
	}
	else {
		Proximity::obj.render(side);
	}

	Widget_Select::obj.do_render[side] = false;
}

/***********************************************************************************************//**
* \class                               Widget_Select::Raycast
***************************************************************************************************
* Interaction widget for object selection in the default ray-casting mode
*
**************************************************************************************************/
Widget_Select::Raycast Widget_Select::Raycast::obj;

Widget_Select::Raycast::SelectionRect Widget_Select::Raycast::selection_rect[VR_SIDES];


#if 0
/* From view3d_select.c */
static bool selectbuffer_has_bones(const uint *buffer, const uint hits)
{
	unsigned int i;
	for (i = 0; i < hits; i++) {
		if (buffer[(4 * i) + 3] & 0xFFFF0000) {
			return true;
		}
	}
	return false;
}

static int selectbuffer_ret_hits_15(unsigned int *UNUSED(buffer), const int hits15)
{
	return hits15;
}

static int selectbuffer_ret_hits_9(unsigned int *buffer, const int hits15, const int hits9)
{
	const int offs = 4 * hits15;
	memcpy(buffer, buffer + offs, 4 * hits9 * sizeof(unsigned int));
	return hits9;
}

static int selectbuffer_ret_hits_5(unsigned int *buffer, const int hits15, const int hits9, const int hits5)
{
	const int offs = 4 * hits15 + 4 * hits9;
	memcpy(buffer, buffer + offs, 4 * hits5 * sizeof(unsigned int));
	return hits5;
}

/* we want a select buffer with bones, if there are... */
/* so check three selection levels and compare */
static int mixed_bones_object_selectbuffer(
	ViewContext *vc, unsigned int *buffer, const int mval[2],
	bool use_cycle, bool enumerate, eV3DSelectObjectFilter select_filter,
	bool *r_do_nearest)
{
	rcti rect;
	int hits15, hits9 = 0, hits5 = 0;
	bool has_bones15 = false, has_bones9 = false, has_bones5 = false;
	static int last_mval[2] = { -100, -100 };
	bool do_nearest = false;
	View3D *v3d = vc->v3d;

	/* define if we use solid nearest select or not */
	if (use_cycle) {
		if (v3d->shading.type > OB_WIRE) {
			do_nearest = true;
			if (len_manhattan_v2v2_int(mval, last_mval) < 3) {
				do_nearest = false;
			}
		}
		copy_v2_v2_int(last_mval, mval);
	}
	else {
		if (v3d->shading.type > OB_WIRE) {
			do_nearest = true;
		}
	}

	if (r_do_nearest) {
		*r_do_nearest = do_nearest;
	}

	do_nearest = do_nearest && !enumerate;

	const eV3DSelectMode select_mode = (do_nearest ? VIEW3D_SELECT_PICK_NEAREST : VIEW3D_SELECT_PICK_ALL);
	int hits = 0;

	/* we _must_ end cache before return, use 'goto finally' */
	view3d_opengl_select_cache_begin();

	BLI_rcti_init_pt_radius(&rect, mval, 14);
	hits15 = view3d_opengl_select(vc, buffer, MAXPICKBUF, &rect, select_mode, select_filter);
	if (hits15 == 1) {
		hits = selectbuffer_ret_hits_15(buffer, hits15);
		goto finally;
	}
	else if (hits15 > 0) {
		int offs;
		has_bones15 = selectbuffer_has_bones(buffer, hits15);

		offs = 4 * hits15;
		BLI_rcti_init_pt_radius(&rect, mval, 9);
		hits9 = view3d_opengl_select(vc, buffer + offs, MAXPICKBUF - offs, &rect, select_mode, select_filter);
		if (hits9 == 1) {
			hits = selectbuffer_ret_hits_9(buffer, hits15, hits9);
			goto finally;
		}
		else if (hits9 > 0) {
			has_bones9 = selectbuffer_has_bones(buffer + offs, hits9);

			offs += 4 * hits9;
			BLI_rcti_init_pt_radius(&rect, mval, 5);
			hits5 = view3d_opengl_select(vc, buffer + offs, MAXPICKBUF - offs, &rect, select_mode, select_filter);
			if (hits5 == 1) {
				hits = selectbuffer_ret_hits_5(buffer, hits15, hits9, hits5);
				goto finally;
			}
			else if (hits5 > 0) {
				has_bones5 = selectbuffer_has_bones(buffer + offs, hits5);
			}
		}

		if (has_bones5) { hits = selectbuffer_ret_hits_5(buffer, hits15, hits9, hits5); goto finally; }
		else if (has_bones9) { hits = selectbuffer_ret_hits_9(buffer, hits15, hits9); goto finally; }
		else if (has_bones15) { hits = selectbuffer_ret_hits_15(buffer, hits15); goto finally; }

		if (hits5 > 0) { hits = selectbuffer_ret_hits_5(buffer, hits15, hits9, hits5); goto finally; }
		else if (hits9 > 0) { hits = selectbuffer_ret_hits_9(buffer, hits15, hits9); goto finally; }
		else { hits = selectbuffer_ret_hits_15(buffer, hits15); goto finally; }
	}

	finally:
	view3d_opengl_select_cache_end();

	if (vc->scene->toolsettings->object_flag & SCE_OBJECT_MODE_LOCK) {
		const bool is_pose_mode = (vc->obact && vc->obact->mode & OB_MODE_POSE);
		struct buf {
			uint data[4];
		};
		buf *buffer4 = (buf*)buffer;
		uint j = 0;
		for (uint i = 0; i < hits; i++) {
			if (((buffer4[i].data[3] & 0xFFFF0000) != 0) == is_pose_mode) {
				if (i != j) {
					buffer4[j] = buffer4[i];
				}
				j++;
			}
		}
		hits = j;
	}

	return hits;
}

/* Adapted from view3d_select.c */
static Base *mouse_select_eval_buffer(
	ViewContext *vc, const uint *buffer, int hits,
	Base *startbase, bool has_bones, bool do_nearest)
{
	ViewLayer *view_layer = vc->view_layer;
	View3D *v3d = vc->v3d;
	Base *base, *basact = NULL;
	int a;

	if (do_nearest) {
		unsigned int min = 0xFFFFFFFF;
		int selcol = 0, notcol = 0;

		if (has_bones) {
			/* we skip non-bone hits */
			for (a = 0; a < hits; a++) {
				if (min > buffer[4 * a + 1] && (buffer[4 * a + 3] & 0xFFFF0000)) {
					min = buffer[4 * a + 1];
					selcol = buffer[4 * a + 3] & 0xFFFF;
				}
			}
		}
		else {
			/* only exclude active object when it is selected... */
			if (BASACT(view_layer) && (BASACT(view_layer)->flag & BASE_SELECTED) && hits > 1) {
				notcol = BASACT(view_layer)->object->select_color;
			}

			for (a = 0; a < hits; a++) {
				if (min > buffer[4 * a + 1] && notcol != (buffer[4 * a + 3] & 0xFFFF)) {
					min = buffer[4 * a + 1];
					selcol = buffer[4 * a + 3] & 0xFFFF;
				}
			}
		}

		base = (Base*)FIRSTBASE(view_layer);
		while (base) {
			if (BASE_SELECTABLE(v3d, base)) {
				if (base->object->select_color == selcol) break;
			}
			base = base->next;
		}
		if (base) basact = base;
	}
	else {

		base = startbase;
		while (base) {
			/* skip objects with select restriction, to prevent prematurely ending this loop
			 * with an un-selectable choice */
			if ((base->flag & BASE_SELECTABLE) == 0) {
				base = base->next;
				if (base == NULL) base = (Base*)FIRSTBASE(view_layer);
				if (base == startbase) break;
			}

			if (BASE_SELECTABLE(v3d, base)) {
				for (a = 0; a < hits; a++) {
					if (has_bones) {
						/* skip non-bone objects */
						if ((buffer[4 * a + 3] & 0xFFFF0000)) {
							if (base->object->select_color == (buffer[(4 * a) + 3] & 0xFFFF))
								basact = base;
						}
					}
					else {
						if (base->object->select_color == (buffer[(4 * a) + 3] & 0xFFFF))
							basact = base;
					}
				}
			}

			if (basact) break;

			base = base->next;
			if (base == NULL) base = (Base*)FIRSTBASE(view_layer);
			if (base == startbase) break;
		}
	}

	return basact;
}

static void deselect_all_tracks(MovieTracking *tracking)
{
	MovieTrackingObject *object;

	object = (MovieTrackingObject*)tracking->objects.first;
	while (object) {
		ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, object);
		MovieTrackingTrack *track = (MovieTrackingTrack*)tracksbase->first;

		while (track) {
			BKE_tracking_track_deselect(track, TRACK_AREA_ALL);

			track = track->next;
		}

		object = object->next;
	}
}
#endif

 
/* Select multiple objects with raycast selection.
 * p0 and p1 should be in screen coordinates (-1, 1). */
static void raycast_select_multiple(
	const float& x0, const float& y0,
	const float& x1, const float& y1,
	bool extend,
	bool deselect,
	bool toggle = false,
	bool enumerate = false,
	bool object = true,
	bool obcenter = true)
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
	//int hits;

	/* Find bounds and center of selection rectangle. */
	float bounds_x = fabsf(x1 - x0) / 2.0f;
	float bounds_y = fabsf(y1 - y0) / 2.0f;
	float center_x = (x0 + x1) / 2.0f;
	float center_y = (y0 + y1) / 2.0f;
	/* Convert from screen coordinates to pixel coordinates. */
	VR *vr = vr_get_obj();
	bounds_x *= (float)vr->tex_width / 2.0f;
	bounds_y *= (float)vr->tex_height / 2.0f;
	center_x = (float)vr->tex_width * (center_x + 1.0f) / 2.0f;
	center_y = (float)vr->tex_height * (1.0f - center_y) / 2.0f;

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

	bool hit = false;

	if (!extend && !deselect) {
		/* Do pre-deselection. */
		/* TODO_XR: Compare selection before and after for proper undo behavior. */
		VR_Util::object_deselect_all_visible(view_layer, v3d);

		DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
		ED_undo_push(C, "Select");
	}

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
					if (VR_Util::view3d_project(
						ar, rv3d->persmat, false, base->object->obmat[3], screen_co,
						(eV3DProjTest)(V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR)) == V3D_PROJ_RET_OK)
					{
						if (fabsf(screen_co[0] - center_x) < bounds_x &&
							fabsf(screen_co[1] - center_y) < bounds_y) {
							basact = base;
							if (vc.obedit) {
								/* only do select */
								VR_Util::deselectall_except(view_layer, basact);
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
										ED_object_base_select(basact, BA_SELECT);
									}
								}
								else {
									ED_object_base_select(basact, BA_SELECT);
									/* When enabled, this puts other objects out of multi pose-mode. */
									/*if (is_pose_mode == false) {
										VR_Util::deselectall_except(view_layer, basact);
										ED_object_base_select(basact, BA_SELECT);
									}*/
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
							hit = true;
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
		int mval[2];
		mval[0] = (int)center_x;
		mval[1] = (int)center_y;
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
	if (hit) {
		DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
		ED_undo_push(C, "Select");
	}
}


/* Adapted from view3d_select.c */
static void raycast_select_multiple_vertex(
	const float& x0, const float& y0,
	const float& x1, const float& y1,
	ViewContext *vc, bool extend, bool deselect)
{
	/* Find bounds and center of selection rectangle. */
	float bounds_x = fabsf(x1 - x0) / 2.0f;
	float bounds_y = fabsf(y1 - y0) / 2.0f;
	float center_x = (x0 + x1) / 2.0f;
	float center_y = (y0 + y1) / 2.0f;
	/* Convert from screen coordinates to pixel coordinates. */
	VR *vr = vr_get_obj();
	bContext *C = vr->ctx;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = (RegionView3D*)ar->regiondata;
	bounds_x *= (float)vr->tex_width / 2.0f;
	bounds_y *= (float)vr->tex_height / 2.0f;
	center_x = (float)vr->tex_width * (center_x + 1.0f) / 2.0f;
	center_y = (float)vr->tex_height * (1.0f - center_y) / 2.0f;
	float screen_co[2];
	bool is_inside = false;

	if (!extend && !deselect) {
		/* Do pre-deselection. */
		/* TODO_XR: Compare selection before and after for proper undo behavior. */
		VR_Util::deselectall_edit(vc->em->bm, 0);

		DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
		ED_undo_push(C, "Select");
	}

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
			if (VR_Util::view3d_project(
				ar, rv3d->persmat, false, (float*)&pos, screen_co,
				(eV3DProjTest)(V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR)) == V3D_PROJ_RET_OK)
			{
				if (fabsf(screen_co[0] - center_x) < bounds_x &&
					fabsf(screen_co[1] - center_y) < bounds_y) {
					sv = v;
					is_inside = true;
					const bool is_select = BM_elem_flag_test(sv, BM_ELEM_SELECT);
					const int sel_op_result = ED_select_op_action_deselected(deselect ? SEL_OP_SUB : SEL_OP_ADD, is_select, is_inside);
					if (sel_op_result != -1) {
						BM_vert_select_set(vc->em->bm, sv, sel_op_result);
					}
				}
			}
		}
	}

	if (is_inside) {
		DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
		ED_undo_push(C, "Select");
	}
}

static void raycast_select_multiple_edge(
	const float& x0, const float& y0,
	const float& x1, const float& y1,
	ViewContext *vc, bool extend, bool deselect)
{
	/* Find bounds and center of selection rectangle. */
	float bounds_x = fabsf(x1 - x0) / 2.0f;
	float bounds_y = fabsf(y1 - y0) / 2.0f;
	float center_x = (x0 + x1) / 2.0f;
	float center_y = (y0 + y1) / 2.0f;
	/* Convert from screen coordinates to pixel coordinates. */
	VR *vr = vr_get_obj();
	bContext *C = vr->ctx;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = (RegionView3D*)ar->regiondata;
	bounds_x *= (float)vr->tex_width / 2.0f;
	bounds_y *= (float)vr->tex_height / 2.0f;
	center_x = (float)vr->tex_width * (center_x + 1.0f) / 2.0f;
	center_y = (float)vr->tex_height * (1.0f - center_y) / 2.0f;
	float screen_co[2];
	float med_co[3];
	bool is_inside = false;

	if (!extend && !deselect) {
		/* Do pre-deselection. */
		/* TODO_XR: Compare selection before and after for proper undo behavior. */
		VR_Util::deselectall_edit(vc->em->bm, 1);

		DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
		ED_undo_push(C, "Select");
	}

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
			if (VR_Util::view3d_project(
				ar, rv3d->persmat, false, (float*)&pos, screen_co,
				(eV3DProjTest)(V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR)) == V3D_PROJ_RET_OK)
			{
				if (fabsf(screen_co[0] - center_x) < bounds_x &&
					fabsf(screen_co[1] - center_y) < bounds_y) {
					se = e;
					is_inside = true;
					const bool is_select = BM_elem_flag_test(se, BM_ELEM_SELECT);
					const int sel_op_result = ED_select_op_action_deselected(deselect ? SEL_OP_SUB : SEL_OP_ADD, is_select, is_inside);
					if (sel_op_result != -1) {
						BM_edge_select_set(vc->em->bm, se, sel_op_result);
					}
				}
			}
		}
	}

	if (is_inside) {
		DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
		ED_undo_push(C, "Select");
	}
}

static void raycast_select_multiple_face(
	const float& x0, const float& y0,
	const float& x1, const float& y1, 
	ViewContext *vc, bool extend, bool deselect)
{
	/* Find bounds and center of selection rectangle. */
	float bounds_x = fabsf(x1 - x0) / 2.0f;
	float bounds_y = fabsf(y1 - y0) / 2.0f;
	float center_x = (x0 + x1) / 2.0f;
	float center_y = (y0 + y1) / 2.0f;
	/* Convert from screen coordinates to pixel coordinates. */
	VR *vr = vr_get_obj();
	bContext *C = vr->ctx;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = (RegionView3D*)ar->regiondata;
	bounds_x *= (float)vr->tex_width / 2.0f;
	bounds_y *= (float)vr->tex_height / 2.0f;
	center_x = (float)vr->tex_width * (center_x + 1.0f) / 2.0f;
	center_y = (float)vr->tex_height * (1.0f - center_y) / 2.0f;
	float screen_co[2];
	bool is_inside = false;

	if (!extend && !deselect) {
		/* Do pre-deselection. */
		/* TODO_XR: Compare selection before and after for proper undo behavior. */
		VR_Util::deselectall_edit(vc->em->bm, 2);

		DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
		ED_undo_push(C, "Select");
	}

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
			if (VR_Util::view3d_project(
				ar, rv3d->persmat, false, (float*)&pos, screen_co,
				(eV3DProjTest)(V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR)) == V3D_PROJ_RET_OK)
			{
				if (fabsf(screen_co[0] - center_x) < bounds_x &&
					fabsf(screen_co[1] - center_y) < bounds_y) {
					sf = f;
					is_inside = true;
					const bool is_select = BM_elem_flag_test(sf, BM_ELEM_SELECT);
					const int sel_op_result = ED_select_op_action_deselected(deselect ? SEL_OP_SUB : SEL_OP_ADD, is_select, is_inside);
					if (sel_op_result != -1) {
						BM_face_select_set(vc->em->bm, sf, sel_op_result);
					}
				}
			}
		}
	}

	if (is_inside) {
		DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
		ED_undo_push(C, "Select");
	}
}

static void raycast_select_multiple_edit(
	const float& x0, const float& y0,
	const float& x1, const float& y1,
	bool extend,
	bool deselect,
	bool toggle = false,
	bool enumerate = false)
{
	/* Adapted from do_mesh_box_select() in view3d_select.c */
	VR *vr = vr_get_obj();
	bContext *C = vr->ctx;
	ViewContext vc;

	/* setup view context */
	ED_view3d_viewcontext_init(C, &vc);
	ToolSettings *ts = vc.scene->toolsettings;

	if (vc.obedit) {
		ED_view3d_viewcontext_init_object(&vc, vc.obedit);
		vc.em = BKE_editmesh_from_object(vc.obedit);
		if (!vc.em) {
			return;
		}

		if (ts->selectmode & SCE_SELECT_VERTEX) {
			raycast_select_multiple_vertex(x0, y0, x1, y1, &vc, extend, deselect);
		}
		else if (ts->selectmode & SCE_SELECT_EDGE) {
			raycast_select_multiple_edge(x0, y0, x1, y1, &vc, extend, deselect);
		}
		else if (ts->selectmode & SCE_SELECT_FACE) {
			raycast_select_multiple_face(x0, y0, x1, y1, &vc, extend, deselect);
		}

		EDBM_selectmode_flush(vc.em);
	}
}

bool Widget_Select::Raycast::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Select::Raycast::click(VR_UI::Cursor& c)
{
	const Mat44f& m = c.position.get();
	if (CTX_data_edit_object(vr_get_obj()->ctx)) {
		VR_Util::raycast_select_single_edit(*(Coord3Df*)m.m[3], VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
	}
	else {
		VR_Util::raycast_select_single(*(Coord3Df*)m.m[3], VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
	}
	/* Update manipulators */
	Widget_Transform::update_manipulator();
}

void Widget_Select::Raycast::drag_start(VR_UI::Cursor& c)
{
	const Mat44f& m = c.position.get();

	VR_Side side = VR_UI::eye_dominance_get();
	VR_UI::get_screen_coordinates(*(Coord3Df*)m.m[3], selection_rect[side].x0, selection_rect[side].y0, side);
	selection_rect[side].x1 = selection_rect[side].x0;
	selection_rect[side].y1 = selection_rect[side].y0;

	Widget_Select::Raycast::obj.do_render[side] = true;
}

void Widget_Select::Raycast::drag_contd(VR_UI::Cursor& c)
{
	const Mat44f& m = c.position.get();
	const Mat44f& m_interaction = c.interaction_position.get();

	VR_Side side = VR_UI::eye_dominance_get();
	VR_UI::get_screen_coordinates(*(Coord3Df*)m.m[3], selection_rect[side].x1, selection_rect[side].y1, side);
	VR_UI::get_screen_coordinates(*(Coord3Df*)m_interaction.m[3], selection_rect[side].x0, selection_rect[side].y0, side);
	
	Widget_Select::Raycast::obj.do_render[side] = true;
}

void Widget_Select::Raycast::drag_stop(VR_UI::Cursor& c)
{
	const Mat44f& m = c.position.get();
	VR_Side side = VR_UI::eye_dominance_get();
	VR_UI::get_screen_coordinates(*(Coord3Df*)m.m[3], selection_rect[side].x1, selection_rect[side].y1, side);

	if (CTX_data_edit_object(vr_get_obj()->ctx)) {
		raycast_select_multiple_edit(selection_rect[side].x0, selection_rect[side].y0, selection_rect[side].x1, selection_rect[side].y1, VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
	}
	else {
		raycast_select_multiple(selection_rect[side].x0, selection_rect[side].y0, selection_rect[side].x1, selection_rect[side].y1, VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
	}
	/* Update manipulators */
	Widget_Transform::update_manipulator();

	Widget_Select::Raycast::obj.do_render[side] = false;
}

void Widget_Select::Raycast::render(VR_Side side)
{
	if (side != VR_UI::eye_dominance_get()) {
		return;
	}

	const Mat44f& prior_model_matrix = VR_Draw::get_model_matrix();
	const Mat44f& prior_view_matrix = VR_Draw::get_view_matrix();
	const Mat44f prior_projection_matrix = VR_Draw::get_projection_matrix();

	VR_Draw::update_modelview_matrix(&VR_Math::identity_f, &VR_Math::identity_f);
	VR_Draw::update_projection_matrix(VR_Math::identity_f.m);
	VR_Draw::set_color(0.35f, 0.35f, 1.0f, 1.0f);
	VR_Draw::render_frame(Raycast::selection_rect[side].x0, Raycast::selection_rect[side].x1, Raycast::selection_rect[side].y1, Raycast::selection_rect[side].y0, 0.005f);

	VR_Draw::update_modelview_matrix(&prior_model_matrix, &prior_view_matrix);
	VR_Draw::update_projection_matrix(prior_projection_matrix.m);

	/* Set render flag to false to prevent redundant rendering from duplicate widgets */
	Widget_Select::Raycast::obj.do_render[side] = false;
}

/***********************************************************************************************//**
* \class                               Widget_Select::Proximity
***************************************************************************************************
* Interaction widget for object selection in the proximity selection mode
*
**************************************************************************************************/
Widget_Select::Proximity Widget_Select::Proximity::obj;

Coord3Df Widget_Select::Proximity::p0;
Coord3Df Widget_Select::Proximity::p1;

/* Select multiple objects with proximity selection. */
static void proximity_select_multiple(
	const Coord3Df& p0,
	const Coord3Df& p1,
	bool extend,
	bool deselect,
	bool toggle = false,
	bool enumerate = false,
	bool object = true,
	bool obcenter = true)
{
	bContext *C = vr_get_obj()->ctx;

	ViewContext vc;
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	View3D *v3d = CTX_wm_view3d(C);
	Base *base, *startbase = NULL, *basact = NULL, *oldbasact = BASACT(view_layer);
	const eObjectMode object_mode = oldbasact ? (eObjectMode)oldbasact->object->mode : OB_MODE_OBJECT;
	bool is_obedit;
	//int hits;

	/* Find bounds and center of selection box. */
	float bounds_x = fabsf(p1.x - p0.x) / 2.0f;
	float bounds_y = fabsf(p1.y - p0.y) / 2.0f;
	float bounds_z = fabsf(p1.z - p0.z) / 2.0f;
	Coord3Df center = p0 + (p1 - p0) / 2.0f;

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

	bool hit = false;

	if (!extend && !deselect) {
		/* Do pre-deselection. */
		/* TODO_XR: Compare selection before and after for proper undo behavior. */
		VR_Util::object_deselect_all_visible(view_layer, v3d);

		DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
		ED_undo_push(C, "Select");
	}

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
					const Coord3Df& ob_pos = *(Coord3Df*)base->object->obmat[3];
					if (fabs(ob_pos.x - center.x) < bounds_x &&
						fabs(ob_pos.y - center.y) < bounds_y &&
						fabs(ob_pos.z - center.z) < bounds_z)
					{
						basact = base;
						if (vc.obedit) {
							/* only do select */
							VR_Util::deselectall_except(view_layer, basact);
							ED_object_base_select(basact, BA_SELECT);
						}
						/* also prevent making it active on mouse selection */
						else if (BASE_SELECTABLE(v3d, basact)) {
							//if (extend) {
							//	ED_object_base_select(basact, BA_SELECT);
							//}
							//else
							if (deselect) {
								ED_object_base_select(basact, BA_DESELECT);
							}
							else if (toggle) {
								if (basact->flag & BASE_SELECTED) {
									if (basact == oldbasact) {
										ED_object_base_select(basact, BA_DESELECT);
									}
								}
								else {
									ED_object_base_select(basact, BA_SELECT);
								}
							}
							else {
								ED_object_base_select(basact, BA_SELECT);
								/* When enabled, this puts other objects out of multi pose-mode. */
								/*if (is_pose_mode == false) {
									VR_Util::deselectall_except(view_layer, basact);
									ED_object_base_select(basact, BA_SELECT);
								}*/
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
						hit = true;
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
		int mval[2];
		mval[0] = (int)center_x;
		mval[1] = (int)center_y;
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
	if (hit) {
		DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
		ED_undo_push(C, "Select");
	}
}

/* Adapted from view3d_select.c */
static void proximity_select_multiple_vertex(
	const Coord3Df& p0,
	const Coord3Df& p1,
	ViewContext *vc, bool extend, bool deselect)
{
	bContext *C = vr_get_obj()->ctx;
	/* Find bounds and center of selection box. */
	float bounds_x = fabsf(p1.x - p0.x) / 2.0f;
	float bounds_y = fabsf(p1.y - p0.y) / 2.0f;
	float bounds_z = fabsf(p1.z - p0.z) / 2.0f;
	Coord3Df center = p0 + (p1 - p0) / 2.0f;
	bool is_inside = false;

	if (!extend && !deselect) {
		/* Do pre-deselection. */
		/* TODO_XR: Compare selection before and after for proper undo behavior. */
		VR_Util::deselectall_edit(vc->em->bm, 0);

		DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
		ED_undo_push(C, "Select");
	}

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
			if (fabs(pos.x - center.x) < bounds_x &&
				fabs(pos.y - center.y) < bounds_y &&
				fabs(pos.z - center.z) < bounds_z)
			{
				sv = v;
				is_inside = true;
				const bool is_select = BM_elem_flag_test(sv, BM_ELEM_SELECT);
				const int sel_op_result = ED_select_op_action_deselected(deselect ? SEL_OP_SUB : SEL_OP_ADD, is_select, is_inside);
				if (sel_op_result != -1) {
					BM_vert_select_set(vc->em->bm, sv, sel_op_result);
				}
			}
		}
	}

	if (is_inside) {
		DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
		ED_undo_push(C, "Select");
	}
}

static void proximity_select_multiple_edge(
	const Coord3Df& p0,
	const Coord3Df& p1,
	ViewContext *vc, bool extend, bool deselect)
{
	bContext *C = vr_get_obj()->ctx;
	/* Find bounds and center of selection box. */
	float bounds_x = fabsf(p1.x - p0.x) / 2.0f;
	float bounds_y = fabsf(p1.y - p0.y) / 2.0f;
	float bounds_z = fabsf(p1.z - p0.z) / 2.0f;
	Coord3Df center = p0 + (p1 - p0) / 2.0f;
	float med_co[3];
	bool is_inside = false;

	if (!extend && !deselect) {
		/* Do pre-deselection. */
		/* TODO_XR: Compare selection before and after for proper undo behavior. */
		VR_Util::deselectall_edit(vc->em->bm, 1);

		DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
		ED_undo_push(C, "Select");
	}

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
			if (fabs(pos.x - center.x) < bounds_x &&
				fabs(pos.y - center.y) < bounds_y &&
				fabs(pos.z - center.z) < bounds_z)
			{
				se = e;
				is_inside = true;
				const bool is_select = BM_elem_flag_test(se, BM_ELEM_SELECT);
				const int sel_op_result = ED_select_op_action_deselected(deselect ? SEL_OP_SUB : SEL_OP_ADD, is_select, is_inside);
				if (sel_op_result != -1) {
					BM_edge_select_set(vc->em->bm, se, sel_op_result);
				}
			}
		}
	}

	if (is_inside) {
		DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
		ED_undo_push(C, "Select");
	}
}

static void proximity_select_multiple_face(
	const Coord3Df& p0,
	const Coord3Df& p1,
	ViewContext *vc, bool extend, bool deselect)
{
	bContext *C = vr_get_obj()->ctx;
	/* Find bounds and center of selection box. */
	float bounds_x = fabsf(p1.x - p0.x) / 2.0f;
	float bounds_y = fabsf(p1.y - p0.y) / 2.0f;
	float bounds_z = fabsf(p1.z - p0.z) / 2.0f;
	Coord3Df center = p0 + (p1 - p0) / 2.0f;
	bool is_inside = false;

	if (!extend && !deselect) {
		/* Do pre-deselection. */
		/* TODO_XR: Compare selection before and after for proper undo behavior. */
		VR_Util::deselectall_edit(vc->em->bm, 2);

		DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
		ED_undo_push(C, "Select");
	}

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
			if (fabs(pos.x - center.x) < bounds_x &&
				fabs(pos.y - center.y) < bounds_y &&
				fabs(pos.z - center.z) < bounds_z)
			{
				sf = f;
				is_inside = true;
				const bool is_select = BM_elem_flag_test(sf, BM_ELEM_SELECT);
				const int sel_op_result = ED_select_op_action_deselected(deselect ? SEL_OP_SUB : SEL_OP_ADD, is_select, is_inside);
				if (sel_op_result != -1) {
					BM_face_select_set(vc->em->bm, sf, sel_op_result);
				}
			}
		}
	}

	if (is_inside) {
		DEG_id_tag_update((ID*)vc->obedit->data, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
		ED_undo_push(C, "Select");
	}
}

static void proximity_select_multiple_edit(
	const Coord3Df& p0,
	const Coord3Df& p1,
	bool extend,
	bool deselect,
	bool toggle = false,
	bool enumerate = false)
{
	/* Adapted from do_mesh_box_select() in view3d_select.c */
	VR *vr = vr_get_obj();
	bContext *C = vr->ctx;
	ViewContext vc;

	/* setup view context */
	ED_view3d_viewcontext_init(C, &vc);
	ToolSettings *ts = vc.scene->toolsettings;

	if (vc.obedit) {
		ED_view3d_viewcontext_init_object(&vc, vc.obedit);
		vc.em = BKE_editmesh_from_object(vc.obedit);

		if (ts->selectmode & SCE_SELECT_VERTEX) {
			proximity_select_multiple_vertex(p0, p1, &vc, extend, deselect);
		}
		else if (ts->selectmode & SCE_SELECT_EDGE) {
			proximity_select_multiple_edge(p0, p1, &vc, extend, deselect);
		}
		else if (ts->selectmode & SCE_SELECT_FACE) {
			proximity_select_multiple_face(p0, p1, &vc, extend, deselect);
		}

		EDBM_selectmode_flush(vc.em);
	}
}

bool Widget_Select::Proximity::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Select::Proximity::click(VR_UI::Cursor& c)
{
	if (VR_UI::ctrl_key_get() || VR_UI::shift_key_get()) {
		return;
	}

	bContext *C = vr_get_obj()->ctx;
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	View3D *v3d = CTX_wm_view3d(C);

	/* For now, just use click to clear selection. */
	Object *obedit = CTX_data_edit_object(C);
	if (obedit) {
		ToolSettings *ts = scene->toolsettings;
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		if (ts->selectmode & SCE_SELECT_VERTEX) {
			VR_Util::deselectall_edit(em->bm, 0);
		}
		else if (ts->selectmode & SCE_SELECT_EDGE) {
			VR_Util::deselectall_edit(em->bm, 1);
		}
		else if (ts->selectmode & SCE_SELECT_FACE) {
			VR_Util::deselectall_edit(em->bm, 2);
		}

		EDBM_selectmode_flush(em);

		DEG_id_tag_update((ID*)obedit->data, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	}
	else {
		VR_Util::object_deselect_all_visible(view_layer, v3d);

		DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	}
	/* Update manipulators */
	Widget_Transform::update_manipulator();
	ED_undo_push(C, "Select");
}

void Widget_Select::Proximity::drag_start(VR_UI::Cursor& c)
{
	const Mat44f& m0 = c.interaction_position.get();
	memcpy(&p0, m0.m[3], sizeof(float) * 3);
	const Mat44f& m1 = c.position.get();
	memcpy(&p1, m1.m[3], sizeof(float) * 3);

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Select::Proximity::obj.do_render[i] = true;
	}
}

void Widget_Select::Proximity::drag_contd(VR_UI::Cursor& c)
{
	const Mat44f& m1 = c.position.get();
	memcpy(&p1, m1.m[3], sizeof(float) * 3);

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Select::Proximity::obj.do_render[i] = true;
	}
}

void Widget_Select::Proximity::drag_stop(VR_UI::Cursor& c)
{
	const Mat44f& m1 = c.position.get();
	memcpy(&p1, m1.m[3], sizeof(float) * 3);

	p0 = VR_UI::convert_space(p0, VR_SPACE_REAL, VR_SPACE_BLENDER);
	p1 = VR_UI::convert_space(p1, VR_SPACE_REAL, VR_SPACE_BLENDER);

	if (CTX_data_edit_object(vr_get_obj()->ctx)) {
		proximity_select_multiple_edit(p0, p1, VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
	}
	else {
		proximity_select_multiple(p0, p1, VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
	}
	/* Update manipulators */
	Widget_Transform::update_manipulator();

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Select::Proximity::obj.do_render[i] = false;
	}
}

void Widget_Select::Proximity::render(VR_Side side)
{
	const Mat44f& prior_model_matrix = VR_Draw::get_model_matrix();
	const Mat44f& prior_view_matrix = VR_Draw::get_view_matrix();
	const Mat44f prior_projection_matrix = VR_Draw::get_projection_matrix();

	static Coord3Df p0i;
	static Coord3Df p1i;

	const Mat44f& nav = VR_UI::navigation_matrix_get();
	const Mat44f& nav_inv = VR_UI::navigation_inverse_get();
	VR_Math::multiply_mat44_coord3D(p0i, nav, p0);
	VR_Math::multiply_mat44_coord3D(p1i, nav, p1);

	VR_Draw::update_modelview_matrix(&nav_inv, 0);
	VR_Draw::set_depth_test(false, false);
	VR_Draw::set_color(0.35f, 0.35f, 1.0f, 0.1f);
	VR_Draw::render_box(p0i, p1i, true);
	VR_Draw::set_depth_test(true, false);
	VR_Draw::set_color(0.35f, 0.35f, 1.0f, 0.4f);
	VR_Draw::render_box(p0i, p1i, true);
	VR_Draw::set_depth_test(true, true);

	VR_Draw::update_modelview_matrix(&prior_model_matrix, &prior_view_matrix);
	VR_Draw::update_projection_matrix(prior_projection_matrix.m);

	/* Set render flag to false to prevent redundant rendering from duplicate widgets. */
	Widget_Select::Proximity::obj.do_render[side] = false;
}
