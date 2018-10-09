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
* The Original Code is Copyright (C) 2016 by Mike Erwin.
* All rights reserved.
*
* Contributor(s): Blender Foundation
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

#include "vr_math.h"
#include "vr_draw.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"

#include "BLI_math.h"

#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"

#include "gpencil_intern.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

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
	case TYPE_NAVI_TELEPORT:
		return &Widget_Navi::Teleport::obj;
	case TYPE_NAVI_JOYSTICK:
		return &Widget_Navi::Joystick::obj;
	case TYPE_SHIFT:
		return &Widget_Shift::obj;
	case TYPE_ALT:
		return &Widget_Alt::obj;
	case TYPE_CURSOR_OFFSET:
		return &Widget_CursorOffset::obj;
	case TYPE_ANNOTATE:
		return &Widget_Annotate::obj;
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
	if (str == "NAVI_TELEPORT") {
		return TYPE_NAVI_TELEPORT;
	}
	if (str == "NAVI_JOYSTICK") {
		return TYPE_NAVI_JOYSTICK;
	}
	if (str == "SHIFT") {
		return TYPE_SHIFT;
	}
	if (str == "ALT") {
		return TYPE_ALT;
	}
	if (str == "CURSOR_OFFSET") {
		return TYPE_CURSOR_OFFSET;
	}
	if (str == "ANNOTATE") {
		return TYPE_ANNOTATE;
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
	if (str == "NAVI_TELEPORT") {
		return &Widget_Navi::Teleport::obj;
	}
	if (str == "NAVI_JOYSTICK") {
		return &Widget_Navi::Joystick::obj;
	}
	if (str == "SHIFT") {
		return &Widget_Shift::obj;
	}
	if (str == "ALT") {
		return &Widget_Alt::obj;
	}
	if (str == "CURSOR_OFFSET") {
		return &Widget_CursorOffset::obj;
	}
	if (str == "ANNOTATE") {
		return &Widget_Annotate::obj;
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
	ret.push_back("NAVI_TELEPORT");
	ret.push_back("NAVI_JOYSTICK");
	ret.push_back("SHIFT");
	ret.push_back("ALT");
	ret.push_back("CURSOR_OFFSET");
	ret.push_back("ANNOTATE");

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
	case TYPE_NAVI_TELEPORT:
		return "NAVI_TELEPORT";
	case TYPE_NAVI_JOYSTICK:
		return "NAVI_JOYSTICK";
	case TYPE_SHIFT:
		return "SHIFT";
	case TYPE_ALT:
		return "ALT";
	case TYPE_CURSOR_OFFSET:
		return "CURSOR_OFFSET";
	case TYPE_ANNOTATE:
		return "ANNOTATE";
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

void VR_Widget::VR_Widget::render_icon(const Mat44f& t, bool active, bool touched)
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
	/* TODO_MARUI */

	return true;
}

bool Widget_Trigger::allows_focus_steal(Type by) const
{
	/* TODO_MARUI */

	return false;
}

void Widget_Trigger::click(VR_UI::Cursor& c)
{
	/* TODO_MARUI */

	Widget_Select::obj.click(c);
}

void Widget_Trigger::drag_start(VR_UI::Cursor& c)
{
	/* TODO_MARUI */

	Widget_Select::obj.drag_start(c);

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Trigger::obj.do_render[i] = true;
	}
}

void Widget_Trigger::drag_contd(VR_UI::Cursor& c)
{
	/* TODO_MARUI */

	Widget_Select::obj.drag_contd(c);

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Trigger::obj.do_render[i] = true;
	}
}

