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
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget.h"
#include "vr_widget_layout.h"

#include "vr_math.h"
#include "vr_draw.h"

#include "BLI_math.h"
#include "BLI_listbase.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_editmesh.h"
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
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_speaker.h"

#include "DNA_gpencil_types.h"
#include "DNA_mesh_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_gpencil.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_select_utils.h"
#include "ED_undo.h"
#include "ED_view3d.h"

#include "WM_gizmo_types.h"
#include "gizmo_library_intern.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "gpencil_intern.h"

#include "MEM_guardedalloc.h"
#include "mesh_intern.h"

#include "WM_api.h"
#include "WM_types.h"

/* Static transformation matrix for rendering touched widgets. */
float m_wt[4][4] = { 1.5f, 0.0f, 0.0f, 0.0f,
					 0.0f, 1.5f, 0.0f, 0.0f,
					 0.0f, 0.0f, 1.5f, 0.0f,
					 0.0f, 0.0f, 0.003f, 1.0f };
static const Mat44f m_widget_touched = m_wt;

VR_Widget* VR_Widget::get_widget(Type type, const char* ident)
{
	switch (type) {
	case TYPE_NAVI:
		return &Widget_Navi::obj;
	case TYPE_NAVI_GRABAIR:
		return &Widget_Navi::GrabAir::obj;
	case TYPE_NAVI_JOYSTICK:
		return &Widget_Navi::Joystick::obj;
	case TYPE_NAVI_TELEPORT:
		return &Widget_Navi::Teleport::obj;
	case TYPE_CTRL:
		return &Widget_Ctrl::obj;
	case TYPE_SHIFT:
		return &Widget_Shift::obj;
	case TYPE_ALT:
		return &Widget_Alt::obj;
	case TYPE_SELECT:
		return &Widget_Select::obj;
	case TYPE_SELECT_RAYCAST:
		return &Widget_Select::Raycast::obj;
	case TYPE_SELECT_PROXIMITY:
		return &Widget_Select::Proximity::obj;
	case TYPE_CURSOR:
		return &Widget_Cursor::obj;
	case TYPE_TRANSFORM:
		return &Widget_Transform::obj;
	case TYPE_ANNOTATE:
		return &Widget_Annotate::obj;
	case TYPE_MEASURE:
		return &Widget_Measure::obj;
	case TYPE_EXTRUDE:
		return &Widget_Extrude::obj;
	case TYPE_CURSOROFFSET:
		return &Widget_CursorOffset::obj;
	case TYPE_DELETE:
		return &Widget_Delete::obj;
	case TYPE_DUPLICATE:
		return &Widget_Duplicate::obj;
	case TYPE_UNDO:
		return &Widget_Undo::obj;
	case TYPE_REDO:
		return &Widget_Redo::obj;
	case TYPE_SWITCHLAYOUT:
		return &Widget_SwitchLayout::obj;
	case TYPE_SWITCHCOMPONENT:
		return &Widget_SwitchComponent::obj;
	case TYPE_SWITCHSPACE:
		return &Widget_SwitchSpace::obj;
	case TYPE_SWITCHTOOL:
		return &Widget_SwitchTool::obj;
	case TYPE_MENU:
		return &Widget_Menu::obj;
	case TYPE_MENU_LEFT:
		return &Widget_Menu::Left::obj;
	case TYPE_MENU_RIGHT:
		return &Widget_Menu::Right::obj;
	default:
		return 0; /* not found or invalid type */
	}
}

