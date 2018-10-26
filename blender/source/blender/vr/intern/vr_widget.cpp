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

/** \file blender/vr/intern/vr_widget.cpp
*   \ingroup vr
* 
* Main module for the VR widget UI.
*/

#include "vr_types.h"

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget.h"
#include "vr_widget_layout.h"

#include "vr_math.h"
#include "vr_draw.h"

#include "BLI_math.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_gpencil.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_layer.h"
#include "BKE_lamp.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
#include "BKE_speaker.h"
#include "BKE_tracking.h"

#include "DNA_gpencil_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_armature.h"
#include "ED_gpencil.h"
#include "ED_object.h"
#include "ED_undo.h"
#include "ED_view3d.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

#include "gpencil_intern.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "WM_types.h"

/* Modified from view3d_project.c */
#define VR_NEAR_CLIP 0.0001f
#define VR_ZERO_CLIP 0.0001f

/* Static transformation matrix for rendering touched widgets. */
float m_wt[4][4] = { 1.5f, 0.0f, 0.0f, 0.0f,
					 0.0f, 1.5f, 0.0f, 0.0f,
					 0.0f, 0.0f, 1.5f, 0.0f,
					 0.0f, 0.0f, 0.003f, 1.0f };
static const Mat44f m_widget_touched = m_wt;

VR_Widget* VR_Widget::get_widget(Type type, const char* ident)
{
	switch (type) {
	case TYPE_TRIGGER:
		return &Widget_Trigger::obj;
	case TYPE_SELECT:
		return &Widget_Select::obj;
	case TYPE_SELECT_RAYCAST:
		return &Widget_Select::Raycast::obj;
	case TYPE_SELECT_PROXIMITY:
		return &Widget_Select::Proximity::obj;
	case TYPE_NAVI:
		return &Widget_Navi::obj;
	case TYPE_NAVI_GRABAIR:
		return &Widget_Navi::GrabAir::obj;
	case TYPE_NAVI_JOYSTICK:
		return &Widget_Navi::Joystick::obj;
	case TYPE_NAVI_TELEPORT:
		return &Widget_Navi::Teleport::obj;
	case TYPE_SHIFT:
		return &Widget_Shift::obj;
	case TYPE_ALT:
		return &Widget_Alt::obj;
	case TYPE_CURSOROFFSET:
		return &Widget_CursorOffset::obj;
	case TYPE_ANNOTATE:
		return &Widget_Annotate::obj;
	case TYPE_MEASURE:
		return &Widget_Measure::obj;
	case TYPE_QUICKGRAB:
		return &Widget_QuickGrab::obj;
	case TYPE_DELETE:
		return &Widget_Delete::obj;
	case TYPE_DUPLICATE:
		return &Widget_Duplicate::obj;
	case TYPE_UNDO:
		return &Widget_Undo::obj;
	case TYPE_REDO:
		return &Widget_Redo::obj;
	case TYPE_SWITCHTOOL:
		return &Widget_SwitchTool::obj;
	case TYPE_SWITCHTOOLMODE:
		return &Widget_SwitchToolMode::obj;
	case TYPE_MENU_COLORWHEEL:
		return &Widget_Menu_ColorWheel::obj;
	default:
		return 0; /* not found or invalid type */
	}
}

VR_Widget::Type VR_Widget::get_widget_type(const std::string& str)
{
	if (str == "TRIGGER") {
		return TYPE_TRIGGER;
	}
	if (str == "SELECT") {
		return TYPE_SELECT;
	}
	if (str == "SELECT_RAYCAST") {
		return TYPE_SELECT_RAYCAST;
	}
	if (str == "SELECT_PROXIMITY") {
		return TYPE_SELECT_PROXIMITY;
	}
	if (str == "NAVI") {
		return TYPE_NAVI;
	}
	if (str == "NAVI_GRABAIR") {
		return TYPE_NAVI_GRABAIR;
	}
	if (str == "NAVI_JOYSTICK") {
		return TYPE_NAVI_JOYSTICK;
	}
	if (str == "NAVI_TELEPORT") {
		return TYPE_NAVI_TELEPORT;
	}
	if (str == "SHIFT") {
		return TYPE_SHIFT;
	}
	if (str == "ALT") {
		return TYPE_ALT;
	}
	if (str == "CURSOROFFSET") {
		return TYPE_CURSOROFFSET;
	}
	if (str == "ANNOTATE") {
		return TYPE_ANNOTATE;
	}
	if (str == "MEASURE") {
		return TYPE_MEASURE;
	}
	if (str == "QUICKGRAB") {
		return TYPE_QUICKGRAB;
	}
	if (str == "DELETE") {
		return TYPE_DELETE;
	}
	if (str == "DUPLICATE") {
		return TYPE_DUPLICATE;
	}
	if (str == "UNDO") {
		return TYPE_UNDO;
	}
	if (str == "REDO") {
		return TYPE_REDO;
	}
	if (str == "SWITCHTOOL") {
		return TYPE_SWITCHTOOL;
	}
	if (str == "SWITCHTOOLMODE") {
		return TYPE_SWITCHTOOLMODE;
	}
	if (str == "MENU_COLORWHEEL") {
		return TYPE_MENU_COLORWHEEL;
	}
	return TYPE_INVALID;
}

VR_Widget* VR_Widget::get_widget(const std::string& str)
{
	if (str == "TRIGGER") {
		return &Widget_Trigger::obj;
	}
	if (str == "SELECT") {
		return &Widget_Select::obj;
	}
	if (str == "SELECT_RAYCAST") {
		return &Widget_Select::Raycast::obj;
	}
	if (str == "SELECT_PROXIMITY") {
		return &Widget_Select::Proximity::obj;
	}
	if (str == "NAVI") {
		return &Widget_Navi::obj;
	}
	if (str == "NAVI_GRABAIR") {
		return &Widget_Navi::GrabAir::obj;
	}
	if (str == "NAVI_JOYSTICK") {
		return &Widget_Navi::Joystick::obj;
	}
	if (str == "NAVI_TELEPORT") {
		return &Widget_Navi::Teleport::obj;
	}
	if (str == "SHIFT") {
		return &Widget_Shift::obj;
	}
	if (str == "ALT") {
		return &Widget_Alt::obj;
	}
	if (str == "CURSOROFFSET") {
		return &Widget_CursorOffset::obj;
	}
	if (str == "ANNOTATE") {
		return &Widget_Annotate::obj;
	}
	if (str == "MEASURE") {
		return &Widget_Measure::obj;
	}
	if (str == "QUICKGRAB") {
		return &Widget_QuickGrab::obj;
	}
	if (str == "DELETE") {
		return &Widget_Delete::obj;
	}
	if (str == "DUPLICATE") {
		return &Widget_Duplicate::obj;
	}
	if (str == "UNDO") {
		return &Widget_Undo::obj;
	}
	if (str == "REDP") {
		return &Widget_Redo::obj;
	}
	if (str == "SWITCHTOOL") {
		return &Widget_SwitchTool::obj;
	}
	if (str == "SWITCHTOOLMODE") {
		return &Widget_SwitchToolMode::obj;
	}
	if (str == "MENU_COLORWHEEL") {
		return &Widget_Menu_ColorWheel::obj;
	}

	return 0;
}

std::vector<std::string> VR_Widget::list_widgets()
{
	std::vector<std::string> ret;
	ret.push_back("TRIGGER");
	ret.push_back("SELECT");
	ret.push_back("SELECT_RAYCAST");
	ret.push_back("SELECT_PROXIMITY");
	ret.push_back("NAVI");
	ret.push_back("NAVI_GRABAIR");
	ret.push_back("NAVI_JOYSTICK");
	ret.push_back("NAVI_TELEPORT");
	ret.push_back("SHIFT");
	ret.push_back("ALT");
	ret.push_back("CURSOROFFSET");
	ret.push_back("ANNOTATE");
	ret.push_back("MEASURE");
	ret.push_back("QUICKGRAB");
	ret.push_back("DELETE");
	ret.push_back("DUPLICATE");
	ret.push_back("UNDO");
	ret.push_back("REDO");
	ret.push_back("SWITCHTOOL");
	ret.push_back("SWITCHTOOLMODE");
	ret.push_back("MENU_COLORWHEEL");

	return ret;
}

std::string VR_Widget::type_to_string(Type type)
{
	switch (type) {
	case TYPE_TRIGGER:
		return "TRIGGER";
	case TYPE_SELECT:
		return "SELECT";
	case TYPE_SELECT_RAYCAST:
		return "SELECT_RAYCAST";
	case TYPE_SELECT_PROXIMITY:
		return "SELECT_PROXIMITY";
	case TYPE_NAVI:
		return "NAVI";
	case TYPE_NAVI_GRABAIR:
		return "NAVI_GRABAIR";
	case TYPE_NAVI_JOYSTICK:
		return "NAVI_JOYSTICK";
	case TYPE_NAVI_TELEPORT:
		return "NAVI_TELEPORT";
	case TYPE_SHIFT:
		return "SHIFT";
	case TYPE_ALT:
		return "ALT";
	case TYPE_CURSOROFFSET:
		return "CURSOROFFSET";
	case TYPE_ANNOTATE:
		return "ANNOTATE";
	case TYPE_MEASURE:
		return "MEASURE";
	case TYPE_QUICKGRAB:
		return "QUICKGRAB";
	case TYPE_DELETE:
		return "DELETE";
	case TYPE_DUPLICATE:
		return "DUPLICATE";
	case TYPE_UNDO:
		return "UNDO";
	case TYPE_REDO:
		return "REDO";
	case TYPE_SWITCHTOOL:
		return "SWITCHTOOL";
	case TYPE_SWITCHTOOLMODE:
		return "SWITCHTOOLMODE";
	case TYPE_MENU_COLORWHEEL:
		return "MENU_COLORWHEEL";
	default:
		return "UNKNOWN";
	}
}

bool VR_Widget::delete_widget(const std::string& str)
{
	return false;
}

/***********************************************************************************************//**
* \class									VR_Widget
***************************************************************************************************
* Interaction widget (abstract superclass).
*
**************************************************************************************************/
VR_Widget::VR_Widget()
{
	do_render[VR_SIDE_LEFT] = false;
	do_render[VR_SIDE_RIGHT] = false;
}

VR_Widget::~VR_Widget()
{
	//
}

bool VR_Widget::has_click(VR_UI::Cursor& c) const
{
	return false; /* by default, widgets don't have a "click" */
}

bool VR_Widget::allows_focus_steal(Type by) const
{
	return false;
}

bool VR_Widget::steals_focus(Type from) const
{
	return false;
}

bool VR_Widget::has_drag(VR_UI::Cursor& c) const
{
	return true; /* by default, widgets have a "drag" */
}

void VR_Widget::click(VR_UI::Cursor& c)
{
	// 
}

void VR_Widget::drag_start(VR_UI::Cursor& c)
{
	// 
}

void VR_Widget::drag_contd(VR_UI::Cursor& c)
{
	// 
}

void VR_Widget::drag_stop(VR_UI::Cursor& c)
{
	// 
}

void VR_Widget::VR_Widget::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
{
	//
}

void VR_Widget::VR_Widget::render(VR_Side side)
{
	//
}

/***********************************************************************************************//**
* \class                               Widget_Trigger
***************************************************************************************************
* Interaction widget for the controller trigger (generalized).
*
**************************************************************************************************/
Widget_Trigger Widget_Trigger::obj;

bool Widget_Trigger::has_click(VR_UI::Cursor& c) const
{
	/* TODO_XR */

	return true;
}

bool Widget_Trigger::allows_focus_steal(Type by) const
{
	/* TODO_XR */

	return false;
}

void Widget_Trigger::click(VR_UI::Cursor& c)
{
	/* TODO_XR */

	Widget_Select::obj.click(c);
}

void Widget_Trigger::drag_start(VR_UI::Cursor& c)
{
	/* TODO_XR */

	Widget_Select::obj.drag_start(c);

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Trigger::obj.do_render[i] = true;
	}
}

void Widget_Trigger::drag_contd(VR_UI::Cursor& c)
{
	/* TODO_XR */

	Widget_Select::obj.drag_contd(c);

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Trigger::obj.do_render[i] = true;
	}
}

void Widget_Trigger::drag_stop(VR_UI::Cursor& c)
{
	/* TODO_XR */

	Widget_Select::obj.drag_stop(c);
}