void Widget_Trigger::drag_stop(VR_UI::Cursor& c)
{
	/* TODO_MARUI */

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

bool Widget_Select::Raycast::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Select::Raycast::click(VR_UI::Cursor& c)
{
	/* TODO_MARUI */
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
	/* TODO_MARUI */

	VR_Side side = VR_UI::eye_dominance_get();
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

bool Widget_Select::Proximity::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Select::Proximity::click(VR_UI::Cursor& c)
{
	/* TODO_MARUI */
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
	/* TODO_MARUI */

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
			VR_Draw::set_color(0.0f, 0.7f, 1.0f, 0.1f);
			VR_Draw::render_box(p0i, p1i);
			VR_Draw::set_depth_test(true, false);
			VR_Draw::set_color(0.0f, 0.7f, 1.0f, 0.4f);
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
			VR_Draw::set_color(0.0f, 0.7f, 1.0f, 0.1f);
			VR_Draw::render_box(p0i, p1i);
			VR_Draw::set_depth_test(true, false);
			VR_Draw::set_color(0.0f, 0.7f, 1.0f, 0.4f);
			VR_Draw::render_box(p0i, p1i);
			VR_Draw::set_depth_test(true, true);
			break;
		}
		case VR_UI::SELECTIONVOLUMEALIGNMENT_REAL:
		{
			VR_Draw::update_modelview_matrix(&VR_Math::identity_f, 0);
			VR_Draw::set_depth_test(false, false);
			VR_Draw::set_color(0.0f, 0.7f, 1.0f, 0.1f);
			VR_Draw::render_box(p0, p1);
			VR_Draw::set_depth_test(true, false);
			VR_Draw::set_color(0.0f, 0.7f, 1.0f, 0.4f);
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
	case VR_UI::NAVIGATIONMODE_TELEPORT:
		return Teleport::obj.drag_start(c);
	case VR_UI::NAVIGATIONMODE_JOYSTICK:
		return Joystick::obj.drag_start(c);
	case VR_UI::NAVIGATIONMODE_NONE:
		return;
	}
}

void Widget_Navi::drag_contd(VR_UI::Cursor& c)
{
	switch (VR_UI::navigation_mode) {
	case VR_UI::NAVIGATIONMODE_GRABAIR:
		return GrabAir::obj.drag_contd(c);
	case VR_UI::NAVIGATIONMODE_TELEPORT:
		return Teleport::obj.drag_contd(c);
	case VR_UI::NAVIGATIONMODE_JOYSTICK:
		return Joystick::obj.drag_contd(c);
	case VR_UI::NAVIGATIONMODE_NONE:
		return;
	}
}

void Widget_Navi::drag_stop(VR_UI::Cursor& c)
{
	switch (VR_UI::navigation_mode) {
	case VR_UI::NAVIGATIONMODE_GRABAIR:
		return GrabAir::obj.drag_stop(c);
	case VR_UI::NAVIGATIONMODE_TELEPORT:
		return Teleport::obj.drag_stop(c);
	case VR_UI::NAVIGATIONMODE_JOYSTICK:
		return Joystick::obj.drag_stop(c);
	case VR_UI::NAVIGATIONMODE_NONE:
		return;
	}
}

void Widget_Navi::render_icon(const Mat44f& t, bool active, bool touched)
{
	switch (VR_UI::navigation_mode) {
	case VR_UI::NAVIGATIONMODE_GRABAIR:
		return GrabAir::obj.render_icon(t, active, touched);
	case VR_UI::NAVIGATIONMODE_TELEPORT:
		return Teleport::obj.render_icon(t, active, touched);
	case VR_UI::NAVIGATIONMODE_JOYSTICK:
		return Joystick::obj.render_icon(t, active, touched);
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
	/* else: single hand drag */

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
		else { /* z is up:
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

void Widget_Navi::GrabAir::render_icon(const Mat44f& t, bool active, bool touched)
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
 * \class                               Widget_Navi::Teleport
 ***************************************************************************************************
 * Interaction widget for teleport navigation.
 *
 **************************************************************************************************/
Widget_Navi::Teleport Widget_Navi::Teleport::obj;

float Widget_Navi::Teleport::transition_time(10.0f);
//                                                          _________________________________________
//_________________________________________________________/   Widget_Navi::Teleport::move_speed
/**
 * Movement speed of the arrow.
 */
float Widget_Navi::Teleport::move_speed(0.005f);

//                                                          _________________________________________
//_________________________________________________________/   Widget_Navi::Teleport::arrow_position
/**
 * Position of the arrow.
 */
Mat44f Widget_Navi::Teleport::arrow_position;


//                                                              ___________________________________
//_____________________________________________________________/ Widget_Navi::Teleport::render()
/**
 * * Apply the widget's custom render function (if any).
 */

void Widget_Navi::Teleport::render(VR_Side side) 
{
	const Mat44f& prior_model_matrix = VR_Draw::get_model_matrix();
	const Mat44f& prior_view_matrix = VR_Draw::get_view_matrix();
	const Mat44f prior_projection_matrix = VR_Draw::get_projection_matrix();
	
	VR_Draw::update_modelview_matrix(&arrow_position, 0);
	VR_Draw::set_color(0.0f, 0.7f, 1.0f, 0.1f);
	VR_Draw::render_arrow(Coord3Df(-0.01f, -0.01f, -0.01f), Coord3Df(0.01f, 0.01f, 0.01f), 0.2f);

	VR_Draw::update_modelview_matrix(&prior_model_matrix, &prior_view_matrix);
	VR_Draw::update_projection_matrix(prior_projection_matrix.m);

	// Set render flag to false to prevent redundant rendering from duplicate widgets
	Widget_Navi::Teleport::obj.do_render[side] = false;
}

//                                                              ___________________________________
//_____________________________________________________________/ Widget_Navi::Teleport::dragStart()
/**
 * Start a drag/hold-motion with the navigation button.
 * \param   hand    Pointer to the hand struct related to the interaction.
 */
void Widget_Navi::Teleport::drag_start(VR_UI::Cursor& c)
{
	// Remember where we started from in navigation space
	c.interaction_position = c.position;
	c.reference = c.position.get(VR_SPACE_REAL);

	arrow_position = c.reference * VR_UI::navigation_matrix_get();

}

void Widget_Navi::Teleport::drag_contd(VR_UI::Cursor& c)
{
	static Mat44f delta;
	delta.m[3][0] = (c.position.get(VR_SPACE_REAL).m[3][0] - c.reference.m[3][0]);
	delta.m[3][0] = delta.m[3][0] * abs(delta.m[3][0]) * -1.0f * move_speed;

	delta.m[3][1] = (c.position.get(VR_SPACE_REAL).m[3][1] - c.reference.m[3][1]);
	delta.m[3][1] = delta.m[3][1] * abs(delta.m[3][1]) * -1.0f * move_speed;
	
	float scale_factor = 1.05f;
	
	delta.m[0][0] *= scale_factor; delta.m[0][1] *= scale_factor; delta.m[0][2] *= scale_factor;
	delta.m[1][0] *= scale_factor; delta.m[1][1] *= scale_factor; delta.m[1][2] *= scale_factor;
	delta.m[2][0] *= scale_factor; delta.m[2][1] *= scale_factor; delta.m[2][2] *= scale_factor;

	arrow_position  = arrow_position * delta;
	Widget_Navi::Teleport::obj.do_render[0] = true;
}

void Widget_Navi::Teleport::drag_stop(VR_UI::Cursor& c)
{
	/* TODO_MARUI */
}

void Widget_Navi::Teleport::render_icon(const Mat44f& t, bool active, bool touched)
{
	/* TODO_MARUI */
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

		VR_UI::navigation_apply(delta, VR_SPACE_REAL);
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
	VR_UI::navigation_apply(delta, VR_SPACE_REAL);
}

void Widget_Navi::Joystick::drag_stop(VR_UI::Cursor& c)
{
	//
}

void Widget_Navi::Joystick::render_icon(const Mat44f& t, bool active, bool touched)
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
 * \class                               Widget_Shift
 ***************************************************************************************************
 * Interaction widget for emulating the shift key on a keyboard.
 *
 **************************************************************************************************/
Widget_Shift Widget_Shift::obj;

void Widget_Shift::render_icon(const Mat44f& t, bool active, bool touched)
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

void Widget_Alt::render_icon(const Mat44f& t, bool active, bool touched)
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

void Widget_CursorOffset::render_icon(const Mat44f& t, bool active, bool touched)
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
	VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::cursor_offset_tex);
}

/***********************************************************************************************//**
 * \class                               Widget_Annotate
 ***************************************************************************************************
 * Interaction widget for the gpencil annotation tool.
 *
 **************************************************************************************************/
Widget_Annotate Widget_Annotate::obj;

bGPdata *Widget_Annotate::gpd(0);
bGPDlayer *Widget_Annotate::gpl(0);
bGPDframe *Widget_Annotate::gpf(0);
Main *Widget_Annotate::main(0);

std::vector<bGPDspoint> Widget_Annotate::points;

//float Widget_Annotate::point_thickness(40.0f);
float Widget_Annotate::line_thickness(10.0f);
float Widget_Annotate::color[4] = { 0.6f, 0.0f, 1.0f, 1.0f };

bool Widget_Annotate::eraser(false);
VR_Side Widget_Annotate::cursor_side;
float Widget_Annotate::eraser_radius(0.05f);

int Widget_Annotate::init()
{
	/* Allocate gpencil data/layer/frame and set to active. */
	bContext *C = vr_get_obj()->ctx;
	gpd = BKE_gpencil_data_addnew(CTX_data_main(C), "Annotations");
	if (!gpd) {
		return -1;
	}
	gpd->flag |= (GP_DATA_ANNOTATIONS | GP_DATA_STROKE_EDITMODE);
	//gpd->xray_mode = GP_XRAY_3DSPACE;

	gpl = BKE_gpencil_layer_addnew(gpd, "VR_Annotations", true);
	if (!gpl) {
		BKE_gpencil_free(gpd, 0);
		return -1;
	}
	memcpy(gpl->color, color, sizeof(float) * 4);
	gpl->thickness = line_thickness / 1.15f;

	gpf = BKE_gpencil_frame_addnew(gpl, 0);
	if (!gpf) {
		BKE_gpencil_free(gpd, 1);
		return -1;
	}

	/* TODO_MARUI: Find a way to "coexist" with any existing scene gpd */
	Scene *scene = CTX_data_scene(C);
	scene->gpd = gpd;

	return 0;
}

void Widget_Annotate::erase_stroke(bGPDstroke *gps) {

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
				gp_stroke_delete_tagged_points(gpf, gps, gps->next, GP_SPOINT_TAG, false);
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
			gp_stroke_delete_tagged_points(gpf, gps, gps->next, GP_SPOINT_TAG, false);
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
//		/* TODO_MARUI: Find a way to "coexist" with any existing scene gpd. */
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
		if (!gpf || main != curr_main) {
			main = curr_main;
			int error = Widget_Annotate::init();
			if (error) {
				return;
			}
		}

		/* Loop over VR strokes and check if they should be erased.
		 * Maybe there's a better way to do this? */
		bGPDstroke *gpn;
		for (bGPDstroke *gps = (bGPDstroke*)gpf->strokes.first; gps; gps = gpn) {
			gpn = gps->next;
			Widget_Annotate::erase_stroke(gps);
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
		if (gpf) {
			/* Loop over VR strokes and check if they should be erased.
			 * Maybe there's a better way to do this? */
			bGPDstroke *gpn;
			for (bGPDstroke *gps = (bGPDstroke*)gpf->strokes.first; gps; gps = gpn) {
				gpn = gps->next;
				Widget_Annotate::erase_stroke(gps);
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
	/* Eraser */
	if (eraser) {
		return;
	}

	/* Finalize curve (save space data) */

	Main *curr_main = CTX_data_main(vr_get_obj()->ctx);
	if (!gpf || main != curr_main) {
		main = curr_main;
		int error = Widget_Annotate::init();
		if (error) {
			return;
		}
	}

	/* TODO_MARUI: Find a way to "coexist" with any existing scene gpd. */
	//BKE_gpencil_layer_setactive(gpd, gpl);

	/* Add new stroke. */
	int tot_points = points.size();
	bGPDstroke *gps = BKE_gpencil_add_stroke(gpf, 0, tot_points, line_thickness /*/25.0f*/);
	/* Could probably avoid the memcpy by allocating the stroke in drag_start()
	 * but it's nice to store the points in a vector. */
	memcpy(gps->points, &points[0], sizeof(bGPDspoint) * tot_points);
}

void Widget_Annotate::render(VR_Side side)
{
	int tot_points = points.size();

	/* Eraser */
	if (eraser) {
		const Mat44f& prior_model_matrix = VR_Draw::get_model_matrix();

		VR_Draw::update_modelview_matrix(&VR_UI::cursor_position_get(VR_SPACE_REAL, cursor_side), 0);
		VR_Draw::set_depth_test(false, false);
		VR_Draw::set_color(1.0f, 0.5f, 0.0f, 0.1f);
		VR_Draw::render_ball(eraser_radius);
		VR_Draw::set_depth_test(true, false);
		VR_Draw::set_color(1.0f, 0.5f, 0.0f, 0.4f);
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