VR_Widget::Type VR_Widget::get_widget_type(const std::string& str)
{
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
	if (str == "CTRL") {
		return TYPE_CTRL;
	}
	if (str == "SHIFT") {
		return TYPE_SHIFT;
	}
	if (str == "ALT") {
		return TYPE_ALT;
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
	if (str == "CURSOR") {
		return TYPE_CURSOR;
	}
	if (str == "TRANSFORM") {
		return TYPE_TRANSFORM;
	}
	if (str == "ANNOTATE") {
		return TYPE_ANNOTATE;
	}
	if (str == "MEASURE") {
		return TYPE_MEASURE;
	}
	if (str == "EXTRUDE") {
		return TYPE_EXTRUDE;
	}
	if (str == "CURSOROFFSET") {
		return TYPE_CURSOROFFSET;
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
	if (str == "SWITCHLAYOUT") {
		return TYPE_SWITCHLAYOUT;
	}
	if (str == "SWITCHCOMPONENT") {
		return TYPE_SWITCHCOMPONENT;
	}
	if (str == "SWITCHSPACE") {
		return TYPE_SWITCHSPACE;
	}
	if (str == "SWITCHTOOL") {
		return TYPE_SWITCHTOOL;
	}
	if (str == "MENU") {
		return TYPE_MENU;
	}
	if (str == "MENU_LEFT") {
		return TYPE_MENU_LEFT;
	}
	if (str == "MENU_RIGHT") {
		return TYPE_MENU_RIGHT;
	}
	return TYPE_INVALID;
}

VR_Widget* VR_Widget::get_widget(const std::string& str)
{
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
	if (str == "CTRL") {
		return &Widget_Ctrl::obj;
	}
	if (str == "SHIFT") {
		return &Widget_Shift::obj;
	}
	if (str == "ALT") {
		return &Widget_Alt::obj;
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
	if (str == "CURSOR") {
		return &Widget_Cursor::obj;
	}
	if (str == "TRANSFORM") {
		return &Widget_Transform::obj;
	}
	if (str == "ANNOTATE") {
		return &Widget_Annotate::obj;
	}
	if (str == "MEASURE") {
		return &Widget_Measure::obj;
	}
	if (str == "EXTRUDE") {
		return &Widget_Extrude::obj;
	}
	if (str == "CURSOROFFSET") {
		return &Widget_CursorOffset::obj;
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
	if (str == "REDO") {
		return &Widget_Redo::obj;
	}
	if (str == "SWITCHLAYOUT") {
		return &Widget_SwitchLayout::obj;
	}
	if (str == "SWITCHCOMPONENT") {
		return &Widget_SwitchComponent::obj;
	}
	if (str == "SWITCHSPACE") {
		return &Widget_SwitchSpace::obj;
	}
	if (str == "SWITCHTOOL") {
		return &Widget_SwitchTool::obj;
	}
	if (str == "MENU") {
		return &Widget_Menu::obj;
	}
	if (str == "MENU_LEFT") {
		return &Widget_Menu::Left::obj;
	}
	if (str == "MENU_RIGHT") {
		return &Widget_Menu::Right::obj;
	}

	return 0;
}

std::vector<std::string> VR_Widget::list_widgets()
{
	std::vector<std::string> ret;
	ret.push_back("NAVI");
	ret.push_back("NAVI_GRABAIR");
	ret.push_back("NAVI_JOYSTICK");
	ret.push_back("NAVI_TELEPORT");
	ret.push_back("CTRL");
	ret.push_back("SHIFT");
	ret.push_back("ALT");
	ret.push_back("SELECT");
	ret.push_back("SELECT_RAYCAST");
	ret.push_back("SELECT_PROXIMITY");
	ret.push_back("CURSOR");
	ret.push_back("TRANSFORM");
	ret.push_back("ANNOTATE");
	ret.push_back("MEASURE");
	ret.push_back("EXTRUDE");
	ret.push_back("CURSOROFFSET");
	ret.push_back("DELETE");
	ret.push_back("DUPLICATE");
	ret.push_back("UNDO");
	ret.push_back("REDO");
	ret.push_back("SWITCHLAYOUT");
	ret.push_back("SWITCHCOMPONENT");
	ret.push_back("SWITCHSPACE");
	ret.push_back("SWITCHTOOL");
	ret.push_back("MENU");
	ret.push_back("MENU_LEFT");
	ret.push_back("MENU_RIGHT");

	return ret;
}

std::string VR_Widget::type_to_string(Type type)
{
	switch (type) {
	case TYPE_NAVI:
		return "NAVI";
	case TYPE_NAVI_GRABAIR:
		return "NAVI_GRABAIR";
	case TYPE_NAVI_JOYSTICK:
		return "NAVI_JOYSTICK";
	case TYPE_NAVI_TELEPORT:
		return "NAVI_TELEPORT";
	case TYPE_CTRL:
		return "CTRL";
	case TYPE_SHIFT:
		return "SHIFT";
	case TYPE_ALT:
		return "ALT";
	case TYPE_SELECT:
		return "SELECT";
	case TYPE_SELECT_RAYCAST:
		return "SELECT_RAYCAST";
	case TYPE_SELECT_PROXIMITY:
		return "SELECT_PROXIMITY";
	case TYPE_CURSOR:
		return "CURSOR";
	case TYPE_TRANSFORM:
		return "TRANSFORM";
	case TYPE_ANNOTATE:
		return "ANNOTATE";
	case TYPE_MEASURE:
		return "MEASURE";
	case TYPE_EXTRUDE:
		return "EXTRUDE";
	case TYPE_CURSOROFFSET:
		return "CURSOROFFSET";
	case TYPE_DELETE:
		return "DELETE";
	case TYPE_DUPLICATE:
		return "DUPLICATE";
	case TYPE_UNDO:
		return "UNDO";
	case TYPE_REDO:
		return "REDO";
	case TYPE_SWITCHLAYOUT:
		return "SWITCHLAYOUT";
	case TYPE_SWITCHCOMPONENT:
		return "SWITCHCOMPONENT";
	case TYPE_SWITCHSPACE:
		return "SWITCHSPACE";
	case TYPE_SWITCHTOOL:
		return "SWITCHTOOL";
	case TYPE_MENU:
		return "MENU";
	case TYPE_MENU_LEFT:
		return "MENU_LEFT";
	case TYPE_MENU_RIGHT:
		return "MENU_RIGHT";
	default:
		return "INVALID";
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
 * \class                                  Widget_Navi
 ***************************************************************************************************
 * Interaction widget for grabbing-the-air navigation.
 * Will select the appropriate sub-widget based on the setting UserInterface::navigation_mode.
 **************************************************************************************************/
Widget_Navi Widget_Navi::obj;

VR_UI::NavLock Widget_Navi::nav_lock[3] = { VR_UI::NAVLOCK_NONE };

void Widget_Navi::drag_start(VR_UI::Cursor& c)
{
	switch (VR_UI::navigation_mode) {
	case VR_UI::NAVMODE_GRABAIR:
		return GrabAir::obj.drag_start(c);
	case VR_UI::NAVMODE_JOYSTICK:
		return Joystick::obj.drag_start(c);
	case VR_UI::NAVMODE_TELEPORT:
		return Teleport::obj.drag_start(c);
	case VR_UI::NAVMODE_NONE:
		return;
	}
}

void Widget_Navi::drag_contd(VR_UI::Cursor& c)
{
	switch (VR_UI::navigation_mode) {
	case VR_UI::NAVMODE_GRABAIR:
		return GrabAir::obj.drag_contd(c);
	case VR_UI::NAVMODE_JOYSTICK:
		return Joystick::obj.drag_contd(c);
	case VR_UI::NAVMODE_TELEPORT:
		return Teleport::obj.drag_contd(c);
	case VR_UI::NAVMODE_NONE:
		return;
	}
}

void Widget_Navi::drag_stop(VR_UI::Cursor& c)
{
	switch (VR_UI::navigation_mode) {
	case VR_UI::NAVMODE_GRABAIR:
		return GrabAir::obj.drag_stop(c);
	case VR_UI::NAVMODE_JOYSTICK:
		return Joystick::obj.drag_stop(c);
	case VR_UI::NAVMODE_TELEPORT:
		return Teleport::obj.drag_stop(c);
	case VR_UI::NAVMODE_NONE:
		return;
	}
}

void Widget_Navi::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
{
	switch (VR_UI::navigation_mode) {
	case VR_UI::NAVMODE_GRABAIR:
		return GrabAir::obj.render_icon(t, controller_side, active, touched);
	case VR_UI::NAVMODE_JOYSTICK:
		return Joystick::obj.render_icon(t, controller_side, active, touched);
	case VR_UI::NAVMODE_TELEPORT:
		return Teleport::obj.render_icon(t, controller_side, active, touched);
	case VR_UI::NAVMODE_NONE:
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

	if (VR_UI::ctrl_key_get() || Widget_Navi::nav_lock[1]) {
		/* Lock rotation */
		switch (Widget_Navi::nav_lock[1]) {
		case VR_UI::NAVLOCK_ROT_UP: {
			static Coord3Df up;
			if (!VR_UI::is_zaxis_up()) {
				up = Coord3Df(0.0f, 1.0f, 0.0f);
			}
			else { /* z is up : */
				up = Coord3Df(0.0f, 0.0f, 1.0f);
			}
			VR_Math::orient_matrix_z(curr, up);
			VR_Math::orient_matrix_z(prev, up);
			break;
		}
		case VR_UI::NAVLOCK_ROT:
		default: {
			float prev_scale = Coord3Df(prev.m[0][0], prev.m[0][1], prev.m[0][2]).length();
			float curr_scale = Coord3Df(curr.m[0][0], curr.m[0][1], curr.m[0][2]).length();
			float rot_ident[3][4] = { {prev_scale,0,0,0} , {0,prev_scale,0,0} , {0,0,prev_scale,0} };
			memcpy(prev.m, rot_ident, sizeof(float) * 3 * 4);
			rot_ident[0][0] = rot_ident[1][1] = rot_ident[2][2] = curr_scale;
			memcpy(curr.m, rot_ident, sizeof(float) * 3 * 4);
			break;
		}
		}
	}
	if (Widget_Navi::nav_lock[0]) {
		/* Lock translation */
		switch (Widget_Navi::nav_lock[0]) {
		case VR_UI::NAVLOCK_TRANS_UP: {
			prev = VR_UI::convert_space(prev, VR_SPACE_BLENDER, VR_SPACE_REAL);
			curr = VR_UI::convert_space(curr, VR_SPACE_BLENDER, VR_SPACE_REAL); /* locked in real-world coordinates */
			curr.m[3][2] = prev.m[3][2];
			prev = VR_UI::convert_space(prev, VR_SPACE_REAL, VR_SPACE_BLENDER);
			curr = VR_UI::convert_space(curr, VR_SPACE_REAL, VR_SPACE_BLENDER); /* revert to Blender coordinates */
			break;
		}
		case VR_UI::NAVLOCK_TRANS:
		default: {
			prev = VR_UI::convert_space(prev, VR_SPACE_BLENDER, VR_SPACE_REAL);
			curr = VR_UI::convert_space(curr, VR_SPACE_BLENDER, VR_SPACE_REAL); /* locked in real-world coodinates */
			Coord3Df& t_prev = *(Coord3Df*)prev.m[3];
			Coord3Df& t_curr = *(Coord3Df*)curr.m[3];
			t_curr = t_prev;
			prev = VR_UI::convert_space(prev, VR_SPACE_REAL, VR_SPACE_BLENDER);
			curr = VR_UI::convert_space(curr, VR_SPACE_REAL, VR_SPACE_BLENDER); /* revert to Blender coordinates */
			break;
		}
		}
	}
	if (VR_UI::shift_key_get() || Widget_Navi::nav_lock[2]) {
		/* Lock scale */
		switch (Widget_Navi::nav_lock[2]) {
		case VR_UI::NAVLOCK_SCALE_REAL: {
			/* TODO_XR */
			static Mat44f temp = VR_Math::identity_f;
			//static Mat44f temp;
			//temp = VR_UI::navigation_matrix_get();
			////static Coord3Df up;
			////if (!VR_UI::is_zaxis_up()) {
			////	up = Coord3Df(0.0f, 1.0f, 0.0f);
			////}
			////else { /* z is up : */
			////	up = Coord3Df(0.0f, 0.0f, 1.0f);
			////}
			////VR_Math::orient_matrix_z(temp, up);
			//float scale = VR_UI::navigation_scale_get();
			//for (int i = 0; i < 3 /*4*/; ++i) {
			//	*(Coord3Df*)temp.m[i] /= scale;
			//}
			VR_UI::navigation_set(temp/*temp.inverse()*/);
			c.position.set(temp.m, VR_SPACE_BLENDER);
			c.interaction_position.set(temp.m, VR_SPACE_BLENDER);
			prev = curr = temp;
			Widget_Navi::nav_lock[2] = VR_UI::NAVLOCK_SCALE;
			return;
		}
		case VR_UI::NAVLOCK_SCALE:
		default: {
			if (c.bimanual) {
				for (int i = 0; i < 3; ++i) {
					((Coord3Df*)prev.m[i])->normalize_in_place();
					((Coord3Df*)curr.m[i])->normalize_in_place();
				}
			}
			break;
		}
		}
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
	VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::nav_grabair_tex);
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
		if (VR_UI::ctrl_key_get()) {
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
		if (a < 0.36f * PI) {
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
		else if (a > 0.64f * PI) {
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
	if (VR_UI::ctrl_key_get()) {
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
	/* Remember where we started from in navigation space */
	c.interaction_position = c.position;
	c.reference = c.position.get(VR_SPACE_REAL);
	arrow = VR_Math::identity_f;
	memcpy(arrow.m[3], c.reference.m[3], sizeof(float) * 4);

	cancel = false;
}

void Widget_Navi::Teleport::drag_contd(VR_UI::Cursor& c)
{
	if (VR_UI::ctrl_key_get()) {
		cancel = true;
	}

	if (!cancel) {
		const Mat44f& curr = c.position.get(VR_SPACE_REAL);

		static Mat44f delta = VR_Math::identity_f;
		delta.m[3][0] = curr.m[3][0] - c.reference.m[3][0];
		delta.m[3][0] = delta.m[3][0] * abs(delta.m[3][0]);
		delta.m[3][1] = curr.m[3][1] - c.reference.m[3][1];
		delta.m[3][1] = delta.m[3][1] * abs(delta.m[3][1]);
		if (!VR_UI::shift_key_get()) {
			delta.m[3][2] = curr.m[3][2] - c.reference.m[3][2];
			delta.m[3][2] = delta.m[3][2] * abs(delta.m[3][2]);
		}
		else {
			delta.m[3][2] = 0;
		}

		arrow = delta * arrow;

		for (int i = 0; i < VR_SIDES; ++i) {
			Widget_Navi::Teleport::obj.do_render[i] = true;
		}
	}
}

void Widget_Navi::Teleport::drag_stop(VR_UI::Cursor& c)
{
	if (VR_UI::ctrl_key_get()) {
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
	VR_Draw::set_depth_test(true, false);
	VR_Draw::set_color(0.0f, 0.7f, 1.0f, 0.4f);
	VR_Draw::render_ball(0.05f, true);
	VR_Draw::set_depth_test(true, true);

	VR_Draw::update_modelview_matrix(&prior_model_matrix, 0);

	Widget_Navi::Teleport::obj.do_render[side] = false;
}

/***********************************************************************************************//**
 * \class                               Widget_Ctrl
 ***************************************************************************************************
 * Interaction widget for emulating the ctrl key on a keyboard.
 *
 **************************************************************************************************/
Widget_Ctrl Widget_Ctrl::obj;

void Widget_Ctrl::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
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
	VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::shift_tex);
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

/* Modified from view3d_project.c */
#define WIDGET_SELECT_RAYCAST_NEAR_CLIP 0.0001f
#define WIDGET_SELECT_RAYCAST_ZERO_CLIP 0.0001f

/* From view3d_select.c */
static void object_deselect_all_visible(ViewLayer *view_layer, View3D *v3d)
{
	for (Base *base = (Base*)view_layer->object_bases.first; base; base = base->next) {
		if (BASE_SELECTABLE(v3d, base)) {
			ED_object_base_select(base, BA_DESELECT);
		}
	}
}

/* From view3d_select.c */
static void deselectall_except(ViewLayer *view_layer, Base *b)
{
	for (Base *base = (Base*)view_layer->object_bases.first; base; base = base->next) {
		if (base->flag & BASE_SELECTED) {
			if (b != base) {
				ED_object_base_select(base, BA_DESELECT);
			}
		}
	}
}

/* Adapted from	edbm_backbuf_check_and_select_verts() in view3d_select.c */
static void deselectall_edit(BMesh *bm, int mode)
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

/* Select a single object with raycast selection.
 * Adapted from ed_object_select_pick() in view3d_select.c. */
static void raycast_select_single(
	const Coord3Df& p,
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
		object_deselect_all_visible(view_layer, v3d);

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
static void raycast_select_single_vertex(const Coord3Df& p, ViewContext *vc, bool extend, bool deselect)
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
				dist_temp += 10.0f;
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

static void raycast_select_single_edge(const Coord3Df& p, ViewContext *vc, bool extend, bool deselect)
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
				dist_temp += 10.0f;
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

static void raycast_select_single_face(const Coord3Df& p, ViewContext *vc, bool extend, bool deselect)
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
				dist_temp += 10.0f;
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

static void raycast_select_single_edit(
	const Coord3Df& p,
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
		deselectall_edit(vc->em->bm, 0);

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
			if (view3d_project(
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
		deselectall_edit(vc->em->bm, 1);

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
			if (view3d_project(
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
		deselectall_edit(vc->em->bm, 2);

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
			if (view3d_project(
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
		raycast_select_single_edit(*(Coord3Df*)m.m[3], VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
	}
	else {
		raycast_select_single(*(Coord3Df*)m.m[3], VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
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
		object_deselect_all_visible(view_layer, v3d);

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
							deselectall_except(view_layer, basact);
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
		deselectall_edit(vc->em->bm, 0);

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
		deselectall_edit(vc->em->bm, 1);

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
		deselectall_edit(vc->em->bm, 2);

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
			deselectall_edit(em->bm, 0);
		}
		else if (ts->selectmode & SCE_SELECT_EDGE) {
			deselectall_edit(em->bm, 1);
		}
		else if (ts->selectmode & SCE_SELECT_FACE) {
			deselectall_edit(em->bm, 2);
		}

		EDBM_selectmode_flush(em);

		DEG_id_tag_update((ID*)obedit->data, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	}
	else {
		object_deselect_all_visible(view_layer, v3d);

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

/**************************************************************************************************\
 * \class                               Widget_Cursor											   *
 ***************************************************************************************************
 * Interaction widget for the Blender cursor.													   *
 **************************************************************************************************/

Widget_Cursor Widget_Cursor::obj;

bool Widget_Cursor::cursor_enabled;
Coord3Df Widget_Cursor::cursor_current_location;

void Widget_Cursor::cursor_reset() 
{ 
	cursor_enabled = false; 
}

void Widget_Cursor::cursor_teleport()
{
	/* Convert cursor from blender to real space. */
	Coord3Df current_cursor_pos_temp = VR_UI::convert_space(cursor_current_location,
		VR_SPACE_BLENDER, VR_SPACE_REAL);

	/* Save current HMD position && Create new identity matrix for the cursor location. */
	Mat44f current_hmd_pos = VR_UI::hmd_position_get(VR_SPACE_REAL);
	Mat44f current_cursor_pos = VR_Math::identity_f;
	Mat44f ref = VR_Math::identity_f;
	memcpy(ref.m[3], current_hmd_pos.m[3], sizeof(float) * 4);
	/* Fill cursor identity matrix with current_cursor_position's location. */
	memcpy(&current_cursor_pos.m[3], &current_cursor_pos_temp, sizeof(float) * 3);
	VR_UI::navigation_apply_transformation(current_cursor_pos.inverse() * ref, VR_SPACE_REAL);
}

bool Widget_Cursor::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Cursor::click(VR_UI::Cursor& c)
{
	/* Cursor is seen as disabled when it's still stuck to the controller. */
	if (VR_UI::ctrl_key_get() && cursor_enabled) {
		/* Disable the world cursor, set back to controller. */
		cursor_enabled = false;
		return;
	}
	else if (!cursor_enabled)
		cursor_enabled = true;

	cursor_current_location = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];
}

bool Widget_Cursor::has_drag(VR_UI::Cursor& c) const 
{
	return false;  
}

/***********************************************************************************************//**
 * \class									Widget_Transform
 ***************************************************************************************************
 * Interaction widget for the Transform tool.
 *
 **************************************************************************************************/
Widget_Transform Widget_Transform::obj;

Widget_Transform::TransformMode Widget_Transform::transform_mode(Widget_Transform::TRANSFORMMODE_OMNI);
bool Widget_Transform::omni(true);
VR_UI::ConstraintMode Widget_Transform::constraint_mode(VR_UI::CONSTRAINTMODE_NONE);
int Widget_Transform::constraint_flag[3] = { 0 };
VR_UI::SnapMode Widget_Transform::snap_mode(VR_UI::SNAPMODE_TRANSLATION);
int Widget_Transform::snap_flag[3] = { 1, 1, 1 };
std::vector<Mat44f*> Widget_Transform::nonsnap_t;
bool Widget_Transform::snapped(false);

/* Multiplier for one and two-handed scaling transformations. */
#define WIDGET_TRANSFORM_SCALING_SENSITIVITY 0.5f

/* Precision multipliers. */
#define WIDGET_TRANSFORM_TRANS_PRECISION 0.1f
#define WIDGET_TRANSFORM_ROT_PRECISION (PI/36.0f)
#define WIDGET_TRANSFORM_SCALE_PRECISION 0.005f

//bool Widget_Transform::edit(false);
VR_UI::TransformSpace Widget_Transform::transform_space(VR_UI::TRANSFORMSPACE_GLOBAL);
bool Widget_Transform::is_dragging(false);

bool Widget_Transform::manipulator(false);
Mat44f Widget_Transform::manip_t = VR_Math::identity_f;
Mat44f Widget_Transform::manip_t_orig;
Mat44f Widget_Transform::manip_t_snap;
Coord3Df Widget_Transform::manip_angle[VR_UI::TRANSFORMSPACES];
float Widget_Transform::manip_scale_factor(2.0f);

Mat44f Widget_Transform::obmat_inv;

/* Manipulator colors. */
static const float c_manip[4][4] = { 1.0f, 0.2f, 0.322f, 0.4f,
									 0.545f, 0.863f, 0.0f, 0.4f,
								     0.157f, 0.565f, 1.0f, 0.4f,
									 1.0f, 1.0f, 1.0f, 0.4f };
static const float c_manip_select[4][4] = { 1.0f, 0.2f, 0.322f, 1.0f,
											0.545f, 0.863f, 0.0f, 1.0f,
											0.157f, 0.565f, 1.0f, 1.0f,
											1.0f, 1.0f, 1.0f, 1.0f };
/* Scale factors for manipulator rendering. */
#define WIDGET_TRANSFORM_ARROW_SCALE_FACTOR	0.1f
#define WIDGET_TRANSFORM_BOX_SCALE_FACTOR 0.05f
#define WIDGET_TRANSFORM_BALL_SCALE_FACTOR 0.08f
#define WIDGET_TRANSFORM_DIAL_RESOLUTION 100

/* Select a manipulator component with raycast selection. */
void Widget_Transform::raycast_select_manipulator(const Coord3Df& p, bool *extrude)
{
	bContext *C = vr_get_obj()->ctx;
	ARegion *ar = CTX_wm_region(C);
	/* TODO_XR: Use rv3d->persmat of dominant eye. */
	RegionView3D *rv3d = (RegionView3D*)ar->regiondata;
	float dist = ED_view3d_select_dist_px() * 1.3333f;
	int mval[2];
	float screen_co[2];

	VR_Side side = VR_UI::eye_dominance_get();
	VR_UI::get_pixel_coordinates(p, mval[0], mval[1], side);
	const float mval_fl[2] = { (float)mval[0], (float)mval[1] };

	static Coord3Df axis[3];
	static float axis_length[3];

	static Coord3Df pos;
	float length;

	bool hit = false;

	for (int i = 0; i < 3; ++i) {
		axis[i] = (*(Coord3Df*)manip_t.m[i]).normalize();
		axis_length[i] = (*(Coord3Df*)manip_t.m[i]).length();
	}
	const Coord3Df& manip_pos = *(Coord3Df*)manip_t.m[3];

	/* Do hit / selection test for shared manipulator. */
	for (int i = 0; i < 16; ++i) {
		switch (i) {
		case 0: { /* x extrude ball */
			if (!extrude) {
				i += 2;
				continue;
			}
			length = axis_length[0] * manip_scale_factor * 1.6f;
			pos = manip_pos + axis[0] * length;
			break;
		}
		case 1: { /* y extrude ball */
			length = axis_length[1] * manip_scale_factor * 1.6f;
			pos = manip_pos + axis[1] * length;
			break;
		}
		case 2: { /* z extrude ball */
			length = axis_length[2] * manip_scale_factor * 1.6f;
			pos = manip_pos + axis[2] * length;
			break;
		}
		case 3: { /* x-axis arrow */
			if (transform_mode != TRANSFORMMODE_MOVE && !omni) {
				i += 2;
				continue;
			}
			length = axis_length[0] * manip_scale_factor;
			pos = manip_pos + axis[0] * length;
			break;
		}
		case 4: { /* y-axis arrow */
			length = axis_length[1] * manip_scale_factor;
			pos = manip_pos + axis[1] * length;
			break;
		}
		case 5: { /* z-axis arrow */
			length = axis_length[2] * manip_scale_factor;
			pos = manip_pos + axis[2] * length;
			break;
		}
		case 6: { /* x-axis box */
			if (transform_mode != TRANSFORMMODE_SCALE && !omni) {
				i += 2;
				continue;
			}
			length = axis_length[0] * manip_scale_factor / 2.0f;
			pos = manip_pos + axis[0] * length;
			break;
		}
		case 7: { /* y-axis box */
			length = axis_length[1] * manip_scale_factor / 2.0f;
			pos = manip_pos + axis[1] * length;
			break;
		}
		case 8: { /* z-axis box */
			length = axis_length[2] * manip_scale_factor / 2.0f;
			pos = manip_pos + axis[2] * length;
			break;
		}
		case 9: { /* x-rotation ball */
			if (transform_mode != TRANSFORMMODE_ROTATE && !omni) {
				i += 2;
				continue;
			}
			rotate_v3_v3v3fl((float*)&pos, (float*)&axis[1], (float*)&axis[0], PI / 4.0f);
			length = axis_length[1] * manip_scale_factor / 2.0f;
			pos = manip_pos + pos * length;
			break;
		}
		case 10: { /* y-rotation ball */
			rotate_v3_v3v3fl((float*)&pos, (float*)&axis[2], (float*)&axis[1], PI / 4.0f);
			length = axis_length[2] * manip_scale_factor / 2.0f;
			pos = manip_pos + pos * length;
			break;
		}
		case 11: { /* z-rotation ball */
			rotate_v3_v3v3fl((float*)&pos, (float*)&axis[0], (float*)&axis[2], PI / 4.0f);
			length = axis_length[0] * manip_scale_factor / 2.0f;
			pos = manip_pos + pos * length;
			break;
		}
		case 12: { /* xy plane */
			if (omni || (transform_mode != TRANSFORMMODE_MOVE && transform_mode != TRANSFORMMODE_SCALE)) {
				i += 2;
				continue;
			};
			pos = manip_pos + (axis[0] * axis_length[0] + axis[1] * axis_length[1]) * manip_scale_factor / 2.0f;
			break;
		}
		case 13: { /* yz plane */
			pos = manip_pos + (axis[1] * axis_length[1] + axis[2] * axis_length[2]) * manip_scale_factor / 2.0f;
			break;
		}
		case 14: { /* zx plane */
			pos = manip_pos + (axis[0] * axis_length[0] + axis[2] * axis_length[2]) * manip_scale_factor / 2.0f;
			break;
		}
		case 15: { /* center box */
			if (!omni) {
				continue;
			}
			pos = manip_pos;
			break;
		}
		}

		if (view3d_project(
			ar, rv3d->persmat, false, (float*)&pos, screen_co,
			(eV3DProjTest)(V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR)) == V3D_PROJ_RET_OK)
		{
			float dist_temp = len_manhattan_v2v2(mval_fl, screen_co);
			dist_temp += 150.0f; //50.0f;
			if (dist_temp < dist) {
				hit = true;
				switch (i) {
				case 0: {
					constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_X;
					*extrude = true;
					return;
				}
				case 1: {
					constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_Y;
					*extrude = true;
					return;
				}
				case 2: {
					constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_Z;
					*extrude = true;
					return;
				}
				case 3: {
					constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_X;
					return;
				}
				case 4: {
					constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_Y;
					return;
				}
				case 5: {
					constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_Z;
					return;
				}
				case 6: {
					constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_X;
					return;
				}
				case 7: {
					constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_Y;
					return;
				}
				case 8: {
					constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_Z;
					return;
				}
				case 9: {
					constraint_mode = VR_UI::CONSTRAINTMODE_ROT_X;
					return;
				}
				case 10: {
					constraint_mode = VR_UI::CONSTRAINTMODE_ROT_Y;
					return;
				}
				case 11: {
					constraint_mode = VR_UI::CONSTRAINTMODE_ROT_Z;
					return;
				}
				case 12: {
					if (transform_mode == TRANSFORMMODE_SCALE) {
						constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_XY;
					}
					else {
						constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_XY;
					}
					return;
				}
				case 13: {
					if (transform_mode == TRANSFORMMODE_SCALE) {
						constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_YZ;
					}
					else {
						constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_YZ;
					}
					return;
				}
				case 14: {
					if (transform_mode == TRANSFORMMODE_SCALE) {
						constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_ZX;
					}
					else {
						constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_ZX;
					}
					return;
				}
				case 15: {
					transform_mode = TRANSFORMMODE_SCALE;
					snap_mode = VR_UI::SNAPMODE_SCALE;
					constraint_mode = VR_UI::CONSTRAINTMODE_NONE;
					return;
				}
				}
			}
		}
	}

	if (!hit) {
		constraint_mode = VR_UI::CONSTRAINTMODE_NONE;
	}
}

void Widget_Transform::update_manipulator()
{
	bContext *C = vr_get_obj()->ctx;
	ListBase ctx_data_list;
	CTX_data_selected_objects(C, &ctx_data_list);
	CollectionPointerLink *ctx_link = (CollectionPointerLink *)ctx_data_list.first;
	Object *obact = NULL;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		if (!ctx_link) {
			memset(manip_t.m, 0, sizeof(float) * 4 * 4);
			return;
		}
		obact = (Object*)ctx_link->ptr.data;
	}

	static float rot[3][3];
	static float z_axis[3] = { 0.0f, 0.0f, 1.0f };
	if (obedit && obedit->type == OB_MESH) {
		/* Edit mode */
		Scene *scene = CTX_data_scene(C);
		ToolSettings *ts = scene->toolsettings;
		BMesh *bm = ((Mesh*)obedit->data)->edit_btmesh->bm;
		if (bm) {
			BMIter iter;
			int count;

			const Mat44f& offset = *(Mat44f*)obedit->obmat;
			static Mat44f offset_no;
			offset_no = offset;
			memset(offset_no.m[3], 0, sizeof(float) * 3);
			static Coord3Df pos, no, temp;
			memset(&no, 0, sizeof(float) * 3);
			memset(&pos, 0, sizeof(float) * 3);

			switch (transform_space) {
			case VR_UI::TRANSFORMSPACE_NORMAL: {
				if (ts->selectmode & SCE_SELECT_VERTEX) {
					BMVert *v;
					count = 0;
					BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
							no += *(Coord3Df*)v->no;

							pos += *(Coord3Df*)v->co;
							++count;
						}
					}

					no /= (float)count;
					VR_Math::multiply_mat44_coord3D(temp, offset_no, no);
					temp.normalize_in_place();
					rotation_between_vecs_to_mat3(rot, z_axis, (float*)&temp);
					for (int i = 0; i < 3; ++i) {
						memcpy(manip_t.m[i], rot[i], sizeof(float) * 3);
					}

					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				else if (ts->selectmode & SCE_SELECT_EDGE) {
					BMEdge *e;
					count = 0;
					BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH) {
						if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
							no += *(Coord3Df*)e->v1->no + *(Coord3Df*)e->v2->no;

							pos += *(Coord3Df*)e->v1->co + *(Coord3Df*)e->v2->co;
							count += 2;
						}
					}
					no /= (float)count;
					VR_Math::multiply_mat44_coord3D(temp, offset_no, no);
					temp.normalize_in_place();
					rotation_between_vecs_to_mat3(rot, z_axis, (float*)&temp);
					for (int i = 0; i < 3; ++i) {
						memcpy(manip_t.m[i], rot[i], sizeof(float) * 3);
					}

					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				else if (ts->selectmode & SCE_SELECT_FACE) {
					BMFace *f;
					BMLoop *l;
					count = 0;
					BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH) {
						if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
							l = f->l_first;
							for (int i = 0; i < f->len; ++i, l = l->next) {
								no += *(Coord3Df*)l->v->no;

								pos += *(Coord3Df*)l->v->co;
								++count;
							}
						}
					}
					no /= (float)count;
					VR_Math::multiply_mat44_coord3D(temp, offset_no, no);
					temp.normalize_in_place();
					rotation_between_vecs_to_mat3(rot, z_axis, (float*)&temp);
					for (int i = 0; i < 3; ++i) {
						memcpy(manip_t.m[i], rot[i], sizeof(float) * 3);
					}

					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				break;
			}
			case VR_UI::TRANSFORMSPACE_LOCAL: {
				const Mat44f& obmat = *(Mat44f*)obedit->obmat;
				for (int i = 0; i < 3; ++i) {
					memcpy(manip_t.m[i], obmat.m[i], sizeof(float) * 3);
				}
				if (ts->selectmode & SCE_SELECT_VERTEX) {
					BMVert *v;
					count = 0;
					BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
							pos += *(Coord3Df*)v->co;
							++count;
						}
					}
					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				else if (ts->selectmode & SCE_SELECT_EDGE) {
					BMEdge *e;
					count = 0;
					BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH) {
						if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
							pos += *(Coord3Df*)e->v1->co + *(Coord3Df*)e->v2->co;
							count += 2;
						}
					}
					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				else if (ts->selectmode & SCE_SELECT_FACE) {
					BMFace *f;
					BMLoop *l;
					count = 0;
					BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH) {
						if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
							l = f->l_first;
							for (int i = 0; i < f->len; ++i, l = l->next) {
								pos += *(Coord3Df*)l->v->co;
								++count;
							}
						}
					}
					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				break;
			}
			case VR_UI::TRANSFORMSPACE_GLOBAL:
			default: {
				manip_t.set_to_identity();
				if (ts->selectmode & SCE_SELECT_VERTEX) {
					BMVert *v;
					count = 0;
					BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
							pos += *(Coord3Df*)v->co;
							++count;
						}
					}
					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				else if (ts->selectmode & SCE_SELECT_EDGE) {
					BMEdge *e;
					count = 0;
					BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH) {
						if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
							pos += *(Coord3Df*)e->v1->co + *(Coord3Df*)e->v2->co;
							count += 2;
						}
					}
					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				else if (ts->selectmode & SCE_SELECT_FACE) {
					BMFace *f;
					BMLoop *l;
					count = 0;
					BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH) {
						if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
							l = f->l_first;
							for (int i = 0; i < f->len; ++i, l = l->next) {
								pos += *(Coord3Df*)l->v->co;
								++count;
							}
						}
					}
					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				break;
			}
			}
		}
		return;
	} /* else, object mode */

	manip_t.set_to_identity();
	if (transform_space == VR_UI::TRANSFORMSPACE_LOCAL) {
		manip_t.m[0][0] = manip_t.m[1][1] = manip_t.m[2][2] = 0.0f;
	}
	float manip_length = 0.0f;
	int num_objects = 0;
	for (; ctx_link; ctx_link = ctx_link->next) {
		obact = (Object*)ctx_link->ptr.data;
		if (!obact) {
			continue;
		}

		if (transform_space == VR_UI::TRANSFORMSPACE_LOCAL) {
			/* Average object rotations (z-axis). */
			*(Coord3Df*)manip_t.m[2] += *(Coord3Df*)obact->obmat[2];
		}
		/* Average object positions for manipulator location */
		*(Coord3Df*)manip_t.m[3] += *(Coord3Df*)obact->obmat[3];
		/* Use largest axis size (across all objects) for manipulator size */
		for (int i = 0; i < 3; ++i) {
			const float& len = (*(Coord3Df*)obact->obmat[i]).length();
			if (len > manip_length) {
				manip_length = len;
			}
		}
		++num_objects;
	}

	*(Coord3Df*)manip_t.m[3] /= num_objects;
	if (transform_space == VR_UI::TRANSFORMSPACE_LOCAL) {
		*(Coord3Df*)manip_t.m[2] /= num_objects;
		(*(Coord3Df*)manip_t.m[2]).normalize_in_place();
		rotation_between_vecs_to_mat3(rot, z_axis, manip_t.m[2]);
		for (int i = 0; i < 3; ++i) {
			memcpy(manip_t.m[i], rot[i], sizeof(float) * 3);
		}
		/* Apply uniform scaling to manipulator */
		for (int i = 0; i < 3; ++i) {
			*(Coord3Df*)manip_t.m[i] *= manip_length;
		}
	}
	else {
		/* Apply uniform scaling to manipulator */
		for (int i = 0; i < 3; ++i) {
			(*(Coord3Df*)manip_t.m[i]).normalize_in_place() *= manip_length;
		}
	}
}

bool Widget_Transform::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Transform::click(VR_UI::Cursor& c)
{
	const Mat44f& m = c.position.get();
	if (CTX_data_edit_object(vr_get_obj()->ctx)) {
		raycast_select_single_edit(*(Coord3Df*)m.m[3], VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
	}
	else {
		raycast_select_single(*(Coord3Df*)m.m[3], VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
	}
	/* Update manipulator transform. */
	update_manipulator();
	
	if (manipulator) {
		for (int i = 0; i < VR_SIDES; ++i) {
			Widget_Transform::obj.do_render[i] = true;
		}
	}
}	

void Widget_Transform::drag_start(VR_UI::Cursor& c)
{
	/* If other hand is already dragging, don't change the current state of the Transform tool. */
	if (c.bimanual) {
		return;
	}

	if (manipulator) {
		/* Test for manipulator selection and set constraints. */
		const Mat44f& m = c.position.get();
		raycast_select_manipulator(*(Coord3Df*)m.m[3]);
	}

	/* Set transform/snapping modes based on constraints */
	memset(constraint_flag, 0, sizeof(int) * 3);
	if (constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
		switch (constraint_mode) {
		case VR_UI::CONSTRAINTMODE_TRANS_X: {
			transform_mode = TRANSFORMMODE_MOVE;
			snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			constraint_flag[0] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_Y: {
			transform_mode = TRANSFORMMODE_MOVE;
			snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			constraint_flag[1] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_Z: {
			transform_mode = TRANSFORMMODE_MOVE;
			snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_XY: {
			transform_mode = TRANSFORMMODE_MOVE;
			snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			constraint_flag[0] = constraint_flag[1] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_YZ: {
			transform_mode = TRANSFORMMODE_MOVE;
			snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			constraint_flag[1] = constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_ZX: {
			transform_mode = TRANSFORMMODE_MOVE;
			snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			constraint_flag[0] = constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_X: {
			transform_mode = TRANSFORMMODE_ROTATE;
			snap_mode = VR_UI::SNAPMODE_ROTATION;
			constraint_flag[0] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_Y: {
			transform_mode = TRANSFORMMODE_ROTATE;
			snap_mode = VR_UI::SNAPMODE_ROTATION;
			constraint_flag[1] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_Z: {
			transform_mode = TRANSFORMMODE_ROTATE;
			snap_mode = VR_UI::SNAPMODE_ROTATION;
			constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_X: {
			transform_mode = TRANSFORMMODE_SCALE;
			snap_mode = VR_UI::SNAPMODE_SCALE;
			constraint_flag[0] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_Y: {
			transform_mode = TRANSFORMMODE_SCALE;
			snap_mode = VR_UI::SNAPMODE_SCALE;
			constraint_flag[1] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_Z: {
			transform_mode = TRANSFORMMODE_SCALE;
			snap_mode = VR_UI::SNAPMODE_SCALE;
			constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_XY: {
			transform_mode = TRANSFORMMODE_SCALE;
			snap_mode = VR_UI::SNAPMODE_SCALE;
			constraint_flag[0] = constraint_flag[1] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_YZ: {
			transform_mode = TRANSFORMMODE_SCALE;
			snap_mode = VR_UI::SNAPMODE_SCALE;
			constraint_flag[1] = constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_ZX: {
			transform_mode = TRANSFORMMODE_SCALE;
			snap_mode = VR_UI::SNAPMODE_SCALE;
			constraint_flag[0] = constraint_flag[2] = 1;
			break;
		}
		default: {
			break;
		}
		}
		memcpy(snap_flag, constraint_flag, sizeof(int) * 3);
	}
	else {
		memset(snap_flag, 1, sizeof(int) * 3);
	}

	/* Set up snapping positions vector */
	bContext *C = vr_get_obj()->ctx;
	ListBase ctx_data_list;
	CTX_data_selected_objects(C, &ctx_data_list);
	CollectionPointerLink *ctx_link = (CollectionPointerLink *)ctx_data_list.first;
	Object *obedit = CTX_data_edit_object(C);
	if (!ctx_link && !obedit) {
		return;
	}
	for (int i = 0; i < nonsnap_t.size(); ++i) {
		delete nonsnap_t[i];
	}
	nonsnap_t.clear();
	for (; ctx_link; ctx_link = ctx_link->next) {
		nonsnap_t.push_back(new Mat44f());
	}
	snapped = false;

	/* Reset manipulator angles */
	memset(&manip_angle, 0, sizeof(float) * 9);
	/* Save original manipulator transformation */
	if (obedit) {
		obmat_inv = (*(Mat44f*)obedit->obmat).inverse();
		manip_t_orig = manip_t * obmat_inv;
	}
	else {
		manip_t_orig = manip_t;
	}

	if (manipulator || constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
		for (int i = 0; i < VR_SIDES; ++i) {
			Widget_Transform::obj.do_render[i] = true;
		}
	}

	//is_dragging = true;

	/* Call drag_contd() immediately? */
	Widget_Transform::obj.drag_contd(c);
}

void Widget_Transform::drag_contd(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	ListBase ctx_data_list;
	CTX_data_selected_objects(C, &ctx_data_list);
	CollectionPointerLink *ctx_link = (CollectionPointerLink *)ctx_data_list.first;
	Object *obedit = CTX_data_edit_object(C);
	if (!ctx_link && !obedit) {
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

		if (transform_mode != TRANSFORMMODE_ROTATE) {
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

	if (obedit) {
		curr = curr * obmat_inv;
		prev = prev * obmat_inv;
	}

	/* Calculate delta based on transform mode. */
	static Mat44f delta;
	if (c.bimanual) {
		delta = prev.inverse() * curr;
	}
	else {
		switch (transform_mode) {
		case TRANSFORMMODE_MOVE: {
			delta = VR_Math::identity_f;
			*(Coord3Df*)delta.m[3] = *(Coord3Df*)curr.m[3] - *(Coord3Df*)prev.m[3];
			break;
		}
		case TRANSFORMMODE_SCALE: {
			delta = VR_Math::identity_f;
			if (constraint_mode == VR_UI::CONSTRAINTMODE_NONE) {
				/* Scaling based on distance from manipulator center. */
				static Coord3Df prev_d, curr_d;		
				prev_d = *(Coord3Df*)prev.m[3] - *(Coord3Df*)manip_t_orig.m[3];
				curr_d = *(Coord3Df*)curr.m[3] - *(Coord3Df*)manip_t_orig.m[3];
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
		case TRANSFORMMODE_ROTATE:
		case TRANSFORMMODE_OMNI:
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
	if (constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
		delta_orig = delta;
		delta = VR_Math::identity_f;
		constrain = true;
	}

	/* Snapping */
	bool snap = false;
	if (VR_UI::ctrl_key_get()) {
		snap = true;
	}

	for (int index = 0; ctx_link; ctx_link = ctx_link->next, ++index) {
		Object *obact = (Object*)ctx_link->ptr.data;
		if (!obact) {
			continue;
		}

		/* Constraints */
		if (constrain) {
			static float axis[3];
			static float angle;
			static Coord3Df temp3;
			switch (constraint_mode) {
			case VR_UI::CONSTRAINTMODE_TRANS_X: {
				project_v3_v3v3(delta.m[3], delta_orig.m[3], manip_t_orig.m[0]);
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_Y: {
				project_v3_v3v3(delta.m[3], delta_orig.m[3], manip_t_orig.m[1]);
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_Z: {
				project_v3_v3v3(delta.m[3], delta_orig.m[3], manip_t_orig.m[2]);
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_XY: {
				project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[0]);
				project_v3_v3v3(&temp2.x, delta_orig.m[3], manip_t_orig.m[1]);
				*(Coord3Df*)delta.m[3] = temp1 + temp2;
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_YZ: {
				project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[1]);
				project_v3_v3v3(&temp2.x, delta_orig.m[3], manip_t_orig.m[2]);
				*(Coord3Df*)delta.m[3] = temp1 + temp2;
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_ZX: {
				project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[0]);
				project_v3_v3v3(&temp2.x, delta_orig.m[3], manip_t_orig.m[2]);
				*(Coord3Df*)delta.m[3] = temp1 + temp2;
				break;
			}
			case VR_UI::CONSTRAINTMODE_ROT_X: {
				mat4_to_axis_angle(axis, &angle, delta_orig.m);
				if ((*(Coord3Df*)axis) * (*(Coord3Df*)manip_t_orig.m[0]) < 0) {
					angle = -angle;
				}
				axis_angle_to_mat4(delta.m, manip_t_orig.m[0], angle);
				if (VR_UI::shift_key_get()) {
					manip_angle[transform_space].x += angle * WIDGET_TRANSFORM_ROT_PRECISION;
				}
				else {
					manip_angle[transform_space].x += angle;
				}
				break;
			}
			case VR_UI::CONSTRAINTMODE_ROT_Y: {
				mat4_to_axis_angle(axis, &angle, delta_orig.m);
				if ((*(Coord3Df*)axis) * (*(Coord3Df*)manip_t_orig.m[1]) < 0) {
					angle = -angle;
				}
				axis_angle_to_mat4(delta.m, manip_t_orig.m[1], angle);
				if (VR_UI::shift_key_get()) {
					manip_angle[transform_space].y += angle * WIDGET_TRANSFORM_ROT_PRECISION;
				}
				else {
					manip_angle[transform_space].y += angle;
				}
				break;
			}
			case VR_UI::CONSTRAINTMODE_ROT_Z: {
				mat4_to_axis_angle(axis, &angle, delta_orig.m);
				if ((*(Coord3Df*)axis) * (*(Coord3Df*)manip_t_orig.m[2]) < 0) {
					angle = -angle;
				}
				axis_angle_to_mat4(delta.m, manip_t_orig.m[2], angle);
				if (VR_UI::shift_key_get()) {
					manip_angle[transform_space].z += angle * WIDGET_TRANSFORM_ROT_PRECISION;
				}
				else {
					manip_angle[transform_space].z += angle;
				}
				break;
			}
			case VR_UI::CONSTRAINTMODE_SCALE_X: {
				float length;
				*(Coord3Df*)scale = (*(Coord3Df*)manip_t_orig.m[0]).normalize();
				if (c.bimanual) {
					length = -delta_orig.m[3][0];
				}
				else {
					project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[0]);
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
				*(Coord3Df*)scale = (*(Coord3Df*)manip_t_orig.m[1]).normalize();
				if (c.bimanual) {
					length = -delta_orig.m[3][1];
				}
				else {
					project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[1]);
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
				*(Coord3Df*)scale = (*(Coord3Df*)manip_t_orig.m[2]).normalize();
				if (c.bimanual) {
					length = -delta_orig.m[3][2];
				}
				else {
					project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[2]);
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
					*(Coord3Df*)scale = ((*(Coord3Df*)manip_t_orig.m[0]).normalize() + (*(Coord3Df*)manip_t_orig.m[1]).normalize()) / 2.0f;
				}
				else {
					project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[0]);
					length = temp1.length();
					(*(Coord3Df*)scale = (*(Coord3Df*)delta_orig.m[3]).normalize());
					temp1 = (*(Coord3Df*)manip_t_orig.m[0]).normalize();
					if (dot_v3v3((float*)&temp1, scale) < 0) {
						length = -length;
					}
					project_v3_v3v3(&temp3.x, delta_orig.m[3], manip_t_orig.m[1]);
					temp2 = (*(Coord3Df*)manip_t_orig.m[1]).normalize();
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
					*(Coord3Df*)scale = ((*(Coord3Df*)manip_t_orig.m[1]).normalize() + (*(Coord3Df*)manip_t_orig.m[2]).normalize()) / 2.0f;
				}
				else {
					project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[1]);
					length = temp1.length();
					(*(Coord3Df*)scale = (*(Coord3Df*)delta_orig.m[3]).normalize());
					temp1 = (*(Coord3Df*)manip_t_orig.m[1]).normalize();
					if (dot_v3v3((float*)&temp1, scale) < 0) {
						length = -length;
					}
					project_v3_v3v3(&temp3.x, delta_orig.m[3], manip_t_orig.m[2]);
					temp2 = (*(Coord3Df*)manip_t_orig.m[2]).normalize();
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
					*(Coord3Df*)scale = ((*(Coord3Df*)manip_t_orig.m[0]).normalize() + (*(Coord3Df*)manip_t_orig.m[2]).normalize()) / 2.0f;
				}
				else {
					project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[0]);
					length = temp1.length();
					(*(Coord3Df*)scale = (*(Coord3Df*)delta_orig.m[3]).normalize());
					temp1 = (*(Coord3Df*)manip_t_orig.m[0]).normalize();
					if (dot_v3v3((float*)&temp1, scale) < 0) {
						length = -length;
					}
					project_v3_v3v3(&temp3.x, delta_orig.m[3], manip_t_orig.m[2]);
					temp2 = (*(Coord3Df*)manip_t_orig.m[2]).normalize();
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
			if (obedit) { /* Edit mode */
				Mat44f& nonsnap_m = *nonsnap_t[index];
				if (!snapped) {
					nonsnap_m = manip_t * obmat_inv;
					manip_t_snap = manip_t * obmat_inv;
				}
				else {
					m = nonsnap_m;
					nonsnap_m = m * delta;
				}
				static Mat44f manip_t_prev;
				manip_t_prev = manip_t_snap;

				/* Apply snapping. */
				float precision, iter_fac, val;
				for (int i = 0; i < 3; ++i) {
					scale[i] = (*(Coord3Df*)nonsnap_m.m[i]).length();
				}
				switch (snap_mode) {
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
						if (!snap_flag[i]) {
							continue;
						}
						iter_fac = precision * scale[i];
						val = roundf(*pos / iter_fac);
						manip_t_snap.m[3][i] = iter_fac * val;
					}
					switch (constraint_mode) {
					case VR_UI::CONSTRAINTMODE_TRANS_X: {
						temp1 = *(Coord3Df*)manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[0]);
						*(Coord3Df*)manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
						break;
					}
					case VR_UI::CONSTRAINTMODE_TRANS_Y: {
						temp1 = *(Coord3Df*)manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[1]);
						*(Coord3Df*)manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
						break;
					}
					case VR_UI::CONSTRAINTMODE_TRANS_Z: {
						temp1 = *(Coord3Df*)manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[2]);
						*(Coord3Df*)manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
						break;
					}
					case VR_UI::CONSTRAINTMODE_TRANS_XY: {
						temp1 = *(Coord3Df*)manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[0]);
						*(Coord3Df*)manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[1]);
						*(Coord3Df*)manip_t_snap.m[3] += temp2;
						break;
					}
					case VR_UI::CONSTRAINTMODE_TRANS_YZ: {
						temp1 = *(Coord3Df*)manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[1]);
						*(Coord3Df*)manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[2]);
						*(Coord3Df*)manip_t_snap.m[3] += temp2;
						break;
					}
					case VR_UI::CONSTRAINTMODE_TRANS_ZX: {
						temp1 = *(Coord3Df*)manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[0]);
						*(Coord3Df*)manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[2]);
						*(Coord3Df*)manip_t_snap.m[3] += temp2;
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
						if (!snap_flag[i]) {
							continue;
						}
						val = roundf(eul[i] / precision);
						eul[i] = precision * val;
					}
					eul_to_mat3(rot, eul);
					for (int i = 0; i < 3; ++i) {
						memcpy(manip_t_snap.m[i], rot[i], sizeof(float) * 3);
						*(Coord3Df*)manip_t_snap.m[i] *= scale[i];
					}
					/* Update manipulator angles. */
					/* TODO_XR */
					//for (int i = 0; i < 3; ++i) {
					//	if (!snap_flag[i]) {
					//		continue;
					//	}
					//	switch (i) {
					//	case 0: {
					//		float& m_angle = manip_angle[transform_space].x;
					//		m_angle += eul[i] - eul_orig[i];
					//		val = roundf(m_angle / precision);
					//		m_angle = precision * val;
					//		break;
					//	}
					//	case 1: {
					//		float& m_angle = manip_angle[transform_space].y;
					//		m_angle += eul[i] - eul_orig[i];
					//		val = roundf(m_angle / precision);
					//		m_angle = precision * val;
					//		break;
					//	}
					//	case 2: {
					//		float& m_angle = manip_angle[transform_space].z;
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
					//if (transform_space == VR_UI::TRANSFORMSPACE_GLOBAL && constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
					//	/* TODO_XR */
					//	break;
					//	/*for (int i = 0; i < 3; ++i) {
					//		if (snap_flag[i]) {
					//			continue;
					//		}
					//		(*(Coord3Df*)manip_t_snap.m[i]).normalize_in_place() *= (*(Coord3Df*)nonsnap_m.m[i]).length();
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
					//	(*(Coord3Df*)manip_t_snap.m[i]).normalize_in_place() *= (precision * val);
					//}
					break;
				}
				default: {
					break;
				}
				}

				delta = manip_t_prev.inverse() * manip_t_snap;
				if (snap_mode == VR_UI::SNAPMODE_ROTATION) {
					memset(delta.m[3], 0, sizeof(float) * 3);
				}
				BMIter iter;

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

				/* Set recalc flags. */
				DEG_id_tag_update((ID*)obedit->data, 0);
				/* Exit object iteration loop. */
				break;
			}
			else { /* Object mode */
				/* Save actual position. */
				Mat44f& nonsnap_m = *nonsnap_t[index];
				Mat44f& obmat = *(Mat44f*)obact->obmat;
				if (!snapped) {
					nonsnap_m = obmat;
				}
				else {
					m = nonsnap_m;
					nonsnap_m = m * delta;
				}

				/* Apply snapping. */
				float precision, iter_fac, val;
				for (int i = 0; i < 3; ++i) {
					scale[i] = (*(Coord3Df*)nonsnap_m.m[i]).length();
				}
				switch (snap_mode) {
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
						if (!snap_flag[i]) {
							continue;
						}
						iter_fac = precision * scale[i];
						val = roundf(*pos / iter_fac);
						obmat.m[3][i] = iter_fac * val;
					}
					if (transform_space == VR_UI::TRANSFORMSPACE_LOCAL) {
						switch (constraint_mode) {
						case VR_UI::CONSTRAINTMODE_TRANS_X: {
							temp1 = *(Coord3Df*)obmat.m[3] - *(Coord3Df*)nonsnap_m.m[3];
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[0]);
							*(Coord3Df*)obmat.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
							break;
						}
						case VR_UI::CONSTRAINTMODE_TRANS_Y: {
							temp1 = *(Coord3Df*)obmat.m[3] - *(Coord3Df*)nonsnap_m.m[3];
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[1]);
							*(Coord3Df*)obmat.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
							break;
						}
						case VR_UI::CONSTRAINTMODE_TRANS_Z: {
							temp1 = *(Coord3Df*)obmat.m[3] - *(Coord3Df*)nonsnap_m.m[3];
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[2]);
							*(Coord3Df*)obmat.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
							break;
						}
						case VR_UI::CONSTRAINTMODE_TRANS_XY: {
							temp1 = *(Coord3Df*)obmat.m[3] - *(Coord3Df*)nonsnap_m.m[3];
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[0]);
							*(Coord3Df*)obmat.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[1]);
							*(Coord3Df*)obmat.m[3] += temp2;
							break;
						}
						case VR_UI::CONSTRAINTMODE_TRANS_YZ: {
							temp1 = *(Coord3Df*)obmat.m[3] - *(Coord3Df*)nonsnap_m.m[3];
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[1]);
							*(Coord3Df*)obmat.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[2]);
							*(Coord3Df*)obmat.m[3] += temp2;
							break;
						}
						case VR_UI::CONSTRAINTMODE_TRANS_ZX: {
							temp1 = *(Coord3Df*)obmat.m[3] - *(Coord3Df*)nonsnap_m.m[3];
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[0]);
							*(Coord3Df*)obmat.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[2]);
							*(Coord3Df*)obmat.m[3] += temp2;
							break;
						}
						default: {
							/* TODO_XR: Local translation snappping (no constraints) */
							break;
						}
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
					/* TODO_XR: Local rotation snapping (no constraints). */
					mat4_to_eul(eul, nonsnap_m.m);
					//static float eul_orig[3];
					//memcpy(eul_orig, eul, sizeof(float) * 3);
					for (int i = 0; i < 3; ++i) {
						if (!snap_flag[i]) {
							continue;
						}
						val = roundf(eul[i] / precision);
						eul[i] = precision * val;
					}
					eul_to_mat3(rot, eul);
					for (int i = 0; i < 3; ++i) {
						memcpy(obmat.m[i], rot[i], sizeof(float) * 3);
						*(Coord3Df*)obmat.m[i] *= scale[i];
					}
					/* Update manipulator angles. */
					/* TODO_XR */
					/*for (int i = 0; i < 3; ++i) {
						if (!snap_flag[i]) {
							continue;
						}
						switch (i) {
						case 0: {
							float& m_angle = manip_angle[transform_space].x;
							m_angle += eul[i] - eul_orig[i];
							val = roundf(m_angle / precision);
							m_angle = precision * val;
							break;
						}
						case 1: {
							float& m_angle = manip_angle[transform_space].y;
							m_angle += eul[i] - eul_orig[i];
							val = roundf(m_angle / precision);
							m_angle = precision * val;
							break;
						}
						case 2: {
							float& m_angle = manip_angle[transform_space].z;
							m_angle += eul[i] - eul_orig[i];
							val = roundf(m_angle / precision);
							m_angle = precision * val;
							break;
						}
						}
					}*/
					break;
				}
				case VR_UI::SNAPMODE_SCALE: {
					/* Scale */
					if (transform_space == VR_UI::TRANSFORMSPACE_GLOBAL && constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
						/* TODO_XR */
						break;
						/*for (int i = 0; i < 3; ++i) {
							if (snap_flag[i]) {
								continue;
							}
							(*(Coord3Df*)obmat.m[i]).normalize_in_place() *= (*(Coord3Df*)nonsnap_m.m[i]).length();
						}
						static Mat44f t;
						transpose_m4_m4(t.m, nonsnap_m.m);
						for (int i = 0; i < 3; ++i) {
							scale[i] = (*(Coord3Df*)t.m[i]).length();
						}*/
					}
					for (int i = 0; i < 3; ++i) {
						if (!snap_flag[i]) {
							continue;
						}
						if (VR_UI::shift_key_get()) {
							/* Snap scale based on the power of ten magnitude of the curent scale */
							precision = 0.1f * powf(10.0f, floor(log10(scale[i])));
						}
						else {
							precision = 0.5f * powf(10.0f, floor(log10(scale[i])));
						}
						val = roundf(scale[i] / precision);
						if (val == 0.0f) {
							val = 1.0f;
						}
						(*(Coord3Df*)obmat.m[i]).normalize_in_place() *= (precision * val);
					}
					break;
				}
				default: {
					break;
				}
				}
				/* Set recalc flags. */
				DEG_id_tag_update((ID*)obact->data, 0);
			}
		}
		else {
			if (obedit) { /* Edit mode */
				/* Transform mode */
				switch (transform_mode) {
				case TRANSFORMMODE_MOVE: {
					for (int i = 0; i < 3; ++i) {
						memcpy(delta.m[i], VR_Math::identity_f.m[i], sizeof(float) * 3);
					}
					break;
				}
				case TRANSFORMMODE_ROTATE: {
					memset(delta.m[3], 0, sizeof(float) * 3);
					break;
				}
				case TRANSFORMMODE_SCALE: {
					memset(delta.m[3], 0, sizeof(float) * 3);
					break;
				}
				case TRANSFORMMODE_OMNI: 
				default: {
					break;
				}
				}

				BMIter iter;
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

				/* Set recalc flags. */
				DEG_id_tag_update((ID*)obedit->data, 0);
				/* Exit object iteration loop. */
				break;
			}
			else { /* Object mode */
				m = *(Mat44f*)obact->obmat * delta;

				/* Transform mode */
				switch (transform_mode) {
				case TRANSFORMMODE_MOVE: {
					memcpy(obact->obmat[3], m.m[3], sizeof(float) * 3);
					break;
				}
				case TRANSFORMMODE_ROTATE: {
					for (int i = 0; i < 3; ++i) {
						scale[i] = (*(Coord3Df*)(obact->obmat[i])).length();
						(*(Coord3Df*)(m.m[i])).normalize_in_place();
						memcpy(obact->obmat[i], m.m[i], sizeof(float) * 3);
						*(Coord3Df*)obact->obmat[i] *= scale[i];
					}
					break;
				}
				case TRANSFORMMODE_SCALE: {
					if (transform_space == VR_UI::TRANSFORMSPACE_LOCAL && constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
						for (int i = 0; i < 3; ++i) {
							if (!constraint_flag[i]) {
								continue;
							}
							(*(Coord3Df*)obact->obmat[i]).normalize_in_place() *= (*(Coord3Df*)m.m[i]).length();
						}
					}
					else {
						for (int i = 0; i < 3; ++i) {
							(*(Coord3Df*)obact->obmat[i]).normalize_in_place() *= (*(Coord3Df*)m.m[i]).length();
						}
					}
					break;
				}
				case TRANSFORMMODE_OMNI: {
					memcpy(obact->obmat, m.m, sizeof(float) * 4 * 4);
				}
				default: {
					break;
				}
				}
				/* Set recalc flags. */
				DEG_id_tag_update((ID*)obact->data, 0);
			}
		}
	}

	if (snap) {
		snapped = true;
	}
	else {
		snapped = false;
	}

	if (manipulator || constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
		/* Update manipulator transform (also used when rendering constraints). */
		static VR_UI::TransformSpace prev_space = VR_UI::TRANSFORMSPACE_GLOBAL;
		if (prev_space != transform_space) {
			prev_space = transform_space;
			if (obedit) {
				BMEditMesh *em = BKE_editmesh_from_object(obedit);
				EDBM_mesh_normals_update(em);
			}
			update_manipulator();
			if (obedit) {
				manip_t_orig = manip_t * (*(Mat44f*)obedit->obmat).inverse();
			}
			else {
				manip_t_orig = manip_t;
			}
		}
		else {
			/* Don't update manipulator transformation for rotations. */
			if (transform_mode != TRANSFORMMODE_ROTATE) {
				update_manipulator();
			}
		}

		for (int i = 0; i < VR_SIDES; ++i) {
			Widget_Transform::obj.do_render[i] = true;
		}
	}

	is_dragging = true;
}

void Widget_Transform::drag_stop(VR_UI::Cursor& c)
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

	/* TODO_XR: Avoid doing this twice (already done in drag_start() */
	if (manipulator) {
		constraint_mode = VR_UI::CONSTRAINTMODE_NONE;
		memset(constraint_flag, 0, sizeof(int) * 3);
		memset(snap_flag, 1, sizeof(int) * 3);
	}
	else {
		for (int i = 0; i < VR_SIDES; ++i) {
			Widget_Transform::obj.do_render[i] = false;
		}
	}
	if (omni) {
		transform_mode = TRANSFORMMODE_OMNI;
		snap_mode = VR_UI::SNAPMODE_TRANSLATION;
	}

	is_dragging = false;

	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (obedit) { /* Edit mode */
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		EDBM_mesh_normals_update(em);
		update_manipulator();

		DEG_id_tag_update((ID*)obedit->data, ID_RECALC_GEOMETRY);
		WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
		ED_undo_push(C, "Transform");
	}
	else { /* Object mode */
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
		update_manipulator();

		DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
		ED_undo_push(C, "Transform");
	}
}

void Widget_Transform::render_axes(const float length[3], int draw_style)
{
	if (draw_style == 2 && !manipulator) {
		return;
	}

	/* Adapted from arrow_draw_geom() in arrow3d_gizmo.c */
	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	bool unbind_shader = true;

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	/* Axes */
	GPU_line_width(1.0f);
	for (int i = 0; i < 3; ++i) {
		if (constraint_flag[i] || manipulator) {
			if (constraint_flag[i]) {
				immUniformColor4fv(c_manip_select[i]);
			}
			else {
				immUniformColor4fv(c_manip[i]);
			}
			immBegin(GPU_PRIM_LINES, 2);
			switch (i) {
			case 0: { /* x-axis */
				if (manipulator || transform_mode == TRANSFORMMODE_ROTATE) {
					immVertex3f(pos, 0.0f, 0.0f, 0.0f);
				}
				else {
					immVertex3f(pos, -length[i] , 0.0f, 0.0f);
				}
				immVertex3f(pos, length[i], 0.0f, 0.0f);
				break;
			}
			case 1: { /* y-axis */
				if (manipulator || transform_mode == TRANSFORMMODE_ROTATE) {
					immVertex3f(pos, 0.0f, 0.0f, 0.0f);
				}
				else {
					immVertex3f(pos, 0.0f, -length[i], 0.0f);
				}
				immVertex3f(pos, 0.0f, length[i], 0.0f);
				break;
			}
			case 2: { /* z-axis */
				if (manipulator || transform_mode == TRANSFORMMODE_ROTATE) {
					immVertex3f(pos, 0.0f, 0.0f, 0.0f);
				}
				else {
					immVertex3f(pos, 0.0f, 0.0f, -length[i]);
				}
				immVertex3f(pos, 0.0f, 0.0f, length[i]);
				break;
			}
			}
			immEnd();
		}
	}
		
	/* *** draw arrow head *** */
	GPU_matrix_push();

	switch (draw_style) {
	case 3: { /* Extrude Ball */
		unbind_shader = true;
		GPU_line_width(1.0f);
		GPUBatch *sphere = GPU_batch_preset_sphere(0);
		GPU_batch_program_set_builtin(sphere, GPU_SHADER_3D_UNIFORM_COLOR);
		float offset[3];
		if (Widget_Extrude::extrude_mode == Widget_Extrude::EXTRUDEMODE_REGION) {
			/* xyz extrude balls */
			for (int i = 0; i < 3; ++i) {
				if (Widget_Extrude::extrude && constraint_flag[i]) {
					GPU_batch_uniform_4fv(sphere, "color", c_manip_select[i]);
				}
				else {
					GPU_batch_uniform_4fv(sphere, "color", c_manip[i]);
				}
				float scale = length[i] * WIDGET_TRANSFORM_BALL_SCALE_FACTOR * 2.0f;
				switch (i) {
				case 0: { /* x-axis */
					offset[0] = length[0] + scale * 3.0f; offset[1] = 0.0f; offset[2] = 0.0f;
					break;
				}
				case 1: { /* y-axis */
					offset[0] = 0.0f; offset[1] = length[1] + scale * 3.0f; offset[2] = 0.0f;
					break;
				}
				case 2: { /* z-axis */
					offset[0] = 0.0f; offset[1] = 0.0f; offset[2] = length[2] + scale * 3.0f;
					break;
				}
				}

				GPU_matrix_translate_3fv(offset);
				GPU_matrix_scale_1f(scale);

				GPU_batch_draw(sphere);

				GPU_matrix_scale_1f(1.0f / (scale));
				*(Coord3Df*)offset *= -1.0f;
				GPU_matrix_translate_3fv(offset);
			}
		}
		else {
			/* extrude ball */
			if (Widget_Extrude::extrude && constraint_flag[2]) {
				GPU_batch_uniform_4fv(sphere, "color", c_manip_select[3]);
			}
			else {
				GPU_batch_uniform_4fv(sphere, "color", c_manip[3]);
			}
			float scale = length[2] * WIDGET_TRANSFORM_BALL_SCALE_FACTOR * 2.0f;
			offset[0] = 0.0f; offset[1] = 0.0f; offset[2] = length[2] + scale * 3.0f;

			GPU_matrix_translate_3fv(offset);
			GPU_matrix_scale_1f(scale);

			GPU_batch_draw(sphere);

			GPU_matrix_scale_1f(1.0f / (scale));
			*(Coord3Df*)offset *= -1.0f;
			GPU_matrix_translate_3fv(offset);
		}
		break;
	}
	case 2: { /* Ball */
		unbind_shader = true;
		GPU_line_width(1.0f);
		GPUBatch *sphere = GPU_batch_preset_sphere(0);
		GPU_batch_program_set_builtin(sphere, GPU_SHADER_3D_UNIFORM_COLOR);
		float offset[3];
		for (int i = 0; i < 3; ++i) {
			if (constraint_flag[i]) {
				GPU_batch_uniform_4fv(sphere, "color", c_manip_select[i]);
			}
			else {
				GPU_batch_uniform_4fv(sphere, "color", c_manip[i]);
			}
			float scale = length[i] * WIDGET_TRANSFORM_BALL_SCALE_FACTOR;
			switch (i) {
			case 0: { /* x-axis */
				offset[0] = 0.0f; offset[1] = length[1] / 1.5f + scale / 2.0f; offset[2] = length[2] / 1.5f + scale / 2.0f;
				break;
			}
			case 1: { /* y-axis */
				offset[0] = length[0] / 1.5f + scale / 2.0f; offset[1] = 0.0f; offset[2] = length[2] / 1.5f + scale / 2.0f;
				break;
			}
			case 2: { /* z-axis */
				offset[0] = length[0] / 1.5f + scale / 2.0f; offset[1] = length[1] / 1.5f + scale / 2.0f; offset[2] = 0.0f;
				break;
			}
			}

			GPU_matrix_translate_3fv(offset);
			GPU_matrix_scale_1f(scale);

			GPU_batch_draw(sphere);

			GPU_matrix_scale_1f(1.0f / scale);
			*(Coord3Df*)offset *= -1.0f;
			GPU_matrix_translate_3fv(offset);
		}
		break;
	}
	case 1: { /* Box */
		static float size[3];
		for (int i = 0; i < 3; ++i) {
			size[i] = length[i] * WIDGET_TRANSFORM_BOX_SCALE_FACTOR;
		}

		for (int i = 0; i < 3; ++i) {
			if (constraint_flag[i] || manipulator) {
				switch (i) {
				case 0: { /* x-axis */
					GPU_matrix_translate_3f(length[i] + size[i], 0.0f, 0.0f);
					GPU_matrix_rotate_axis(90.0f, 'Y');
					GPU_matrix_scale_3f(size[i], size[i], size[i]);

					immUnbindProgram();
					unbind_shader = false;
					if (constraint_flag[i]) {
						wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, true, c_manip_select[i]);
					}
					else {
						wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, false, c_manip[i]);
					}

					GPU_matrix_scale_3f(1.0f / size[i], 1.0f / size[i], 1.0f / size[i]);
					GPU_matrix_rotate_axis(-90.0f, 'Y');
					GPU_matrix_translate_3f(-(length[i] + size[i]), 0.0f, 0.0f);
					break;
				}
				case 1: { /* y-axis */
					GPU_matrix_translate_3f(0.0f, length[i] + size[i], 0.0f);
					GPU_matrix_rotate_axis(-90.0f, 'X');
					GPU_matrix_scale_3f(size[i], size[i], size[i]);

					if (constraint_flag[i]) {
						wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, true, c_manip_select[i]);
					}
					else {
						wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, false, c_manip[i]);
					}

					GPU_matrix_scale_3f(1.0f / size[i], 1.0f / size[i], 1.0f / size[i]);
					GPU_matrix_rotate_axis(90.0f, 'X');
					GPU_matrix_translate_3f(0.0f, -(length[i] + size[i]), 0.0f);
					break;
				}
				case 2: { /* z-axis */
					GPU_matrix_translate_3f(0.0f, 0.0f, length[i] + size[i]);
					GPU_matrix_scale_3f(size[i], size[i], size[i]);

					if (constraint_flag[i]) {
						wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, true, c_manip_select[i]);
					}
					else {
						wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, false, c_manip[i]);
					}

					GPU_matrix_scale_3f(1.0f / size[i], 1.0f / size[i], 1.0f / size[i]);
					GPU_matrix_translate_3f(0.0f, 0.0f, -(length[i] + size[i]));
					break;
				}
				}
			}
		}
		/* Center scale box */
		if (omni && manipulator) {
			size[0] = length[0] * WIDGET_TRANSFORM_BOX_SCALE_FACTOR;
			GPU_matrix_scale_3f(size[0], size[0], size[0]);
			if (transform_mode == TRANSFORMMODE_SCALE && constraint_mode == VR_UI::CONSTRAINTMODE_NONE) {
				wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, true, c_manip_select[3]);
			}
			else {
				wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, false, c_manip[3]);
			}
			GPU_matrix_scale_3f(1.0f / size[0], 1.0f / size[0], 1.0f / size[0]);
		}
		break;
	}
	case 0: 
	default: { /* Arrow */
		for (int i = 0; i < 3; ++i) {
			if (constraint_flag[i] || manipulator) {
				float len = length[i] * WIDGET_TRANSFORM_ARROW_SCALE_FACTOR;
				float width = length[i] * 0.04f;
				switch (i) {
				case 0: { /* x-axis */
					if (constraint_flag[i]) {
						immUniformColor4fv(c_manip_select[i]);
					}
					else {
						immUniformColor4fv(c_manip[i]);
					}
					GPU_matrix_translate_3f(length[i], 0.0f, 0.0f);
					GPU_matrix_rotate_axis(90.0f, 'Y');

					imm_draw_circle_fill_3d(pos, 0.0, 0.0, width, 8);
					imm_draw_cylinder_fill_3d(pos, width, 0.0, len, 8, 1);

					GPU_matrix_rotate_axis(-90.0f, 'Y');
					GPU_matrix_translate_3f(-length[i], 0.0f, 0.0f);
					break;
				}
				case 1: { /* y-axis */
					if (constraint_flag[i]) {
						immUniformColor4fv(c_manip_select[i]);
					}
					else {
						immUniformColor4fv(c_manip[i]);
					}
					GPU_matrix_translate_3f(0.0f, length[i], 0.0f);
					GPU_matrix_rotate_axis(-90.0f, 'X');

					imm_draw_circle_fill_3d(pos, 0.0, 0.0, width, 8);
					imm_draw_cylinder_fill_3d(pos, width, 0.0, len, 8, 1);

					GPU_matrix_rotate_axis(90.0f, 'X');
					GPU_matrix_translate_3f(0.0f, -length[i], 0.0f);
					break;
				}
				case 2: { /* z-axis */
					if (constraint_flag[i]) {
						immUniformColor4fv(c_manip_select[i]);
					}
					else {
						immUniformColor4fv(c_manip[i]);
					}
					GPU_matrix_translate_3f(0.0f, 0.0f, length[i]);

					imm_draw_circle_fill_3d(pos, 0.0, 0.0, width, 8);
					imm_draw_cylinder_fill_3d(pos, width, 0.0, len, 8, 1);

					GPU_matrix_translate_3f(0.0f, 0.0f, -length[i]);
					break;
				}
				}
			}
		}
		break;
	}
	}

	GPU_matrix_pop();

	if (unbind_shader) {
		immUnbindProgram();
	}
}

void Widget_Transform::render_planes(const float length[3])
{
	/* Adapated from gizmo_primitive_draw_geom() in primitive3d_gizmo.c */

	if (!manipulator) {
		return;
	}

	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	static float verts_plane[4][3] = { 0.0f };
	float len, len2;

	for (int i = 0; i < 3; ++i) {
		len = length[i] / 4.0f;
		len2 = len / 8.0f;
		verts_plane[0][0] = -len2; verts_plane[0][1] = -len2;
		verts_plane[1][0] = len2; verts_plane[1][1] = -len2;
		verts_plane[2][0] = len2; verts_plane[2][1] = len2;
		verts_plane[3][0] = -len2; verts_plane[3][1] = len2;

		switch (i) {
		case 0: { /* yz-plane */
			GPU_matrix_translate_3f(0.0f, len, len);
			GPU_matrix_rotate_axis(90.0f, 'Y');

			if (constraint_flag[1] && constraint_flag[2]) {
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_TRI_FAN);
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_LINE_LOOP);
			}
			else {
				wm_gizmo_vec_draw(c_manip[i], verts_plane, 4, pos, GPU_PRIM_TRI_FAN);
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_LINE_LOOP);
			}

			GPU_matrix_rotate_axis(-90.0f, 'Y');
			GPU_matrix_translate_3f(0.0f, -len, -len);
			break;
		}
		case 1: { /* zx-plane */
			GPU_matrix_translate_3f(len, 0.0f, len);
			GPU_matrix_rotate_axis(90.0f, 'X');

			if (constraint_flag[0] && constraint_flag[2]) {
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_TRI_FAN);
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_LINE_LOOP);
			}
			else {
				wm_gizmo_vec_draw(c_manip[i], verts_plane, 4, pos, GPU_PRIM_TRI_FAN);
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_LINE_LOOP);
			}

			GPU_matrix_rotate_axis(-90.0f, 'X');
			GPU_matrix_translate_3f(-len, 0.0f, -len);
			break;
		}
		case 2: { /* xy-axis */
			GPU_matrix_translate_3f(len, len, 0.0f);

			if (constraint_flag[0] && constraint_flag[1]) {
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_TRI_FAN);
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_LINE_LOOP);
			}
			else {
				wm_gizmo_vec_draw(c_manip[i], verts_plane, 4, pos, GPU_PRIM_TRI_FAN);
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_LINE_LOOP);
			}

			GPU_matrix_translate_3f(-len, -len, 0.0f);
			break;
		}
		}
	}

	immUnbindProgram();
}