void Widget_Trigger::render(VR_Side side)
{
	Widget_Select::obj.render(side);

	Widget_Trigger::obj.do_render[side] = false;
}

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
		if (VR_UI::selection_mode_click_switched) {
			Proximity::obj.click(c);
		}
		else {
			Raycast::obj.click(c);
		}
	}
	else { /* SELECTIONMODE_PROXIMITY */
		if (VR_UI::selection_mode_click_switched) {
			Raycast::obj.click(c);
		}
		else {
			Proximity::obj.click(c);
		}
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

/* From view3d_select.c */
static void object_deselect_all_visible(ViewLayer *view_layer)
{
	for (Base *base = (Base*)view_layer->object_bases.first; base; base = base->next) {
		if (BASE_SELECTABLE(base)) {
			ED_object_base_select(base, BA_DESELECT);
		}
	}
}

/* From view3d_select.c */
static void deselectall_except(ViewLayer *view_layer, Base *b)   /* deselect all except b */
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
static eV3DProjStatus view3d_project(const ARegion *ar,
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

	if (((flag & V3D_PROJ_TEST_CLIP_ZERO) == 0) || (fabsf(vec4[3]) > VR_ZERO_CLIP)) {
		if (((flag & V3D_PROJ_TEST_CLIP_NEAR) == 0) || (vec4[3] > VR_NEAR_CLIP)) {
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
	Base *base, *basact = NULL;
	int a;

	if (do_nearest) {
		unsigned int min = 0xFFFFFFFF;
		int selcol = 0, notcol = 0;

		if (has_bones) {
			/* we skip non-bone hits */
			for (a = 0; a < hits; a++) {
				if (min > buffer[4 * a + 1] && (buffer[4 * a + 3] & 0xFFFF0000) ) {
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
			if (BASE_SELECTABLE(base)) {
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

			if (BASE_SELECTABLE(base)) {
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

/* Select a single object with raycast selection. 
 * Adapted from ed_object_select_pick() in view3d_select.c. */
static void raycast_select_single(
	const Coord3Df& p, 
	bool deselect,
	bool extend = false,
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
				if (BASE_SELECTABLE(base) &&
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
						if (base == BASACT(view_layer)) dist_temp += 10.0f;
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

								DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
								DEG_id_tag_update(&clip->id, DEG_TAG_SELECT_UPDATE);
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
		else if (BASE_SELECTABLE(basact)) {
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
					object_deselect_all_visible(view_layer);
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
					OB_MODE_GPENCIL_PAINT,
					OB_MODE_GPENCIL_SCULPT,
					OB_MODE_GPENCIL_WEIGHT))
				{
					ED_gpencil_toggle_brush_cursor(C, true, NULL);
				}
				else {
					/* TODO: maybe is better use restore */
					ED_gpencil_toggle_brush_cursor(C, false, NULL);
				}
			}
		}

		DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
		ED_undo_push(C, "Select");
	}
	else {
		if (deselect) {
			object_deselect_all_visible(view_layer);
			DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
			WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
			ED_undo_push(C, "Select");
		}
	}
}

/* Select multiple objects with raycast selection.
 * p0 and p1 should be in screen coordinates (-1, 1). */
static void raycast_select_multiple(
	const float& x0, const float& y0,
	const float& x1, const float& y1,
	bool deselect,
	bool extend = false,
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

	bool changed = false;

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
				if (BASE_SELECTABLE(base) &&
					((object_type_exclude_select & (1 << base->object->type)) == 0))
				{
					float screen_co[2];
					/* TODO_XR: Use rv3d->persmat of dominant eye. */
					RegionView3D *rv3d = (RegionView3D*)ar->regiondata;
					if (view3d_project(
						ar, rv3d->persmat, false, base->object->obmat[3], screen_co,
						(eV3DProjTest)(V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR)) == V3D_PROJ_RET_OK)
					{
						if (fabsf(screen_co[0] - center_x) < bounds_x &&
							fabsf(screen_co[1] - center_y) < bounds_y) {
							basact = base;
							if (vc.obedit) {
								/* only do select */
								deselectall_except(view_layer, basact);
								ED_object_base_select(basact, BA_SELECT);
							}
							/* also prevent making it active on mouse selection */
							else if (BASE_SELECTABLE(basact)) {
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
										deselectall_except(view_layer, basact);
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
										OB_MODE_GPENCIL_PAINT,
										OB_MODE_GPENCIL_SCULPT,
										OB_MODE_GPENCIL_WEIGHT))
									{
										ED_gpencil_toggle_brush_cursor(C, true, NULL);
									}
									else {
										/* TODO: maybe is better use restore */
										ED_gpencil_toggle_brush_cursor(C, false, NULL);
									}
								}
							}
							changed = true;
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

								DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
								DEG_id_tag_update(&clip->id, DEG_TAG_SELECT_UPDATE);
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
	if (changed) {
		DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
		ED_undo_push(C, "Select");
	}
}

bool Widget_Select::Raycast::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Select::Raycast::click(VR_UI::Cursor& c)
{
	const Mat44f& m = c.position.get();
	raycast_select_single(*(Coord3Df*)m.m[3], VR_UI::shift_key_get());
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

	raycast_select_multiple(selection_rect[side].x0, selection_rect[side].y0, selection_rect[side].x1, selection_rect[side].y1, VR_UI::shift_key_get());

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
	VR_Draw::set_color(0.0f, 1.0f, 0.7f, 1.0f);
	VR_Draw::render_frame(Raycast::selection_rect[side].x0, Raycast::selection_rect[side].x1, Raycast::selection_rect[side].y1, Raycast::selection_rect[side].y0, 0.005f);

	VR_Draw::update_modelview_matrix(&prior_model_matrix, &prior_view_matrix);
	VR_Draw::update_projection_matrix(prior_projection_matrix.m);

	// Set render flag to false to prevent redundant rendering from duplicate widgets
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
	bool deselect,
	bool extend = false,
	bool toggle = false,
	bool enumerate = false,
	bool object = true,
	bool obcenter = true)
{
	bContext *C = vr_get_obj()->ctx;

	ViewContext vc;
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
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

	bool changed = false;

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
				if (BASE_SELECTABLE(base) &&
					((object_type_exclude_select & (1 << base->object->type)) == 0))
				{
					Coord3Df& ob_pos = *(Coord3Df*)base->object->obmat[3];
					if (fabs(ob_pos.x - center.x) < bounds_x &&
						fabs(ob_pos.y - center.y) < bounds_y &&
						fabs(ob_pos.z - center.z) < bounds_z)
					{
						basact = base;
						if (vc.obedit) {
							/* only do select */
							deselectall_except(view_layer, basact);
							ED_object_base_select(basact, BA_SELECT);
						}
						/* also prevent making it active on mouse selection */
						else if (BASE_SELECTABLE(basact)) {
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
									deselectall_except(view_layer, basact);
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
									OB_MODE_GPENCIL_PAINT,
									OB_MODE_GPENCIL_SCULPT,
									OB_MODE_GPENCIL_WEIGHT))
								{
									ED_gpencil_toggle_brush_cursor(C, true, NULL);
								}
								else {
									/* TODO: maybe is better use restore */
									ED_gpencil_toggle_brush_cursor(C, false, NULL);
								}
							}
						}
						changed = true;
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

								DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
								DEG_id_tag_update(&clip->id, DEG_TAG_SELECT_UPDATE);
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
	if (changed) {
		DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
		ED_undo_push(C, "Select");
	}
}

bool Widget_Select::Proximity::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Select::Proximity::click(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);

	/* For now, just use click to clear selection. */
	if (VR_UI::shift_key_get()) {
		object_deselect_all_visible(view_layer);
		DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
		ED_undo_push(C, "Select");
	}
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

	// Need Maya coordinates
	p0 = VR_UI::convert_space(p0, VR_SPACE_REAL, VR_SPACE_BLENDER);
	p1 = VR_UI::convert_space(p1, VR_SPACE_REAL, VR_SPACE_BLENDER);
	// Rotation from the head, position from the points;
	//static Mat44f m;
	//switch (VR_UI::selection_volume_alignment) {
	//case VR_UI::SELECTIONVOLUMEALIGNMENT_HEAD:
	//	m = VR_UI::hmd_position_get(VR_SPACE_BLENDER);
	//	break;
	//case VR_UI::SELECTIONVOLUMEALIGNMENT_BLENDER:
	//	m = VR_Math::identity_f;
	//	break;
	//case VR_UI::SELECTIONVOLUMEALIGNMENT_REAL:
	//	m = VR_UI::navigation_matrix_get();
	//	{
	//		((Coord3Df*)m.m[0])->normalize();
	//		((Coord3Df*)m.m[1])->normalize();
	//		((Coord3Df*)m.m[2])->normalize();
	//	}
	//	break;
	//}
	//memcpy(m.m[3], &p0, sizeof(float) * 3);

	proximity_select_multiple(p0, p1, VR_UI::shift_key_get());

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

	switch (VR_UI::selection_volume_alignment) {
		case VR_UI::SELECTIONVOLUMEALIGNMENT_HEAD:
		{
			const Mat44f& eye = VR_UI::hmd_position_get(VR_SPACE_REAL);
			const Mat44f& eye_inv = VR_UI::hmd_position_get(VR_SPACE_REAL, true);
			static Coord3Df p0i;
			static Coord3Df p1i;
			VR_Math::multiply_mat44_coord3D(p0i, eye_inv, p0);
			VR_Math::multiply_mat44_coord3D(p1i, eye_inv, p1);

			VR_Draw::update_modelview_matrix(&eye, 0);
			VR_Draw::set_depth_test(false, false);
			VR_Draw::set_color(0.35f, 0.35f, 1.0f, 0.1f);
			VR_Draw::render_box(p0i, p1i);
			VR_Draw::set_depth_test(true, false);
			VR_Draw::set_color(0.35f, 0.35f, 1.0f, 0.4f);
			VR_Draw::render_box(p0i, p1i);
			VR_Draw::set_depth_test(true, true);
			break;
		}
		case VR_UI::SELECTIONVOLUMEALIGNMENT_BLENDER:
		{
			const Mat44f& nav = VR_UI::navigation_matrix_get();
			const Mat44f& nav_inv = VR_UI::navigation_inverse_get();
			VR_Math::multiply_mat44_coord3D(p0i, nav, p0);
			VR_Math::multiply_mat44_coord3D(p1i, nav, p1);

			VR_Draw::update_modelview_matrix(&nav_inv, 0);
			VR_Draw::set_depth_test(false, false);
			VR_Draw::set_color(0.35f, 0.35f, 1.0f, 0.1f);
			VR_Draw::render_box(p0i, p1i);
			VR_Draw::set_depth_test(true, false);
			VR_Draw::set_color(0.35f, 0.35f, 1.0f, 0.4f);
			VR_Draw::render_box(p0i, p1i);
			VR_Draw::set_depth_test(true, true);
			break;
		}
		case VR_UI::SELECTIONVOLUMEALIGNMENT_REAL:
		{
			VR_Draw::update_modelview_matrix(&VR_Math::identity_f, 0);
			VR_Draw::set_depth_test(false, false);
			VR_Draw::set_color(0.35f, 0.35f, 1.0f, 0.1f);
			VR_Draw::render_box(p0, p1);
			VR_Draw::set_depth_test(true, false);
			VR_Draw::set_color(0.35f, 0.35f, 1.0f, 0.4f);
			VR_Draw::render_box(p0, p1);
			VR_Draw::set_depth_test(true, true);
			break;
		}
		default: {
			break;
		}
	}

	VR_Draw::update_modelview_matrix(&prior_model_matrix, &prior_view_matrix);
	VR_Draw::update_projection_matrix(prior_projection_matrix.m);

	/* Set render flag to false to prevent redundant rendering from duplicate widgets. */
	Widget_Select::Proximity::obj.do_render[side] = false;
}

/***********************************************************************************************//**
 * \class                                  Widget_Navi
 ***************************************************************************************************
 * Interaction widget for grabbing-the-air navigation.
 * Will select the appropriate sub-widget based on the setting UserInterface::navigation_mode.
 **************************************************************************************************/
Widget_Navi Widget_Navi::obj;

void Widget_Navi::drag_start(VR_UI::Cursor& c)
{
	switch (VR_UI::navigation_mode) {
	case VR_UI::NAVIGATIONMODE_GRABAIR:
		return GrabAir::obj.drag_start(c);
	case VR_UI::NAVIGATIONMODE_JOYSTICK:
		return Joystick::obj.drag_start(c);
	case VR_UI::NAVIGATIONMODE_TELEPORT:
		return Teleport::obj.drag_start(c);
	case VR_UI::NAVIGATIONMODE_NONE:
		return;
	}
}

void Widget_Navi::drag_contd(VR_UI::Cursor& c)
{
	switch (VR_UI::navigation_mode) {
	case VR_UI::NAVIGATIONMODE_GRABAIR:
		return GrabAir::obj.drag_contd(c);
	case VR_UI::NAVIGATIONMODE_JOYSTICK:
		return Joystick::obj.drag_contd(c);
	case VR_UI::NAVIGATIONMODE_TELEPORT:
		return Teleport::obj.drag_contd(c);
	case VR_UI::NAVIGATIONMODE_NONE:
		return;
	}
}

void Widget_Navi::drag_stop(VR_UI::Cursor& c)
{
	switch (VR_UI::navigation_mode) {
	case VR_UI::NAVIGATIONMODE_GRABAIR:
		return GrabAir::obj.drag_stop(c);
	case VR_UI::NAVIGATIONMODE_JOYSTICK:
		return Joystick::obj.drag_stop(c);
	case VR_UI::NAVIGATIONMODE_TELEPORT:
		return Teleport::obj.drag_stop(c);
	case VR_UI::NAVIGATIONMODE_NONE:
		return;
	}
}

void Widget_Navi::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
{
	switch (VR_UI::navigation_mode) {
	case VR_UI::NAVIGATIONMODE_GRABAIR:
		return GrabAir::obj.render_icon(t, controller_side, active, touched);
	case VR_UI::NAVIGATIONMODE_JOYSTICK:
		return Joystick::obj.render_icon(t, controller_side, active, touched);
	case VR_UI::NAVIGATIONMODE_TELEPORT:
		return Teleport::obj.render_icon(t, controller_side, active, touched);
	case VR_UI::NAVIGATIONMODE_NONE:
		return;
	}
}

/***********************************************************************************************//**
 * \class                               Widget_Navi::GrabAir
 ***************************************************************************************************
 * Interaction widget for grabbing-the-air navigation.
 *
 **************************************************************************************************/
Widget_Navi::GrabAir Widget_Navi::GrabAir::obj;

void Widget_Navi::GrabAir::drag_start(VR_UI::Cursor& c)
{
	/* Remember where we started from in navigation space. */
	c.interaction_position.set(((Mat44f)(c.position.get(VR_SPACE_REAL))).m, VR_SPACE_REAL);
}

void Widget_Navi::GrabAir::drag_contd(VR_UI::Cursor& c)
{
	static Mat44f curr;
	static Mat44f prev;

	/* Check if we're two-hand navi dragging */
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
	else { /* one-handed navigation */
		curr = c.position.get(VR_SPACE_BLENDER);
		prev = c.interaction_position.get(VR_SPACE_BLENDER);
	}

	if (VR_UI::navigation_lock_rotation) {
		float prev_scale = Coord3Df(prev.m[0][0], prev.m[0][1], prev.m[0][2]).length();
		float curr_scale = Coord3Df(curr.m[0][0], curr.m[0][1], curr.m[0][2]).length();
		float rot_ident[3][4] = { {prev_scale,0,0,0} , {0,prev_scale,0,0} , {0,0,prev_scale,0} };
		std::memcpy(prev.m, rot_ident, sizeof(float) * 3 * 4);
		rot_ident[0][0] = rot_ident[1][1] = rot_ident[2][2] = curr_scale;
		std::memcpy(curr.m, rot_ident, sizeof(float) * 3 * 4);
	}
	else if (VR_UI::navigation_lock_up) {
		Coord3Df z; /* (m.m[2][0], m.m[2][1], m.m[2][2]); // z-axis */
		if (!VR_UI::is_zaxis_up()) {
			z = Coord3Df(0, 1, 0); /* rectify z to point "up" */
		}
		else { /* z is up : */
			z = Coord3Df(0, 0, 1); /* rectify z to point up */
		}
		VR_Math::orient_matrix_z(curr, z);
		VR_Math::orient_matrix_z(prev, z);
	}
	if (VR_UI::navigation_lock_translation) {
		prev = VR_UI::convert_space(prev, VR_SPACE_BLENDER, VR_SPACE_REAL);
		curr = VR_UI::convert_space(curr, VR_SPACE_BLENDER, VR_SPACE_REAL); /* locked in real-world coodinates */
		Coord3Df& t_prev = *(Coord3Df*)prev.m[3];
		Coord3Df& t_curr = *(Coord3Df*)curr.m[3];
		t_curr = t_prev;
		prev = VR_UI::convert_space(prev, VR_SPACE_REAL, VR_SPACE_BLENDER);
		curr = VR_UI::convert_space(curr, VR_SPACE_REAL, VR_SPACE_BLENDER); /* revert to Blender coordinates */
	}
	else if (VR_UI::navigation_lock_altitude) {
		prev = VR_UI::convert_space(prev, VR_SPACE_BLENDER, VR_SPACE_REAL);
		curr = VR_UI::convert_space(curr, VR_SPACE_BLENDER, VR_SPACE_REAL); /* locked in real-world coordinates */
		Coord3Df& t_prev = *(Coord3Df*)prev.m[3];
		Coord3Df& t_curr = *(Coord3Df*)curr.m[3];
		t_curr.z = t_prev.z;
		prev = VR_UI::convert_space(prev, VR_SPACE_REAL, VR_SPACE_BLENDER);
		curr = VR_UI::convert_space(curr, VR_SPACE_REAL, VR_SPACE_BLENDER); /* revert to Blender coordinates */
	}
	if (VR_UI::navigation_lock_scale) {
		((Coord3Df*)prev.m[0])->normalize_in_place();
		((Coord3Df*)prev.m[1])->normalize_in_place();
		((Coord3Df*)prev.m[2])->normalize_in_place();
		((Coord3Df*)curr.m[0])->normalize_in_place();
		((Coord3Df*)curr.m[1])->normalize_in_place();
		((Coord3Df*)curr.m[2])->normalize_in_place();
	}

	VR_UI::navigation_set(VR_UI::navigation_matrix_get() * curr.inverse() * prev);
}

void Widget_Navi::GrabAir::drag_stop(VR_UI::Cursor& c)
{
	/* Check if we're two-hand navi dragging */
	if (c.bimanual) {
		VR_UI::Cursor* other = c.other_hand;
		c.bimanual = VR_UI::Cursor::BIMANUAL_OFF;
		/* the other hand is still dragging - we're leaving a two-hand drag. */
		other->bimanual = VR_UI::Cursor::BIMANUAL_OFF;
		/* ALSO: the other hand should start one-hand manipulating from here: */
		c.other_hand->interaction_position.set(((Mat44f)VR_UI::cursor_position_get(VR_SPACE_REAL, other->side)).m, VR_SPACE_REAL);
	}
}

void Widget_Navi::GrabAir::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
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
	VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::nav_tex);
}

/***********************************************************************************************//**
 * \class                               Widget_Navi::Joystick
 ***************************************************************************************************
 * Interaction widget for joystick-style-navigation.
 *
 **************************************************************************************************/
Widget_Navi::Joystick Widget_Navi::Joystick::obj;

float Widget_Navi::Joystick::move_speed(1.0f);
float Widget_Navi::Joystick::turn_speed(0.4f);
float Widget_Navi::Joystick::zoom_speed(1.0f);

void Widget_Navi::Joystick::drag_start(VR_UI::Cursor& c)
{
	/* Remember where we started from in navigation space. */
	c.interaction_position = c.position;
	c.reference = c.position.get(VR_SPACE_REAL);

}

void Widget_Navi::Joystick::drag_contd(VR_UI::Cursor& c)
{
	/* Get the relative position between start position and now. */
	const Mat44f& hmd = VR_UI::hmd_position_get(VR_SPACE_REAL);
	const Mat44f& curr = c.position.get(VR_SPACE_REAL);

	static Mat44f delta;

	if (vr_get_obj()->ui_type == VR_UI_TYPE_FOVE) {
		/* Move in forward direction of eye cursor. */
		Coord3Df v;
		if (VR_UI::cursor_offset_enabled) {
			/* Maybe we actually want to use the cursor position instead of the controller (gaze convergence) position, 
			 * but for now disable it because it makes joystick navigation difficult. */
			v = *(Coord3Df*)(vr_get_obj()->t_controller[VR_SPACE_REAL][VR_SIDE_MONO][3]) - *(Coord3Df*)hmd.m[3];
		}
		else {
			v = *(Coord3Df*)curr.m[3] - *(Coord3Df*)hmd.m[3];
		}
		v.normalize_in_place();
		delta = VR_Math::identity_f;
		delta.m[3][0] = -v.x * 0.1f * move_speed;
		delta.m[3][1] = -v.y * 0.1f * move_speed;
		if (VR_UI::shift_key_get()) {
			delta.m[3][2] = -v.z * 0.1f * move_speed;
		}
		else {
			delta.m[3][2] = 0;
		}

		/* Apply rotation around z-axis (if any). */
		Coord3Df hmd_right = *(Coord3Df*)hmd.m[0];
		/* flatten on z-(up)-plane */
		v.z = 0;
		hmd_right.z = 0;
		float a = v.angle(hmd_right);
		if (a < 0.36f * PI) { //0.32f*PI
			a *= -a * 0.1f * turn_speed;
			float cos_a = cos(a);
			float sin_a = sin(a);
			/* get angle between and apply to navigation z-rotation */
			delta.m[0][0] = delta.m[1][1] = cos_a;
			delta.m[1][0] = sin_a;
			delta.m[0][1] = -sin_a;
			delta.m[3][0] += cos_a * hmd.m[3][0] - sin_a * hmd.m[3][1] - hmd.m[3][0]; /* rotate around HMD/POV: */
			delta.m[3][1] += cos_a * hmd.m[3][1] + sin_a * hmd.m[3][0] - hmd.m[3][1]; /* use HMD position as rotation pivot */
			delta.m[2][2] = 1;
			delta.m[3][3] = 1;
		}
		else if (a > 0.64f * PI) { //0.68f*PI
			a *= a * 0.02f * turn_speed;
			float cos_a = cos(a);
			float sin_a = sin(a);
			/* get angle between and apply to navigation z-rotation */
			delta.m[0][0] = delta.m[1][1] = cos_a;
			delta.m[1][0] = sin_a;
			delta.m[0][1] = -sin_a;
			delta.m[3][0] += cos_a * hmd.m[3][0] - sin_a * hmd.m[3][1] - hmd.m[3][0]; /* rotate around HMD/POV: */
			delta.m[3][1] += cos_a * hmd.m[3][1] + sin_a * hmd.m[3][0] - hmd.m[3][1]; /* use HMD position as rotation pivot */
			delta.m[2][2] = 1;
			delta.m[3][3] = 1;
		}

		VR_UI::navigation_apply_transformation(delta, VR_SPACE_REAL);
		return;
	}
	
	delta.m[3][0] = curr.m[3][0] - c.reference.m[3][0];
	delta.m[3][0] = delta.m[3][0] * abs(delta.m[3][0]) * -1.0f * move_speed;
	delta.m[3][1] = curr.m[3][1] - c.reference.m[3][1];
	delta.m[3][1] = delta.m[3][1] * abs(delta.m[3][1]) * -1.0f * move_speed;
	if (VR_UI::shift_key_get()) {
		delta.m[3][2] = curr.m[3][2] - c.reference.m[3][2];
		delta.m[3][2] = delta.m[3][2] * abs(delta.m[3][2]) * -1.0f * move_speed;
	}
	else {
		delta.m[3][2] = 0;
	}

	/* rotation from front-facing y-axis */
	Coord3Df y0(c.reference.m[1][0], c.reference.m[1][1], c.reference.m[1][2]);
	Coord3Df y1(curr.m[1][0], curr.m[1][1], curr.m[1][2]);

	/* flatten on z-(up)-plane */
	y0.z = 0;
	y1.z = 0;
	float a = y0.angle(y1);
	a *= a * 0.1f * turn_speed;

	/* get rotation direction */
	Coord3Df z = y0 ^ y1; /* cross product will be up if anti-clockwise, down if clockwise */
	if (z.z < 0) a = -a;
	float cos_a = cos(a);
	float sin_a = sin(a);

	/* get angle between and apply to navigation z-rotation */
	delta.m[0][0] = delta.m[1][1] = cos_a;
	delta.m[1][0] = sin_a;
	delta.m[0][1] = -sin_a;
	delta.m[3][0] += cos_a * hmd.m[3][0] - sin_a * hmd.m[3][1] - hmd.m[3][0]; /* rotate around HMD/POV: */
	delta.m[3][1] += cos_a * hmd.m[3][1] + sin_a * hmd.m[3][0] - hmd.m[3][1]; /* use HMD position as rotation pivot */
	delta.m[2][2] = 1;
	delta.m[3][3] = 1;

	/* Apply with HMD as pivot */
	VR_UI::navigation_apply_transformation(delta, VR_SPACE_REAL);
}

void Widget_Navi::Joystick::drag_stop(VR_UI::Cursor& c)
{
	//
}

void Widget_Navi::Joystick::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
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
	VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::nav_joystick_tex);
}

/***********************************************************************************************//**
 * \class                               Widget_Navi::Teleport
 ***************************************************************************************************
 * Interaction widget for teleport navigation.
 *
 **************************************************************************************************/
Widget_Navi::Teleport Widget_Navi::Teleport::obj;

Mat44f Widget_Navi::Teleport::arrow;

bool Widget_Navi::Teleport::cancel(false);

void Widget_Navi::Teleport::drag_start(VR_UI::Cursor& c)
{
	// Remember where we started from in navigation space
	c.interaction_position = c.position;
	c.reference = c.position.get(VR_SPACE_REAL);
	arrow = VR_Math::identity_f;
	memcpy(arrow.m[3], c.reference.m[3], sizeof(float) * 4);

	cancel = false;
}

void Widget_Navi::Teleport::drag_contd(VR_UI::Cursor& c)
{
	if (VR_UI::shift_key_get()) {
		cancel = true;
	}

	if (!cancel) {
		const Mat44f& curr = c.position.get(VR_SPACE_REAL);

		static Mat44f delta = VR_Math::identity_f;
		delta.m[3][0] = curr.m[3][0] - c.reference.m[3][0];
		delta.m[3][0] = delta.m[3][0] * abs(delta.m[3][0]);

		delta.m[3][1] = curr.m[3][1] - c.reference.m[3][1];
		delta.m[3][1] = delta.m[3][1] * abs(delta.m[3][1]);

		//if (VR_UI::shift_key_get()) {
		delta.m[3][2] = curr.m[3][2] - c.reference.m[3][2];
		delta.m[3][2] = delta.m[3][2] * abs(delta.m[3][2]);
		//}

		arrow = delta * arrow;

		for (int i = 0; i < VR_SIDES; ++i) {
			Widget_Navi::Teleport::obj.do_render[i] = true;
		}
	}
}

void Widget_Navi::Teleport::drag_stop(VR_UI::Cursor& c)
{
	if (VR_UI::shift_key_get()) {
		cancel = true;
	}

	if (!cancel) {
		static Mat44f reference = VR_Math::identity_f;
		memcpy(reference.m[3], c.reference.m[3], sizeof(float) * 4);

		VR_UI::navigation_apply_transformation(arrow.inverse() * reference, VR_SPACE_REAL);
	}
}

void Widget_Navi::Teleport::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
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
	VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::nav_teleport_tex);
}

