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

/** \file blender/vr/intern/vr_widget_navi.cpp
*   \ingroup vr
* 
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_navi.h"

#include "vr_math.h"
#include "vr_draw.h"

/***********************************************************************************************//**
 * \class                                  Widget_Navi
 ***************************************************************************************************
 * Interaction widget for VR navigation.
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