void Widget_Transform::render_gimbal(
	const float radius[3],
	const bool filled,
	const float axis_modal_mat[4][4], const float clip_plane[4],
	const float arc_partial_angle, const float arc_inner_factor)
{
	/* Adapted from dial_geom_draw() in dial3d_gizmo.c */
	GPU_line_width(1.0f);
	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

	if (clip_plane) {
		immBindBuiltinProgram(GPU_SHADER_3D_CLIPPED_UNIFORM_COLOR);
		immUniform4fv("ClipPlane", clip_plane);
		immUniformMatrix4fv("ModelMatrix", axis_modal_mat);
		glEnable(GL_CLIP_DISTANCE0);
	}
	else {
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	}

	float rad = 0.0f;
	for (int i = 0; i < 3; ++i) {
		if (constraint_flag[i] || manipulator) {
			if (constraint_flag[i]) {
				immUniformColor4fv(c_manip_select[i]);
			}
			else {
				immUniformColor4fv(c_manip[i]);
			}
			switch (i) { /* x-axis */
			case 0: {
				GPU_matrix_rotate_axis(-90.0f, 'Y');
				break;
			}
			case 1: { /* y-axis */
				GPU_matrix_rotate_axis(90.0f, 'X');
				break;
			}
			case 2: { /* z-axis */
				break;
			}
			}

			rad = radius[i] / 4.0f;

			if (filled) {
				imm_draw_circle_fill_2d(pos, 0, 0, rad, WIDGET_TRANSFORM_DIAL_RESOLUTION);
			}
			else {
				if (arc_partial_angle == 0.0f) {
					imm_draw_circle_wire_2d(pos, 0, 0, rad, WIDGET_TRANSFORM_DIAL_RESOLUTION);
					if (arc_inner_factor != 0.0f) {
						imm_draw_circle_wire_2d(pos, 0, 0, arc_inner_factor, WIDGET_TRANSFORM_DIAL_RESOLUTION);
					}
				}
				else {
					float arc_partial_deg = RAD2DEGF((M_PI * 2) - arc_partial_angle);
					imm_draw_circle_partial_wire_2d(
						pos, 0, 0, rad, WIDGET_TRANSFORM_DIAL_RESOLUTION,
						0.0f, arc_partial_deg);
				}
			}

			switch (i) { /* x-axis */
			case 0: {
				GPU_matrix_rotate_axis(90.0f, 'Y');
				break;
			}
			case 1: { /* y-axis */
				GPU_matrix_rotate_axis(-90.0f, 'X');
				break;
			}
			case 2: { /* z-axis */
				break;
			}
			}
		}
	}

	immUnbindProgram();

	if (clip_plane) {
		glDisable(GL_CLIP_DISTANCE0);
	}
}