void Widget_Navi::Teleport::render(VR_Side side)
{
	const Mat44f& prior_model_matrix = VR_Draw::get_model_matrix();
	VR_Draw::update_modelview_matrix(&arrow, 0);

	VR_Draw::set_depth_test(false, false);
	VR_Draw::set_color(0.0f, 0.7f, 1.0f, 0.1f);
	VR_Draw::render_ball(0.05f, true);
	//VR_Draw::render_arrow(Coord3Df(-0.05f, -0.05f, -0.05f), Coord3Df(0.05f, 0.05f, 0.05f), 0.1f, 1.0f, 1.0f, VR_Draw::zoom_tex);
	VR_Draw::set_depth_test(true, false);
	VR_Draw::set_color(0.0f, 0.7f, 1.0f, 0.4f);
	VR_Draw::render_ball(0.05f, true);
	//VR_Draw::render_arrow(Coord3Df(-0.05f, -0.05f, -0.05f), Coord3Df(0.05f, 0.05f, 0.05f), 0.1f, 1.0f, 1.0f, VR_Draw::zoom_tex);
	VR_Draw::set_depth_test(true, true);

	VR_Draw::update_modelview_matrix(&prior_model_matrix, 0);

	Widget_Navi::Teleport::obj.do_render[side] = false;
}

/***********************************************************************************************//**
 * \class                               Widget_Shift
 ***************************************************************************************************
 * Interaction widget for emulating the shift key on a keyboard.
 *
 **************************************************************************************************/
Widget_Shift Widget_Shift::obj;

void Widget_Shift::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
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
	VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::ctrl_tex);
}

/***********************************************************************************************//**
 * \class                               Widget_Alt
 ***************************************************************************************************
 * Interaction widget for emulating the alt key on a keyboard.
 *
 **************************************************************************************************/
Widget_Alt Widget_Alt::obj;

bool Widget_Alt::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Alt::click(VR_UI::Cursor& c)
{
	VR_UI::AltState alt = VR_UI::alt_key_get();

	/* Toggle the alt state. */
	VR_UI::alt_key_set((VR_UI::AltState)!alt);
}

bool Widget_Alt::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_Alt::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
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
	VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::alt_tex);
}

/***********************************************************************************************//**
 * \class                               Widget_CursorOffset
 ***************************************************************************************************
 * Interaction widget for manipulating the VR UI cursor offset.
 *
 **************************************************************************************************/
Widget_CursorOffset Widget_CursorOffset::obj;

bool Widget_CursorOffset::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_CursorOffset::click(VR_UI::Cursor& c)
{
	VR_UI::cursor_offset_enabled = !VR_UI::cursor_offset_enabled;
	VR_UI::cursor_offset_update = false;
}

void Widget_CursorOffset::drag_start(VR_UI::Cursor& c)
{
	VR_UI::cursor_offset_enabled = true;
	VR_UI::cursor_offset_update = true;
}

void Widget_CursorOffset::drag_contd(VR_UI::Cursor& c)
{
	//
}

void Widget_CursorOffset::drag_stop(VR_UI::Cursor& c)
{
	VR_UI::cursor_offset_enabled = true;
	VR_UI::cursor_offset_update = false;
}

void Widget_CursorOffset::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
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
	VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::cursoroffset_tex);
}

/***********************************************************************************************//**
 * \class                               Widget_Annotate
 ***************************************************************************************************
 * Interaction widget for the gpencil annotation tool.
 *
 **************************************************************************************************/
Widget_Annotate Widget_Annotate::obj;

bGPdata *Widget_Annotate::gpd(0);
std::vector<bGPDlayer *> Widget_Annotate::gpl;
std::vector<bGPDframe *> Widget_Annotate::gpf;
Main *Widget_Annotate::main(0);

uint Widget_Annotate::num_layers(13); 
uint Widget_Annotate::active_layer(0);

std::vector<bGPDspoint> Widget_Annotate::points;

//float Widget_Annotate::point_thickness(40.0f);
float Widget_Annotate::line_thickness(10.0f);
float Widget_Annotate::color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

bool Widget_Annotate::eraser(false);
VR_Side Widget_Annotate::cursor_side;
float Widget_Annotate::eraser_radius(0.05f);

int Widget_Annotate::init(bool new_scene)
{
	/* Allocate gpencil data/layer/frame and set to active. */
	bContext *C = vr_get_obj()->ctx;
	if (new_scene) {
		gpl.clear();
		gpf.clear();
		/* TODO_XR: This causes memory access errors... */
		/*if (gpd) {
			BKE_gpencil_free(gpd, 1);
		}*/

		gpd = BKE_gpencil_data_addnew(CTX_data_main(C), "VR_Annotate");
		if (!gpd) {
			return -1;
		}
		gpd->flag |= (GP_DATA_ANNOTATIONS); //| GP_DATA_STROKE_EDITMODE);
		//gpd->xray_mode = GP_XRAY_3DSPACE;
		//ED_gpencil_add_defaults(C);
	}

	/* The last layer is the measure tool layer. 
	 * TODO_XR: Refactor this / use a std::map. */
	for (uint i = 0; i < num_layers; ++i) {
		bGPDlayer *gp_layer = BKE_gpencil_layer_addnew(gpd, "VR_Annotate", true);
		if (!gp_layer) {
			if (gpl.size() > 0) {
				BKE_gpencil_free(gpd, 1);
			}
			else {
				BKE_gpencil_free(gpd, 0);
			}
			return -1;
		}
		memcpy(gp_layer->color, color, sizeof(float) * 4);
		gp_layer->thickness = line_thickness / 1.15f;

		bGPDframe *gp_frame = BKE_gpencil_frame_addnew(gp_layer, 0);
		if (!gp_frame) {
			BKE_gpencil_free(gpd, 1);
			return -1;
		}

		gpl.push_back(gp_layer);
		gpf.push_back(gp_frame);
	}

	/* TODO_XR: Find a way to "coexist" with any existing scene gpd */
	Scene *scene = CTX_data_scene(C);
	scene->gpd = gpd;

	return 0;
}

void Widget_Annotate::erase_stroke(bGPDstroke *gps, bGPDframe *gp_frame) {

	/* Adapted from gp_stroke_eraser_do_stroke() in annotate_paint.c */

	bGPDspoint *pt1, *pt2;
	int i;

	if (gps->totpoints == 0) {
		/* just free stroke */
		BKE_gpencil_free_stroke(gps);
	}
	else if (gps->totpoints == 1) {
		/* only process if it hasn't been masked out... */
		//if (!(gps->points->flag & GP_SPOINT_SELECT)) {		
			const Mat44f& c = VR_UI::cursor_position_get(VR_SPACE_BLENDER, cursor_side);
			const Coord3Df& c_pos = *(Coord3Df*)c.m[3];
			const Coord3Df& pt_pos = *(Coord3Df*)gps->points;
			if ((pt_pos - c_pos).length() <= eraser_radius * VR_UI::navigation_scale_get()) {
				gps->points->flag |= GP_SPOINT_TAG;
				gp_stroke_delete_tagged_points(gp_frame, gps, gps->next, GP_SPOINT_TAG, false);
			}
		//}
	}
	else {
		bool inside_sphere = false;

		/* Clear Tags
		 *
		 * Note: It's better this way, as we are sure that
		 * we don't miss anything, though things will be
		 * slightly slower as a result
		 */
		for (i = 0; i < gps->totpoints; ++i) {
			bGPDspoint *pt = &gps->points[i];
			pt->flag &= ~GP_SPOINT_TAG;
		}

		/* First Pass: Loop over the points in the stroke
		 *   1) Thin out parts of the stroke under the brush
		 *   2) Tag "too thin" parts for removal (in second pass)
		 */
		for (i = 0; (i + 1) < gps->totpoints; ++i) {
			/* get points to work with */
			pt1 = gps->points + i;
			pt2 = gps->points + i + 1;

			/* only process if it hasn't been masked out... */
			//if (!(gps->points->flag & GP_SPOINT_SELECT))
			//	continue;

			/* Check if point segment of stroke had anything to do with
			 * eraser region (either within stroke painted, or on its lines)
			 *  - this assumes that linewidth is irrelevant */
			const Mat44f& c = VR_UI::cursor_position_get(VR_SPACE_BLENDER, cursor_side);
			const Coord3Df& c_pos = *(Coord3Df*)c.m[3];
			const Coord3Df& pt1_pos = *(Coord3Df*)pt1;
			const Coord3Df& pt2_pos = *(Coord3Df*)pt2;
			if ((pt1_pos - c_pos).length() <= eraser_radius * VR_UI::navigation_scale_get()) {
				pt1->flag |= GP_SPOINT_TAG;
				inside_sphere = true;
			}
			if ((pt2_pos - c_pos).length() <= eraser_radius * VR_UI::navigation_scale_get()) {
				pt2->flag |= GP_SPOINT_TAG;
				inside_sphere = true;
			}
		}

		/* Second Pass: Remove any points that are tagged */
		if (inside_sphere) {
			gp_stroke_delete_tagged_points(gp_frame, gps, gps->next, GP_SPOINT_TAG, false);
		}
	} 
}

//bool Widget_Annotate::has_click(VR_UI::Cursor& c) const
//{
//	return true;
//}
//
//void Widget_Annotate::click(VR_UI::Cursor& c)
//{
//	/* Eraser */
//	if (VR_UI::shift_key_get() == VR_UI::SHIFTSTATE_ON) {
//		eraser = true;
//		cursor_side = c.side;
//		if (gpf) {
//			/* Loop over VR strokes and check if they should be erased.
//			 * Maybe there's a better way to do this? */
//			bGPDstroke *gpn;
//			for (bGPDstroke *gps = (bGPDstroke*)gpf->strokes.first; gps; gps = gpn) {
//				gpn = gps->next;
//				Widget_Annotate::erase_stroke(gps);
//			}
//		}
//	}
//	else {
//		eraser = false;
//
//		/* Draw a single point. */
//		points.clear();
//
//		bGPDspoint pt;
//
//		const Mat44f& cursor = c.position.get(VR_SPACE_BLENDER);
//		memcpy(&pt, cursor.m[3], sizeof(float) * 3);
//		pt.pressure = 1.0f; //(vr->controller[c.side]->trigger_pressure);
//		pt.strength = 1.0f;
//		//pt.flag = GP_SPOINT_SELECT;
//
//		points.push_back(pt);
//
//		if (!gpf) {
//			int error = Widget_Annotate::init();
//			if (error) {
//				return;
//			}
//		}
//
//		/* TODO_XR: Find a way to "coexist" with any existing scene gpd. */
//		//BKE_gpencil_layer_setactive(gpd, gpl);
//
//		/* Add new stroke. */
//		bGPDstroke *gps = BKE_gpencil_add_stroke(gpf, 0, 1, point_thickness /*/ 25.0f */);
//		gps->points[0] = points[0];
//	}
//
//	for (int i = 0; i < VR_SIDES; ++i) {
//		Widget_Annotate::obj.do_render[i] = true;
//	}
//}