/* From dial3d_gizmo.c. */
static void dial_ghostarc_draw(
	const float angle_ofs, const float angle_delta,
	const float arc_inner_factor, const float color[4], const float radius)
{
	const float width_inner = radius;
	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	if (arc_inner_factor != 0.0) {
		float color_dark[4] = { 0 };
		color_dark[3] = color[3] / 2;
		immUniformColor4fv(color_dark);
		imm_draw_disk_partial_fill_2d(
			pos, 0, 0, arc_inner_factor, width_inner, WIDGET_TRANSFORM_DIAL_RESOLUTION, RAD2DEGF(angle_ofs), RAD2DEGF(M_PI * 2));
	}

	immUniformColor4fv(color);
	imm_draw_disk_partial_fill_2d(
		pos, 0, 0, arc_inner_factor, width_inner, WIDGET_TRANSFORM_DIAL_RESOLUTION, RAD2DEGF(angle_ofs), RAD2DEGF(angle_delta));
	immUnbindProgram();
}

static void dial_ghostarc_draw_helpline(
	const float angle, const float co_outer[3], const float color[4])
{
	GPU_matrix_push();
	GPU_matrix_rotate_3f(RAD2DEGF(angle), 0.0f, 0.0f, -1.0f);

	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	immUniformColor4fv(color);

	immBegin(GPU_PRIM_LINE_STRIP, 2);
	immVertex3f(pos, 0.0f, 0, 0.0f);
	immVertex3fv(pos, co_outer);
	immEnd();

	immUnbindProgram();

	GPU_matrix_pop();
}

void Widget_Transform::render_dial(
	const float angle_ofs, const float angle_delta,
	const float arc_inner_factor, const float radius)
{
	/* From dial_ghostarc_draw_with_helplines() in dial3d_gizmo.c */

	/* Coordinate at which the arc drawing will be started. */
	const float co_outer[4] = { 0.0f, radius, 0.0f };
	const float color[4] = { 0.8f, 0.8f, 0.8f, 0.4f };
	dial_ghostarc_draw(angle_ofs, angle_delta, arc_inner_factor, color, radius);
	GPU_line_width(1.0f);
	int index;
	switch (constraint_mode) {
	case VR_UI::CONSTRAINTMODE_ROT_X: {
		index = 0;
		break;
	}
	case VR_UI::CONSTRAINTMODE_ROT_Y: {
		index = 1;
		break;
	}
	case VR_UI::CONSTRAINTMODE_ROT_Z: {
		index = 2;
		break;
	}
	case VR_UI::CONSTRAINTMODE_NONE:
	default: {
		const float color_helpline[4] = { 0.4f, 0.4f, 0.4f, 0.6f };
		dial_ghostarc_draw_helpline(angle_ofs, co_outer, color_helpline);
		dial_ghostarc_draw_helpline(angle_ofs + angle_delta, co_outer, color_helpline);
		return;
	}
	}
	dial_ghostarc_draw_helpline(angle_ofs, co_outer, c_manip_select[index]);
	dial_ghostarc_draw_helpline(angle_ofs + angle_delta, co_outer, c_manip_select[index]);
}

void Widget_Transform::render_incremental_angles(
	const float incremental_angle, const float offset, const float radius)
{
	/* From dial_ghostarc_draw_incremental_angle() in dial3d_gizmo.c */

	const int tot_incr = (2 * M_PI) / incremental_angle;
	GPU_line_width(2.0f);

	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor3f(1.0f, 1.0f, 1.0f);
	immBegin(GPU_PRIM_LINES, tot_incr * 2);

	float v[3] = { 0 };
	for (int i = 0; i < tot_incr; i++) {
		v[0] = sinf(offset + incremental_angle * i);
		v[1] = cosf(offset + incremental_angle * i);

		mul_v2_fl(v, radius * 1.1f);
		immVertex3fv(pos, v);

		mul_v2_fl(v, 1.1f);
		immVertex3fv(pos, v);
	}

	immEnd();
	immUnbindProgram();
}

void Widget_Transform::render(VR_Side side)
{
	if (!manipulator) {
		Widget_Transform::obj.do_render[side] = false;
	}

	static float manip_length[3];
	for (int i = 0; i < 3; ++i) {
		manip_length[i] = manip_scale_factor * 2.0f;
	}
	static float clip_plane[4] = { 0.0f };

	if (omni && manipulator) {
		/* Dial and Gimbal */
		GPU_blend(true);
		GPU_matrix_push();
		GPU_matrix_mul(manip_t.m);
		GPU_polygon_smooth(false);
		if (transform_mode == TRANSFORMMODE_ROTATE) {
			switch (constraint_mode) {
			case VR_UI::CONSTRAINTMODE_ROT_X: {
				GPU_matrix_rotate_axis(-90.0f, 'Y');
				render_dial(PI / 4.0f, manip_angle[transform_space].x, 0.0f, manip_length[0] / 4.0f);
				if (VR_UI::ctrl_key_get()) {
					if (VR_UI::shift_key_get()) {
						render_incremental_angles(PI / 180.0f, 0.0f, manip_length[0] / 4.0f);
					}
					else {
						render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[0] / 4.0f);
					}
				}
				GPU_matrix_rotate_axis(90.0f, 'Y');
				break;
			}
			case VR_UI::CONSTRAINTMODE_ROT_Y: {
				GPU_matrix_rotate_axis(90.0f, 'X');
				render_dial(PI / 4.0f, manip_angle[transform_space].y, 0.0f, manip_length[1] / 4.0f);
				if (VR_UI::ctrl_key_get()) {
					if (VR_UI::shift_key_get()) {
						render_incremental_angles(PI / 180.0f, 0.0f, manip_length[1] / 4.0f);
					}
					else {
						render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[1] / 4.0f);
					}
				}
				GPU_matrix_rotate_axis(-90.0f, 'X');
				break;
			}
			case VR_UI::CONSTRAINTMODE_ROT_Z: {
				GPU_matrix_rotate_axis(-90.0f, 'Z');
				render_dial(-PI / 4.0f, -manip_angle[transform_space].z, 0.0f, manip_length[2] / 4.0f);
				if (VR_UI::ctrl_key_get()) {
					if (VR_UI::shift_key_get()) {
						render_incremental_angles(PI / 180.0f, 0.0f, manip_length[2] / 4.0f);
					}
					else {
						render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[2] / 4.0f);
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
		render_gimbal(manip_length, false, manip_t.m, clip_plane, 3 * PI / 2.0f, 0.0f);
		/* Arrow */
		*((Coord3Df*)manip_length) /= 2.0f;
		render_axes(manip_length, 0);
		/* Box */
		*((Coord3Df*)manip_length) /= 2.0f;
		render_axes(manip_length, 1);
		/* Ball */
		render_axes(manip_length, 2);
		GPU_blend(false);
		GPU_matrix_pop();
		return;
	}

	switch (transform_mode) {
	case TRANSFORMMODE_OMNI: {
		/* Arrow */
		*((Coord3Df*)manip_length) /= 2.0f;
		GPU_matrix_push();
		GPU_matrix_mul(manip_t.m);
		GPU_blend(true);
		render_axes(manip_length, 0);
		GPU_blend(false);
		GPU_matrix_pop();
		break;
	}
	case TRANSFORMMODE_MOVE: {
		/* Plane */
		GPU_matrix_push();
		GPU_matrix_mul(manip_t.m);
		GPU_blend(true);
		render_planes(manip_length);
		/* Arrow */
		*((Coord3Df*)manip_length) /= 2.0f;
		render_axes(manip_length, 0);
		GPU_blend(false);
		GPU_matrix_pop();
		break;
	}
	case TRANSFORMMODE_ROTATE: {
		/* Dial and Gimbal */
		GPU_blend(true);
		GPU_matrix_push();
		GPU_matrix_mul(manip_t.m);
		GPU_polygon_smooth(false);
		switch (constraint_mode) {
		case VR_UI::CONSTRAINTMODE_ROT_X: {
			GPU_matrix_rotate_axis(-90.0f, 'Y');
			render_dial(PI / 4.0f, manip_angle[transform_space].x, 0.0f, manip_length[0] / 4.0f);
			if (VR_UI::ctrl_key_get()) {
				if (VR_UI::shift_key_get()) {
					render_incremental_angles(PI / 180.0f, 0.0f, manip_length[0] / 4.0f);
				}
				else {
					render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[0] / 4.0f);
				}
			}
			GPU_matrix_rotate_axis(90.0f, 'Y');
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_Y: {
			GPU_matrix_rotate_axis(90.0f, 'X');
			render_dial(PI / 4.0f, manip_angle[transform_space].y, 0.0f, manip_length[1] / 4.0f);
			if (VR_UI::ctrl_key_get()) {
				if (VR_UI::shift_key_get()) {
					render_incremental_angles(PI / 180.0f, 0.0f, manip_length[1] / 4.0f);
				}
				else {
					render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[1] / 4.0f);
				}
			}
			GPU_matrix_rotate_axis(-90.0f, 'X');
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_Z: {
			GPU_matrix_rotate_axis(-90.0f, 'Z');
			render_dial(-PI / 4.0f, -manip_angle[transform_space].z, 0.0f, manip_length[2] / 4.0f);
			if (VR_UI::ctrl_key_get()) {
				if (VR_UI::shift_key_get()) {
					render_incremental_angles(PI / 180.0f, 0.0f, manip_length[2] / 4.0f);
				}
				else {
					render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[2] / 4.0f);
				}
			}
			GPU_matrix_rotate_axis(90.0f, 'Z');
			break;
		}
		default: {
			break;
		}
		}
		if (!manipulator) {
			render_gimbal(manip_length, false, manip_t.m, clip_plane, 0.0f, 0.0f);
		}
		else {
			render_gimbal(manip_length, false, manip_t.m, clip_plane, 3 * PI / 2.0f, 0.0f);
		}
		/* Ball */
		*((Coord3Df*)manip_length) /= 4.0f;
		render_axes(manip_length, 2);
		GPU_blend(false);
		GPU_matrix_pop();
		break;
	}
	case TRANSFORMMODE_SCALE: {
		/* Plane */
		GPU_matrix_push();
		GPU_matrix_mul(manip_t.m);
		GPU_blend(true);
		render_planes(manip_length);
		/* Box */
		*((Coord3Df*)manip_length) /= 4.0f;
		render_axes(manip_length, 1);
		/* TODO_XR */
		static float zero[4][4] = { 0 };
		GPU_matrix_mul(zero);
		GPUBatch *sphere = GPU_batch_preset_sphere(0);
		GPU_batch_program_set_builtin(sphere, GPU_SHADER_3D_UNIFORM_COLOR);
		GPU_batch_draw(sphere);
		/**/
		GPU_blend(false);
		GPU_matrix_pop();
		break;
	}
	default: {
		break;
	}
	}
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
//	if (VR_UI::ctrl_key_get() == VR_UI::CTRLSTATE_ON) {
//		eraser = true;
//		cursor_side = c.side;
//		if (gpf) {
//			/* Loop over VR strokes and check if they should be erased. */
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
	if (VR_UI::ctrl_key_get() == VR_UI::CTRLSTATE_ON) {
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
			/* Loop over VR strokes and check if they should be erased. */
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
	if (c.bimanual) {
		VR_UI::Cursor *other = c.other_hand;
		c.bimanual = VR_UI::Cursor::BIMANUAL_OFF;
		other->bimanual = VR_UI::Cursor::BIMANUAL_OFF;
		return; /* calculations are only performed by the second hand */
	}
	
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
	bGPDstroke *gps = BKE_gpencil_add_stroke(gpf[active_layer], 0, tot_points, line_thickness);

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

Coord3Df Widget_Measure::measure_points[3];
bGPDstroke *Widget_Measure::current_stroke(NULL);
bGPDspoint Widget_Measure::current_stroke_points[3];

Widget_Measure::Measure_State Widget_Measure::measure_state(Widget_Measure::Measure_State::INIT);
VR_UI::CtrlState Widget_Measure::measure_ctrl_state(VR_UI::CTRLSTATE_OFF);
int Widget_Measure::measure_ctrl_count(0);

float Widget_Measure::line_thickness(10.0f);
float Widget_Measure::color[4] = { 1.0f, 0.3f, 0.3f, 1.0f };

float Widget_Measure::angle(0.0f);

VR_Side Widget_Measure::cursor_side;

void Widget_Measure::drag_start(VR_UI::Cursor& c)
{
	cursor_side = c.side;
	c.reference = c.position.get();

	memcpy(&measure_points[0], c.position.get(VR_SPACE_BLENDER).m[3], sizeof(float) * 3);
}

void Widget_Measure::drag_contd(VR_UI::Cursor& c)
{
	//if (measure_state == VR_UI::CTRLSTATE_OFF) {
		memcpy(&measure_points[1], c.position.get(VR_SPACE_BLENDER).m[3], sizeof(float) * 3);
	//}
	/*else {
		measure_points[2] = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];
		Coord3Df dir_a = (measure_points[0] - measure_points[1]).normalize();
		Coord3Df dir_b = (measure_points[2] - measure_points[1]).normalize();
		angle = angle_normalized_v3v3((float*)&dir_a, (float*)&dir_b) * (180.0f / PI);
		angle = (float)((int)(angle) % 180);
	}
	if (VR_UI::ctrl_key_get()) {
		if (++measure_ctrl_count == 1) {
			draw_line(c, measure_points[0], measure_points[1]);
		}
		measure_ctrl_state = VR_UI::CTRLSTATE_ON;
	}*/

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Measure::obj.do_render[i] = true;
	}
}

void Widget_Measure::drag_stop(VR_UI::Cursor& c)
{
	//if (measure_ctrl_state == VR_UI::CTRLSTATE_OFF) {
		draw_line(c, measure_points[0], measure_points[1]);
	/*}
	else {
		draw_line(c, measure_points[1], measure_points[2]);
	}*/

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Measure::obj.do_render[i] = false;
	}

	//Coord3Df p = *(Coord3Df*)&current_stroke_points[0];
	//render_GPFont(1, 5, p);

	measure_state = Widget_Measure::Measure_State::INIT;
	measure_ctrl_state = VR_UI::CTRLSTATE_OFF;
	measure_ctrl_count = 0;

	for (int i = 0; i < 3; ++i) {
		memset(&measure_points[i], 0, sizeof(float) * 3);
	}
}

void Widget_Measure::draw_line(VR_UI::Cursor& c, Coord3Df& localP0, Coord3Df& localP1) {
	switch (measure_state) {
	case Widget_Measure::Measure_State::INIT: {
		measure_state = Widget_Measure::Measure_State::DRAW;
		break;
	}
	case Widget_Measure::Measure_State::DRAW: {
		measure_state = Widget_Measure::Measure_State::MEASURE;
		break;
	}
	case Widget_Measure::Measure_State::MEASURE: {
		measure_state = Widget_Measure::Measure_State::DONE;
		break;
	}
	default: {
		break;
	}
	}

	/* Get active drawing layer */
	uint active_layer = Widget_Annotate::num_layers - 1;

	if (measure_state == Widget_Measure::Measure_State::DRAW) {
		bContext *C = vr_get_obj()->ctx;
		Main* curr_main = CTX_data_main(C);
		if (Widget_Annotate::gpl.size() < 1 || Widget_Annotate::main != curr_main) {
			int error = Widget_Annotate::init(Widget_Annotate::main != curr_main ? true : false);
			Widget_Annotate::main = curr_main;
			if (error) {
				return;
			}
		}

		/* Create and parse our previous points into bGPDspoint structures */
		memcpy(&current_stroke_points[0], &localP0, sizeof(float) * 3);
		memcpy(&current_stroke_points[1], &localP1, sizeof(float) * 3);
		memcpy(&current_stroke_points[2], &localP1, sizeof(float) * 3);

		/* Set the pressure and strength for proper display */
		for (int i = 0; i < 3; ++i) {
			current_stroke_points[i].strength = 1.0f;
			current_stroke_points[i].pressure = 1.0f;
		}
	}
	if (measure_state == Widget_Measure::Measure_State::MEASURE) {
		/* Current state is MEASURE. This is our last draw, so add the arc for degree display. */
		memcpy(&current_stroke_points[2], &localP1, sizeof(float) * 3);
	}

	current_stroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 3, line_thickness * 1.6f);
	memcpy(current_stroke->points, current_stroke_points, sizeof(bGPDspoint) * 3);

	memcpy(Widget_Annotate::gpl[active_layer]->color, color, sizeof(float) * 4);
	BKE_gpencil_layer_setactive(Widget_Annotate::gpd, Widget_Annotate::gpl[active_layer]);
}