void Widget_Annotate::drag_start(VR_UI::Cursor& c)
{
	/* Eraser */
	if (VR_UI::shift_key_get() == VR_UI::SHIFTSTATE_ON) {
		eraser = true;
		cursor_side = c.side;

		Main *curr_main = CTX_data_main(vr_get_obj()->ctx);
		if (gpf.size() < 1 || main != curr_main) {
			int error = Widget_Annotate::init(main != curr_main ? true : false);
			main = curr_main;
			if (error) {
				return;
			}
		}

		uint tot_layers = gpl.size();
		if (tot_layers > 0) {
			/* Loop over VR strokes and check if they should be erased.
			 * Maybe there's a better way to do this? */
			bGPDstroke *gpn;
			for (int i = 0; i < tot_layers; ++i) {
				if (gpf[i]) {
					for (bGPDstroke *gps = (bGPDstroke*)gpf[i]->strokes.first; gps; gps = gpn) {
						gpn = gps->next;
						Widget_Annotate::erase_stroke(gps, gpf[i]);
					}
				}
			}
		}
	}
	else {
		eraser = false;

		points.clear();

		bGPDspoint pt;

		const Mat44f& cursor = c.position.get(VR_SPACE_BLENDER);
		memcpy(&pt, cursor.m[3], sizeof(float) * 3);
		VR *vr = vr_get_obj();
		pt.pressure = vr->controller[c.side]->trigger_pressure;
		pt.strength = 1.0f;
		//pt.flag = GP_SPOINT_SELECT;

		points.push_back(pt);
	}

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Annotate::obj.do_render[i] = true;
	}
}

void Widget_Annotate::drag_contd(VR_UI::Cursor& c)
{
	/* Eraser */
	if (eraser) {
		uint tot_layers = gpl.size();
		if (tot_layers > 0) {
			/* Loop over VR strokes and check if they should be erased.
			 * Maybe there's a better way to do this? */
			bGPDstroke *gpn;
			for (int i = 0; i < tot_layers; ++i) {
				if (gpf[i]) {
					for (bGPDstroke *gps = (bGPDstroke*)gpf[i]->strokes.first; gps; gps = gpn) {
						gpn = gps->next;
						Widget_Annotate::erase_stroke(gps, gpf[i]);
					}
				}
			}
		}
	}
	else {
		bGPDspoint pt;

		const Mat44f& cursor = c.position.get(VR_SPACE_BLENDER);
		memcpy(&pt, cursor.m[3], sizeof(float) * 3);
		VR *vr = vr_get_obj();
		pt.pressure = vr->controller[c.side]->trigger_pressure;
		pt.strength = 1.0f;
		//pt.flag = GP_SPOINT_SELECT;

		points.push_back(pt);
	}

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Annotate::obj.do_render[i] = true;
	}
}

void Widget_Annotate::drag_stop(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;

	/* Eraser */
	if (eraser) {
		return;
	}

	/* Finalize curve (save space data) */

	Main *curr_main = CTX_data_main(C);
	if (gpf.size() < 1 || main != curr_main) {
		int error = Widget_Annotate::init(main != curr_main ? true : false);
		main = curr_main;
		if (error) {
			return;
		}
	}

	/* TODO_XR: Find a way to "coexist" with any existing scene gpd. */
	//BKE_gpencil_layer_setactive(gpd, gpl);

	/* Add new stroke. */
	int tot_points = points.size();
	bGPDstroke *gps = BKE_gpencil_add_stroke(gpf[active_layer], 0, tot_points, line_thickness /*/25.0f*/);
	/* Could probably avoid the memcpy by allocating the stroke in drag_start()
	 * but it's nice to store the points in a vector. */
	memcpy(gps->points, &points[0], sizeof(bGPDspoint) * tot_points);

	memcpy(gpl[active_layer]->color, color, sizeof(float) * 4);
	BKE_gpencil_layer_setactive(gpd, gpl[active_layer]);

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Annotate::obj.do_render[i] = false;
	}
}

void Widget_Annotate::render(VR_Side side)
{
	int tot_points = points.size();

	/* Eraser */
	if (eraser) {
		const Mat44f& prior_model_matrix = VR_Draw::get_model_matrix();

		VR_Draw::update_modelview_matrix(&VR_UI::cursor_position_get(VR_SPACE_REAL, cursor_side), 0);
		VR_Draw::set_depth_test(false, false);
		VR_Draw::set_color(1.0f, 0.2f, 0.6f, 0.1f);
		VR_Draw::render_ball(eraser_radius);
		VR_Draw::set_depth_test(true, false);
		VR_Draw::set_color(1.0f, 0.2f, 0.6f, 0.4f);
		VR_Draw::render_ball(eraser_radius);
		VR_Draw::set_depth_test(true, true);

		VR_Draw::update_modelview_matrix(&prior_model_matrix, 0);

		Widget_Annotate::obj.do_render[side] = false;
		return;
	}

	/* Adapted from gp_draw_stroke_3d() in annotate_draw.c. */
	if (tot_points <= 1) { 
		/* If click, point will already be finalized and drawn. 
		 * If drag, need at least two points to draw a line. */
	}
	else { 
		/* if cyclic needs one vertex more */
		bool cyclic = false;
		if ((*(Coord3Df*)&points[0] == *(Coord3Df*)&points[tot_points - 1])) {
			cyclic = true;
		}
		int cyclic_add = 0;
		if (cyclic) {
			++cyclic_add;
		}

		float cyclic_fpt[3];
		int draw_points = 0;

		float cur_pressure = points[0].pressure;

		GPUVertFormat *format = immVertexFormat();
		uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformColor3fvAlpha(color, color[3]);

		/* TODO: implement this with a geometry shader to draw one continuous tapered stroke */

		/* draw stroke curve */
		GPU_line_width(max_ff(cur_pressure * line_thickness, 1.0f));
		immBeginAtMost(GPU_PRIM_LINE_STRIP, tot_points + cyclic_add);
		for (int i = 0; i < tot_points; ++i) {
			/* if there was a significant pressure change, stop the curve, change the thickness of the stroke,
			 * and continue drawing again (since line-width cannot change in middle of GL_LINE_STRIP)
			 * Note: we want more visible levels of pressures when thickness is bigger.
			 */
			if (fabsf(points[i].pressure - cur_pressure) > 0.2f / (float)line_thickness) {
				/* if the pressure changes before get at least 2 vertices, need to repeat last point to avoid assert in immEnd() */
				if (draw_points < 2) {
					immVertex3fv(pos, &points[i - 1].x);
				}
				immEnd();
				draw_points = 0;

				cur_pressure = points[i].pressure;
				GPU_line_width(max_ff(cur_pressure * line_thickness, 1.0f));
				immBeginAtMost(GPU_PRIM_LINE_STRIP, tot_points - i + 1 + cyclic_add);

				/* need to roll-back one point to ensure that there are no gaps in the stroke */
				if (i != 0) {
					immVertex3fv(pos, &points[i - 1].x);
					++draw_points;
				}
			}

			/* now the point we want */
			immVertex3fv(pos, &points[i].x);
			++draw_points;

			if (cyclic && i == 0) {
				/* save first point to use in cyclic */
				copy_v3_v3(cyclic_fpt, &points[i].x);
			}
		}

		if (cyclic) {
			/* draw line to first point to complete the cycle */
			immVertex3fv(pos, cyclic_fpt);
			++draw_points;
		}

		/* if less of two points, need to repeat last point to avoid assert in immEnd() */
		if (draw_points < 2) {
			immVertex3fv(pos, &points[tot_points - 1].x);
		}

		immEnd();
		immUnbindProgram();
	}

	Widget_Annotate::obj.do_render[side] = false;
}

/***********************************************************************************************//**
 * \class                               Widget_Measure
 ***************************************************************************************************
 * Interaction widget for the gpencil measure tool.
 *
 **************************************************************************************************/

Widget_Measure Widget_Measure::obj;

//bGPdata *Widget_Measure::gpd(0);
//bGPDlayer *Widget_Measure::gpl(0);
//bGPDframe *Widget_Measure::gpf(0);
//Main *Widget_Measure::main(0);

Coord3Df Widget_Measure::p0;
Coord3Df Widget_Measure::p1;

float Widget_Measure::line_thickness = 100.0f;
float Widget_Measure::color[4] = { 1.0f, 0.3f, 0.3f, 1.0f };

VR_Side Widget_Measure::cursor_side;

//int Widget_Measure::init() {
//	bContext *C = vr_get_obj()->ctx;
//	gpd = BKE_gpencil_data_addnew(CTX_data_main(C), "Measure");
//	if (!gpd) {
//		return -1;
//	}
//
//	gpd->flag |= (GP_DATA_ANNOTATIONS); //| GP_DATA_STROKE_EDITMODE);
//
//	gpl = BKE_gpencil_layer_addnew(gpd, "VR_Measure", true);
//	if (!gpl) {
//		BKE_gpencil_free(gpd, 0);
//		return -1;
//	}
//	memcpy(gpl->color, color, sizeof(float) * 4);
//	gpl->thickness = line_thickness * 1.6f;
//
//	gpf = BKE_gpencil_frame_addnew(gpl, 0);
//	if (!gpd) {
//		BKE_gpencil_free(gpd, 1);
//		return -1;
//	}
//
//	Scene *scene = CTX_data_scene(C);
//	scene->gpd = gpd;
//
//	return 0;
//}

void Widget_Measure::drag_start(VR_UI::Cursor& c) 
{	
	cursor_side = c.side;
	c.reference = c.position.get();
	memcpy(&p0, &(c.position.get(VR_SPACE_BLENDER).m[3]), sizeof(float) * 3);
}

void Widget_Measure::drag_contd(VR_UI::Cursor& c) 
{
	memcpy(&p1, &(c.position.get(VR_SPACE_BLENDER).m[3]), sizeof(float)*3);

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Measure::obj.do_render[i] = true;
	}
}

void Widget_Measure::drag_stop(VR_UI::Cursor& c) 
{
	bContext *C = vr_get_obj()->ctx;

	Main* curr_main = CTX_data_main(C);
	if (Widget_Annotate::gpl.size() < 1 || Widget_Annotate::main != curr_main) {
		int error = Widget_Annotate::init(Widget_Annotate::main != curr_main ? true : false);
		Widget_Annotate::main = curr_main;
		if (error)
			return;
	}

	bGPDspoint p0T, p1T;
	memcpy(&p0T, &p0, sizeof(float)*3);
	memcpy(&p1T, &p1, sizeof(float)*3);
	p0T.pressure = 1.0f;
	p0T.strength = 1.0f;
	p1T.pressure = 1.0f;
	p1T.strength = 1.0f;
	bGPDspoint pC[]{p0T, p1T};

	uint active_layer = Widget_Annotate::num_layers - 1;
	bGPDstroke *gps = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 2, line_thickness * 1.6f);
	memcpy(gps->points, &(pC)[0], sizeof(bGPDspoint) * 2);

	memcpy(Widget_Annotate::gpl[active_layer]->color, color, sizeof(float) * 4);
	BKE_gpencil_layer_setactive(Widget_Annotate::gpd, Widget_Annotate::gpl[active_layer]);

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Measure::obj.do_render[i] = false;
	}
}

void Widget_Measure::render(VR_Side side)
{
	/* Render measurement text. */
	const Mat44f& prior_model_matrix = VR_Draw::get_model_matrix();
	static Mat44f m;
	m = VR_UI::hmd_position_get(VR_SPACE_REAL);
	const Mat44f& c = VR_UI::cursor_position_get(VR_SPACE_REAL, cursor_side);
	memcpy(m.m[3], c.m[3], sizeof(float) * 3);
	VR_Draw::update_modelview_matrix(&m, 0);

	static std::string distance;
	sprintf((char*)distance.data(), "%.3f", (p1 - p0).length());
	VR_Draw::set_depth_test(false, false);
	VR_Draw::set_color(0.8f, 0.8f, 0.8f, 1.0f);
	VR_Draw::render_string(distance.c_str(), 0.02f, 0.02f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.08f, 0.001f);

	VR_Draw::set_depth_test(true, true);
	VR_Draw::update_modelview_matrix(&prior_model_matrix, 0);

	/* Instead of working with multiple points that make up a whole line, we work with just p0/p1. */
	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor3fvAlpha(color, color[3]);

	GPU_line_width(10.0f);
	immBeginAtMost(GPU_PRIM_LINE_STRIP, 2);

	immVertex3fv(pos, (float*)&p0);
	immVertex3fv(pos, (float*)&p1);

	if (p0 == p1) {
		/* cyclic */
		immVertex3fv(pos, (float*)&p0);
	}

	immEnd();
	immUnbindProgram();

	Widget_Measure::obj.do_render[side] = false;
}

/***********************************************************************************************//**
 * \class									Widget_QuickGrab
 ***************************************************************************************************
 * Interaction widget for the QuickGrab tool.
 *
 **************************************************************************************************/
Widget_QuickGrab Widget_QuickGrab::obj;

bool Widget_QuickGrab::bimanual(false);

/* From rna_scene.c */
static void object_simplify_update(Object *ob)
{
	ModifierData *md;
	ParticleSystem *psys;

	if ((ob->id.tag & LIB_TAG_DOIT) == 0) {
		return;
	}

	ob->id.tag &= ~LIB_TAG_DOIT;

	for (md = (ModifierData*)ob->modifiers.first; md; md = (md->next)) {
		if (ELEM(md->type, eModifierType_Subsurf, eModifierType_Multires, eModifierType_ParticleSystem)) {
			DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
		}
	}

	for (psys = (ParticleSystem*)ob->particlesystem.first; psys; psys = psys->next)
		psys->recalc |= PSYS_RECALC_CHILD;

	if (ob->dup_group) {
		CollectionObject *cob;

		for (cob = (CollectionObject*)ob->dup_group->gobject.first; cob; cob = cob->next)
			object_simplify_update(cob->ob);
	}
}

bool Widget_QuickGrab::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_QuickGrab::click(VR_UI::Cursor& c)
{
	const Mat44f& m = c.position.get();
	raycast_select_single(*(Coord3Df*)m.m[3], VR_UI::shift_key_get());
}

void Widget_QuickGrab::drag_start(VR_UI::Cursor& c)
{
	/* Remember where we started from in navigation space. */
	c.interaction_position.set(((Mat44f)(c.position.get(VR_SPACE_REAL))).m, VR_SPACE_REAL);
}

void Widget_QuickGrab::drag_contd(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	ListBase ctx_data_list;	
	CTX_data_selected_objects(C, &ctx_data_list);
	CollectionPointerLink *ctx_link = (CollectionPointerLink *)ctx_data_list.first;
    if (!ctx_link) {
		return;
	}

	static Mat44f curr;
	static Mat44f prev;

	/* Check if we're two-hand dragging */
	if (c.bimanual) {
		bimanual = true;
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

		/* Scaling: distance between pointers */
		float curr_s = sqrt(((curr_h.m[3][0] - curr_o.m[3][0])*(curr_h.m[3][0] - curr_o.m[3][0])) + ((curr_h.m[3][1]) - curr_o.m[3][1])*(curr_h.m[3][1] - curr_o.m[3][1])) + ((curr_h.m[3][2] - curr_o.m[3][2])*(curr_h.m[3][2] - curr_o.m[3][2]));
		float start_s = sqrt(((prev_h.m[3][0] - prev_o.m[3][0])*(prev_h.m[3][0] - prev_o.m[3][0])) + ((prev_h.m[3][1]) - prev_o.m[3][1])*(prev_h.m[3][1] - prev_o.m[3][1])) + ((prev_h.m[3][2] - prev_o.m[3][2])*(prev_h.m[3][2] - prev_o.m[3][2]));

		prev.m[0][0] *= start_s; prev.m[1][0] *= start_s; prev.m[2][0] *= start_s;
		prev.m[0][1] *= start_s; prev.m[1][1] *= start_s; prev.m[2][1] *= start_s;
		prev.m[0][2] *= start_s; prev.m[1][2] *= start_s; prev.m[2][2] *= start_s;

		curr.m[0][0] *= curr_s; curr.m[1][0] *= curr_s; curr.m[2][0] *= curr_s;
		curr.m[0][1] *= curr_s; curr.m[1][1] *= curr_s; curr.m[2][1] *= curr_s;
		curr.m[0][2] *= curr_s; curr.m[1][2] *= curr_s; curr.m[2][2] *= curr_s;

		c.interaction_position.set(curr_h.m, VR_SPACE_BLENDER);
		c.other_hand->interaction_position.set(curr_o.m, VR_SPACE_BLENDER);
	}
	else { /* one-handed drag */
		bimanual = false;
		curr = c.position.get(VR_SPACE_BLENDER);
		prev = c.interaction_position.get(VR_SPACE_BLENDER);
		c.interaction_position.set(curr.m, VR_SPACE_BLENDER);
	}

	/* TODO_XR: Constraints and snapping */
	//if (VR_UI::navigation_lock_rotation) {
	//	float prev_scale = Coord3Df(prev.m[0][0], prev.m[0][1], prev.m[0][2]).length();
	//	float curr_scale = Coord3Df(curr.m[0][0], curr.m[0][1], curr.m[0][2]).length();
	//	float rot_ident[3][4] = { {prev_scale,0,0,0} , {0,prev_scale,0,0} , {0,0,prev_scale,0} };
	//	std::memcpy(prev.m, rot_ident, sizeof(float) * 3 * 4);
	//	rot_ident[0][0] = rot_ident[1][1] = rot_ident[2][2] = curr_scale;
	//	std::memcpy(curr.m, rot_ident, sizeof(float) * 3 * 4);
	//}
	//else if (VR_UI::navigation_lock_up) {
	//	Coord3Df z; /* (m.m[2][0], m.m[2][1], m.m[2][2]); // z-axis */
	//	if (!VR_UI::is_zaxis_up()) {
	//		z = Coord3Df(0, 1, 0); /* rectify z to point "up" */
	//	}
	//	else { /* z is up : */
	//		z = Coord3Df(0, 0, 1); /* rectify z to point up */
	//	}
	//	VR_Math::orient_matrix_z(curr, z);
	//	VR_Math::orient_matrix_z(prev, z);
	//}
	//if (VR_UI::navigation_lock_translation) {
	//	prev = VR_UI::convert_space(prev, VR_SPACE_BLENDER, VR_SPACE_REAL);
	//	curr = VR_UI::convert_space(curr, VR_SPACE_BLENDER, VR_SPACE_REAL); /* locked in real-world coodinates */
	//	Coord3Df& t_prev = *(Coord3Df*)prev.m[3];
	//	Coord3Df& t_curr = *(Coord3Df*)curr.m[3];
	//	t_curr = t_prev;
	//	prev = VR_UI::convert_space(prev, VR_SPACE_REAL, VR_SPACE_BLENDER);
	//	curr = VR_UI::convert_space(curr, VR_SPACE_REAL, VR_SPACE_BLENDER); /* revert to Blender coordinates */
	//}
	//else if (VR_UI::navigation_lock_altitude) {
	//	prev = VR_UI::convert_space(prev, VR_SPACE_BLENDER, VR_SPACE_REAL);
	//	curr = VR_UI::convert_space(curr, VR_SPACE_BLENDER, VR_SPACE_REAL); /* locked in real-world coordinates */
	//	Coord3Df& t_prev = *(Coord3Df*)prev.m[3];
	//	Coord3Df& t_curr = *(Coord3Df*)curr.m[3];
	//	t_curr.z = t_prev.z;
	//	prev = VR_UI::convert_space(prev, VR_SPACE_REAL, VR_SPACE_BLENDER);
	//	curr = VR_UI::convert_space(curr, VR_SPACE_REAL, VR_SPACE_BLENDER); /* revert to Blender coordinates */
	//}
	//if (VR_UI::navigation_lock_scale) {
	//	((Coord3Df*)prev.m[0])->normalize_in_place();
	//	((Coord3Df*)prev.m[1])->normalize_in_place();
	//	((Coord3Df*)prev.m[2])->normalize_in_place();
	//	((Coord3Df*)curr.m[0])->normalize_in_place();
	//	((Coord3Df*)curr.m[1])->normalize_in_place();
	//	((Coord3Df*)curr.m[2])->normalize_in_place();
	//}

	const Mat44f& delta = prev.inverse() * curr;

	for (; ctx_link; ctx_link = ctx_link->next) {
		Object *obact = (Object*)ctx_link->ptr.data;
		if (!obact) {
			continue;
		}

		const Mat44f& t = *(Mat44f*)obact->obmat * delta;
		//const Coord3Df *x_axis = (Coord3Df*)t.m;
		//float length = x_axis->length();
		//if (length < VR_UI_MINNAVIGATIONSCALE || length > VR_UI_MAXNAVIGATIONSCALE) {
		//	return; /* To avoid hitting the "singularity" or clipping the object out of visibility */
		//}
		//t.m[0][3] = 0;
		//t.m[1][3] = 0;
		//t.m[2][3] = 0;
		//t.m[3][3] = 1;
		memcpy(obact->obmat, t.m, sizeof(float) * 4 * 4);

		DEG_id_tag_update((ID*)obact->data, 0);  /* sets recalc flags */
	}
}

void Widget_QuickGrab::drag_stop(VR_UI::Cursor& c)
{
	bool update_transforms = true;
	if (bimanual) {
		if (c.bimanual == VR_UI::Cursor::BIMANUAL_OFF) {
			update_transforms = false; /* Calculations are only performed by the first hand. */
		}
	}

	/* Check if we're two-hand navi dragging */
	if (c.bimanual) {
		VR_UI::Cursor *other = c.other_hand;
		c.bimanual = VR_UI::Cursor::BIMANUAL_OFF;
		/* the other hand is still dragging - we're leaving a two-hand drag. */
		other->bimanual = VR_UI::Cursor::BIMANUAL_OFF;
		/* ALSO: the other hand should start one-hand manipulating from here: */
		c.other_hand->interaction_position.set(((Mat44f)VR_UI::cursor_position_get(VR_SPACE_REAL, other->side)).m, VR_SPACE_REAL);
	}

	if (update_transforms) {
		bContext *C = vr_get_obj()->ctx;
		Scene *scene = CTX_data_scene(C);

		ListBase ctx_data_list;
		CTX_data_selected_objects(C, &ctx_data_list);
		CollectionPointerLink *ctx_link = (CollectionPointerLink *)ctx_data_list.first;
		if (!ctx_link) {
			return;
		}

		for (; ctx_link; ctx_link = ctx_link->next) {
			Object *obact = (Object*)ctx_link->ptr.data;
			if (!obact) {
				continue;
			}

			/* Translation */
			Mat44f& t = *(Mat44f*)obact->obmat;
			memcpy(obact->loc, t.m[3], sizeof(float) * 3);
			/* Rotation */
			mat4_to_eul(obact->rot, t.m);
			/* Scale */
			obact->size[0] = (*(Coord3Df*)(t.m[0])).length();
			obact->size[1] = (*(Coord3Df*)(t.m[1])).length();
			obact->size[2] = (*(Coord3Df*)(t.m[2])).length();
		}

		DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT /*| ND_TRANSFORM_DONE*/, scene);
		//WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

		ED_undo_push(C, "Transform");
	}
}

/***********************************************************************************************//**
 * \class                               Widget_Delete
 ***************************************************************************************************
 * Interaction widget for performing a 'delete' operation.
 *
 **************************************************************************************************/
Widget_Delete Widget_Delete::obj;