void Widget_Measure::render_GPFont(const uint num, const uint numPoint, const Coord3Df& o)
{
	uint active_layer = Widget_Annotate::num_layers - 1;

	bContext *C = vr_get_obj()->ctx;
	Main *curr_main = CTX_data_main(C);
	if (Widget_Annotate::gpl.size() < 1 || Widget_Annotate::main != curr_main) {
		int error = Widget_Annotate::init(Widget_Annotate::main != curr_main ? true : false);
		Widget_Annotate::main = curr_main;
		if (error) {
			return;
		}
	}

	bGPDstroke *GPFStroke = NULL;

	/* Based on the number and (o)rigin passed, we fill our stroke with points related to the requested number. */
	switch (num) {
	case 0: {
		static bGPDspoint GPpoints[9];
		GPpoints[0].x = -0.01f;	GPpoints[0].y = +0.01f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = +0.00f;	GPpoints[1].y = +0.02f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = +0.01f;	GPpoints[2].y = +0.02f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = +0.02f;	GPpoints[3].y = +0.01f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = +0.02f;	GPpoints[4].y = -0.01f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;
		GPpoints[5].x = +0.01f;	GPpoints[5].y = -0.02f;	GPpoints[5].z = 0.0f; GPpoints[5].pressure = 1.0f; GPpoints[5].strength = 1.0f;
		GPpoints[6].x = +0.00f;	GPpoints[6].y = -0.02f;	GPpoints[6].z = 0.0f; GPpoints[6].pressure = 1.0f; GPpoints[6].strength = 1.0f;
		GPpoints[7].x = -0.01f;	GPpoints[7].y = -0.01f;	GPpoints[7].z = 0.0f; GPpoints[7].pressure = 1.0f; GPpoints[7].strength = 1.0f;
		GPpoints[8].x = -0.01f;	GPpoints[8].y = +0.01f;	GPpoints[8].z = 0.0f; GPpoints[8].pressure = 1.0f; GPpoints[8].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 9, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 9);
		break;
	}
	case 1: {
		static bGPDspoint GPpoints[5];
		GPpoints[0].x = -0.01f;	GPpoints[0].y = -0.01f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = 0.00f;	GPpoints[1].y = +0.02f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = 0.00f;	GPpoints[2].y = -0.02f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = -0.01f;	GPpoints[3].y = -0.02f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = +0.01f;	GPpoints[4].y = -0.02f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 5, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 5);
		break;
	}
	case 2: {
		static bGPDspoint GPpoints[6];
		GPpoints[0].x = -0.02f;	GPpoints[0].y = +0.01f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = -0.01f;	GPpoints[1].y = +0.02f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = 0.00f;	GPpoints[2].y = +0.02f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = -0.01f;	GPpoints[3].y = +0.01f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = +0.02f;	GPpoints[4].y = -0.02f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;
		GPpoints[5].x = -0.01f;	GPpoints[5].y = -0.02f;	GPpoints[5].z = 0.0f; GPpoints[5].pressure = 1.0f; GPpoints[5].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 6, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 6);
		break;
	}
	case 3: {
		static bGPDspoint GPpoints[9];
		GPpoints[0].x = -0.01f;	GPpoints[0].y = +0.02f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = +0.01f;	GPpoints[1].y = +0.02f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = +0.02f;	GPpoints[2].y = +0.01f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = +0.01f;	GPpoints[3].y = 0.00f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = 0.00f;	GPpoints[4].y = 0.00f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;
		GPpoints[5].x = +0.01f;	GPpoints[5].y = 0.00f;	GPpoints[5].z = 0.0f; GPpoints[5].pressure = 1.0f; GPpoints[5].strength = 1.0f;
		GPpoints[6].x = +0.02f;	GPpoints[6].y = -0.01f;	GPpoints[6].z = 0.0f; GPpoints[6].pressure = 1.0f; GPpoints[6].strength = 1.0f;
		GPpoints[7].x = +0.01f;	GPpoints[7].y = -0.02f;	GPpoints[7].z = 0.0f; GPpoints[7].pressure = 1.0f; GPpoints[7].strength = 1.0f;
		GPpoints[8].x = -0.01f;	GPpoints[8].y = -0.02f;	GPpoints[8].z = 0.0f; GPpoints[8].pressure = 1.0f; GPpoints[8].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 9, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 9);
		break;
	}
	case 4: {
		static bGPDspoint GPpoints[8];
		GPpoints[0].x = -0.02f;	GPpoints[0].y = 0.00f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = 0.00f;	GPpoints[1].y = +0.02f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = +0.01f;	GPpoints[2].y = +0.02f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = +0.01f;	GPpoints[3].y = -0.01f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = +0.01f;	GPpoints[4].y = -0.02f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;
		GPpoints[5].x = +0.01f;	GPpoints[5].y = -0.01f;	GPpoints[5].z = 0.0f; GPpoints[5].pressure = 1.0f; GPpoints[5].strength = 1.0f;
		GPpoints[6].x = -0.02f;	GPpoints[6].y = -0.01f;	GPpoints[6].z = 0.0f; GPpoints[6].pressure = 1.0f; GPpoints[6].strength = 1.0f;
		GPpoints[7].x = -0.02f;	GPpoints[7].y = -0.001;	GPpoints[7].z = 0.0f; GPpoints[7].pressure = 1.0f; GPpoints[7].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 8, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 8);
		break;
	}
	case 5: {
		static bGPDspoint GPpoints[7];
		GPpoints[0].x = +0.02f;	GPpoints[0].y = +0.02f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = -0.01f;	GPpoints[1].y = +0.02f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = -0.01f;	GPpoints[2].y = 0.00f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = +0.02f;	GPpoints[3].y = 0.00f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = +0.02f;	GPpoints[4].y = -0.01f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;
		GPpoints[5].x = +0.01f;	GPpoints[5].y = -0.02f;	GPpoints[5].z = 0.0f; GPpoints[5].pressure = 1.0f; GPpoints[5].strength = 1.0f;
		GPpoints[6].x = -0.01f;	GPpoints[6].y = -0.02f;	GPpoints[6].z = 0.0f; GPpoints[6].pressure = 1.0f; GPpoints[6].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 7, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 7);
		break;
	}
	case 6: {
		static bGPDspoint GPpoints[9];
		GPpoints[0].x = +0.02f;	GPpoints[0].y = +0.02f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = 0.00f;	GPpoints[1].y = +0.02f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = -0.01f;	GPpoints[2].y = +0.01f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = -0.01f;	GPpoints[3].y = -0.01f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = 0.00f;	GPpoints[4].y = -0.02f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;
		GPpoints[5].x = +0.01f;	GPpoints[5].y = -0.02f;	GPpoints[5].z = 0.0f; GPpoints[5].pressure = 1.0f; GPpoints[5].strength = 1.0f;
		GPpoints[6].x = +0.02f;	GPpoints[6].y = -0.01f;	GPpoints[6].z = 0.0f; GPpoints[6].pressure = 1.0f; GPpoints[6].strength = 1.0f;
		GPpoints[7].x = +0.01f;	GPpoints[7].y = 0.00f;	GPpoints[7].z = 0.0f; GPpoints[7].pressure = 1.0f; GPpoints[7].strength = 1.0f;
		GPpoints[8].x = -0.01f;	GPpoints[8].y = 0.00f;	GPpoints[8].z = 0.0f; GPpoints[8].pressure = 1.0f; GPpoints[8].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 9, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 9);
		break;
	}
	case 7: {
		static bGPDspoint GPpoints[5];
		GPpoints[0].x = -0.01f;	GPpoints[0].y = +0.02f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = +0.02f;	GPpoints[1].y = +0.02f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = +0.02f;	GPpoints[2].y = +0.01f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = 0.00f;	GPpoints[3].y = -0.01f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = 0.00f;	GPpoints[4].y = -0.02f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 5, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 5);
		break;
	}
	case 8: {
		static bGPDspoint GPpoints[11];
		GPpoints[0].x = 0.00f;	GPpoints[0].y = 0.00f;	GPpoints[0].z = 0.0f;	GPpoints[0].pressure = 1.0f;	GPpoints[0].strength = 1.0f;
		GPpoints[1].x = -0.01f;	GPpoints[1].y = +0.01f;	GPpoints[1].z = 0.0f;	GPpoints[1].pressure = 1.0f;	GPpoints[1].strength = 1.0f;
		GPpoints[2].x = 0.00f;	GPpoints[2].y = +0.02f;	GPpoints[2].z = 0.0f;	GPpoints[2].pressure = 1.0f;	GPpoints[2].strength = 1.0f;
		GPpoints[3].x = +0.01f;	GPpoints[3].y = +0.02f;	GPpoints[3].z = 0.0f;	GPpoints[3].pressure = 1.0f;	GPpoints[3].strength = 1.0f;
		GPpoints[4].x = +0.02f;	GPpoints[4].y = +0.01f;	GPpoints[4].z = 0.0f;	GPpoints[4].pressure = 1.0f;	GPpoints[4].strength = 1.0f;
		GPpoints[5].x = +0.01f;	GPpoints[5].y = 0.00f;	GPpoints[5].z = 0.0f;	GPpoints[5].pressure = 1.0f;	GPpoints[5].strength = 1.0f;
		GPpoints[6].x = +0.02f;	GPpoints[6].y = -0.01f;	GPpoints[6].z = 0.0f;	GPpoints[6].pressure = 1.0f;	GPpoints[6].strength = 1.0f;
		GPpoints[7].x = +0.01f;	GPpoints[7].y = -0.02f;	GPpoints[7].z = 0.0f;	GPpoints[7].pressure = 1.0f;	GPpoints[7].strength = 1.0f;
		GPpoints[8].x = 0.00f;	GPpoints[8].y = -0.02f;	GPpoints[8].z = 0.0f;	GPpoints[8].pressure = 1.0f;	GPpoints[8].strength = 1.0f;
		GPpoints[9].x = -0.01f;	GPpoints[9].y = -0.01f;	GPpoints[9].z = 0.0f;	GPpoints[9].pressure = 1.0f;	GPpoints[9].strength = 1.0f;
		GPpoints[10].x = 0.00f;	GPpoints[10].y = 0.00f;	GPpoints[10].z = 0.0f;	GPpoints[10].pressure = 1.0f;	GPpoints[10].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 11, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 11);
		break;
	}
	case 9: {
		static bGPDspoint GPpoints[9];
		GPpoints[0].x = +0.01f;	GPpoints[0].y = 0.00f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = -0.01f;	GPpoints[1].y = 0.00f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = -0.02f;	GPpoints[2].y = +0.01f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = -0.01f;	GPpoints[3].y = +0.02f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = 0.00f;	GPpoints[4].y = +0.02f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;
		GPpoints[5].x = +0.01f;	GPpoints[5].y = +0.01f;	GPpoints[5].z = 0.0f; GPpoints[5].pressure = 1.0f; GPpoints[5].strength = 1.0f;
		GPpoints[6].x = +0.01f;	GPpoints[6].y = -0.01f;	GPpoints[6].z = 0.0f; GPpoints[6].pressure = 1.0f; GPpoints[6].strength = 1.0f;
		GPpoints[7].x = 0.00f;	GPpoints[7].y = -0.02f;	GPpoints[7].z = 0.0f; GPpoints[7].pressure = 1.0f; GPpoints[7].strength = 1.0f;
		GPpoints[8].x = -0.02f;	GPpoints[8].y = -0.02f;	GPpoints[8].z = 0.0f; GPpoints[8].pressure = 1.0f; GPpoints[8].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 9, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 9);
		break;
	}
	default: {
		return;
	}
	}

	/*static Mat44f hmd;
	hmd = VR_UI::hmd_position_get(VR_SPACE_REAL);
	memset(hmd.m[3], 0, sizeof(float) * 3);
	static Mat44f hmd_temp;
	hmd_temp = hmd;
	*(Coord3Df*)hmd.m[1] = *(Coord3Df*)hmd_temp.m[2];
	*(Coord3Df*)hmd.m[2] = *(Coord3Df*)hmd_temp.m[1];*/

	//static Mat44f hmd2;
	//hmd2 = VR_UI::hmd_position_get(VR_SPACE_BLENDER);
	//memset(hmd2.m[3], 0, sizeof(float) * 3);

	/* Rotation */
	//static Coord3Df temp;
	//for (int p = 0; p < numPoint; ++p) {
	//	Coord3Df &curSP = *(Coord3Df*)&GPFStroke->points[p];
	//	/* Rotate numbers to point upright */
	//	temp = curSP;
	//	VR_Math::multiply_mat44_coord3D(curSP, rot_matrix_x, temp);
	//	/* Rotate numbers to match hmd local rotation */
	//	temp = curSP;
	//	VR_Math::multiply_mat44_coord3D(curSP, hmd, temp);
	//	/* TODO: Rotate numbers around world axis to face hmd */
	//	//temp = curSP;
	//	//VR_Math::multiply_mat44_coord3D(curSP, hmd2, temp);
	//}

	//static float temp[4] = { 0, 0, 0, 1.0f };
	//for (int p = 0; p < numPoint; p++)
	//{
	//	Coord3Df hmdX = { hmd.m[0][0], hmd.m[0][1], hmd.m[0][2] };
	//	Coord3Df hmdY = { hmd.m[1][0], hmd.m[1][1], hmd.m[1][2] };
	//	Coord3Df hmdZ = { hmd.m[2][0], hmd.m[2][1], hmd.m[2][2] };
	//	Coord3Df spaceorigin = { 0.0f, 0.0f, 0.0f };
	//	memcpy(temp, &GPFStroke->points[p], sizeof(float) * 3);
	//	hmdX *= temp[0];
	//	hmdY *= temp[1];
	//	hmdZ *= temp[2];
	//	spaceorigin += hmdX + hmdY + hmdZ;

	//	memcpy(&GPFStroke->points[p], &spaceorigin, sizeof(float) * 3);
	//}

	/* Translation */
	for (int p = 0; p < numPoint; p++)
	{
		bGPDspoint &curSP = GPFStroke->points[p];
		curSP.x += o.x;
		curSP.y += o.y;
		curSP.z += o.z;
	}

	if (!GPFStroke) {
		return;
	}

	memcpy(Widget_Annotate::gpl[active_layer]->color, color, sizeof(float) * 4);
	BKE_gpencil_layer_setactive(Widget_Annotate::gpd, Widget_Annotate::gpl[active_layer]);
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

	VR_Draw::set_depth_test(false, false);
	VR_Draw::set_color(0.8f, 0.8f, 0.8f, 1.0f);
	static std::string distance, degrees;
	//if (measure_ctrl_state == VR_UI::CTRLSTATE_ON) {
		/* Angle measurement */
		//sprintf((char*)degrees.data(), "%.f", angle);
		//VR_Draw::render_string(degrees.c_str(), 0.02f, 0.02f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.08f, 0.001f);
	//}
	//else {
		/* Line measurement */
		sprintf((char*)distance.data(), "%.3f", (measure_points[1] - measure_points[0]).length());
		VR_Draw::render_string(distance.c_str(), 0.02f, 0.02f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.08f, 0.001f);
	//}
	VR_Draw::set_depth_test(true, true);
	VR_Draw::update_modelview_matrix(&prior_model_matrix, 0);

	/* Instead of working with multiple points that make up a whole line, we work with just p0/p1. */
	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

	GPU_line_width(10.0f);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	//immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);
	if (measure_ctrl_state == VR_UI::CTRLSTATE_OFF && measure_state == Widget_Measure::Measure_State::INIT) {
		immBeginAtMost(GPU_PRIM_LINES, 2);
		immUniformColor3fvAlpha(color, color[3]);
		//immUniform1f("dash_width", 6.0f);

		immVertex3fv(pos, (float*)&measure_points[0]);
		immVertex3fv(pos, (float*)&measure_points[1]);

		if (measure_points[0] == measure_points[1]) {
			/* cyclic */
			immVertex3fv(pos, (float*)&measure_points[0]);
		}
		immEnd();
	}
	//else {
	//	immBeginAtMost(GPU_PRIM_LINES, 2);
	//	immUniformColor3fvAlpha(color, color[3]);
	//	immVertex3fv(pos, (float*)&measure_points[1]);
	//	immVertex3fv(pos, (float*)&measure_points[2]);

	//	if (measure_points[1] == measure_points[2]) {
	//		/* cyclic */
	//		immVertex3fv(pos, (float*)&measure_points[2]);
	//	}
	//	immEnd();
	//	immUnbindProgram();

	//	static Mat44f m_circle = VR_Math::identity_f;
	//	static float temp[3][3];
	//	/* Set arc rotation and position. */
	//	Coord3Df dir_a = (measure_points[0] - measure_points[1]).normalize();
	//	Coord3Df dir_b = (measure_points[2] - measure_points[1]).normalize();
	//	rotation_between_vecs_to_mat3(temp, (float*)&dir_a, (float*)&dir_b);
	//	for (int i = 0; i < 3; ++i) {
	//		memcpy(m_circle.m[i], temp[i], sizeof(float) * 3);
	//	}
	//	*(Coord3Df*)m_circle.m[3] = measure_points[1];

	//	GPU_matrix_push();
	//	GPU_matrix_mul(m_circle.m);
	//	GPU_blend(true);

	//	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	//	immUniformColor3fvAlpha(color, color[3]);
	//	int nsegments = 100;
	//	float angle_start = PI;// 0.0f;
	//	float angle_end = PI - (DEG2RADF(angle));
	//	float rad = ((measure_points[0] - measure_points[1]) / 2.0f).length();
	//	immBegin(GPU_PRIM_LINE_STRIP, nsegments);
	//	static Coord3Df p(0.0f, 0.0f, 0.0f);
	//	for (int i = 0; i < nsegments; ++i) {
	//		const float angle_in = interpf(angle_start, angle_end, ((float)i / (float)(nsegments - 1)));
	//		const float angle_sin = sinf(angle_in);
	//		const float angle_cos = cosf(angle_in);
	//		p.x = rad * angle_cos;
	//		p.y = rad * angle_sin;
	//		immVertex3fv(pos, (float*)&p);
	//	}
	//	immEnd();
	//	immUnbindProgram();

	//	GPU_blend(false);
	//	GPU_matrix_pop();
	//}

	Widget_Measure::obj.do_render[side] = false;
}

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

/* Dummy op to pass to EDBM_op_init() and EDBM_op_finish() */
static wmOperator dummy_op;

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
		em, &bmop, &dummy_op,
		"extrude_vert_indiv verts=%hv use_select_history=%b",
		hflag, true);

	/* deselect original verts */
	BMO_slot_buffer_hflag_disable(em->bm, bmop.slots_in, "verts", BM_VERT, BM_ELEM_SELECT, true);

	BMO_op_exec(em->bm, &bmop);
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "verts.out", BM_VERT, BM_ELEM_SELECT, true);

	if (!EDBM_op_finish(em, &bmop, &dummy_op, true)) {
		return false;
	}

	return true;
}

static bool edbm_extrude_edges_indiv(BMEditMesh *em, const char hflag, const bool use_normal_flip)
{
	BMesh *bm = em->bm;
	BMOperator bmop;

	EDBM_op_init(
		em, &bmop, &dummy_op,
		"extrude_edge_only edges=%he use_normal_flip=%b use_select_history=%b",
		hflag, use_normal_flip, true);

	/* deselect original verts */
	BM_SELECT_HISTORY_BACKUP(bm);
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	BM_SELECT_HISTORY_RESTORE(bm);

	BMO_op_exec(em->bm, &bmop);
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "geom.out", BM_VERT | BM_EDGE, BM_ELEM_SELECT, true);

	if (!EDBM_op_finish(em, &bmop, &dummy_op, true)) {
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
		em, &bmop, &dummy_op,
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

	if (!EDBM_op_finish(em, &bmop, &dummy_op, true)) {
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
		raycast_select_single_edit(*(Coord3Df*)m.m[3], VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
	}
	else {
		for (int i = 0; i < VR_SIDES; ++i) {
			Widget_Extrude::obj.do_render[i] = false;
		}
		return;
	}
	/* Update manipulator transform. */
	Widget_Transform::manipulator = true;
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

	Widget_Transform::omni = true;
	Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_OMNI;
	Widget_Transform::snap_mode = VR_UI::SNAPMODE_TRANSLATION;

	/* Test for manipulator selection and set constraints. */
	const Mat44f& m = c.position.get();
	extrude = false;
	Widget_Transform::raycast_select_manipulator(*(Coord3Df*)m.m[3], &extrude);
	if (extrude) {
		/* Manipulator extrude region was hit, do extrude operation. */
		switch (extrude_mode) {
		case EXTRUDEMODE_NORMALS: {
			/* TODO_XR */
			//edbm_extrude_indiv_exec(C, flip_normals);
			break;
		}
		case EXTRUDEMODE_INDIVIDUAL: {
			edbm_extrude_indiv_exec(C, flip_normals);
			break;
		}
		case EXTRUDEMODE_REGION: 
		default: {
			edbm_extrude_indiv_exec(C, flip_normals);
			//edbm_extrude_region_exec(C, flip_normals);
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
			project_v3_v3v3(delta.m[3], delta_orig.m[3], Widget_Transform::manip_t_orig.m[0]);
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_Y: {
			project_v3_v3v3(delta.m[3], delta_orig.m[3], Widget_Transform::manip_t_orig.m[1]);
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_Z: {
			project_v3_v3v3(delta.m[3], delta_orig.m[3], Widget_Transform::manip_t_orig.m[2]);
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
				project_v3_v3v3(&temp2.x, &temp1.x, Widget_Transform::manip_t_orig.m[0]);
				*(Coord3Df*)Widget_Transform::manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_Y: {
				temp1 = *(Coord3Df*)Widget_Transform::manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
				project_v3_v3v3(&temp2.x, &temp1.x, Widget_Transform::manip_t_orig.m[1]);
				*(Coord3Df*)Widget_Transform::manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_Z: {
				temp1 = *(Coord3Df*)Widget_Transform::manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
				project_v3_v3v3(&temp2.x, &temp1.x, Widget_Transform::manip_t_orig.m[2]);
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
		BMIter iter;

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

		/* Extrude mode */
		BMIter iter;
		switch (extrude_mode) {
		case EXTRUDEMODE_NORMALS: {
			/* TODO_XR */
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
						(*(Coord3Df*)delta.m[3]) *= mag;
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

	/* TODO_XR: Avoid doing this twice (already done in drag_start() */
	Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_NONE;
	memset(Widget_Transform::constraint_flag, 0, sizeof(int) * 3);
	memset(Widget_Transform::snap_flag, 1, sizeof(int) * 3);

	Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_OMNI;
	Widget_Transform::snap_mode = VR_UI::SNAPMODE_TRANSLATION;

	Widget_Transform::is_dragging = false;
	extrude = false;

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
	ED_undo_push(C, "Transform");
}

void Widget_Extrude::render(VR_Side side) 
{
	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		Widget_Extrude::obj.do_render[side] = false;
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
		GPU_blend(false);
		GPU_matrix_pop();
		break;
	}
	default: {
		break;
	}
	}
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
			DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
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

			DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
			WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
			WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);
		}
	}
	ED_undo_push(C, "Delete");

	return 0;
}

/* Note, these values must match delete_mesh() event values */
enum {
	MESH_DELETE_VERT = 0,
	MESH_DELETE_EDGE = 1,
	MESH_DELETE_FACE = 2,
	MESH_DELETE_EDGE_FACE = 3,
	MESH_DELETE_ONLY_FACE = 4,
};

/* From editmesh_tools.c */
static int edbm_delete_exec(bContext *C)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	uint objects_len = 0;
	static ObjectsInModeParams params = { OB_MODE_EDIT, true };
	Object **objects = BKE_view_layer_array_from_objects_in_mode_params(view_layer, CTX_wm_view3d(C), &objects_len, &params);
	bool changed_multi = false;

	ToolSettings *ts = CTX_data_scene(C)->toolsettings;
	int type;
	/* TODO_XR: Multi-select mode. */
	switch (ts->selectmode) {
	case SCE_SELECT_VERTEX: {
		type = MESH_DELETE_VERT;
		break;
	}
	case SCE_SELECT_EDGE: {
		type = MESH_DELETE_EDGE;
		break;
	}
	case SCE_SELECT_FACE: {
		type = MESH_DELETE_FACE;
		break;
	}
	default: {
		break;
	}
	}

	for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
		Object *obedit = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(obedit);

		switch (type) {
		case MESH_DELETE_VERT: /* Erase Vertices */
			if (!(em->bm->totvertsel &&
				EDBM_op_callf(em, &dummy_op, "delete geom=%hv context=%i", BM_ELEM_SELECT, DEL_VERTS)))
			{
				continue;
			}
			break;
		case MESH_DELETE_EDGE: /* Erase Edges */
			if (!(em->bm->totedgesel &&
				EDBM_op_callf(em, &dummy_op, "delete geom=%he context=%i", BM_ELEM_SELECT, DEL_EDGES)))
			{
				continue;
			}
			break;
		case MESH_DELETE_FACE: /* Erase Faces */
			if (!(em->bm->totfacesel &&
				EDBM_op_callf(em, &dummy_op, "delete geom=%hf context=%i", BM_ELEM_SELECT, DEL_FACES)))
			{
				continue;
			}
			break;
		case MESH_DELETE_EDGE_FACE:
			/* Edges and Faces */
			if (!((em->bm->totedgesel || em->bm->totfacesel) &&
				EDBM_op_callf(em, &dummy_op, "delete geom=%hef context=%i", BM_ELEM_SELECT, DEL_EDGESFACES)))
			{
				continue;
			}
			break;
		case MESH_DELETE_ONLY_FACE:
			/* Only faces. */
			if (!(em->bm->totfacesel &&
				EDBM_op_callf(em, &dummy_op, "delete geom=%hf context=%i", BM_ELEM_SELECT, DEL_ONLYFACES)))
			{
				continue;
			}
			break;
		default:
			BLI_assert(0);
			break;
		}

		changed_multi = true;

		EDBM_flag_disable_all(em, BM_ELEM_SELECT);

		EDBM_update_generic(em, true, true);
	}

	MEM_freeN(objects);
	if (changed_multi) {
		ED_undo_push(C, "Delete");
	}
	return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

bool Widget_Delete::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Delete::click(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (obedit) {
		edbm_delete_exec(C);
	}
	else {
		delete_selected_objects();
	}

	/* Update manipulators */
	Widget_Transform::update_manipulator();
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
		DEG_id_tag_update(&obn->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

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
			DEG_id_tag_update(&obn->id, ID_RECALC_GEOMETRY);
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
	DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE | ID_RECALC_SELECT);

	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	ED_undo_push(C, "Duplicate");

	return 0;
}

/* From editmesh_tools.c */
static int edbm_duplicate_exec(bContext *C, wmOperator *op)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	uint objects_len = 0;
	static ObjectsInModeParams params = { OB_MODE_EDIT, true };
	Object **objects = BKE_view_layer_array_from_objects_in_mode_params(view_layer, CTX_wm_view3d(C), &objects_len, &params);

	for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
		Object *obedit = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		if (em->bm->totvertsel == 0) {
			continue;
		}

		BMOperator bmop;
		BMesh *bm = em->bm;

		EDBM_op_init(
			em, &bmop, op,
			"duplicate geom=%hvef use_select_history=%b",
			BM_ELEM_SELECT, true);

		BMO_op_exec(bm, &bmop);

		/* de-select all would clear otherwise */
		BM_SELECT_HISTORY_BACKUP(bm);

		EDBM_flag_disable_all(em, BM_ELEM_SELECT);

		BMO_slot_buffer_hflag_enable(bm, bmop.slots_out, "geom.out", BM_ALL_NOLOOP, BM_ELEM_SELECT, true);

		/* rebuild editselection */
		BM_SELECT_HISTORY_RESTORE(bm);

		if (!EDBM_op_finish(em, &bmop, op, true)) {
			continue;
		}
		EDBM_update_generic(em, true, true);
	}
	MEM_freeN(objects);
	ED_undo_push(C, "Duplicate");

	return OPERATOR_FINISHED;
}

bool Widget_Duplicate::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Duplicate::click(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (obedit) {
		edbm_duplicate_exec(C, &dummy_op);
	}
	else {
		duplicate_selected_objects();
	}

	/* Update manipulators */
	Widget_Transform::update_manipulator();
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
 * \class                               Widget_SwitchLayout
 ***************************************************************************************************
 * Interaction widget for switching the currently active layout.
 *
 **************************************************************************************************/
Widget_SwitchLayout Widget_SwitchLayout::obj;

bool Widget_SwitchLayout::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_SwitchLayout::click(VR_UI::Cursor& c)
{
	if (Widget_Transform::is_dragging) {
		/* Don't switch layouts if object data is currently being modified. */
		return;
	}

	/* Toggle object / edit mode. */
	bContext *C = vr_get_obj()->ctx;
	if (CTX_data_edit_object(C)) {
		ED_object_editmode_exit(C, EM_FREEDATA);
		/* Set transform space to normal by default. */
		Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_NORMAL;
	}
	else {
		ED_object_editmode_enter(C, EM_NO_CONTEXT);
		/* Set transform space to local by default. */
		Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_LOCAL;
	}

	/* Update manipulators. */
	Widget_Transform::update_manipulator();
}

bool Widget_SwitchLayout::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_SwitchLayout::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
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
	if (CTX_data_edit_object(vr_get_obj()->ctx)) {
		VR_Draw::render_rect(-0.008f, 0.008f, 0.008f, -0.008f, 0.001f, 1.0f, 1.0f, VR_Draw::editmode_tex);
	}
	else { 
		VR_Draw::render_rect(-0.008f, 0.008f, 0.008f, -0.008f, 0.001f, 1.0f, 1.0f, VR_Draw::objectmode_tex);
	}
}

/***********************************************************************************************//**
 * \class                               Widget_SwitchComponent
 ***************************************************************************************************
 * Interaction widget for switching the currently active component mode.
 *
 **************************************************************************************************/
Widget_SwitchComponent Widget_SwitchComponent::obj;

short Widget_SwitchComponent::mode(SCE_SELECT_VERTEX);

bool Widget_SwitchComponent::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_SwitchComponent::click(VR_UI::Cursor& c)
{
	if (Widget_Transform::is_dragging) {
		/* Don't switch component modes if object data is currently being modified. */
		return;
	}

	bContext *C = vr_get_obj()->ctx;
	ToolSettings *ts = CTX_data_scene(C)->toolsettings;
	short& select_mode = ts->selectmode;

	/* Cycle through component modes. */
	if (CTX_data_edit_object(C)) {
		if (select_mode == SCE_SELECT_VERTEX) {
			select_mode = SCE_SELECT_EDGE;
		}
		else if (select_mode == SCE_SELECT_EDGE) {
			select_mode = SCE_SELECT_FACE;
		}
		else if (select_mode == SCE_SELECT_FACE) {
			/* Exit edit mode */
			select_mode = SCE_SELECT_VERTEX;
			/* Execute operation and update manipulator on post-render */
			VR_UI::editmode_exit = true;
			//ED_object_editmode_exit(C, EM_FREEDATA);
			/* Set transform space to local by default. */
			Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_LOCAL;
			mode = select_mode;
			return;
		}
		else { /* Multi mode */
			/* TODO_XR */
		}
	}
	else {
		/* Enter object mode */
		ED_object_editmode_enter(C, EM_NO_CONTEXT);
		/* Set transform space to local by default. */
		Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_NORMAL;
		select_mode = SCE_SELECT_VERTEX;
	}

	mode = select_mode;
	/* Update manipulators. */
	Widget_Transform::update_manipulator();
}

bool Widget_SwitchComponent::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_SwitchComponent::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
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
	
	bContext *C = vr_get_obj()->ctx;
	if (CTX_data_edit_object(C)) {
		switch (mode) {
		case SCE_SELECT_VERTEX: {
			VR_Draw::render_rect(-0.008f, 0.008f, 0.008f, -0.008f, 0.001f, 1.0f, 1.0f, VR_Draw::vertex_tex);
			break;
		}
		case SCE_SELECT_EDGE: {
			VR_Draw::render_rect(-0.008f, 0.008f, 0.008f, -0.008f, 0.001f, 1.0f, 1.0f, VR_Draw::edge_tex);
			break;
		}
		case SCE_SELECT_FACE: {
			VR_Draw::render_rect(-0.008f, 0.008f, 0.008f, -0.008f, 0.001f, 1.0f, 1.0f, VR_Draw::face_tex);
			break;
		}
		default: {
			//
			break;
		}
		}
	}
	else {
		VR_Draw::render_rect(-0.008f, 0.008f, 0.008f, -0.008f, 0.001f, 1.0f, 1.0f, VR_Draw::object_tex);
	}
}

/***********************************************************************************************//**
 * \class                               Widget_SwitchSpace
 ***************************************************************************************************
 * Interaction widget for switching the currently active transform space.
 *
 **************************************************************************************************/
Widget_SwitchSpace Widget_SwitchSpace::obj;

bool Widget_SwitchSpace::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_SwitchSpace::click(VR_UI::Cursor& c)
{
	/* Cycle through transform spaces. */
	bContext *C = vr_get_obj()->ctx;
	if (CTX_data_edit_object(C)) {
		/* Edit mode */
		switch (Widget_Transform::transform_space) {
		case VR_UI::TRANSFORMSPACE_NORMAL: {
			Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_GLOBAL;
			break;
		}
		case VR_UI::TRANSFORMSPACE_GLOBAL: {
			Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_LOCAL;
			break;
		}
		case VR_UI::TRANSFORMSPACE_LOCAL:
		default: {
			Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_NORMAL;
			break;
		}
		}
	}
	else {
		/* Object mode */
		switch (Widget_Transform::transform_space) {
		case VR_UI::TRANSFORMSPACE_LOCAL: {
			Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_GLOBAL;
			break;
		}
		case VR_UI::TRANSFORMSPACE_GLOBAL:
		default: {
			Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_LOCAL;
			break;
		}
		}
	}

	/* Update manipulators. */
	Widget_Transform::update_manipulator();
}

bool Widget_SwitchSpace::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_SwitchSpace::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
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

	switch (Widget_Transform::transform_space) {
	case VR_UI::TRANSFORMSPACE_NORMAL: {
		VR_Draw::render_rect(-0.008f, 0.008f, 0.008f, -0.008f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_normal_tex);
		break;
	}
	case VR_UI::TRANSFORMSPACE_LOCAL: {
		VR_Draw::render_rect(-0.008f, 0.008f, 0.008f, -0.008f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_local_tex);
		break;
	}
	case VR_UI::TRANSFORMSPACE_GLOBAL:
	default: {
		VR_Draw::render_rect(-0.008f, 0.008f, 0.008f, -0.008f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_global_tex);
		break;
	}
	}
}