/* From object_delete_exec() in object_add.c */
static int delete_selected_objects(bool use_global = true)
{
	bContext *C = vr_get_obj()->ctx;
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win;
	bool changed = false;

	if (CTX_data_edit_object(C)) {
		return -1;
	}

	ListBase ctx_data_list;
	CollectionPointerLink *ctx_link;
	CTX_data_selected_objects(C, &ctx_data_list);
	for (ctx_link = (CollectionPointerLink*)ctx_data_list.first; ctx_link; ctx_link = ctx_link->next) {
		Object *ob = (Object*)ctx_link->ptr.data;
		if (ob->type == OB_CAMERA && ob == scene->camera) {
			/* TODO_XR: Deleting the default scene camera causes the VR viewmats / viewplanes to be incorrect.
			 * Need to fix this. */
			continue;
		}

		const bool is_indirectly_used = BKE_library_ID_is_indirectly_used(bmain, ob);
		if (ob->id.tag & LIB_TAG_INDIRECT) {
			/* Can this case ever happen? */
			continue;
		}
		else if (is_indirectly_used && ID_REAL_USERS(ob) <= 1 && ID_EXTRA_USERS(ob) == 0) {
			continue;
		}

		/* if grease pencil object, set cache as dirty */
		if (ob->type == OB_GPENCIL) {
			bGPdata *gpd = (bGPdata *)ob->data;
			DEG_id_tag_update(&gpd->id, OB_RECALC_OB | OB_RECALC_DATA);
		}

		/* This is sort of a quick hack to address T51243 - Proper thing to do here would be to nuke most of all this
		 * custom scene/object/base handling, and use generic lib remap/query for that.
		 * But this is for later (aka 2.8, once layers & co are settled and working).
		 */
		if (use_global && ob->id.lib == NULL) {
			/* We want to nuke the object, let's nuke it the easy way (not for linked data though)... */
			BKE_libblock_delete(bmain, &ob->id);
			changed = true;
			continue;
		}

		/* remove from Grease Pencil parent */
		/* XXX This is likely not correct? Will also remove parent from grease pencil from other scenes,
		 *     even when use_global is false... */
		for (bGPdata *gpd = (bGPdata*)bmain->gpencil.first; gpd; gpd = (bGPdata*)gpd->id.next) {
			for (bGPDlayer *gpl = (bGPDlayer*)gpd->layers.first; gpl; gpl = gpl->next) {
				if (gpl->parent != NULL) {
					if (gpl->parent == ob) {
						gpl->parent = NULL;
					}
				}
			}
		}

		/* remove from current scene only */
		ED_object_base_free_and_unlink(bmain, scene, ob);
		changed = true;

		if (use_global) {
			Scene *scene_iter;
			for (scene_iter = (Scene*)bmain->scene.first; scene_iter; scene_iter = (Scene*)scene_iter->id.next) {
				if (scene_iter != scene && !ID_IS_LINKED(scene_iter)) {
					if (is_indirectly_used && ID_REAL_USERS(ob) <= 1 && ID_EXTRA_USERS(ob) == 0) {
						break;
					}
					ED_object_base_free_and_unlink(bmain, scene_iter, ob);
				}
			}
		}
		/* end global */
	}
	BLI_freelistN(&ctx_data_list);

	if (!changed) {
		return -1;
	}

	/* delete has to handle all open scenes */
	BKE_main_id_tag_listbase(&bmain->scene, LIB_TAG_DOIT, true);
	for (win = (wmWindow*)wm->windows.first; win; win = win->next) {
		scene = WM_window_get_active_scene(win);

		if (scene->id.tag & LIB_TAG_DOIT) {
			scene->id.tag &= ~LIB_TAG_DOIT;

			DEG_relations_tag_update(bmain);

			DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
			WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
			WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);
		}
	}
	ED_undo_push(C, "Delete");

	return 0;
}

bool Widget_Delete::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Delete::click(VR_UI::Cursor& c)
{
	delete_selected_objects();
}

bool Widget_Delete::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_Delete::render_icon(const Mat44f& t, VR_Side controller_side,  bool active, bool touched)
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
	VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::delete_tex);
}

/***********************************************************************************************//**
 * \class                               Widget_Duplicate
 ***************************************************************************************************
* Interaction widget for performing a 'duplicate' operation.
*
**************************************************************************************************/
Widget_Duplicate Widget_Duplicate::obj;

/* From object_add.c */
static void copy_object_set_idnew(bContext *C)
{
	Main *bmain = CTX_data_main(C);

	ListBase ctx_data_list;
	CollectionPointerLink *ctx_link;
	CTX_data_selected_editable_objects(C, &ctx_data_list);
	for (ctx_link = (CollectionPointerLink*)ctx_data_list.first; ctx_link; ctx_link = ctx_link->next) {
		Object *ob = (Object*)ctx_link->ptr.data;
		BKE_libblock_relink_to_newid(&ob->id);
	}
	BLI_freelistN(&ctx_data_list);

	BKE_main_id_clear_newpoins(bmain);
}