/***********************************************************************************************//**
 * \class                               Widget_SwitchTool
 ***************************************************************************************************
 * Interaction widget for switching the currently active tool.
 *
 **************************************************************************************************/
Widget_SwitchTool Widget_SwitchTool::obj;

VR_Widget *Widget_SwitchTool::curr_tool[VR_SIDES] = { &Widget_Select::obj, &Widget_Transform::obj };

bool Widget_SwitchTool::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_SwitchTool::click(VR_UI::Cursor& c)
{
	Widget_Menu::obj.menu_type[c.side] = MENUTYPE_SWITCHTOOL;
	VR_UI::pie_menu_active[c.side] = true;
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

	switch (((VR_Widget*)curr_tool[controller_side])->type()) {
		case TYPE_SELECT: {
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::select_tex);
			break;
		}
		case TYPE_TRANSFORM: {
			switch (Widget_Transform::transform_mode) {
				case Widget_Transform::TRANSFORMMODE_OMNI: {
					VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::transform_tex);
					break;
				}
				case Widget_Transform::TRANSFORMMODE_MOVE: {
					VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::move_tex);
					break;
				}
				case Widget_Transform::TRANSFORMMODE_ROTATE: {
					VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::rotate_tex);
					break;
				}
				case Widget_Transform::TRANSFORMMODE_SCALE: {
					VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::scale_tex);
					break;
				}
				default: {
					break;
				}
			}
			break;
		}
		case TYPE_ANNOTATE: {
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::annotate_tex);
			break;
		}
		case TYPE_MEASURE: {
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::measure_tex);
			break;
		}
		case TYPE_EXTRUDE: {
			switch (Widget_Extrude::extrude_mode) {
			case Widget_Extrude::EXTRUDEMODE_REGION: {
				VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::extrude_tex);
				break;
			}
			case Widget_Extrude::EXTRUDEMODE_INDIVIDUAL: {
				VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::extrude_individual_tex);
				break;
			}
			case Widget_Extrude::EXTRUDEMODE_NORMALS: {
				VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::extrude_normals_tex);
				break;
			}
			default: {
				break;
			}
			}
			break;
		}
		default: {
			break;
		}
	}
}

/***********************************************************************************************//**
 * \class									Widget_Menu
 ***************************************************************************************************
 * Interaction widget for a VR pie menu.
 *
 **************************************************************************************************/
Widget_Menu Widget_Menu::obj;

std::vector<VR_Widget*> Widget_Menu::items[VR_SIDES];
uint Widget_Menu::num_items[VR_SIDES] = { 0 };
uint Widget_Menu::depth[VR_SIDES] = { 0 };

Coord2Df Widget_Menu::stick[VR_SIDES] = { Coord2Df(0.0f, 0.0f), Coord2Df(0.0f, 0.0f) };
float Widget_Menu::angle[VR_SIDES] = { PI, PI };
int Widget_Menu::highlight_index[VR_SIDES] = {-1,-1};

VR_Widget::MenuType Widget_Menu::menu_type[VR_SIDES] = { MENUTYPE_TS_SELECT, MENUTYPE_TS_TRANSFORM };
bool Widget_Menu::action_settings[VR_SIDES] = { false };

/* Highlight colors */
static const float c_menu_white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static const float c_menu_red[4] = { 0.926f, 0.337f, 0.337f, 1.0f };
static const float c_menu_green[4] = { 0.337f, 0.926f, 0.337f, 1.0f };
static const float c_menu_blue[4] = { 0.337f, 0.502f, 0.761f, 1.0f };

/* Colorwheel colors */
static const float c_wheel[11][4] = { 
	{ 0.95f, 0.95f, 0.95f,1.0f },
	{ 0.05f, 0.05f, 0.05f,1.0f },
	{ 0.6f,  0.2f,  1.0f, 1.0f },
	{ 0.72f, 0.46f, 1.0f, 1.0f },
	{ 0.2f,  0.6f,  1.0f, 1.0f },
	{ 0.2f,  1.0f,  1.0f, 1.0f },
	{ 0.6f,  1.0f,  0.2f, 1.0f },
	{ 0.4f,  0.8,   0.2f, 1.0f },
	{ 1.0f,  1.0f,  0.2f, 1.0f },
	{ 1.0f,  0.6f,  0.2f, 1.0f },
	{ 1.0f,  0.2f,  0.2f, 1.0f }
};

/* Icon positions (8 items) */
static const Coord3Df p8_stick(0.0f, 0.0f, 0.001f);
static const Coord3Df p8_0(0.0f, 0.06f, 0.0f);
static const Coord3Df p8_1(-0.06f, 0.0f, 0.0f);
static const Coord3Df p8_2(0.06f, 0.0f, 0.0f);
static const Coord3Df p8_3(-0.043f, 0.043f, 0.0f);
static const Coord3Df p8_4(0.043f, 0.043f, 0.0f);
static const Coord3Df p8_5(-0.043f, -0.043f, 0.0f);
static const Coord3Df p8_6(0.043f, -0.043f, 0.0f);
static const Coord3Df p8_7(0.0f, -0.06f, 0.0f);
/* Icon positions (12 items) */
static const Coord3Df p12_stick(0.0f, 0.0f, 0.001f);
static const Coord3Df p12_0(0.0f, 0.065f, 0.0f);
static const Coord3Df p12_1(-0.065f, 0.0f, 0.0f);
static const Coord3Df p12_2(0.065f, 0.0f, 0.0f);
static const Coord3Df p12_3(-0.03f, 0.06f, 0.0f);
static const Coord3Df p12_4(0.03f, 0.06f, 0.0f);
static const Coord3Df p12_5(-0.06f, 0.03f, 0.0f);
static const Coord3Df p12_6(0.06f, 0.03f, 0.0f);
static const Coord3Df p12_7(-0.06f, -0.03, 0.0f);
static const Coord3Df p12_8(0.06f, -0.03f, 0.0f);
static const Coord3Df p12_9(-0.03f, -0.06, 0.0f);
static const Coord3Df p12_10(0.03f, -0.06, 0.0f);
static const Coord3Df p12_11(0.0f, -0.065f, 0.0f);
/* Icon positions (action settings) */
static const Coord3Df p_as_stick(0.0f, 0.0f, 0.0f);
static const Coord3Df p_as_0(0.0f, 0.02f, 0.0f);
static const Coord3Df p_as_1(-0.02f, 0.0f, 0.0f);
static const Coord3Df p_as_2(0.02f, 0.0f, 0.0f);
static const Coord3Df p_as_3(-0.012f, 0.012f, 0.0f);
static const Coord3Df p_as_4(0.012f, 0.012f, 0.0f);
static const Coord3Df p_as_5(-0.012f, -0.012f, 0.0f);
static const Coord3Df p_as_6(0.012f, -0.012f, 0.0f);
static const Coord3Df p_as_7(0.0f, -0.02f, 0.0f);

void Widget_Menu::stick_center_click(VR_UI::Cursor& c)
{
	switch (menu_type[c.side]) {
		case MENUTYPE_AS_SELECT: {
			VR_UI::mouse_cursor_enabled = !VR_UI::mouse_cursor_enabled;
			break;
		}
		case MENUTYPE_AS_TRANSFORM: 
		case MENUTYPE_AS_EXTRUDE: {
			bContext *C = vr_get_obj()->ctx;
			Object *obedit = CTX_data_edit_object(C);
			if (obedit) {
				switch (Widget_Transform::transform_space) {
				case VR_UI::TRANSFORMSPACE_NORMAL: {
					Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_GLOBAL;
					break;
				}
				case VR_UI::TRANSFORMSPACE_GLOBAL: {
					Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_LOCAL;
					break;
				}
				case VR_UI::TRANSFORMSPACE_LOCAL:
				default: {
					Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_NORMAL;
					break;
				}
				}
			}
			else {
				if (Widget_Transform::transform_space == VR_UI::TRANSFORMSPACE_LOCAL) {
					Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_GLOBAL;
				}
				else {
					Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_LOCAL;
				}
			}
			break;
		}
		default: {
			break;
		}
	}
}

bool Widget_Menu::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Menu::click(VR_UI::Cursor& c)
{
	VR_Widget *tool = VR_UI::get_current_tool(c.side);
	if (!tool) {
		menu_type[c.side] = MENUTYPE_MAIN_12;
		return;
	}
	switch (tool->type()) {
	case TYPE_SELECT: {
		menu_type[c.side] = MENUTYPE_TS_SELECT;
		/* Toggle raycast/proximity selection. */
		VR_UI::SelectionMode& mode = VR_UI::selection_mode;
		if (mode == VR_UI::SELECTIONMODE_RAYCAST) {
			mode = VR_UI::SELECTIONMODE_PROXIMITY;
		}
		else {
			mode = VR_UI::SELECTIONMODE_RAYCAST;
		}
		return; 
	}
	case TYPE_TRANSFORM: {
		menu_type[c.side] = MENUTYPE_TS_TRANSFORM;
		break;
	}
	case TYPE_ANNOTATE: {
		menu_type[c.side] = MENUTYPE_TS_ANNOTATE;
		break;
	}
	case TYPE_MEASURE: {
		menu_type[c.side] = MENUTYPE_TS_MEASURE;
		return;
	}
	case TYPE_EXTRUDE: {
		menu_type[c.side] = MENUTYPE_TS_EXTRUDE;
		break;
	}
	default: {
		menu_type[c.side] = MENUTYPE_MAIN_12;
		break;
	}
	}
		
	VR_UI::pie_menu_active[c.side] = true;
}

bool Widget_Menu::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_Menu::drag_start(VR_UI::Cursor& c)
{
	if (!VR_UI::pie_menu_active[c.side] || menu_type[c.side] == MENUTYPE_TS_SELECT) {
		return;
	}

	if (menu_type[c.side] != MENUTYPE_SWITCHTOOL) {
		if (!action_settings[c.side] && depth[c.side] == 0) {
			VR_Widget *tool = VR_UI::get_current_tool(c.side);
			if (!tool) {
				menu_type[c.side] = MENUTYPE_MAIN_12;
				return;
			}
			switch (tool->type()) {
			case TYPE_SELECT: {
				menu_type[c.side] = MENUTYPE_TS_SELECT;
				return;
			}
			case TYPE_TRANSFORM: {
				menu_type[c.side] = MENUTYPE_TS_TRANSFORM;
				break;
			}
			case TYPE_ANNOTATE: {
				menu_type[c.side] = MENUTYPE_TS_ANNOTATE;
				break;
			}
			case TYPE_MEASURE: {
				menu_type[c.side] = MENUTYPE_TS_MEASURE;
				return;
			}
			case TYPE_EXTRUDE: {
				menu_type[c.side] = MENUTYPE_TS_EXTRUDE;
				break;
			}
			default: {
				menu_type[c.side] = MENUTYPE_MAIN_12;
				break;
			}
			}
		}
	}

	/* Populate menu based on type */
	std::vector<VR_Widget*>& menu_items = items[c.side];
	menu_items.clear();
	num_items[c.side] = 0;
	switch (menu_type[c.side]) {
	case MENUTYPE_MAIN_8: {
		menu_items.push_back(&Widget_Alt::obj);
		menu_items.push_back(&Widget_Undo::obj);
		menu_items.push_back(&Widget_Redo::obj);
		menu_items.push_back(&Widget_SwitchLayout::obj);
		menu_items.push_back(&Widget_SwitchComponent::obj);
		menu_items.push_back(&Widget_Delete::obj);
		menu_items.push_back(&Widget_Duplicate::obj);
		num_items[c.side] = 7;
		break;
	}
	case MENUTYPE_MAIN_12: {
		menu_items.push_back(&Widget_Menu::obj);
		menu_items.push_back(&Widget_Undo::obj);
		menu_items.push_back(&Widget_Redo::obj);
		menu_items.push_back(&Widget_SwitchLayout::obj);
		menu_items.push_back(&Widget_SwitchComponent::obj);
		menu_items.push_back(&Widget_Delete::obj);
		menu_items.push_back(&Widget_Duplicate::obj);
		menu_items.push_back(&Widget_Delete::obj);
		menu_items.push_back(&Widget_Duplicate::obj);
		menu_items.push_back(&Widget_SwitchLayout::obj);
		menu_items.push_back(&Widget_SwitchComponent::obj);
		num_items[c.side] = 11;
		break;
	}
	case MENUTYPE_SWITCHTOOL: {
		// Transform
		// Select
		// Measure
		// Extrude
		// Annotate
		num_items[c.side] = 5;
		break;
	}
	case MENUTYPE_TS_TRANSFORM: {
		// Manipulator
		// Move
		// Transform
		// Rotate
		// Scale
		// Delete (Vive only)
		// Duplicate (Vive only)
		num_items[c.side] = 7;
		break;
	}
	case MENUTYPE_TS_EXTRUDE: {
		// Flip normals
		// Region
		// Individual
		num_items[c.side] = 3;
		break;
	}
	case MENUTYPE_TS_ANNOTATE: {
		num_items[c.side] = 11;
		break;
	}
	case MENUTYPE_AS_NAVI: {
		// Lock rotation
		// Lock translation
		// Lock scale
		// Lock up-translation
		// Lock up-direction
		// Off 
		// Set/lock scale 1:1 with real world
		num_items[c.side] = 7;
		break;
	}
	case MENUTYPE_AS_TRANSFORM: {
		// Stick: Switch transform space
		// Y
		// X / Decrease manip size
		// Z / Increase manip size
		// XY
		// YZ
		// Off
		// ZX
		num_items[c.side] = 7;
		break;
	}
	case MENUTYPE_AS_EXTRUDE: {
		// Stick: Switch transform space
		// Decrease manip size
		// Increase manip size
		num_items[c.side] = 2;
		break;
	}
	default: {
		return;
	}
	}

	VR_Controller *controller = vr_get_obj()->controller[c.side];
	if (!controller) {
		return;
	}
	switch (VR_UI::ui_type) {
	case VR_UI_TYPE_FOVE: {
		const Coord3Df& c_pos = *(Coord3Df*)c.position.get(VR_SPACE_REAL).m[3];
		const Coord3Df& hmd_pos = *(Coord3Df*)VR_UI::hmd_position_get(VR_SPACE_REAL).m[3];
		const Mat44f& hmd_inv = VR_UI::hmd_position_get(VR_SPACE_REAL, true);
		static Coord3Df v;
		VR_Math::multiply_mat44_coord3D(v, hmd_inv, c_pos - hmd_pos);
		stick[c.side].x = v.x;
		stick[c.side].y = v.y;
		break;
	}
	case VR_UI_TYPE_VIVE: {
		stick[c.side].x = controller->dpad[0];
		stick[c.side].y = controller->dpad[1];
		break;
	}
	case VR_UI_TYPE_MICROSOFT:
	case VR_UI_TYPE_OCULUS:
	default: {
		stick[c.side].x = controller->stick[0];
		stick[c.side].y = controller->stick[1];
		break;
	}
	}
	float angle2 = angle[c.side] = stick[c.side].angle(Coord2Df(0, 1));
	if (stick[c.side].x < 0) {
		angle[c.side] = -angle[c.side];
	}

	if (num_items[c.side] < 8) {
		if (stick[c.side].x > 0) {
			angle2 += PI / 8.0f;
		}
		else {
			angle2 = -angle2 + PI / 8.0f;
		}
		angle2 *= 4.0f;

		if (angle2 >= 0 && angle2 < PI) {
			highlight_index[c.side] = 0;
		}
		else if (angle2 >= PI && angle2 < 2 * PI) {
			highlight_index[c.side] = 4;
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			highlight_index[c.side] = 2;
		}
		else if (angle2 >= 3 * PI && angle2 < 4 * PI) {
			highlight_index[c.side] = 6;
		}
		else if (angle2 >= 4 * PI || (angle2 < -3 * PI && angle2 >= -4 * PI)) {
			/* exit region */
			highlight_index[c.side] = 7;
			return;
		}
		else if (angle2 < -2 * PI && angle2 >= -3 * PI) {
			highlight_index[c.side] = 5;
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			highlight_index[c.side] = 1;
		}
		else {
			highlight_index[c.side] = 3;
		}
	}
	else {
		if (stick[c.side].x > 0) {
			angle2 += PI / 12.0f;
		}
		else {
			angle2 = -angle2 + PI / 12.0f;
		}
		angle2 *= 6.0f;

		if (angle2 >= 0 && angle2 < PI) {
			highlight_index[c.side] = 0;
		}
		else if (angle2 >= PI && angle2 < 2 * PI) {
			highlight_index[c.side] = 4;
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			highlight_index[c.side] = 6;
		}
		else if (angle2 >= 3 * PI && angle2 < 4 * PI) {
			highlight_index[c.side] = 2;
		}
		else if (angle2 >= 4 * PI && angle2 < 5 * PI) {
			highlight_index[c.side] = 8;
		}
		else if (angle2 >= 5 * PI && angle2 < 6 * PI) {
			highlight_index[c.side] = 10;
			return;
		}
		else if (angle2 >= 6 * PI || (angle2 < -5 * PI && angle2 >= -6 * PI)) {
			/* exit region */
			highlight_index[c.side] = 11;
			return;
		}
		else if (angle2 < -4 * PI && angle2 >= -5 * PI) {
			highlight_index[c.side] = 9;
			return;
		}
		else if (angle2 < -3 * PI && angle2 >= -4 * PI) {
			highlight_index[c.side] = 7;
		}
		else if (angle2 < -2 * PI && angle2 >= -3 * PI) {
			highlight_index[c.side] = 1;
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			highlight_index[c.side] = 5;
		}
		else {
			highlight_index[c.side] = 3;
		}
	}
}

void Widget_Menu::drag_contd(VR_UI::Cursor& c)
{
	const MenuType& type = menu_type[c.side];
	if (!VR_UI::pie_menu_active[c.side] ||
		type == MENUTYPE_TS_SELECT ||
		type == MENUTYPE_TS_MEASURE) {
		return;
	}

	VR_Controller *controller = vr_get_obj()->controller[c.side];
	if (!controller) {
		return;
	}
	switch (VR_UI::ui_type) {
	case VR_UI_TYPE_FOVE: {
		const Coord3Df& c_pos = *(Coord3Df*)c.position.get(VR_SPACE_REAL).m[3];
		const Coord3Df& hmd_pos = *(Coord3Df*)VR_UI::hmd_position_get(VR_SPACE_REAL).m[3];
		const Mat44f& hmd_inv = VR_UI::hmd_position_get(VR_SPACE_REAL, true);
		static Coord3Df v;
		VR_Math::multiply_mat44_coord3D(v, hmd_inv, c_pos - hmd_pos);
		stick[c.side].x = v.x;
		stick[c.side].y = v.y;
		break;
	}
	case VR_UI_TYPE_VIVE: {
		stick[c.side].x = controller->dpad[0];
		stick[c.side].y = controller->dpad[1];
		break;
	}
	case VR_UI_TYPE_MICROSOFT:
	case VR_UI_TYPE_OCULUS:
	default: {
		stick[c.side].x = controller->stick[0];
		stick[c.side].y = controller->stick[1];
		break;
	}
	}
	float angle2 = angle[c.side] = stick[c.side].angle(Coord2Df(0, 1));
	if (stick[c.side].x < 0) {
		angle[c.side] = -angle[c.side];
	}

	if (num_items[c.side] < 8) {
		if (stick[c.side].x > 0) {
			angle2 += PI / 8.0f;
		}
		else {
			angle2 = -angle2 + PI / 8.0f;
		}
		angle2 *= 4.0f;

		if (angle2 >= 0 && angle2 < PI) {
			highlight_index[c.side] = 0;
		}
		else if (angle2 >= PI && angle2 < 2 * PI) {
			highlight_index[c.side] = 4;
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			highlight_index[c.side] = 2;
		}
		else if (angle2 >= 3 * PI && angle2 < 4 * PI) {
			highlight_index[c.side] = 6;
		}
		else if (angle2 >= 4 * PI || (angle2 < -3 * PI && angle2 >= -4 * PI)) {
			/* exit region */
			highlight_index[c.side] = 7;
			return;
		}
		else if (angle2 < -2 * PI && angle2 >= -3 * PI) {
			highlight_index[c.side] = 5;
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			highlight_index[c.side] = 1;
		}
		else {
			highlight_index[c.side] = 3;
		}
	}
	else {
		if (stick[c.side].x > 0) {
			angle2 += PI / 12.0f;
		}
		else {
			angle2 = -angle2 + PI / 12.0f;
		}
		angle2 *= 6.0f;

		if (angle2 >= 0 && angle2 < PI) {
			highlight_index[c.side] = 0;
		}
		else if (angle2 >= PI && angle2 < 2 * PI) {
			highlight_index[c.side] = 4;
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			highlight_index[c.side] = 6;
		}
		else if (angle2 >= 3 * PI && angle2 < 4 * PI) {
			highlight_index[c.side] = 2;
		}
		else if (angle2 >= 4 * PI && angle2 < 5 * PI) {
			highlight_index[c.side] = 8;
		}
		else if (angle2 >= 5 * PI && angle2 < 6 * PI) {
			highlight_index[c.side] = 10;
			return;
		}
		else if (angle2 >= 6 * PI || (angle2 < -5 * PI && angle2 >= -6 * PI)) {
			/* exit region */
			highlight_index[c.side] = 11;
			return;
		}
		else if (angle2 < -4 * PI && angle2 >= -5 * PI) {
			highlight_index[c.side] = 9;
			return;
		}
		else if (angle2 < -3 * PI && angle2 >= -4 * PI) {
			highlight_index[c.side] = 7;
		}
		else if (angle2 < -2 * PI && angle2 >= -3 * PI) {
			highlight_index[c.side] = 1;
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			highlight_index[c.side] = 5;
		}
		else {
			highlight_index[c.side] = 3;
		}
	}
}