/* From object_add.c */
/* used below, assumes id.new is correct */
/* leaves selection of base/object unaltered */
/* Does set ID->newid pointers. */
static Base *object_add_duplicate_internal(Main *bmain, Scene *scene, ViewLayer *view_layer, Object *ob, int dupflag)
{
#define ID_NEW_REMAP_US(a, type) if (      (a)->id.newid) { (a) = (type *)(a)->id.newid;       (a)->id.us++; }
#define ID_NEW_REMAP_US2(a)	if (((ID *)a)->newid)    { (a) = ((ID  *)a)->newid;     ((ID *)a)->us++;    }

	Base *base, *basen = NULL;
	Material ***matarar;
	Object *obn;
	ID *id;
	int a, didit;

	if (ob->mode & OB_MODE_POSE) {
		; /* nothing? */
	}
	else {
		obn = (Object*)ID_NEW_SET(ob, BKE_object_copy(bmain, ob));
		DEG_id_tag_update(&obn->id, OB_RECALC_OB | OB_RECALC_DATA);

		base = BKE_view_layer_base_find(view_layer, ob);
		if ((base != NULL) && (base->flag & BASE_VISIBLE)) {
			BKE_collection_object_add_from(bmain, scene, ob, obn);
		}
		else {
			LayerCollection *layer_collection = BKE_layer_collection_get_active(view_layer);
			BKE_collection_object_add(bmain, layer_collection->collection, obn);
		}
		basen = BKE_view_layer_base_find(view_layer, obn);

		/* 1) duplis should end up in same collection as the original
		 * 2) Rigid Body sim participants MUST always be part of a collection...
		 */
		 // XXX: is 2) really a good measure here?
		if (ob->rigidbody_object || ob->rigidbody_constraint) {
			Collection *collection;
			for (collection = (Collection*)bmain->collection.first; collection; collection = (Collection*)collection->id.next) {
				if (BKE_collection_has_object(collection, ob))
					BKE_collection_object_add(bmain, collection, obn);
			}
		}

		/* duplicates using userflags */
		if (dupflag & USER_DUP_ACT) {
			BKE_animdata_copy_id_action(bmain, &obn->id, true);
		}

		if (dupflag & USER_DUP_MAT) {
			for (a = 0; a < obn->totcol; a++) {
				id = (ID *)obn->mat[a];
				if (id) {
					ID_NEW_REMAP_US(obn->mat[a], Material)
				else {
					obn->mat[a] = (Material*)ID_NEW_SET(obn->mat[a], BKE_material_copy(bmain, obn->mat[a]));
					/* duplicate grease pencil settings */
					if (ob->mat[a]->gp_style) {
						obn->mat[a]->gp_style = (MaterialGPencilStyle*)MEM_dupallocN(ob->mat[a]->gp_style);
					}
				}
				id_us_min(id);

				if (dupflag & USER_DUP_ACT) {
					BKE_animdata_copy_id_action(bmain, &obn->mat[a]->id, true);
				}
				}
			}
		}
		if (dupflag & USER_DUP_PSYS) {
			ParticleSystem *psys;
			for (psys = (ParticleSystem*)obn->particlesystem.first; psys; psys = psys->next) {
				id = (ID *)psys->part;
				if (id) {
					ID_NEW_REMAP_US(psys->part, ParticleSettings)
				else {
					psys->part = (ParticleSettings*)ID_NEW_SET(psys->part, BKE_particlesettings_copy(bmain, psys->part));
				}

				if (dupflag & USER_DUP_ACT) {
					BKE_animdata_copy_id_action(bmain, &psys->part->id, true);
				}

				id_us_min(id);
				}
			}
		}

		id = (ID*)obn->data;
		didit = 0;

		switch (obn->type) {
		case OB_MESH:
			if (dupflag & USER_DUP_MESH) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_mesh_copy(bmain, (const Mesh*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_CURVE:
			if (dupflag & USER_DUP_CURVE) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_curve_copy(bmain, (const Curve*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_SURF:
			if (dupflag & USER_DUP_SURF) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_curve_copy(bmain, (const Curve*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_FONT:
			if (dupflag & USER_DUP_FONT) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_curve_copy(bmain, (const Curve*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_MBALL:
			if (dupflag & USER_DUP_MBALL) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_mball_copy(bmain, (const MetaBall*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_LAMP:
			if (dupflag & USER_DUP_LAMP) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_lamp_copy(bmain, (const Lamp*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_ARMATURE:
			DEG_id_tag_update(&obn->id, OB_RECALC_DATA);
			if (obn->pose)
				BKE_pose_tag_recalc(bmain, obn->pose);
			if (dupflag & USER_DUP_ARM) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_armature_copy(bmain, (const bArmature*)obn->data));
				BKE_pose_rebuild(bmain, obn, (bArmature*)obn->data, true);
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_LATTICE:
			if (dupflag != 0) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_lattice_copy(bmain, (const Lattice*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_CAMERA:
			if (dupflag != 0) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_camera_copy(bmain, (const Camera*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_SPEAKER:
			if (dupflag != 0) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_speaker_copy(bmain, (const Speaker*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_GPENCIL:
			if (dupflag != 0) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_gpencil_copy(bmain, (const bGPdata*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		}

		/* check if obdata is copied */
		if (didit) {
			Key *key = BKE_key_from_object(obn);

			Key *oldkey = BKE_key_from_object(ob);
			if (oldkey != NULL) {
				ID_NEW_SET(oldkey, key);
			}

			if (dupflag & USER_DUP_ACT) {
				BKE_animdata_copy_id_action(bmain, (ID *)obn->data, true);
				if (key) {
					BKE_animdata_copy_id_action(bmain, (ID *)key, true);
				}
			}

			if (dupflag & USER_DUP_MAT) {
				matarar = give_matarar(obn);
				if (matarar) {
					for (a = 0; a < obn->totcol; a++) {
						id = (ID *)(*matarar)[a];
						if (id) {
							ID_NEW_REMAP_US((*matarar)[a], Material)
						else {
							(*matarar)[a] = (Material*)ID_NEW_SET((*matarar)[a], BKE_material_copy(bmain, (*matarar)[a]));
						}
						id_us_min(id);
						}
					}
				}
			}
		}
	}
	return basen;

#undef ID_NEW_REMAP_US
#undef ID_NEW_REMAP_US2
}

/* From duplicate_exec() in object_add.c */
static int duplicate_selected_objects(bool linked = true)
{
	bContext *C = vr_get_obj()->ctx;
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	int dupflag = (linked) ? 0 : U.dupflag;

	ListBase ctx_data_list;
	CollectionPointerLink *ctx_link;
	CTX_data_selected_bases(C, &ctx_data_list);
	for (ctx_link = (CollectionPointerLink*)ctx_data_list.first; ctx_link; ctx_link = ctx_link->next) {
		Base *base = (Base*)ctx_link->ptr.data;

		Base *basen = object_add_duplicate_internal(bmain, scene, view_layer, base->object, dupflag);

		/* note that this is safe to do with this context iterator,
		 * the list is made in advance */
		ED_object_base_select(base, BA_DESELECT);
		ED_object_base_select(basen, BA_SELECT);

		if (basen == NULL) {
			continue;
		}

		/* new object becomes active */
		if (BASACT(view_layer) == base)
			ED_object_base_activate(C, basen);

		if (basen->object->data) {
			DEG_id_tag_update((ID*)basen->object->data, 0);
		}
	}
	BLI_freelistN(&ctx_data_list);

	copy_object_set_idnew(C);

	BKE_main_id_clear_newpoins(bmain);

	DEG_relations_tag_update(bmain);
	DEG_id_tag_update(&scene->id, DEG_TAG_COPY_ON_WRITE | DEG_TAG_SELECT_UPDATE);

	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	ED_undo_push(C, "Duplicate");

	return 0;
}

bool Widget_Duplicate::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Duplicate::click(VR_UI::Cursor& c)
{
	duplicate_selected_objects();
}

bool Widget_Duplicate::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_Duplicate::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
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
	VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::duplicate_tex);
}

/***********************************************************************************************//**
 * \class                               Widget_Undo
 ***************************************************************************************************
 * Interaction widget for performing an 'undo' operation.
 *
 **************************************************************************************************/
Widget_Undo Widget_Undo::obj;

bool Widget_Undo::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Undo::click(VR_UI::Cursor& c)
{
	++VR_UI::undo_count;
}

bool Widget_Undo::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_Undo::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
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
	VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::undo_tex);
}

/***********************************************************************************************//**
 * \class                               Widget_Redo
 ***************************************************************************************************
 * Interaction widget for performing a 'redo' operation.
 *
 **************************************************************************************************/
Widget_Redo Widget_Redo::obj;

bool Widget_Redo::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Redo::click(VR_UI::Cursor& c)
{
	++VR_UI::redo_count;
}

bool Widget_Redo::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_Redo::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
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
	VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::redo_tex);
}

/***********************************************************************************************//**
 * \class                               Widget_SwitchTool
 ***************************************************************************************************
 * Interaction widget for switching the currently active tool.
 *
 **************************************************************************************************/
Widget_SwitchTool Widget_SwitchTool::obj;

std::string Widget_SwitchTool::curr_tool_str[VR_SIDES] = {"SELECT","QUICKGRAB"};

bool Widget_SwitchTool::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_SwitchTool::click(VR_UI::Cursor& c)
{
	VR_Side side = c.side;
	const VR_Widget *curr_tool = VR_Widget_Layout::get_current_tool(side);
	if (!curr_tool) {
		return;
	}

	VR_Widget *new_tool;
	switch (((VR_Widget*)(curr_tool))->type()) {
		case TYPE_SELECT: {
			new_tool = &Widget_QuickGrab::obj;
			break;
		}
		case TYPE_QUICKGRAB: {
			new_tool = &Widget_Select::obj;
			break;
		}
		case TYPE_ANNOTATE: {
			new_tool = &Widget_Measure::obj;
			break;
		}
		case TYPE_MEASURE: {
			new_tool = &Widget_Annotate::obj;
			break;
		}
		default: {
			return;
		}
	}

	VR_Widget_Layout::set_current_tool(new_tool, side);
	curr_tool_str[side] = new_tool->name();
}

bool Widget_SwitchTool::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_SwitchTool::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
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

	/* Update tool mode string on alt state changes. */
	static VR_UI::AltState alt[VR_SIDES] = { VR_UI::ALTSTATE_OFF };
	VR_UI::AltState curr_alt = VR_UI::alt_key_get();
	if (alt[controller_side] != curr_alt) {
		alt[controller_side] = curr_alt;
		const VR_Widget *curr_tool = VR_Widget_Layout::get_current_tool(controller_side);
		if (!curr_tool) {
			return;
		}
		curr_tool_str[controller_side] = ((VR_Widget*)curr_tool)->name();
	}

	std::string& str = curr_tool_str[controller_side];
	if (str == "SELECT") {
		VR_Draw::render_rect(-0.018f, 0.018f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::select_str_tex);
	}
	else if (str == "QUICKGRAB") {
		VR_Draw::render_rect(-0.018f, 0.018f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::quickgrab_str_tex);
	}
	else if (str == "ANNOTATE") {
		VR_Draw::render_rect(-0.018f, 0.018f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::annotate_str_tex);
	}
	else if (str == "MEASURE") {
		VR_Draw::render_rect(-0.018f, 0.018f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::measure_str_tex);
	}

	//VR_Draw::render_string(curr_tool_str[controller_side].c_str(), 0.02f, 0.02f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.08f, 0.001f);
}

/***********************************************************************************************//**
 * \class                               Widget_SwitchToolMode
 ***************************************************************************************************
 * Interaction widget for switching the tool mode for the currently active tool.
 *
 **************************************************************************************************/
Widget_SwitchToolMode Widget_SwitchToolMode::obj;

std::string Widget_SwitchToolMode::curr_tool_mode_str[VR_SIDES] = {"RAYCAST",""};

void Widget_SwitchToolMode::get_current_tool_mode(const VR_Widget *tool, VR_Side controller_side)
{
	if (!tool) {
		return;
	}

	switch (((VR_Widget*)tool)->type()) {
		case TYPE_SELECT: {
			/* Toggle raycast/proximity selection. */
			VR_UI::SelectionMode& mode = VR_UI::selection_mode;
			if (mode == VR_UI::SELECTIONMODE_RAYCAST) {
				curr_tool_mode_str[controller_side] = "RAYCAST";
			}
			else {
				curr_tool_mode_str[controller_side] = "PROXIMITY";
			}
			const VR_Widget *tool_other = VR_Widget_Layout::get_current_tool((VR_Side)(1-controller_side));
			if (tool_other && ((VR_Widget*)tool_other)->type() == TYPE_SELECT) {
				curr_tool_mode_str[1-controller_side] = curr_tool_mode_str[controller_side];
			}
			return;
		}
		case TYPE_ANNOTATE: {
			curr_tool_mode_str[controller_side] = "COLOR";
			return;
		}
		default: {
			/* TODO_XR */
			curr_tool_mode_str[controller_side] = "";
			return;
		}
	}
}

void Widget_SwitchToolMode::set_current_tool_mode(const VR_Widget *tool, VR_Side controller_side)
{
	if (!tool) {
		return;
	}

	switch (((VR_Widget*)tool)->type()) {
	case TYPE_SELECT: {
		/* Toggle raycast/proximity selection. */
		VR_UI::SelectionMode& mode = VR_UI::selection_mode;
		if (mode == VR_UI::SELECTIONMODE_RAYCAST) {
			mode = VR_UI::SELECTIONMODE_PROXIMITY;
			curr_tool_mode_str[controller_side] = "PROXIMITY";
		}
		else {
			mode = VR_UI::SELECTIONMODE_RAYCAST;
			curr_tool_mode_str[controller_side] = "RAYCAST";
		}
		const VR_Widget *tool_other = VR_Widget_Layout::get_current_tool((VR_Side)(1 - controller_side));
		if (tool_other && ((VR_Widget*)tool_other)->type() == TYPE_SELECT) {
			curr_tool_mode_str[1 - controller_side] = curr_tool_mode_str[controller_side];
		}
		VR_UI::stick_menu_active[controller_side] = false;
		return;
	}
	case TYPE_ANNOTATE: {
		curr_tool_mode_str[controller_side] = "COLOR";
		VR_UI::stick_menu_active[controller_side] = true;
		return;
	}
	default: {
		/* TODO_XR */
		curr_tool_mode_str[controller_side] = "";
		VR_UI::stick_menu_active[controller_side] = false;
		return;
	}
	}
}

bool Widget_SwitchToolMode::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_SwitchToolMode::click(VR_UI::Cursor& c)
{
	VR_Side side = c.side;
	const VR_Widget *curr_tool = VR_Widget_Layout::get_current_tool(side);
	set_current_tool_mode(curr_tool, side);
}

bool Widget_SwitchToolMode::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_SwitchToolMode::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
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

	static VR_UI::AltState alt[VR_SIDES] = { VR_UI::ALTSTATE_OFF };
	static const VR_Widget *tool[VR_SIDES] = { 0 };

	/* Update tool mode string on alt state changes. */
	VR_UI::AltState curr_alt = VR_UI::alt_key_get();
	if (alt[controller_side] != curr_alt) {
		alt[controller_side] = curr_alt;
		tool[controller_side] = VR_Widget_Layout::get_current_tool(controller_side);
		get_current_tool_mode(tool[controller_side], controller_side);
	}
	else {
		const VR_Widget *curr_tool = VR_Widget_Layout::get_current_tool(controller_side);
		/* Update tool mode string on tool changes. */
		if (tool[controller_side] != curr_tool) {
			tool[controller_side] = curr_tool;
			get_current_tool_mode(tool[controller_side], controller_side);
		}
	}

	std::string& str = curr_tool_mode_str[controller_side];
	if (str == "RAYCAST") {
		VR_Draw::render_rect(-0.018f, 0.018f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::raycast_str_tex);
	}
	else if (str == "PROXIMITY") {
		VR_Draw::render_rect(-0.018f, 0.018f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::proximity_str_tex);
	}
	else if (str == "COLOR") {
		VR_Draw::set_color(Widget_Annotate::color);
		VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::colorwheel_menu_triangle_tex);
	}

	//VR_Draw::render_string(curr_tool_mode_str[controller_side].c_str(), 0.02f, 0.02f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.08f, 0.001f);
}

/***********************************************************************************************//**
 * \class                               Widget_Menu_ColorWheel
 ***************************************************************************************************
 * Interaction widget for the color wheel menu.
 *
 **************************************************************************************************/
Widget_Menu_ColorWheel Widget_Menu_ColorWheel::obj;

float Widget_Menu_ColorWheel::color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
//static Coord2Df stick;
//static float angle;

void Widget_Menu_ColorWheel::drag_start(VR_UI::Cursor& c)
{
	/* Select color from final stick position. */
	//VR_Controller *controller = vr_get_obj()->controller[c.side];
	//if (!controller) {
	//	return;
	//}
	//stick.x = controller->stick[0];
	//stick.y = controller->stick[1];
	//Coord2Df y(0, 1);
	//angle = stick.angle(y);
	//if (stick.x < 0) {
	//	angle = -angle;
	//}
}

void Widget_Menu_ColorWheel::drag_contd(VR_UI::Cursor& c)
{
	//VR_Controller *controller = vr_get_obj()->controller[c.side];
	//if (!controller) {
	//	return;
	//}
	//stick.x = controller->stick[0];
	//stick.y = controller->stick[1];
	//Coord2Df y(0, 1);
	//angle = stick.angle(y);
	//if (stick.x < 0) {
	//	angle = -angle;
	//}
}

void Widget_Menu_ColorWheel::drag_stop(VR_UI::Cursor& c)
{
	/* Select color from final stick position. */
	VR_Controller *controller = vr_get_obj()->controller[c.side];
	if (!controller) {
		return;
	}
	Coord2Df stick;
	if (vr_get_obj()->ui_type == VR_UI_TYPE_VIVE) {
		stick.x = controller->dpad[0];
		stick.y = controller->dpad[1];
	}
	else {
		stick.x = controller->stick[0];
		stick.y = controller->stick[1];
	}
	Coord2Df y(0, 1);
	float angle = stick.angle(y);
	if (stick.x > 0) {
		angle += PI / 12.0f;
	}
	else {
		angle = -angle + PI / 12.0f;
	}
	angle *= 6.0f;

	if (angle >= 0 && angle < PI) {
		color[0] = 0.95f; color[1] = 0.95f; color[2] = 0.95f;
		Widget_Annotate::active_layer = 0;
	}
	else if (angle >= PI && angle < 2 * PI) {
		color[0] = 0.05f; color[1] = 0.05f; color[2] = 0.05f;
		Widget_Annotate::active_layer = 1;
	}
	else if (angle >= 2 * PI && angle < 3 * PI) {
		color[0] = 0.6f; color[1] = 0.2f; color[2] = 1.0f;
		Widget_Annotate::active_layer = 2;
	}
	else if (angle >= 3 * PI && angle < 4 * PI) {
		color[0] = 0.2f; color[1] = 0.6f; color[2] = 1.0f;
		Widget_Annotate::active_layer = 3;
	}
	else if (angle >= 4 * PI && angle < 5 * PI) {
		color[0] = 0.2f; color[1] = 1.0f; color[2] = 1.0f;
		Widget_Annotate::active_layer = 4;
	}
	else if (angle >= 5 * PI && angle < 6 * PI) {
		//Widget_Annotate::active_layer = 5;
	}
	else if (angle <= -5 * PI && angle > -6 * PI) {
		//Widget_Annotate::active_layer = 6;
	}
	else if (angle <= -4 * PI && angle > -5 * PI) {
		//Widget_Annotate::active_layer = 7;
	}
	else if (angle <= -3 * PI && angle > -4 * PI) {
		color[0] = 0.6f; color[1] = 1.0f; color[2] = 0.2f;
		Widget_Annotate::active_layer = 8;
	}
	else if (angle <= -2 * PI && angle > -3 * PI) {
		color[0] = 1.0f; color[1] = 1.0f; color[2] = 0.2f;
		Widget_Annotate::active_layer = 9;
	}
	else if (angle <= -PI && angle > -2 * PI) {
		color[0] = 1.0f; color[1] = 0.6f; color[2] = 0.2f;
		Widget_Annotate::active_layer = 10;
	}
	else {
		color[0] = 1.0f; color[1] = 0.2f; color[2] = 0.2f;
		Widget_Annotate::active_layer = 11;
	}

	/* TODO_XR: Alpha */
	//if (VR_UI::shift_key_get()) {
		//color[3] = 
	//}
	
	memcpy(Widget_Annotate::color, color, sizeof(float) * 4);
}

void Widget_Menu_ColorWheel::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
{
	VR_Draw::update_modelview_matrix(&t, 0);
	VR_Draw::set_color(1.0f, 1.0f, 1.0f, 1.0f);

	VR_Draw::render_rect(-0.09f, 0.09f, 0.09f, -0.09f, -0.015f, 1.0f, 1.0f, VR_Draw::colorwheel_menu_tex);

	//if (touched) {
		/* Render arrow to represent stick direction. */
		//static Mat44f m;
		//static float p[3];
		//memcpy(p, t.m[1], sizeof(float) * 3);
		//axis_angle_to_mat4(m.m, p, -angle);
		//static Mat44f t_arrow;
		//t_arrow = m * t;
		//VR_Draw::update_modelview_matrix(&t_arrow, 0);
	//}

	//VR_Draw::render_rect(-0.09f, 0.09f, 0.09f, -0.09f, -0.01f, 1.0f, 1.0f, VR_Draw::colorwheel_menu_triangle_tex);
}