void Widget_Menu::drag_stop(VR_UI::Cursor& c)
{
	const MenuType& type = menu_type[c.side];
	if (type == MENUTYPE_TS_SELECT) {
		/* Toggle raycast/proximity selection. */
		VR_UI::SelectionMode& mode = VR_UI::selection_mode;
		if (mode == VR_UI::SELECTIONMODE_RAYCAST) {
			mode = VR_UI::SELECTIONMODE_PROXIMITY;
		}
		else {
			mode = VR_UI::SELECTIONMODE_RAYCAST;
		}
		VR_UI::pie_menu_active[c.side] = false;
		return;
	}
	else if (type == MENUTYPE_TS_MEASURE) {
		VR_UI::pie_menu_active[c.side] = false;
		return;
	}

	if (!VR_UI::pie_menu_active[c.side]) {
		return;
	}

	VR_UI::pie_menu_active[c.side] = false;
	highlight_index[c.side] = -1;

	/* Select item from final stick position. */
	VR *vr = vr_get_obj();
	VR_Controller *controller = vr->controller[c.side];
	if (!controller) {
		return;
	}
	//if (VR_UI::ui_type == VR_UI_TYPE_VIVE) {
	//	stick[c.side].x = controller->dpad[0];
	//	stick[c.side].y = controller->dpad[1];
	//}
	//else {
	//	stick[c.side].x = controller->stick[0];
	//	stick[c.side].y = controller->stick[1];
	//}
	/* Use angle from last drag_contd() for better result. */
	float angle2 = stick[c.side].angle(Coord2Df(0, 1));
	if (num_items[c.side] < 8) {
		if (stick[c.side].x > 0) {
			angle2 += PI / 8.0f;
		}
		else {
			angle2 = -angle2 + PI / 8.0f;
		}
		angle2 *= 4.0f;
	}
	else {
		if (stick[c.side].x > 0) {
			angle2 += PI / 12.0f;
		}
		else {
			angle2 = -angle2 + PI / 12.0f;
		}
		angle2 *= 6.0f;
	}

	switch (menu_type[c.side]) {
	case MENUTYPE_AS_NAVI: {
		if (angle2 >= 0 && angle2 < PI) {
			/* Lock rotation */
			Widget_Navi::obj.nav_lock[1] = ((Widget_Navi::obj.nav_lock[1] == VR_UI::NAVLOCK_ROT) ? VR_UI::NAVLOCK_NONE : VR_UI::NAVLOCK_ROT);
			return;
		}
		else if (angle2 >= PI && angle2 < 2 * PI) {
			/* Set/lock up-axis to Blender up-axis */
			Widget_Navi::obj.nav_lock[1] = ((Widget_Navi::obj.nav_lock[1] == VR_UI::NAVLOCK_ROT_UP) ? VR_UI::NAVLOCK_NONE : VR_UI::NAVLOCK_ROT_UP);
			return;
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			/* Lock scale */
			Widget_Navi::obj.nav_lock[2] = ((Widget_Navi::obj.nav_lock[2] == VR_UI::NAVLOCK_SCALE) ? VR_UI::NAVLOCK_NONE : VR_UI::NAVLOCK_SCALE);
			return;
		}
		else if (angle2 >= 3 * PI && angle2 < 4 * PI) {
			/* Set/lock scale 1:1 with real world */
			Widget_Navi::obj.nav_lock[2] = ((Widget_Navi::obj.nav_lock[2] == VR_UI::NAVLOCK_SCALE_REAL) ? VR_UI::NAVLOCK_NONE : VR_UI::NAVLOCK_SCALE_REAL);
			return;
		}
		else if (angle2 >= 4 * PI || (angle2 < -3 * PI && angle2 >= -4 * PI)) {
			/* exit region */
			return;
		}
		else if (angle2 < -2 * PI && angle2 >= -3 * PI) {
			/* Off */
			memset(Widget_Navi::nav_lock, 0, sizeof(int) * 3);
			return;
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			/* Lock translation */
			Widget_Navi::obj.nav_lock[0] = ((Widget_Navi::obj.nav_lock[0] == VR_UI::NAVLOCK_TRANS) ? VR_UI::NAVLOCK_NONE : VR_UI::NAVLOCK_TRANS);
			return;
		}
		else if (angle2 < 0 && angle2 >= -PI) {
			/* Lock up-translation */
			Widget_Navi::obj.nav_lock[0] = ((Widget_Navi::obj.nav_lock[0] == VR_UI::NAVLOCK_TRANS_UP) ? VR_UI::NAVLOCK_NONE : VR_UI::NAVLOCK_TRANS_UP);
			return;
		}
		return;
	}
	case MENUTYPE_AS_TRANSFORM: {
		if (Widget_Transform::manipulator) {
			if (angle2 >= 2 * PI && angle2 < 3 * PI) {
				/* Increase manipulator size */
				Widget_Transform::manip_scale_factor *= 1.2f;
				if (Widget_Transform::manip_scale_factor > 5.0f) {
					Widget_Transform::manip_scale_factor = 5.0f;
				}
				return;
			}
			else if (angle2 < -PI && angle2 >= -2 * PI) {
				/* Decrease manipulator size */
				Widget_Transform::manip_scale_factor *= 0.8f;
				if (Widget_Transform::manip_scale_factor < 0.05f) {
					Widget_Transform::manip_scale_factor = 0.05f;
				}
				return;
			}
			/* else */
			return;
		}

		if (angle2 >= 0 && angle2 < PI) {
			/* Y */
			Widget_Transform::constraint_flag[0] = 0; Widget_Transform::constraint_flag[1] = 1; Widget_Transform::constraint_flag[2] = 0;
			memcpy(Widget_Transform::snap_flag, Widget_Transform::constraint_flag, sizeof(int) * 3);
			switch (Widget_Transform::transform_mode) {
			case Widget_Transform::TRANSFORMMODE_OMNI: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_Y;
				Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_MOVE;
				return;
			}
			case Widget_Transform::TRANSFORMMODE_MOVE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_Y;
				return;
			}
			case Widget_Transform::TRANSFORMMODE_ROTATE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_ROT_Y;
				Widget_Transform::update_manipulator();
				return;
			}
			case Widget_Transform::TRANSFORMMODE_SCALE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_Y;
				return;
			}
			default: {
				return;
			}
			}
		}
		else if (angle2 >= PI && angle2 < 2 * PI) {
			/* YZ */
			if (Widget_Transform::transform_mode == Widget_Transform::TRANSFORMMODE_ROTATE) {
				return;
			}
			Widget_Transform::constraint_flag[0] = 0; Widget_Transform::constraint_flag[1] = 1; Widget_Transform::constraint_flag[2] = 1;
			memcpy(Widget_Transform::snap_flag, Widget_Transform::constraint_flag, sizeof(int) * 3);
			switch (Widget_Transform::transform_mode) {
			case Widget_Transform::TRANSFORMMODE_OMNI: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_YZ;
				Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_MOVE;
			}
			case Widget_Transform::TRANSFORMMODE_MOVE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_YZ;
				return;
			}
			case Widget_Transform::TRANSFORMMODE_SCALE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_YZ;
				return;
			}
			default: {
				return;
			}
			}
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			/* Z */
			Widget_Transform::constraint_flag[0] = 0; Widget_Transform::constraint_flag[1] = 0; Widget_Transform::constraint_flag[2] = 1;
			memcpy(Widget_Transform::snap_flag, Widget_Transform::constraint_flag, sizeof(int) * 3);
			switch (Widget_Transform::transform_mode) {
			case Widget_Transform::TRANSFORMMODE_OMNI: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_Z;
				Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_MOVE;
			}
			case Widget_Transform::TRANSFORMMODE_MOVE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_Z;
				return;
			}
			case Widget_Transform::TRANSFORMMODE_ROTATE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_ROT_Z;
				Widget_Transform::update_manipulator();
				return;
			}
			case Widget_Transform::TRANSFORMMODE_SCALE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_Z;
				return;
			}
			default: {
				return;
			}
			}
		}
		else if (angle2 >= 3 * PI && angle2 < 4 * PI) {
			/* ZX */
			if (Widget_Transform::transform_mode == Widget_Transform::TRANSFORMMODE_ROTATE) {
				return;
			}
			Widget_Transform::constraint_flag[0] = 1; Widget_Transform::constraint_flag[1] = 0; Widget_Transform::constraint_flag[2] = 1;
			memcpy(Widget_Transform::snap_flag, Widget_Transform::constraint_flag, sizeof(int) * 3);
			switch (Widget_Transform::transform_mode) {
			case Widget_Transform::TRANSFORMMODE_OMNI: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_ZX;
				Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_MOVE;
			}
			case Widget_Transform::TRANSFORMMODE_MOVE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_ZX;
				return;
			}
			case Widget_Transform::TRANSFORMMODE_SCALE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_ZX;
				return;
			}
			default: {
				return;
			}
			}
			return;
		}
		else if (angle2 >= 4 * PI || (angle2 < -3 * PI && angle2 >= -4 * PI)) {
			/* exit region */
			return;
		}
		else if (angle2 < -2 * PI && angle2 >= -3 * PI) {
			/* Off */
			memset(Widget_Transform::constraint_flag, 0, sizeof(int) * 3);
			memset(Widget_Transform::snap_flag, 1, sizeof(int) * 3);
			Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_NONE;
			if (Widget_Transform::omni) {
				Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_OMNI;
			}
			return;
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			/* X */
			Widget_Transform::constraint_flag[0] = 1; Widget_Transform::constraint_flag[1] = 0; Widget_Transform::constraint_flag[2] = 0;
			memcpy(Widget_Transform::snap_flag, Widget_Transform::constraint_flag, sizeof(int) * 3);
			switch (Widget_Transform::transform_mode) {
			case Widget_Transform::TRANSFORMMODE_OMNI: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_X;
				Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_MOVE;
			}
			case Widget_Transform::TRANSFORMMODE_MOVE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_X;
				return;
			}
			case Widget_Transform::TRANSFORMMODE_ROTATE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_ROT_X;
				Widget_Transform::update_manipulator();
				return;
			}
			case Widget_Transform::TRANSFORMMODE_SCALE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_X;
				return;
			}
			default: {
				return;
			}
			}
		}
		else if (angle2 < 0 && angle2 >= -PI) {
			/* XY */
			if (Widget_Transform::transform_mode == Widget_Transform::TRANSFORMMODE_ROTATE) {
				return;
			}
			Widget_Transform::constraint_flag[0] = 1; Widget_Transform::constraint_flag[1] = 1; Widget_Transform::constraint_flag[2] = 0;
			memcpy(Widget_Transform::snap_flag, Widget_Transform::constraint_flag, sizeof(int) * 3);
			switch (Widget_Transform::transform_mode) {
			case Widget_Transform::TRANSFORMMODE_OMNI: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_XY;
				Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_MOVE;
			}
			case Widget_Transform::TRANSFORMMODE_MOVE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_XY;
				return;
			}
			case Widget_Transform::TRANSFORMMODE_SCALE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_XY;
				return;
			}
			default: {
				return;
			}
			}
		}
		return;
	}
	case MENUTYPE_AS_EXTRUDE: {
		if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			/* Increase manipulator size */
			Widget_Transform::manip_scale_factor *= 1.2f;
			if (Widget_Transform::manip_scale_factor > 5.0f) {
				Widget_Transform::manip_scale_factor = 5.0f;
			}
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			/* Decrease manipulator size */
			Widget_Transform::manip_scale_factor *= 0.8f;
			if (Widget_Transform::manip_scale_factor < 0.05f) {
				Widget_Transform::manip_scale_factor = 0.05f;
			}
		}
		return;
	}
	case MENUTYPE_TS_TRANSFORM: {
		if (angle2 >= 0 && angle2 < PI) {
			/* Manipulator toggle */
			Widget_Transform::manipulator = !Widget_Transform::manipulator;
			if (Widget_Transform::manipulator) {
				for (int i = 0; i < VR_SIDES; ++i) {
					Widget_Transform::obj.do_render[i] = true;
				}
			}
			else {
				for (int i = 0; i < VR_SIDES; ++i) {
					Widget_Transform::obj.do_render[i] = false;
				}
			}
		}
		else if (angle2 >= PI && angle2 < 2 * PI) {
			/* Scale */
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_SCALE;
			Widget_Transform::omni = false;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_SCALE;
			memset(Widget_Transform::snap_flag, 1, sizeof(int) * 3);
			Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_NONE;
			memset(Widget_Transform::constraint_flag, 0, sizeof(int) * 3);
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			/* Transform */
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_OMNI;
			Widget_Transform::omni = true;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			memset(Widget_Transform::snap_flag, 1, sizeof(int) * 3);
			Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_NONE;
			memset(Widget_Transform::constraint_flag, 0, sizeof(int) * 3);
		}
		else if (angle2 >= 3 * PI && angle2 < 4 * PI) {
			if (VR_UI::ui_type == VR_UI_TYPE_VIVE) {
				/* Duplicate */
				Widget_Duplicate::obj.click(c);
			}
			else {
				return;
			}
		}
		else if (angle2 >= 4 * PI || (angle2 < -3 * PI && angle2 >= -4 * PI)) {
			/* exit region */
			if (depth[c.side] > 0) {
				--depth[c.side];
			}
			return;
		}
		else if (angle2 < -2 * PI && angle2 >= -3 * PI) {
			if (VR_UI::ui_type == VR_UI_TYPE_VIVE) {
				/* Delete */
				Widget_Delete::obj.click(c);
			}
			else {
				return;
			}
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			/* Move */
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_MOVE;
			Widget_Transform::omni = false;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			memset(Widget_Transform::snap_flag, 1, sizeof(int) * 3);
			Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_NONE;
			memset(Widget_Transform::constraint_flag, 0, sizeof(int) * 3);
		}
		else if (angle2 < 0 && angle2 >= -PI) {
			/* Rotate */
			Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_ROTATE;
			Widget_Transform::omni = false;
			Widget_Transform::snap_mode = VR_UI::SNAPMODE_ROTATION;
			memset(Widget_Transform::snap_flag, 1, sizeof(int) * 3);
			Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_NONE;
			memset(Widget_Transform::constraint_flag, 0, sizeof(int) * 3);
		}
		return;
	}
	case MENUTYPE_TS_ANNOTATE: {
		/* Colorwheel */
		static float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		if (angle2 >= 0 && angle2 < PI) {
			color[0] = 0.95f; color[1] = 0.95f; color[2] = 0.95f;
			Widget_Annotate::active_layer = 0;
		}
		else if (angle2 >= PI && angle2 < 2 * PI) {
			color[0] = 0.05f; color[1] = 0.05f; color[2] = 0.05f;
			Widget_Annotate::active_layer = 1;
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			color[0] = 0.6f; color[1] = 0.2f; color[2] = 1.0f;
			Widget_Annotate::active_layer = 2;
		}
		else if (angle2 >= 3 * PI && angle2 < 4 * PI) {
			color[0] = 0.72f; color[1] = 0.46f; color[2] = 1.0f;
			Widget_Annotate::active_layer = 3;
		}
		else if (angle2 >= 4 * PI && angle2 < 5 * PI) {
			color[0] = 0.2f; color[1] = 0.6f; color[2] = 1.0f;
			Widget_Annotate::active_layer = 4;
		}
		else if (angle2 >= 5 * PI && angle2 < 6 * PI) {
			color[0] = 0.2f; color[1] = 1.0f; color[2] = 1.0f;
			Widget_Annotate::active_layer = 5;
		}
		else if (angle2 >= 6 * PI || (angle2 < -5 * PI && angle2 >= -6 * PI)) {
			/* exit region */
			if (depth[c.side] > 0) {
				--depth[c.side];
			}
			return;
		}
		else if (angle2 < -4 * PI && angle2 >= -5 * PI) {
			color[0] = 0.6f; color[1] = 1.0f; color[2] = 0.2f;
			Widget_Annotate::active_layer = 7;
		}
		else if (angle2 < -3 * PI && angle2 >= -4 * PI) {
			color[0] = 0.4f; color[1] = 0.8; color[2] = 0.2f;
			Widget_Annotate::active_layer = 8;
		}
		else if (angle2 < -2 * PI && angle2 >= -3 * PI) {
			color[0] = 1.0f; color[1] = 1.0f; color[2] = 0.2f;
			Widget_Annotate::active_layer = 9;
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			color[0] = 1.0f; color[1] = 0.6f; color[2] = 0.2f;
			Widget_Annotate::active_layer = 10;
		}
		else {
			color[0] = 1.0f; color[1] = 0.2f; color[2] = 0.2f;
			Widget_Annotate::active_layer = 11;
		}

		memcpy(Widget_Annotate::color, color, sizeof(float) * 3);
		return;
	}	
	case MENUTYPE_TS_EXTRUDE: {
		if (angle2 >= 0 && angle2 < PI) {
			/* Flip normals */
			Widget_Extrude::flip_normals = !Widget_Extrude::flip_normals;
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			/* Individual */
			Widget_Extrude::extrude_mode = Widget_Extrude::EXTRUDEMODE_INDIVIDUAL;
		}
		else if (angle2 >= 3 * PI || (angle2 < -2 * PI && angle2 >= -3 * PI)) {
			/* exit region */
			if (depth[c.side] > 0) {
				--depth[c.side];
			}
			return;
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			/* Region */
			Widget_Extrude::extrude_mode = Widget_Extrude::EXTRUDEMODE_REGION;
		}
		return;
	}
	case MENUTYPE_SWITCHTOOL: {
		if (angle2 >= 0 && angle2 < PI) {
			/* Transform */
			VR_UI::set_current_tool(&Widget_Transform::obj, c.side);
			Widget_SwitchTool::obj.curr_tool[c.side] = &Widget_Transform::obj;
		}
		else if (angle2 >= PI && angle2 < 2 * PI) {
			/* Annotate */
			VR_UI::set_current_tool(&Widget_Annotate::obj, c.side);
			Widget_SwitchTool::obj.curr_tool[c.side] = &Widget_Annotate::obj;
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			/* Measure */
			VR_UI::set_current_tool(&Widget_Measure::obj, c.side);
			Widget_SwitchTool::obj.curr_tool[c.side] = &Widget_Measure::obj;
		}
		else if (angle2 >= 3 * PI || (angle2 < -2 * PI && angle2 >= -3 * PI)) {
			/* exit region */
			if (depth[c.side] > 0) {
				--depth[c.side];
			}
			return;
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			/* Select */
			VR_UI::set_current_tool(&Widget_Select::obj, c.side);
			Widget_SwitchTool::obj.curr_tool[c.side] = &Widget_Select::obj;
		}
		else if (angle2 < 0 && angle2 >= -PI) {
			/* Extrude */
			VR_UI::set_current_tool(&Widget_Extrude::obj, c.side);
			Widget_SwitchTool::obj.curr_tool[c.side] = &Widget_Extrude::obj;
		}

		VR_Widget *tool = VR_UI::get_current_tool(c.side);
		if (!tool) {
			menu_type[c.side] = MENUTYPE_MAIN_12;
			return;
		}
		switch (tool->type()) {
		case TYPE_SELECT: {
			menu_type[c.side] = MENUTYPE_TS_SELECT;
			return;
		}
		case TYPE_TRANSFORM: {
			menu_type[c.side] = MENUTYPE_TS_TRANSFORM;
			return;
		}
		case TYPE_ANNOTATE: {
			menu_type[c.side] = MENUTYPE_TS_ANNOTATE;
			return;
		}
		case TYPE_MEASURE: {
			menu_type[c.side] = MENUTYPE_TS_MEASURE;
			return;
		}
		case TYPE_EXTRUDE: {
			menu_type[c.side] = MENUTYPE_TS_EXTRUDE;
			return;
		}
		default: {
			menu_type[c.side] = MENUTYPE_MAIN_12;
			return;
		}
		}
		return;
	}
	default: {
		uint index;
		if (num_items[c.side] < 8) { /* MENUTYPE_MAIN_8 */ 
			if (angle2 >= 0 && angle2 < PI) {
				index = 0;
			}
			else if (angle2 >= PI && angle2 < 2 * PI) {
				index = 4;
			}
			else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
				index = 2;
			}
			else if (angle2 >= 3 * PI && angle2 < 4 * PI) {
				index = 6;
			}
			else if (angle2 >= 4 * PI || (angle2 < -3 * PI && angle2 >= -4 * PI)) {
				/* exit region */
				if (depth[c.side] > 0) {
					--depth[c.side];
				}
				return;
			}
			else if (angle2 < -2 * PI && angle2 >= -3 * PI) {
				index = 5;
			}
			else if (angle2 < -PI && angle2 >= -2 * PI) {
				index = 1;
			}
			else {
				index = 3;
			}
		}
		else { /* MENUTYPE_MAIN_12 */
			if (angle2 >= 0 && angle2 < PI) {
				index = 0;
			}
			else if (angle2 >= PI && angle2 < 2 * PI) {
				index = 4;
			}
			else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
				index = 6;
			}
			else if (angle2 >= 3 * PI && angle2 < 4 * PI) {
				index = 2;
			}
			else if (angle2 >= 4 * PI && angle2 < 5 * PI) {
				index = 8;
			}
			else if (angle2 >= 5 * PI && angle2 < 6 * PI) {
				index = 10;
			}
			else if (angle2 >= 6 * PI || (angle2 < -5 * PI && angle2 >= -6 * PI)) {
				/* exit region */
				if (depth[c.side] > 0) {
					--depth[c.side];
				}
				return;
			}
			else if (angle2 < -4 * PI && angle2 >= -5 * PI) {
				index = 9;
			}
			else if (angle2 < -3 * PI && angle2 >= -4 * PI) {
				index = 7;
			}
			else if (angle2 < -2 * PI && angle2 >= -3 * PI) {
				index = 1;
			}
			else if (angle2 < -PI && angle2 >= -2 * PI) {
				index = 5;
			}
			else {
				index = 3;
			}
		}

		if (items[c.side].size() > index && items[c.side][index]) {
			if (items[c.side][index]->type() == TYPE_MENU) {
				/* Open a new menu / submenu. */
				menu_type[c.side] = MENUTYPE_MAIN_8;
				++depth[c.side];
				VR_UI::pie_menu_active[c.side] = true;
				return;
			}
			else {
				/* Execute widget "click" action. */
				items[c.side][index]->click(c);
			}
		}
		return;
	}
	}
}

void Widget_Menu::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
{
	const MenuType& type = menu_type[controller_side];

	if (!VR_UI::pie_menu_active[controller_side]) {
		if (type == MENUTYPE_TS_MEASURE) {
			return;
		}

		if (touched) {
			const Mat44f& t_touched = m_widget_touched * t;
			VR_Draw::update_modelview_matrix(&t_touched, 0);
		}
		else {
			VR_Draw::update_modelview_matrix(&t, 0);
		}
		if (menu_type[controller_side] == MENUTYPE_TS_ANNOTATE) {
			VR_Draw::set_color(Widget_Annotate::color);
		}
		else {
			if (active) {
				VR_Draw::set_color(1.0f, 0.0f, 0.0f, 1.0f);
			}
			else {
				VR_Draw::set_color(1.0f, 1.0f, 1.0f, 1.0f);
			}
		}

		if (type == MENUTYPE_TS_SELECT) {
			VR_UI::SelectionMode& mode = VR_UI::selection_mode;
			if (mode == VR_UI::SELECTIONMODE_RAYCAST) {
				VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::select_raycast_tex);
			}
			else {
				VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::select_proximity_tex);
			}
		}
		else {
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::toolsettings_tex);
		}
		return;
	}

	VR_Draw::update_modelview_matrix(&t, 0);

	if (!action_settings[controller_side]) {
		/* Background */
		if (type == MENUTYPE_TS_ANNOTATE) {
			VR_Draw::set_color(1.0f, 1.0f, 1.0f, 0.9f);
			VR_Draw::render_rect(-0.0728f, 0.0728f, 0.0728f, -0.0728f, -0.005f, 1.0f, 1.0f, VR_Draw::colorwheel_menu_tex);
		}
		else {
			VR_Draw::set_color(1.0f, 1.0f, 1.0f, 0.9f);
			VR_Draw::render_rect(-0.1121f, 0.1121f, 0.1121f, -0.1121f, -0.005f, 1.0f, 1.0f, VR_Draw::background_menu_tex);
		}
	}
	VR_Draw::set_color(1.0f, 1.0f, 1.0f, 1.0f);

	/* Render icons for menu items. */
	static Mat44f t_icon = VR_Math::identity_f;
	static Mat44f m;
	static std::string menu_str;

	int& menu_highlight_index = highlight_index[controller_side];

	if (action_settings[controller_side]) {
		switch (type) {
		case MENUTYPE_AS_NAVI: {
			/* Center */
			/* index = 0 */
			if (Widget_Navi::nav_lock[1] == VR_UI::NAVLOCK_ROT) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p_as_0;
			if (menu_highlight_index == 0) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.006f, 0.006f, 0.006f, -0.006f, 0.001f, 1.0f, 1.0f, VR_Draw::nav_lockrot_tex);
			if (Widget_Navi::nav_lock[1] == VR_UI::NAVLOCK_ROT) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 4 */
			if (Widget_Navi::nav_lock[1] == VR_UI::NAVLOCK_ROT_UP) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p_as_4;
			if (menu_highlight_index == 4) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.006f, 0.006f, 0.006f, -0.006f, 0.001f, 1.0f, 1.0f, VR_Draw::nav_lockrotup_tex);
			if (Widget_Navi::nav_lock[1] == VR_UI::NAVLOCK_ROT_UP) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 2 */
			if (Widget_Navi::nav_lock[2] == VR_UI::NAVLOCK_SCALE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p_as_2;
			if (menu_highlight_index == 2) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.006f, 0.006f, 0.006f, -0.006f, 0.001f, 1.0f, 1.0f, VR_Draw::nav_lockscale_tex);
			if (Widget_Navi::nav_lock[2] == VR_UI::NAVLOCK_SCALE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 6 */
			if (Widget_Navi::nav_lock[2] == VR_UI::NAVLOCK_SCALE_REAL) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 6) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p_as_6;
			if (menu_highlight_index == 6) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.006f, 0.006f, 0.006f, -0.006f, 0.001f, 1.0f, 1.0f, VR_Draw::nav_lockscalereal_tex);
			if (Widget_Navi::nav_lock[2] == VR_UI::NAVLOCK_SCALE_REAL) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 6) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 7 (exit region) */
			/* index = 5 */
			bool lock = (Widget_Navi::nav_lock[0] | Widget_Navi::nav_lock[1] || Widget_Navi::nav_lock[2]);
			if (!lock) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 5) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p_as_5;
			if (menu_highlight_index == 5) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.006f, 0.006f, 0.005f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::off_str_tex);
			if (!lock) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 5) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 1 */
			if (Widget_Navi::nav_lock[0] == VR_UI::NAVLOCK_TRANS) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p_as_1;
			if (menu_highlight_index == 1) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.006f, 0.006f, 0.006f, -0.006f, 0.001f, 1.0f, 1.0f, VR_Draw::nav_locktrans_tex);
			if (Widget_Navi::nav_lock[0] == VR_UI::NAVLOCK_TRANS) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 3 */
			if (Widget_Navi::nav_lock[0] == VR_UI::NAVLOCK_TRANS_UP) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p_as_3;
			if (menu_highlight_index == 3) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.006f, 0.006f, 0.006f, -0.006f, 0.001f, 1.0f, 1.0f, VR_Draw::nav_locktransup_tex);
			if (Widget_Navi::nav_lock[0] == VR_UI::NAVLOCK_TRANS_UP) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_white);
			}
			break;
		}
		case MENUTYPE_AS_SELECT: {
			/* Center */
			if (VR_UI::mouse_cursor_enabled) {
				VR_Draw::set_color(c_menu_red);
			}
			VR_Widget_Layout::ButtonBit btnbit = VR_UI::ui_type == VR_UI_TYPE_OCULUS ? VR_Widget_Layout::BUTTONBITS_STICKS : VR_Widget_Layout::BUTTONBITS_DPADS;
			bool center_touched = ((vr_get_obj()->controller[controller_side]->buttons_touched & btnbit) != 0);
			if (VR_UI::ui_type == VR_UI_TYPE_MICROSOFT) {
				/* Special case for WindowsMR (with SteamVR): replace stick press with dpad press. */
				t_icon.m[1][1] = t_icon.m[2][2] = (float)cos(QUARTPI);
				t_icon.m[1][2] = -(t_icon.m[2][1] = (float)sin(QUARTPI));
				*((Coord3Df*)t_icon.m[3]) = VR_Widget_Layout::button_positions[vr_get_obj()->ui_type][controller_side][VR_Widget_Layout::BUTTONID_DPAD];
				const Mat44f& t_controller = VR_UI::cursor_position_get(VR_SPACE_REAL, controller_side);
				if (center_touched) {
					m = m_widget_touched * t_icon * t_controller;
				}
				else {
					m = t_icon * t_controller;
				}
				VR_Draw::update_modelview_matrix(&m, 0);
				VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mouse_cursor_tex);
				t_icon.m[1][1] = t_icon.m[2][2] = 1;
				t_icon.m[1][2] = t_icon.m[2][1] = 0;
			}
			else {
				*((Coord3Df*)t_icon.m[3]) = p_as_stick;
				if (VR_UI::ui_type == VR_UI_TYPE_OCULUS) {
					if (center_touched && menu_highlight_index < 0) {
						m = m_widget_touched * t_icon * t;
					}
					else {
						m = t_icon * t;
					}
				}
				else {
					if (center_touched) {
						m = m_widget_touched * t_icon * t;
					}
					else {
						m = t_icon * t;
					}
				}
				VR_Draw::update_modelview_matrix(&m, 0);
				VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mouse_cursor_tex);
			}
			if (VR_UI::mouse_cursor_enabled) {
				VR_Draw::set_color(c_menu_white);
			}
			break;
		}
		case MENUTYPE_AS_TRANSFORM: {
			/* Center */
			VR_Widget_Layout::ButtonBit btnbit = VR_UI::ui_type == VR_UI_TYPE_OCULUS ? VR_Widget_Layout::BUTTONBITS_STICKS : VR_Widget_Layout::BUTTONBITS_DPADS;
			bool center_touched = ((vr_get_obj()->controller[controller_side]->buttons_touched & btnbit) != 0);
			if (VR_UI::ui_type == VR_UI_TYPE_MICROSOFT) {
				/* Special case for WindowsMR (with SteamVR): replace stick press with dpad press. */
				t_icon.m[1][1] = t_icon.m[2][2] = (float)cos(QUARTPI);
				t_icon.m[1][2] = -(t_icon.m[2][1] = (float)sin(QUARTPI));
				*((Coord3Df*)t_icon.m[3]) = VR_Widget_Layout::button_positions[vr_get_obj()->ui_type][controller_side][VR_Widget_Layout::BUTTONID_DPAD];
				const Mat44f& t_controller = VR_UI::cursor_position_get(VR_SPACE_REAL, controller_side);
				if (center_touched) {
					m = m_widget_touched * t_icon * t_controller;
				}
				else {
					m = t_icon * t_controller;
				}
				VR_Draw::update_modelview_matrix(&m, 0);
				switch (Widget_Transform::transform_space) {
				case VR_UI::TRANSFORMSPACE_NORMAL: {
					VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_normal_tex);
					break;
				}
				case VR_UI::TRANSFORMSPACE_LOCAL: {
					VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_local_tex);
					break;
				}
				case VR_UI::TRANSFORMSPACE_GLOBAL:
				default: {
					VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_global_tex);
					break;
				}
				}
				t_icon.m[1][1] = t_icon.m[2][2] = 1;
				t_icon.m[1][2] = t_icon.m[2][1] = 0;
			}
			else {
				*((Coord3Df*)t_icon.m[3]) = p_as_stick;
				if (VR_UI::ui_type == VR_UI_TYPE_OCULUS) {
					if (center_touched && menu_highlight_index < 0) {
						m = m_widget_touched * t_icon * t;
					}
					else {
						m = t_icon * t;
					}
				}
				else {
					if (center_touched) {
						m = m_widget_touched * t_icon * t;
					}
					else {
						m = t_icon * t;
					}
				}
				VR_Draw::update_modelview_matrix(&m, 0);
				switch (Widget_Transform::transform_space) {
				case VR_UI::TRANSFORMSPACE_NORMAL: {
					VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_normal_tex);
					break;
				}
				case VR_UI::TRANSFORMSPACE_LOCAL: {
					VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_local_tex);
					break;
				}
				case VR_UI::TRANSFORMSPACE_GLOBAL:
				default: {
					VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_global_tex);
					break;
				}
				}
			}

			/* "Manipulator mode" action settings */
			if (Widget_Transform::manipulator) {
				/* index = 0 */
				/* index = 4 */
				/* index = 2 */
				if (menu_highlight_index == 2) {
					VR_Draw::set_color(c_menu_blue);
				}
				*((Coord3Df*)t_icon.m[3]) = p_as_2;
				if (menu_highlight_index == 2) {
					m = m_widget_touched * t_icon * t;
				}
				else {
					m = t_icon * t;
				}
				VR_Draw::update_modelview_matrix(&m, 0);
				VR_Draw::render_rect(-0.006f, 0.006f, 0.006f, -0.006f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_plus_tex);
				if (menu_highlight_index == 2) {
					VR_Draw::set_color(c_menu_white);
				}
				/* index = 6 */
				/* index = 7 (exit region) */
				/* index = 5 */
				/* index = 1 */
				if (menu_highlight_index == 1) {
					VR_Draw::set_color(c_menu_blue);
				}
				*((Coord3Df*)t_icon.m[3]) = p_as_1;
				if (menu_highlight_index == 1) {
					m = m_widget_touched * t_icon * t;
				}
				else {
					m = t_icon * t;
				}
				VR_Draw::update_modelview_matrix(&m, 0);
				VR_Draw::render_rect(-0.006f, 0.006f, 0.006f, -0.006f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_minus_tex);
				if (menu_highlight_index == 1) {
					VR_Draw::set_color(c_menu_white);
				}
				/* index = 3*/
				break;
			}

			/* index = 0 */
			if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_TRANS_Y ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_ROT_Y ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_SCALE_Y) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p_as_0;
			if (menu_highlight_index == 0) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.004f, 0.004f, 0.003f, -0.005f, 0.001f, 1.0f, 1.0f, VR_Draw::y_str_tex);
			if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_TRANS_Y ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_ROT_Y ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_SCALE_Y) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 4 */
			if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_TRANS_YZ ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_SCALE_YZ) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p_as_4;
			if (menu_highlight_index == 4) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.004f, -0.006f, 0.001f, 1.0f, 1.0f, VR_Draw::yz_str_tex);
			if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_TRANS_YZ ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_SCALE_YZ) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 2 */
			if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_TRANS_Z ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_ROT_Z ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_SCALE_Z) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p_as_2;
			if (menu_highlight_index == 2) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.004f, 0.004f, 0.003f, -0.005f, 0.001f, 1.0f, 1.0f, VR_Draw::z_str_tex);
			if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_TRANS_Z ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_ROT_Z ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_SCALE_Z) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 6 */
			if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_TRANS_ZX ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_SCALE_ZX) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 6) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p_as_6;
			if (menu_highlight_index == 6) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.004f, -0.006f, 0.001f, 1.0f, 1.0f, VR_Draw::zx_str_tex);
			if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_TRANS_ZX ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_SCALE_ZX) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 6) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 7 (exit region) */
			/* index = 5 */
			if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_NONE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 5) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p_as_5;
			if (menu_highlight_index == 5) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.006f, 0.006f, 0.005f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::off_str_tex);
			if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_NONE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 5) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 1 */
			if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_TRANS_X ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_ROT_X ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_SCALE_X) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p_as_1;
			if (menu_highlight_index == 1) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.004f, 0.004f, 0.003f, -0.005f, 0.001f, 1.0f, 1.0f, VR_Draw::x_str_tex);
			if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_TRANS_X ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_ROT_X ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_SCALE_X) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 3 */
			if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_TRANS_XY ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_SCALE_XY) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p_as_3;
			if (menu_highlight_index == 3) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.004f, -0.006f, 0.001f, 1.0f, 1.0f, VR_Draw::xy_str_tex);
			if (Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_TRANS_XY ||
				Widget_Transform::constraint_mode == VR_UI::CONSTRAINTMODE_SCALE_XY) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_white);
			}
			break;
		}
		case MENUTYPE_AS_EXTRUDE: {
			/* Center */
			VR_Widget_Layout::ButtonBit btnbit = VR_UI::ui_type == VR_UI_TYPE_OCULUS ? VR_Widget_Layout::BUTTONBITS_STICKS : VR_Widget_Layout::BUTTONBITS_DPADS;
			bool center_touched = ((vr_get_obj()->controller[controller_side]->buttons_touched & btnbit) != 0);
			if (VR_UI::ui_type == VR_UI_TYPE_MICROSOFT) {
				/* Special case for WindowsMR (with SteamVR): replace stick press with dpad press. */
				t_icon.m[1][1] = t_icon.m[2][2] = (float)cos(QUARTPI);
				t_icon.m[1][2] = -(t_icon.m[2][1] = (float)sin(QUARTPI));
				*((Coord3Df*)t_icon.m[3]) = VR_Widget_Layout::button_positions[vr_get_obj()->ui_type][controller_side][VR_Widget_Layout::BUTTONID_DPAD];
				const Mat44f& t_controller = VR_UI::cursor_position_get(VR_SPACE_REAL, controller_side);
				if (center_touched) {
					m = m_widget_touched * t_icon * t_controller;
				}
				else {
					m = t_icon * t_controller;
				}
				VR_Draw::update_modelview_matrix(&m, 0);
				switch (Widget_Transform::transform_space) {
				case VR_UI::TRANSFORMSPACE_NORMAL: {
					VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_normal_tex);
					break;
				}
				case VR_UI::TRANSFORMSPACE_LOCAL: {
					VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_local_tex);
					break;
				}
				case VR_UI::TRANSFORMSPACE_GLOBAL:
				default: {
					VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_global_tex);
					break;
				}
				}
				t_icon.m[1][1] = t_icon.m[2][2] = 1;
				t_icon.m[1][2] = t_icon.m[2][1] = 0;
			}
			else {
				*((Coord3Df*)t_icon.m[3]) = p_as_stick;
				if (VR_UI::ui_type == VR_UI_TYPE_OCULUS) {
					if (center_touched && menu_highlight_index < 0) {
						m = m_widget_touched * t_icon * t;
					}
					else {
						m = t_icon * t;
					}
				}
				else {
					if (center_touched) {
						m = m_widget_touched * t_icon * t;
					}
					else {
						m = t_icon * t;
					}
				}
				VR_Draw::update_modelview_matrix(&m, 0);
				switch (Widget_Transform::transform_space) {
				case VR_UI::TRANSFORMSPACE_NORMAL: {
					VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_normal_tex);
					break;
				}
				case VR_UI::TRANSFORMSPACE_LOCAL: {
					VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_local_tex);
					break;
				}
				case VR_UI::TRANSFORMSPACE_GLOBAL:
				default: {
					VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_global_tex);
					break;
				}
				}
			}

			/* index = 0 */
			/* index = 4 */
			/* index = 2 */
			if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p_as_2;
			if (menu_highlight_index == 2) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.006f, 0.006f, 0.006f, -0.006f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_plus_tex);
			if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 6 */
			/* index = 7 (exit region) */
			/* index = 5 */
			/* index = 1 */
			if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p_as_1;
			if (menu_highlight_index == 1) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.006f, 0.006f, 0.006f, -0.006f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_minus_tex);
			if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 3*/
			break;
		}
		default: {
			break;
		}
		}
	}
	else {
		if (touched) {
			/* Render sphere to represent stick direction. */
			static Mat44f m = VR_Math::identity_f;
			Coord3Df temp = (*(Coord3Df*)t.m[1]).normalize();
			temp *= 0.06f;
			rotate_v3_v3v3fl(m.m[3], (float*)&temp, t.m[2], -angle[controller_side]);
			*(Coord3Df*)m.m[3] += *(Coord3Df*)t.m[3];
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_ball(0.005f, false);
		}
		menu_str = "";

		switch (menu_type[controller_side]) {
		case MENUTYPE_TS_TRANSFORM: {
			/* index = 0 */
			if (Widget_Transform::manipulator) {
				VR_Draw::set_color(c_menu_red);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_0;
			if (menu_highlight_index == 0) {
				menu_str = "MANIPULATOR";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_tex);
			if (Widget_Transform::manipulator) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 4 */
			if (Widget_Transform::transform_mode == Widget_Transform::TRANSFORMMODE_SCALE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_4;
			if (menu_highlight_index == 4) {
				menu_str = "SCALE";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::scale_tex);
			if (Widget_Transform::transform_mode == Widget_Transform::TRANSFORMMODE_SCALE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 2 */
			if (Widget_Transform::transform_mode == Widget_Transform::TRANSFORMMODE_OMNI) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_2;
			if (menu_highlight_index == 2) {
				menu_str = "TRANSFORM";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::transform_tex);
			if (Widget_Transform::transform_mode == Widget_Transform::TRANSFORMMODE_OMNI) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_white);
			}
			if (VR_UI::ui_type == VR_UI_TYPE_VIVE) {
				/* index = 6 */
				if (menu_highlight_index == 6) {
					VR_Draw::set_color(c_menu_blue);
				}
				*((Coord3Df*)t_icon.m[3]) = p8_6;
				if (menu_highlight_index == 6) {
					menu_str = "DUPLICATE";
					m = m_widget_touched * t_icon * t;
				}
				else {
					m = t_icon * t;
				}
				VR_Draw::update_modelview_matrix(&m, 0);
				VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::duplicate_tex);
				if (menu_highlight_index == 6) {
					VR_Draw::set_color(c_menu_white);
				}
				/* index = 7 (exit region) */
				/* index = 5 */
				if (menu_highlight_index == 5) {
					VR_Draw::set_color(c_menu_blue);
				}
				*((Coord3Df*)t_icon.m[3]) = p8_5;
				if (menu_highlight_index == 5) {
					menu_str = "DELETE";
					m = m_widget_touched * t_icon * t;
				}
				else {
					m = t_icon * t;
				}
				VR_Draw::update_modelview_matrix(&m, 0);
				VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::delete_tex);
				if (menu_highlight_index == 5) {
					VR_Draw::set_color(c_menu_white);
				}
			}
			else {
				/* index = 6 */
				/* index = 7 (exit region) */
				/* index = 5 */
			}
			/* index = 1 */
			if (Widget_Transform::transform_mode == Widget_Transform::TRANSFORMMODE_MOVE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_1;
			if (menu_highlight_index == 1) {
				menu_str = "MOVE";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::move_tex);
			if (Widget_Transform::transform_mode == Widget_Transform::TRANSFORMMODE_MOVE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 3 */
			if (Widget_Transform::transform_mode == Widget_Transform::TRANSFORMMODE_ROTATE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_3;
			if (menu_highlight_index == 3) {
				menu_str = "ROTATE";
				m = m_widget_touched * t_icon  * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::rotate_tex);
			if (Widget_Transform::transform_mode == Widget_Transform::TRANSFORMMODE_ROTATE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_white);
			}
			/* Center */
			*((Coord3Df*)t_icon.m[3]) = p8_stick;
			m = t_icon * t;
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_string(menu_str.c_str(), 0.009f, 0.012f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.005f, 0.001f);
			break;
		}
		case MENUTYPE_TS_EXTRUDE: {
			/* index = 0 */
			if (Widget_Extrude::flip_normals) {
				VR_Draw::set_color(c_menu_red);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_0;
			if (menu_highlight_index == 0) {
				menu_str = "FLIP EDGES";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::flip_normals_tex);
			if (Widget_Extrude::flip_normals) {
				VR_Draw::set_color(c_menu_white);
			}
			if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 4 */
			/* index = 2 */
			if (Widget_Extrude::extrude_mode == Widget_Extrude::EXTRUDEMODE_INDIVIDUAL) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_2;
			if (menu_highlight_index == 2) {
				menu_str = "INDIVIDUAL";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::extrude_individual_tex);
			if (Widget_Extrude::extrude_mode == Widget_Extrude::EXTRUDEMODE_INDIVIDUAL) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 6 */
			/* index = 7 (exit region) */
			/* index = 5 */
			/* index = 1 */
			if (Widget_Extrude::extrude_mode == Widget_Extrude::EXTRUDEMODE_REGION) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_1;
			if (menu_highlight_index == 1) {
				menu_str = "REGION";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::extrude_tex);
			if (Widget_Extrude::extrude_mode == Widget_Extrude::EXTRUDEMODE_REGION) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 3 */
			/* Center */
			*((Coord3Df*)t_icon.m[3]) = p8_stick;
			m = t_icon * t;
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_string(menu_str.c_str(), 0.009f, 0.012f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.005f, 0.001f);
			break;
		}
		case MENUTYPE_SWITCHTOOL: {
			Type type = VR_UI::get_current_tool(controller_side)->type();
			/* index = 0 */
			if (type == TYPE_TRANSFORM) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_0;
			if (menu_highlight_index == 0) {
				menu_str = "TRANSFORM";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::transform_tex);
			if (type == TYPE_TRANSFORM) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 4 */
			if (type == TYPE_ANNOTATE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_4;
			if (menu_highlight_index == 4) {
				menu_str = "ANNOTATE";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::annotate_tex);
			if (type == TYPE_ANNOTATE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 2 */
			if (type == TYPE_MEASURE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_2;
			if (menu_highlight_index == 2) {
				menu_str = "MEASURE";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::measure_tex);
			if (type == TYPE_MEASURE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 6 */
			/* index = 7 (exit region) */
			/* index = 5 */
			/* index = 1 */
			if (type == TYPE_SELECT) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_1;
			if (menu_highlight_index == 1) {
				menu_str = "SELECT";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::select_tex);
			if (type == TYPE_SELECT) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 3 */
			if (type == TYPE_EXTRUDE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_3;
			if (menu_highlight_index == 3) {
				menu_str = "EXTRUDE";
				m = m_widget_touched * t_icon  * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::extrude_tex);
			if (type == TYPE_EXTRUDE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_white);
			}
			/* Center */
			*((Coord3Df*)t_icon.m[3]) = p8_stick;
			m = t_icon * t;
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_string(menu_str.c_str(), 0.009f, 0.012f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.005f, 0.001f);
			break;
		}
		case MENUTYPE_MAIN_8: {
			/* index = 0 */
			if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_0;
			if (menu_highlight_index == 0) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::alt_tex);
			if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 4 */
			if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_4;
			if (menu_highlight_index == 4) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::object_tex);
			if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 2 */
			if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_2;
			if (menu_highlight_index == 2) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::redo_tex);
			if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 6 */
			if (menu_highlight_index == 6) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_6;
			if (menu_highlight_index == 6) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::duplicate_tex);
			if (menu_highlight_index == 6) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 7 (exit region) */
			/* index = 5 */
			if (menu_highlight_index == 5) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_5;
			if (menu_highlight_index == 5) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::delete_tex);
			if (menu_highlight_index == 5) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 1 */
			if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_1;
			if (menu_highlight_index == 1) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::undo_tex);
			if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 3 */
			if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_3;
			if (menu_highlight_index == 3) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::objectmode_tex);
			if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_white);
			}
			/* Center */
			*((Coord3Df*)t_icon.m[3]) = p8_stick;
			m = t_icon * t;
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_string(menu_str.c_str(), 0.009f, 0.012f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.0f, 0.001f);
			break;
		}
		case MENUTYPE_MAIN_12: {
			/* index = 0 */
			if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_0;
			if (menu_highlight_index == 0) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::alt_tex);
			if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 4 */
			if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_4;
			if (menu_highlight_index == 4) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::object_tex);
			if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 6 */
			if (menu_highlight_index == 6) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_6;
			if (menu_highlight_index == 6) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::duplicate_tex);
			if (menu_highlight_index == 6) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 2 */
			if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_2;
			if (menu_highlight_index == 2) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::redo_tex);
			if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 8 */
			if (menu_highlight_index == 8) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_8;
			if (menu_highlight_index == 8) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::duplicate_tex);
			if (menu_highlight_index == 8) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 10 */
			if (menu_highlight_index == 10) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_10;
			if (menu_highlight_index == 10) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::object_tex);
			if (menu_highlight_index == 10) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 11 (exit region) */
			/* index = 9 */
			if (menu_highlight_index == 9) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_9;
			if (menu_highlight_index == 9) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::objectmode_tex);
			if (menu_highlight_index == 9) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 7 */
			if (menu_highlight_index == 7) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_7;
			if (menu_highlight_index == 7) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::delete_tex);
			if (menu_highlight_index == 7) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 1 */
			if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_1;
			if (menu_highlight_index == 1) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::undo_tex);
			if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 5 */
			if (menu_highlight_index == 5) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_5;
			if (menu_highlight_index == 5) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::delete_tex);
			if (menu_highlight_index == 5) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 3 */
			if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_3;
			if (menu_highlight_index == 3) {
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::objectmode_tex);
			if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_white);
			}
			/* Center */
			*((Coord3Df*)t_icon.m[3]) = p12_stick;
			m = t_icon * t;
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_string(menu_str.c_str(), 0.009f, 0.012f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.0f, 0.001f);
			break;
		}
		case MENUTYPE_TS_SELECT:
		case MENUTYPE_TS_ANNOTATE:
		case MENUTYPE_TS_MEASURE:
		default: {
			return;
		}
		}
	}
}

/***********************************************************************************************//**
 * \class                               Widget_Menu::Left
 ***************************************************************************************************
 * Interaction widget for a VR pie menu (left controller).
 *
 **************************************************************************************************/
Widget_Menu::Left Widget_Menu::Left::obj;

bool Widget_Menu::Left::has_click(VR_UI::Cursor& c) const
{
	return Widget_Menu::obj.has_click(c);
}

void Widget_Menu::Left::click(VR_UI::Cursor& c)
{
	Widget_Menu::obj.click(c);
}

bool Widget_Menu::Left::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_Menu::Left::drag_start(VR_UI::Cursor& c)
{
	Widget_Menu::obj.drag_start(c);
}

void Widget_Menu::Left::drag_contd(VR_UI::Cursor& c)
{
	Widget_Menu::obj.drag_contd(c);
}

void Widget_Menu::Left::drag_stop(VR_UI::Cursor& c)
{
	Widget_Menu::obj.drag_stop(c);
}

void Widget_Menu::Left::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
{
	Widget_Menu::obj.render_icon(t, controller_side, active, touched);
}

/***********************************************************************************************//**
 * \class                               Widget_Menu::Right
 ***************************************************************************************************
 * Interaction widget for a VR pie menu (right controller).
 *
 **************************************************************************************************/
Widget_Menu::Right Widget_Menu::Right::obj;

bool Widget_Menu::Right::has_click(VR_UI::Cursor& c) const
{
	return Widget_Menu::obj.has_click(c);
}

void Widget_Menu::Right::click(VR_UI::Cursor& c)
{
	Widget_Menu::obj.click(c);
}

bool Widget_Menu::Right::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_Menu::Right::drag_start(VR_UI::Cursor& c)
{
	Widget_Menu::obj.drag_start(c);
}

void Widget_Menu::Right::drag_contd(VR_UI::Cursor& c)
{
	Widget_Menu::obj.drag_contd(c);
}

void Widget_Menu::Right::drag_stop(VR_UI::Cursor& c)
{
	Widget_Menu::obj.drag_stop(c);
}

void Widget_Menu::Right::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
{
	Widget_Menu::obj.render_icon(t, controller_side, active, touched);
}