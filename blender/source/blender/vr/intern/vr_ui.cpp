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

/** \file blender/vr/intern/vr_ui.cpp
*   \ingroup vr
* 
* The foundation of user interaction with VR content.
*/

#include "vr_types.h"

#include "vr_main.h"
#include "vr_widget.h"

#include "vr_ui.h"

#include "vr_widget_layout.h"

#include "vr_math.h"
#include "vr_draw.h"

#ifdef WIN32
#include "BLI_winstuff.h"
#else
#include <time.h>
#endif

#include "BLI_math.h"

#include "ED_undo.h"

#include "vr_api.h"

#include "WM_types.h"
#include "wm_window.h"

/* Externed from vr_types.h. */
ui64 VR_t_now(0);	/* Current (most recent) timestamp. This will be updated (1) when updating tracking (2) when starting rendering a new frame (3) before executing UI operations. */

/* Get the current timestamp in ms (system dependent). */
static ui64 currentSystemTime()
{
#ifdef WIN32
	SYSTEMTIME t;
	GetSystemTime(&t);
	return (((((ui64)(t.wDay) * 24 + t.wHour) * 60 + t.wMinute) * 60 + t.wSecond) * 1000 + t.wMilliseconds);
#else
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (ui64)(ts.tv_nsec / 1.0e6); //round(ts.tv_nsec / 1.0e6);
#endif
}

/***********************************************************************************************//**
 * \class                               VR_UI
 ***************************************************************************************************
 * VR_UI is the core of VR user interaction in Blender.
 * Every instance of VR UI is used by one user to translate any changes to the modeler.
 * VR_UI has a static part that implements a monitor to avoid race conditions, deadlocks
 * and collisions in multithreading (multi-user) environments.
 * It also serves as a default and "null" implementation of the VR_UI object.
 **************************************************************************************************/

VR_UI* VR_UI::ui = 0;
VR_UI_Type VR_UI::ui_type(VR_UI_TYPE_NULL);

Mat44f VR_UI::navigation_matrix;
Mat44f VR_UI::navigation_inverse;
float VR_UI::navigation_scale(1.0f);

VR_UI::NavigationMode VR_UI::navigation_mode(VR_UI::NAVIGATIONMODE_GRABAIR);
bool VR_UI::navigation_lock_up(false);
bool VR_UI::navigation_lock_rotation(false);
bool VR_UI::navigation_lock_altitude(false);
bool VR_UI::navigation_lock_translation(false);
bool VR_UI::navigation_lock_scale(false);

bool VR_UI::hmd_position_current[VR_SPACES][2] = {0};
bool VR_UI::eye_position_current[VR_SPACES][VR_SIDES][2] = {0};

float VR_UI::eye_baseline(0.050f);
VR_Side VR_UI::eye_dominance = VR_SIDE_RIGHT;

VR_Side VR_UI::hand_dominance(VR_SIDE_RIGHT);

bool VR_UI::controller_position_current[VR_SPACES][VR_MAX_CONTROLLERS][2] = {0};

VR_UI::SelectionMode VR_UI::selection_mode(SELECTIONMODE_RAYCAST);
float VR_UI::selection_tolerance(0.030f);

float VR_UI::drag_threshold_distance(VR_UI_DEFAULTDRAGTHRESDIST);
float VR_UI::drag_threshold_rotation(VR_UI_DEFAULTDRAGTHRESROT);
uint VR_UI::drag_threshold_time(VR_UI_DEFAULTDRAGTHRESTIME);

VR_UI::Cursor VR_UI::cursor[VR_MAX_CONTROLLERS] = { VR_UI::Cursor(), VR_UI::Cursor(), VR_UI::Cursor() };

VR_UI::CtrlState VR_UI::ctrl_key(CTRLSTATE_OFF);
VR_UI::ShiftState VR_UI::shift_key(SHIFTSTATE_OFF);
VR_UI::AltState VR_UI::alt_key(ALTSTATE_OFF);

bool VR_UI::cursor_offset_update(false);
bool VR_UI::cursor_offset_enabled(false);

bool VR_UI::mouse_cursor_enabled(false);
Mat44f VR_UI::viewport_projection[VR_SIDES];
rcti VR_UI::viewport_bounds;

bool VR_UI::updating(false);
ui64 VR_UI::fps_render(0);

int VR_UI::undo_count(0);
int VR_UI::redo_count(0);

bool VR_UI::pie_menu_active[VR_SIDES] = { 0 };
const VR_Widget *VR_UI::pie_menu[VR_SIDES] = { &Widget_Menu::Left::obj, &Widget_Menu::Right::obj };

const Mat44f& VR_UI::navigation_matrix_get()
{
	return VR_UI::navigation_matrix;
}

const Mat44f& VR_UI::navigation_inverse_get()
{
	return VR_UI::navigation_inverse;
}

const float& VR_UI::navigation_scale_get()
{
	return VR_UI::navigation_scale;
}

VR_UI::LMatrix2::LMatrix2()
{
	//
}

VR_UI::LMatrix2::LMatrix2(const LMatrix2& cpy)
{
	this->position[VR_SPACE_REAL].mat_curr = false;
	this->position[VR_SPACE_REAL].inv_curr = false;
	this->position[VR_SPACE_BLENDER].mat_curr = false;
	this->position[VR_SPACE_BLENDER].inv_curr = false;

	if (cpy.position[VR_SPACE_REAL].mat_curr) {
		this->position[VR_SPACE_REAL].mat = cpy.position[VR_SPACE_REAL].mat;
		this->position[VR_SPACE_REAL].mat_curr = true;
	}
	if (cpy.position[VR_SPACE_REAL].inv_curr) {
		this->position[VR_SPACE_REAL].inv = cpy.position[VR_SPACE_REAL].inv;
		this->position[VR_SPACE_REAL].inv_curr = true;
	}
	if (cpy.position[VR_SPACE_BLENDER].mat_curr) {
		this->position[VR_SPACE_BLENDER].mat = cpy.position[VR_SPACE_BLENDER].mat;
		this->position[VR_SPACE_BLENDER].mat_curr = true;
	}
	if (cpy.position[VR_SPACE_BLENDER].inv_curr) {
		this->position[VR_SPACE_BLENDER].inv = cpy.position[VR_SPACE_BLENDER].inv;
		this->position[VR_SPACE_BLENDER].inv_curr = true;
	}
}

VR_UI::LMatrix2& VR_UI::LMatrix2::operator= (const LMatrix2 &o)
{
	this->position[VR_SPACE_REAL].mat_curr = false;
	this->position[VR_SPACE_REAL].inv_curr = false;
	this->position[VR_SPACE_BLENDER].mat_curr = false;
	this->position[VR_SPACE_BLENDER].inv_curr = false;

	if (o.position[VR_SPACE_REAL].mat_curr) {
		this->position[VR_SPACE_REAL].mat = o.position[VR_SPACE_REAL].mat;
		this->position[VR_SPACE_REAL].mat_curr = true;
	}
	if (o.position[VR_SPACE_REAL].inv_curr) {
		this->position[VR_SPACE_REAL].inv = o.position[VR_SPACE_REAL].inv;
		this->position[VR_SPACE_REAL].inv_curr = true;
	}
	if (o.position[VR_SPACE_BLENDER].mat_curr) {
		this->position[VR_SPACE_BLENDER].mat = o.position[VR_SPACE_BLENDER].mat;
		this->position[VR_SPACE_BLENDER].mat_curr = true;
	}
	if (o.position[VR_SPACE_BLENDER].inv_curr) {
		this->position[VR_SPACE_BLENDER].inv = o.position[VR_SPACE_BLENDER].inv;
		this->position[VR_SPACE_BLENDER].inv_curr = true;
	}
	return *this;
}

void VR_UI::LMatrix2::set(const float m[4][4], VR_Space s)
{
	switch (s) {
	case VR_SPACE_REAL:
		position[VR_SPACE_REAL].mat = m;
		position[VR_SPACE_REAL].mat_curr = true;
		position[VR_SPACE_REAL].inv_curr = false;
		position[VR_SPACE_BLENDER].mat_curr = false;
		position[VR_SPACE_BLENDER].inv_curr = false;
		break;
	case VR_SPACE_BLENDER:
		position[VR_SPACE_BLENDER].mat = m;
		position[VR_SPACE_REAL].mat = position[VR_SPACE_BLENDER].mat * navigation_inverse;
		position[VR_SPACE_REAL].mat_curr = true;
		position[VR_SPACE_REAL].inv_curr = false;
		position[VR_SPACE_BLENDER].mat_curr = true;
		position[VR_SPACE_BLENDER].inv_curr = false;
	default:
		return;
	}
}

const Mat44f& VR_UI::LMatrix2::get(VR_Space s, bool inverse)
{
	if (inverse) {
		if (!position[s].inv_curr) {
			if (s == VR_SPACE_REAL) {
				position[VR_SPACE_REAL].inv = position[VR_SPACE_REAL].mat.inverse();
				position[VR_SPACE_REAL].inv_curr = true;
			}
			else { /* VR_SPACE_BLENDER */
				if (!position[VR_SPACE_BLENDER].mat_curr) {
					position[VR_SPACE_BLENDER].mat = position[VR_SPACE_REAL].mat * navigation_matrix;
					position[VR_SPACE_BLENDER].mat_curr = true;
				}
				position[VR_SPACE_BLENDER].inv = position[VR_SPACE_BLENDER].mat.inverse();
				position[VR_SPACE_BLENDER].inv_curr = true;
			}
		}
		return position[s].inv;
	} /* else: non-inverse */

	if (!position[s].mat_curr) {
		/* VR_SPACE_REAL is always available
	     * so it must be Blender space we're missing. */
		position[VR_SPACE_BLENDER].mat = position[VR_SPACE_REAL].mat * navigation_matrix;
		position[VR_SPACE_BLENDER].mat_curr = true;
	}
	return position[s].mat;
}

VR_UI::Cursor::Cursor()
	: active(false)
	, visible(false)
	, last_upd(0)
	, last_buttons(0)
	, trigger(0)
	, ctrl(CTRLSTATE_OFF)
	, shift(SHIFTSTATE_OFF)
	, alt(ALTSTATE_OFF)
	, interaction_state(BUTTONSTATE_IDLE)
	, interaction_button(0)
	, interaction_time(0)
	, interaction_ctrl(CTRLSTATE_OFF)
	, interaction_shift(SHIFTSTATE_OFF)
	, interaction_alt(ALTSTATE_OFF)
	, interaction_widget(0)
	, other_hand(0)
	, bimanual(BIMANUAL_OFF)
	, side(VR_SIDE_MONO)
	, target_obj(0)
	, offset_pos(0, 0, 0)
{
	offset_rot.set_to_identity();
}

const Mat44f& VR_UI::cursor_position_get(VR_Space space, VR_Side side, bool inverse)
{
	if (side == VR_SIDE_DOMINANT) {
		side = VR_UI::hand_dominance;
	}
	if (side < 0 || side > 2) {
		return VR_Math::identity_f;
	}
	return VR_UI::cursor[side].position.get(space, inverse);
}

const Mat44f& VR_UI::cursor_interaction_position_get(VR_Space space, VR_Side side, bool inverse)
{
	if (side == VR_SIDE_DOMINANT) {
		side = VR_UI::hand_dominance;
	}
	if (side < 0 || side > 2) {
		return VR_Math::identity_f;
	}
	return VR_UI::cursor[side].interaction_position.get(space, inverse);
}

ui64 VR_UI::cursor_buttons_get(VR_Side side)
{
	if (side != VR_SIDE_LEFT && side != VR_SIDE_RIGHT) {
		side = VR_UI::hand_dominance;
	}
	VR *vr = vr_get_obj();
	return vr->controller[side]->buttons;
}

bool VR_UI::cursor_trigger_get(VR_Side side)
{
	if (side != VR_SIDE_LEFT && side != VR_SIDE_RIGHT) {
		side = VR_UI::hand_dominance;
	}
	return VR_UI::cursor[side].trigger;
}

bool VR_UI::cursor_active_get(VR_Side side)
{
	if (side != VR_SIDE_LEFT && side != VR_SIDE_RIGHT) {
		side = VR_UI::hand_dominance;
	}
	return VR_UI::cursor[side].active;
}

bool VR_UI::cursor_visible_get(VR_Side side)
{
	if (side != VR_SIDE_LEFT && side != VR_SIDE_RIGHT) {
		side = VR_UI::hand_dominance;
	}
	return VR_UI::cursor[side].visible;
}

void VR_UI::cursor_position_set(VR_Space space, VR_Side side, const Mat44f& m)
{
	if (side == VR_SIDE_DOMINANT) {
		side = VR_UI::hand_dominance;
	}
	
	VR_UI::cursor[side].position.set(m.m, space);
}

void VR_UI::cursor_active_set(VR_Side side, bool b) {
	if (side == VR_SIDE_DOMINANT) {
		side = VR_UI::hand_dominance;
	}
	if (side == VR_SIDE_LEFT) {
		VR_UI::cursor[VR_SIDE_LEFT].active = b;
	}
	else if (side == VR_SIDE_RIGHT) {
		VR_UI::cursor[VR_SIDE_RIGHT].active = b;
	}
	else if (side == VR_SIDE_BOTH) {
		VR_UI::cursor[VR_SIDE_LEFT].active = b;
		VR_UI::cursor[VR_SIDE_RIGHT].active = b;
	}
}

void VR_UI::cursor_visible_set(VR_Side side, bool v) {
	if (side == VR_SIDE_DOMINANT) {
		side = VR_UI::hand_dominance;
	}
	if (side == VR_SIDE_LEFT) {
		VR_UI::cursor[VR_SIDE_LEFT].visible = v;
	}
	else if (side == VR_SIDE_RIGHT) {
		VR_UI::cursor[VR_SIDE_RIGHT].visible = v;
	}
	else if (side == VR_SIDE_BOTH) {
		VR_UI::cursor[VR_SIDE_LEFT].visible = v;
		VR_UI::cursor[VR_SIDE_RIGHT].visible = v;
	}
}

VR_UI::CtrlState VR_UI::ctrl_key_get()
{
	return VR_UI::ctrl_key;
}

VR_UI::ShiftState VR_UI::shift_key_get()
{
	return VR_UI::shift_key;
}

VR_UI::AltState VR_UI::alt_key_get()
{
	return VR_UI::alt_key;
}

void VR_UI::ctrl_key_set(VR_UI::CtrlState state)
{
	VR_UI::ctrl_key = state;
}

void VR_UI::shift_key_set(VR_UI::ShiftState state)
{
	VR_UI::shift_key = state;
}

void VR_UI::alt_key_set(VR_UI::AltState state)
{
	VR_UI::alt_key = state;
}

VR_Widget* VR_UI::get_current_tool(VR_Side side)
{
	if (!VR_Widget_Layout::current_layout) {
		return NULL;
	}

	VR_UI *ui = VR_UI::i();
	if (!ui) {
		return NULL;
	}
	uint type = ui->type();
	VR_UI::AltState alt = VR_UI::alt_key;

	for (int i = 0; i < VR_Widget_Layout::layouts[type].size(); ++i) {
		if (VR_Widget_Layout::layouts[type][i]->name == VR_Widget_Layout::current_layout->name) {
			/* The currently active tool is the one mapped to the controller trigger. */
			return VR_Widget_Layout::layouts[type][i]->m[side][VR_Widget_Layout::ButtonID::BUTTONID_TRIGGER][alt];
		}
	}

	return NULL;
}

VR_UI::Error VR_UI::set_current_tool(const VR_Widget *tool, VR_Side side)
{
	if (!VR_Widget_Layout::current_layout) {
		return VR_UI::ERROR_NOTINITIALIZED;
	}

	VR_UI *ui = VR_UI::i();
	if (!ui) {
		return VR_UI::ERROR_INTERNALFAILURE;
	}
	uint type = ui->type();
	VR_UI::AltState alt = VR_UI::alt_key;

	for (int i = 0; i < VR_Widget_Layout::layouts[type].size(); ++i) {
		if (VR_Widget_Layout::layouts[type][i]->name == VR_Widget_Layout::current_layout->name) {
			/* The currently active tool is the one mapped to the controller trigger. */
			VR_Widget_Layout::layouts[type][i]->m[side][VR_Widget_Layout::ButtonID::BUTTONID_TRIGGER][alt] = (VR_Widget*)tool;
			break;
		}
	}
	return VR_UI::ERROR_NONE;
}

void VR_UI::navigation_set(const Mat44f& m)
{
	/* Find out the unit-to-real-meters scale. */
	const Coord3Df *x_axis = (Coord3Df*)m.m;
	float length = x_axis->length();
	if (length < VR_UI_MINNAVIGATIONSCALE || length > VR_UI_MAXNAVIGATIONSCALE) {
		return; /* To avoid hitting the "singularity" or clipping the scene out of visibility */
	}
	VR_UI::navigation_scale = length;

	VR_UI::navigation_matrix = m;

	/* Never allow skew or inhomogeneous matrices. */
	VR_UI::navigation_matrix.m[0][3] = 0;
	VR_UI::navigation_matrix.m[1][3] = 0;
	VR_UI::navigation_matrix.m[2][3] = 0;
	VR_UI::navigation_matrix.m[3][3] = 1;

	VR_UI::navigation_inverse = VR_UI::navigation_matrix.inverse();
}

void VR_UI::navigation_apply_transformation(const Mat44f& m, VR_Space space, bool inverse)
{
	if (space == VR_SPACE_BLENDER) {
		VR_UI::navigation_set(VR_UI::navigation_matrix * (inverse ? m : m.inverse()));
	}
	else {
		VR_UI::navigation_set((inverse ? m : m.inverse()) * VR_UI::navigation_matrix);
	}
}

void VR_UI::navigation_reset()
{
	VR_UI::navigation_matrix.set_to_identity();
	VR_UI::navigation_inverse.set_to_identity();
	VR_UI::navigation_scale = 1.0f;
	/* If Blender says the y-axis is up,
	 * apply it as a navigation (just flip the content). */
	if (!is_zaxis_up()) {
		// Need to rotate +90deg around the x-axis
		VR_UI::navigation_matrix.m[1][1] = 0;
		VR_UI::navigation_matrix.m[2][2] = 0;
		VR_UI::navigation_matrix.m[2][1] = 1;
		VR_UI::navigation_matrix.m[1][2] = -1;
		// Inverse:
		VR_UI::navigation_inverse.m[1][1] = 0;
		VR_UI::navigation_inverse.m[2][2] = 0;
		VR_UI::navigation_inverse.m[2][1] = -1;
		VR_UI::navigation_inverse.m[1][2] = 1;
	} /* else : z-axis is up, no navigation required */
}

const Mat44f& VR_UI::hmd_position_get(VR_Space space, bool inverse)
{
	/* Get all transforms from VR main module to avoid double-copying matrices. */
	VR *vr = vr_get_obj();

	if (space == VR_SPACE_REAL) {
		if (inverse) {
			if (!VR_UI::hmd_position_current[VR_SPACE_REAL][1]) {
				invert_m4_m4(vr->t_hmd_inv[VR_SPACE_REAL], vr->t_hmd[VR_SPACE_REAL]);
				VR_UI::hmd_position_current[VR_SPACE_REAL][1] = true;
			}
			return *(Mat44f*)vr->t_hmd_inv[VR_SPACE_REAL];
		}
		else {
			return *(Mat44f*)vr->t_hmd[VR_SPACE_REAL];
		}
	}
	else { // VR_SPACE_BLENDER
		if (!VR_UI::hmd_position_current[VR_SPACE_BLENDER][0]) {
			_va_mul_m4_series_3(vr->t_hmd[VR_SPACE_BLENDER], vr->t_hmd[VR_SPACE_REAL], VR_UI::navigation_matrix.m);
			VR_UI::hmd_position_current[VR_SPACE_BLENDER][0] = true;
		}
		if (inverse) {
			if (!VR_UI::hmd_position_current[VR_SPACE_BLENDER][1]) {
				invert_m4_m4(vr->t_hmd_inv[VR_SPACE_BLENDER], vr->t_hmd[VR_SPACE_BLENDER]);
				VR_UI::hmd_position_current[VR_SPACE_BLENDER][1] = true;
			}
			return *(Mat44f*)vr->t_hmd_inv[VR_SPACE_BLENDER];
		}
		else {
			return *(Mat44f*)vr->t_hmd[VR_SPACE_BLENDER];
		}
	}
}

const Mat44f& VR_UI::eye_position_get(VR_Space space, VR_Side side, bool inverse)
{
	if (side == VR_SIDE_DOMINANT) {
		side = VR_UI::eye_dominance;
	}

	/* Get all transforms from VR main module to avoid double-copying matrices. */
	VR *vr = vr_get_obj();
	
	if (space == VR_SPACE_REAL) {
		if (inverse) {
			if (!VR_UI::eye_position_current[VR_SPACE_REAL][side][1]) {
				invert_m4_m4(vr->t_eye_inv[VR_SPACE_REAL][side], vr->t_eye[VR_SPACE_REAL][side]);
				VR_UI::eye_position_current[VR_SPACE_REAL][side][1] = true;
			}
			return *(Mat44f*)vr->t_eye_inv[VR_SPACE_REAL][side];
		}
		else {
			return *(Mat44f*)vr->t_eye[VR_SPACE_REAL][side];
		}
	}
	else { // VR_SPACE_BLENDER
		if (!VR_UI::eye_position_current[VR_SPACE_BLENDER][side][0]) {
			_va_mul_m4_series_3(vr->t_eye[VR_SPACE_BLENDER][side], vr->t_eye[VR_SPACE_REAL][side], VR_UI::navigation_matrix.m);
			VR_UI::eye_position_current[VR_SPACE_BLENDER][side][0] = true;
		}
		if (inverse) {
			if (!VR_UI::eye_position_current[VR_SPACE_BLENDER][side][1]) {
				invert_m4_m4(vr->t_eye_inv[VR_SPACE_BLENDER][side], vr->t_eye[VR_SPACE_BLENDER][side]);
				VR_UI::eye_position_current[VR_SPACE_BLENDER][side][1] = true;
			}
			return *(Mat44f*)vr->t_eye_inv[VR_SPACE_BLENDER][side];
		}
		else {
			return *(Mat44f*)vr->t_eye[VR_SPACE_BLENDER][side];
		}
	}
}

const Mat44f& VR_UI::controller_position_get(VR_Space space, VR_Side side, bool inverse)
{
	if (side == VR_SIDE_DOMINANT) {
		side = VR_UI::hand_dominance;
	}

	/* Get transforms from VR main module to avoid double-copying matrices. */
	VR *vr = vr_get_obj();

	if (space == VR_SPACE_REAL) {
		if (inverse) {
			if (!VR_UI::controller_position_current[VR_SPACE_REAL][side][1]) {
				invert_m4_m4(vr->t_controller_inv[VR_SPACE_REAL][side], vr->t_controller[VR_SPACE_REAL][side]);
				VR_UI::controller_position_current[VR_SPACE_REAL][side][1] = true;
			}
			return *(Mat44f*)vr->t_controller_inv[VR_SPACE_REAL][side];
		}
		else {
			return *(Mat44f*)vr->t_controller[VR_SPACE_REAL][side];
		}
	}
	else { // VR_SPACE_BLENDER
		if (!VR_UI::controller_position_current[VR_SPACE_BLENDER][side][0]) {
			_va_mul_m4_series_3(vr->t_controller[VR_SPACE_BLENDER][side], vr->t_controller[VR_SPACE_REAL][side], VR_UI::navigation_matrix.m);
			VR_UI::controller_position_current[VR_SPACE_BLENDER][side][0] = true;
		}
		if (inverse) {
			if (!VR_UI::controller_position_current[VR_SPACE_BLENDER][side][1]) {
				invert_m4_m4(vr->t_controller_inv[VR_SPACE_BLENDER][side], vr->t_controller[VR_SPACE_BLENDER][side]);
				VR_UI::controller_position_current[VR_SPACE_BLENDER][side][1] = true;
			}
			return *(Mat44f*)vr->t_controller_inv[VR_SPACE_BLENDER][side];
		}
		else {
			return *(Mat44f*)vr->t_controller[VR_SPACE_BLENDER][side];
		}
	}
}

float VR_UI::eye_baseline_get()
{
	return VR_UI::eye_baseline;
}

VR_Side VR_UI::eye_dominance_get()
{
	return VR_UI::eye_dominance;
}

void VR_UI::eye_baseline_set(float baseline)
{
	VR_UI::eye_baseline = baseline;
}

void VR_UI::eye_dominance_set(VR_Side side)
{
	if (side < 0)
		return;
	if (side > 1)
		return;
	VR_UI::eye_dominance = side;
}

VR_Side VR_UI::hand_dominance_get()
{
	return VR_UI::hand_dominance;
}

void VR_UI::hand_dominance_set(VR_Side side)
{
	if (side == VR_SIDE_LEFT || side == VR_SIDE_RIGHT) {
		VR_UI::hand_dominance = side;
	}
}

Mat44f VR_UI::convert_space(const Mat44f& m, VR_Space m_space, VR_Space target_space)
{
	static Mat44f out;
	if (target_space == VR_SPACE_REAL) {
		if (m_space == VR_SPACE_BLENDER) {
			out = m * VR_UI::navigation_inverse;
		}
	}
	else { /* VR_SPACE_BLENDER */
		if (m_space == VR_SPACE_REAL) {
			out = m * VR_UI::navigation_matrix;
		}
	}
	return out;
}

Coord3Df VR_UI::convert_space(const Coord3Df& v, VR_Space v_space, VR_Space target_space)
{
	const Mat44f& m = VR_UI::convert_space(VR_Math::identity_f, v_space, target_space);
	Coord3Df ret(
		v.x*m.m[0][0] + v.y*m.m[1][0] + v.z*m.m[2][0] + m.m[3][0],
		v.x*m.m[0][1] + v.y*m.m[1][1] + v.z*m.m[2][1] + m.m[3][1],
		v.x*m.m[0][2] + v.y*m.m[1][2] + v.z*m.m[2][2] + m.m[3][2]
	);
	return ret;
}

int VR_UI::get_screen_coordinates(const Coord3Df& c, float& x, float& y, VR_Side side)
{
	/* 1: Transformation: */
	const Mat44f& t = VR_UI::eye_position_get(VR_SPACE_REAL, side, true);
	float x_t = c.x * t.m[0][0] + c.y * t.m[1][0] + c.z * t.m[2][0] + t.m[3][0];
	float y_t = c.x * t.m[0][1] + c.y * t.m[1][1] + c.z * t.m[2][1] + t.m[3][1];
	float z_t = c.x * t.m[0][2] + c.y * t.m[1][2] + c.z * t.m[2][2] + t.m[3][2];
	/* x_t, y_t, z_t now in camera-relative coordinates */

	/* 2: Projection: */
	const Mat44f& p = VR_Draw::get_projection_matrix();
	float x_s = x_t * p.m[0][0] + y_t * p.m[1][0] + z_t * p.m[2][0] + p.m[3][0];
	float y_s = x_t * p.m[0][1] + y_t * p.m[1][1] + z_t * p.m[2][1] + p.m[3][1];
	//float z_s = x_t * p.m[0][2] + y_t * p.m[1][2] + z_t * p.m[2][2] + p.m[3][2];
	float w_s = x_t * p.m[0][3] + y_t * p.m[1][3] + z_t * p.m[2][3] + p.m[3][3];
	if (w_s == 0.0f) {
		w_s = 0.001f;
	}
	x_s /= w_s;
	y_s /= w_s;
	//z_s /= w_s;
	/* x_s, y_s, z_s now in screen coordinates (-1 ~ 1) */

	x = x_s;
	y = y_s;

	return 0;
}

int VR_UI::get_pixel_coordinates(const Coord3Df& c, int& x, int& y, VR_Side side)
{
	/* 1: Transformation: */
	const Mat44f& t = VR_UI::eye_position_get(VR_SPACE_REAL, side, true);
	float x_t = c.x * t.m[0][0] + c.y * t.m[1][0] + c.z * t.m[2][0] + t.m[3][0];
	float y_t = c.x * t.m[0][1] + c.y * t.m[1][1] + c.z * t.m[2][1] + t.m[3][1];
	float z_t = c.x * t.m[0][2] + c.y * t.m[1][2] + c.z * t.m[2][2] + t.m[3][2];
	/* x_t, y_t, z_t now in camera-relative coordinates */

	/* 2: Projection: */
	const Mat44f& p = VR_Draw::get_projection_matrix();
	float x_s = x_t * p.m[0][0] + y_t * p.m[1][0] + z_t * p.m[2][0] + p.m[3][0];
	float y_s = x_t * p.m[0][1] + y_t * p.m[1][1] + z_t * p.m[2][1] + p.m[3][1];
	//float z_s = x_t * p.m[0][2] + y_t * p.m[1][2] + z_t * p.m[2][2] + p.m[3][2];
	float w_s = x_t * p.m[0][3] + y_t * p.m[1][3] + z_t * p.m[2][3] + p.m[3][3];
	if (w_s == 0.0f) {
		w_s = 0.001f;
	}
	x_s /= w_s;
	y_s /= w_s;
	//z_s /= w_s;
	/* x_s, y_s, z_s now in screen coordinates (-1 ~ 1) */

	/* 3: Map to pixel coordinates: */
	VR* vr = vr_get_obj();
	x = int(float(vr->tex_width) * (x_s + 1.0f) / 2.0f);
	y = int(float(vr->tex_height) * (1.0f - y_s) / 2.0f);

	return 0;
}

bool VR_UI::is_available(VR_UI_Type type)
{
	switch (type) {
		case VR_UI_TYPE_NULL:
#ifdef WIN32
		case VR_UI_TYPE_OCULUS:
		case VR_UI_TYPE_MICROSOFT:
		case VR_UI_TYPE_FOVE:
#endif
		case VR_UI_TYPE_VIVE: {
			return true;
		}
		default: {
			return false;
		}
	}
}

VR_UI::Error VR_UI::set_ui(VR_UI_Type type)
{
	/* If we already have a UI implementation object, delete it first. */
	if (VR_UI::ui) {
		delete VR_UI::ui;
		VR_UI::ui = 0;
	}

	VR_UI::ui = new VR_UI();
	VR_UI::ui_type = type;

	/* Will automatically assign widget layout based on ui_type */
	//VR_Widget_Layout::resetToDefaultLayouts();

	return ERROR_NONE;
}

VR_UI::Error VR_UI::shutdown()
{
	/* If we have a UI implementation object, delete it. */
	if (VR_UI::ui) {
		delete VR_UI::ui;
		VR_UI::ui = 0;
	}

	/* Close all windows. */
	//MenuWindow::shutdown();

	return ERROR_NONE;
}

VR_UI* VR_UI::i()
{
	if (!VR_UI::ui) {
		VR_UI::ui = new VR_UI(); /* Use dummy UI */
	}
	return VR_UI::ui;
}

VR_UI::VR_UI()
{
	VR_UI::cursor[VR_SIDE_LEFT] = Cursor();
	VR_UI::cursor[VR_SIDE_LEFT].side = VR_SIDE_LEFT;
	VR_UI::cursor[VR_SIDE_LEFT].other_hand = &VR_UI::cursor[VR_SIDE_RIGHT];
	VR_UI::cursor[VR_SIDE_RIGHT] = Cursor();
	VR_UI::cursor[VR_SIDE_RIGHT].side = VR_SIDE_RIGHT;
	VR_UI::cursor[VR_SIDE_RIGHT].other_hand = &VR_UI::cursor[VR_SIDE_LEFT];
	/* Extra / auxiliary cursors (VR_Side_AUX) */
	for (int i = 2; i < VR_MAX_CONTROLLERS; ++i) {
		VR_UI::cursor[i] = Cursor();
		VR_UI::cursor[i].side = VR_SIDE_AUX;
		VR_UI::cursor[i].other_hand = &VR_UI::cursor[VR_SIDE_LEFT];
	}
	
	VR_UI::navigation_matrix.set_to_identity();
	VR_UI::navigation_inverse.set_to_identity();
}

VR_UI::VR_UI(VR_UI& cpy)
{
	//
}

VR_UI::~VR_UI()
{
	//
}

VR_UI_Type VR_UI::type()
{
	return VR_UI::ui_type;
}

void VR_UI::navigation_fit_scene()
{
	/* TODO_XR */
}

void VR_UI::navigation_fit_selection(VR_Direction look_from_direction)
{
	/* TODO_XR */
}

float VR_UI::scene_unit_scale(VR_Space space)
{
	/* Get Blender scale setting in Blender internal units (meters): */
	/* TODO_XR */
	float s = 1.0f;

	if (space == VR_SPACE_REAL) {
		s /= VR_UI::navigation_scale;
	}
	return s;
}

bool VR_UI::is_zaxis_up()
{
	/* TODO_XR */

	return true;
}

void VR_UI::navigation_orient_up(Coord3Df* pivot)
{
	/* TODO_XR */
}

void VR_UI::cursor_offset_set(VR_Side side, const Mat44f& rot, const Coord3Df& pos)
{
	if (side == VR_SIDE_DOMINANT) {
		side = VR_UI::hand_dominance;
	}
	if (side != VR_SIDE_LEFT && side != VR_SIDE_RIGHT) {
		return;
	}
	VR_UI::cursor[side].offset_rot = rot;
	VR_UI::cursor[side].offset_pos = pos;
}

VR_UI::Error VR_UI::update_tracking()
{
	/* Update the statuses of the VR tracking matrices. */
	VR_UI::hmd_position_current[VR_SPACE_REAL][0] = true;
	VR_UI::hmd_position_current[VR_SPACE_REAL][1] = false;
	VR_UI::hmd_position_current[VR_SPACE_BLENDER][0] = false;
	VR_UI::hmd_position_current[VR_SPACE_BLENDER][1] = false;

	for (int i = 0; i < VR_SIDES; ++i) {
		VR_UI::eye_position_current[VR_SPACE_REAL][i][0] = true;
		VR_UI::eye_position_current[VR_SPACE_REAL][i][1] = true; /* Will be calculated each frame in vr_update_view_matrix().. */
		VR_UI::eye_position_current[VR_SPACE_BLENDER][i][0] = true; /* Will be calculated each frame in vr_compute_viewmat(). */
		VR_UI::eye_position_current[VR_SPACE_BLENDER][i][1] = false;
	}

	for (int i = 0; i < VR_MAX_CONTROLLERS; ++i) {
		VR_UI::controller_position_current[VR_SPACE_REAL][i][0] = true;
		VR_UI::controller_position_current[VR_SPACE_REAL][i][1] = false;
		VR_UI::controller_position_current[VR_SPACE_BLENDER][i][0] = false;
		VR_UI::controller_position_current[VR_SPACE_BLENDER][i][1] = false;
	}

	/* Update the fps monitor. */
	static ui64 prev_update = VR_t_now;
	VR_t_now = currentSystemTime();
	ui64 duration = VR_t_now - prev_update;
	if (duration > 0) {
		VR_UI::fps_render = (1000 / (duration));
	}
	ui64 now = prev_update = VR_t_now;

	/* Update the controller states. */
	VR *vr = vr_get_obj();

	if (vr->controller[VR_SIDE_LEFT]->available) {
		VR_UI::cursor[VR_SIDE_LEFT].active = true;
		VR_UI::cursor[VR_SIDE_LEFT].visible = true;
		/* Apply buttons and position. */
		const Mat44f& controller = *(Mat44f*)vr->t_controller[VR_SPACE_REAL][VR_SIDE_LEFT];
		if (VR_UI::cursor_offset_enabled) {
			const Mat44f& cursor = VR_UI::cursor[VR_SIDE_LEFT].position.position[VR_SPACE_REAL].mat;
			const Coord3Df& cursor_pos = *(Coord3Df*)cursor.m[3];
			Coord3Df& offset_pos = VR_UI::cursor[VR_SIDE_LEFT].offset_pos;
			Mat44f& offset_rot = VR_UI::cursor[VR_SIDE_LEFT].offset_rot;
			if (VR_UI::cursor_offset_update) { /* update so that controller position stays the same */
				/* Rotational difference: */
				offset_rot = cursor * controller_position_get(VR_SPACE_REAL, VR_SIDE_LEFT, true);
				offset_rot.m[3][0] = offset_rot.m[3][1] = offset_rot.m[3][2] = 0;
				/* Translational difference: */
				const Coord3Df& controller_pos = *(Coord3Df*)controller.m[3];
				offset_pos = cursor_pos - controller_pos;
			}
			Mat44f new_cursor;
			new_cursor = controller;
			new_cursor.m[3][0] = new_cursor.m[3][1] = new_cursor.m[3][2] = 0;
			new_cursor = offset_rot * new_cursor;
			*((Coord3Df*)new_cursor.m[3]) = (*(Coord3Df*)controller.m[3]) + offset_pos;
			VR_UI::cursor[VR_SIDE_LEFT].position.set(new_cursor.m);
		}
		else { /* cursor offset disabled */
			VR_UI::cursor[VR_SIDE_LEFT].position.set(controller.m);
		}
		VR_UI::cursor[VR_SIDE_LEFT].last_upd = now;
	}
	else {
		VR_UI::cursor[VR_SIDE_LEFT].active = false;
		VR_UI::cursor[VR_SIDE_LEFT].visible = false;
	}

	if (vr->controller[VR_SIDE_RIGHT]->available) {
		VR_UI::cursor[VR_SIDE_RIGHT].active = true;
		VR_UI::cursor[VR_SIDE_RIGHT].visible = true;
		/* Apply buttons and position. */
		const Mat44f& controller = (Mat44f)vr->t_controller[VR_SPACE_REAL][VR_SIDE_RIGHT];
		if (VR_UI::cursor_offset_enabled) {
			const Mat44f& cursor = VR_UI::cursor[VR_SIDE_RIGHT].position.position[VR_SPACE_REAL].mat;
			const Coord3Df& cursor_pos = *(Coord3Df*)cursor.m[3];
			Coord3Df& offset_pos = VR_UI::cursor[VR_SIDE_RIGHT].offset_pos;
			Mat44f& offset_rot = VR_UI::cursor[VR_SIDE_RIGHT].offset_rot;
			if (VR_UI::cursor_offset_update) { /* update so that controller position stays the same */
				/* Rotational difference: */
				offset_rot = cursor * controller_position_get(VR_SPACE_REAL, VR_SIDE_RIGHT, true);
				offset_rot.m[3][0] = offset_rot.m[3][1] = offset_rot.m[3][2] = 0;
				/* Translational difference: */
				const Coord3Df& controller_pos = *(Coord3Df*)controller.m[3];
				offset_pos = cursor_pos - controller_pos;
			}
			Mat44f new_cursor;
			new_cursor = controller;
			new_cursor.m[3][0] = new_cursor.m[3][1] = new_cursor.m[3][2] = 0;
			new_cursor = offset_rot * new_cursor;
			*((Coord3Df*)new_cursor.m[3]) = (*(Coord3Df*)controller.m[3]) + offset_pos;
			VR_UI::cursor[VR_SIDE_RIGHT].position.set(new_cursor.m);
		}
		else { /* cursor offset disabled */
			VR_UI::cursor[VR_SIDE_RIGHT].position.set(controller.m);
		}
		VR_UI::cursor[VR_SIDE_RIGHT].last_upd = now;
	}
	else {
		VR_UI::cursor[VR_SIDE_RIGHT].active = false;
		VR_UI::cursor[VR_SIDE_RIGHT].visible = false;
	}
	
	/* Extra / auxiliary controllers (VR_Side_AUX) */
	for (int i = 2; i < VR_MAX_CONTROLLERS; ++i) {
		if (vr->controller[i]->available) {
			VR_UI::cursor[i].active = true;
			VR_UI::cursor[i].visible = true;
			/*  Apply buttons and position. */
			const Mat44f& controller = (Mat44f)vr->t_controller[VR_SPACE_REAL][i];
			VR_UI::cursor[i].position.set(controller.m);
			VR_UI::cursor[i].last_upd = now;
		}
		else {
			VR_UI::cursor[i].active = false;
			VR_UI::cursor[i].visible = false;
		}
	}

	return ERROR_NONE;
}

VR_UI::Error VR_UI::execute_operations()
{
	if (VR_UI::updating) {
		/* Test to prevent circular calling (shouldn't happen in Blender). */
		return VR_UI::ERROR_INTERNALFAILURE;
	}
	VR_UI::updating = true;

	/* At a moderate interval, perform users actions in Blender. */
	ui64 now = VR_t_now;
	static ui64 last_action_update = 0;
	ui64 action_update_dt = now - last_action_update;
	/* Upper cap: don't update more often than maximum frequency. */
#if !VR_UI_OPTIMIZEPERFORMANCEMELTCPU
	if (action_update_dt < VR_UI_MINUPDATEINTERVAL) {
		VR_UI::updating = false;
		return VR_UI::ERROR_NONE;
	}
#endif
	/* Lower cap: definitely update if we are falling below minimum frequency
	 * (also make sure we have a valid FPS measurement). */
	if (action_update_dt < VR_UI_MAXUPDATEINTERVAL && VR_UI::fps_render) {
		/* Linearly degrade update frequency if rendering framerate drops below 60fps. */
		const ui64 min_fps = 1000 / VR_UI_MAXUPDATEINTERVAL;
		const ui64 max_fps = 1000 / VR_UI_MINUPDATEINTERVAL;
		const float render_ratio = float(VR_UI::fps_render - min_fps) / (60.0f - float(min_fps));
		const float target_fps = (render_ratio * float(max_fps - min_fps)) + min_fps;
		const ui64 target_interval = ui64(1000.0f / target_fps);

		if (action_update_dt < target_interval) {
			VR_UI::updating = false;
			return VR_UI::ERROR_NONE;
		}
	}

	last_action_update = now;

	/* Update the cursor UI. */
	VR *vr = vr_get_obj();
	if (vr->controller[VR_SIDE_LEFT]->available) {
		if (vr->controller[VR_SIDE_RIGHT]->available) {
			VR_UI::update_cursor(VR_UI::cursor[VR_SIDE_LEFT]);
			VR_UI::update_cursor(VR_UI::cursor[VR_SIDE_RIGHT]);
			/* Save old position. */
			VR_UI::cursor[VR_SIDE_LEFT].last_position = VR_UI::cursor[VR_SIDE_LEFT].position;
			VR_UI::cursor[VR_SIDE_LEFT].last_buttons = vr->controller[VR_SIDE_LEFT]->buttons;
			VR_UI::cursor[VR_SIDE_RIGHT].last_position = VR_UI::cursor[VR_SIDE_RIGHT].position;
			VR_UI::cursor[VR_SIDE_RIGHT].last_buttons = vr->controller[VR_SIDE_RIGHT]->buttons;
		}
		else {
			VR_UI::update_cursor(VR_UI::cursor[VR_SIDE_LEFT]);
			/* Save old position. */
			VR_UI::cursor[VR_SIDE_LEFT].last_position = VR_UI::cursor[VR_SIDE_LEFT].position;
			VR_UI::cursor[VR_SIDE_LEFT].last_buttons = vr->controller[VR_SIDE_LEFT]->buttons;

			if (VR_UI::ui_type == VR_UI_TYPE_FOVE) {
				/* Special case: Since Fove only has one cursor, transfer navigation 
				 * to a dummy cursor so we can move and interact at the same time. */
				if (VR_UI::cursor[VR_SIDE_LEFT].last_buttons & VR_Widget_Layout::BUTTONBITS_GRIPS) {
					VR_UI::cursor[VR_SIDE_RIGHT].position.position[VR_SPACE_REAL].mat = VR_UI::cursor[VR_SIDE_LEFT].position.position[VR_SPACE_REAL].mat;
					vr->controller[VR_SIDE_RIGHT]->buttons = VR_Widget_Layout::BUTTONBITS_GRIPS;
					VR_UI::update_cursor(VR_UI::cursor[VR_SIDE_RIGHT]);
					VR_UI::cursor[VR_SIDE_RIGHT].last_position.position[VR_SPACE_REAL].mat = VR_UI::cursor[VR_SIDE_LEFT].position.position[VR_SPACE_REAL].mat;
					VR_UI::cursor[VR_SIDE_RIGHT].last_buttons = VR_Widget_Layout::BUTTONBITS_GRIPS;
				}
			}
		}
	}
	else {
		if (vr->controller[VR_SIDE_RIGHT]->available) {
			VR_UI::update_cursor(VR_UI::cursor[VR_SIDE_RIGHT]);
			/* Save old position. */
			VR_UI::cursor[VR_SIDE_RIGHT].last_position = VR_UI::cursor[VR_SIDE_RIGHT].position;
VR_UI::cursor[VR_SIDE_RIGHT].last_buttons = vr->controller[VR_SIDE_RIGHT]->buttons;
		}
		else {
		/* None available, no update. */
		}
	}

	VR_UI::ctrl_key = CtrlState(VR_UI::cursor[VR_SIDE_LEFT].ctrl | VR_UI::cursor[VR_SIDE_RIGHT].ctrl);
	VR_UI::shift_key = ShiftState(VR_UI::cursor[VR_SIDE_LEFT].shift | VR_UI::cursor[VR_SIDE_RIGHT].shift);
	//VR_UI::alt_key = AltState(VR_UI::cursor[VR_SIDE_LEFT].alt | VR_UI::cursor[VR_SIDE_RIGHT].alt);

	/* Update menus. */
	//VR_UI::update_menus();

	VR_UI::updating = false;

	return ERROR_NONE;
}

VR_UI::Error VR_UI::update_cursor(Cursor& c)
{
	if (!VR_Widget_Layout::current_layout) {
		return ERROR_NOTINITIALIZED;
	}

	ui64 now = VR_t_now;

	VR *vr = vr_get_obj();
	ui64 buttons = vr->controller[c.side]->buttons;

	/* Special treatment for the ctrl/shift/alt keys. */
	if (buttons & VR_Widget_Layout::current_layout->ctrl_button_bits[c.side][VR_UI::ctrl_key]) {
		c.ctrl = CTRLSTATE_ON;
	}
	else {
		c.ctrl = CTRLSTATE_OFF;
	}
	if (buttons & VR_Widget_Layout::current_layout->shift_button_bits[c.side][VR_UI::alt_key]) {
		c.shift = SHIFTSTATE_ON;
	}
	else {
		c.shift = SHIFTSTATE_OFF;
	}
	//if (buttons & VR_Widget_Layout::current_layout->alt_button_bits[c.side]) {
	if (VR_UI::alt_key) {
		c.alt = ALTSTATE_ON;
	}
	else {
		c.alt = ALTSTATE_OFF;
	}

	/* Special recognition for the trigger button. */
	c.trigger = (buttons & VR_Widget_Layout::BUTTONBITS_TRIGGERS) ? true : false;

	static bool pie_menu_init[VR_SIDES] = { true, true };
	if (c.trigger) {
		if (!(c.last_buttons & VR_Widget_Layout::BUTTONBITS_TRIGGERS)) {
			/* First trigger interaction; close any open pie menus. */
			pie_menu_init[c.side] = true;
			/* Activate action settings menu. */
			VR_Widget *tool = get_current_tool(c.side);
			switch (tool->type()) {
			case VR_Widget::TYPE_SELECT: {
				Widget_Menu::menu_type[c.side] = VR_Widget::MENUTYPE_AS_SELECT;
				break;
			}
			case VR_Widget::TYPE_TRANSFORM: {
				Widget_Menu::menu_type[c.side] = VR_Widget::MENUTYPE_AS_TRANSFORM;
				break;
			}
			default: {
				break;
			}
			}
			Widget_Menu::action_settings[c.side] = true;
			VR_UI::pie_menu_active[c.side] = true;
		}
	}
	else {
		if (c.last_buttons & VR_Widget_Layout::BUTTONBITS_TRIGGERS) {
			VR_UI::pie_menu_active[c.side] = false;
			Widget_Menu::action_settings[c.side] = false;
			pie_menu_init[c.side] = true;
		}
	}

	/* Handle pie menu interaction first. */
	/* TODO_XR: Fix issues with center dpad press (other dpad directions can't click). */
	if (VR_UI::pie_menu_active[c.side]) {
		ui64& buttons_touched = vr_get_obj()->controller[c.side]->buttons_touched;
		bool touched = ((buttons_touched & (VR_UI::ui_type == VR_UI_TYPE_VIVE ? VR_Widget_Layout::BUTTONBITS_DPADANY : VR_Widget_Layout::BUTTONBITS_STICKANY)) != 0);
		bool stick_pressed = ((buttons & (VR_UI::ui_type == VR_UI_TYPE_OCULUS ? VR_Widget_Layout::BUTTONBITS_STICKS : VR_Widget_Layout::BUTTONBITS_DPADS)) != 0);
		
		static bool stick_init[VR_SIDES] = { true, true };
		if (stick_init[c.side] && stick_pressed) {
			stick_init[c.side] = false;
		}
		else if (!stick_init[c.side] && !stick_pressed) {
			/* Execute center click operation. */
			Widget_Menu::stick_center_click(c);
			stick_init[c.side] = true;
		}

		VR_Widget *menu = (VR_Widget*)VR_UI::pie_menu[c.side];
		if (menu) {
			if (VR_UI::ui_type == VR_UI_TYPE_VIVE) {
				/* Special case for action settings on the Vive:
				 * It's easy to accidentally hit the dpad so only
				 * execute action on dpad press. */
				bool pressed = ((buttons & (VR_UI::ui_type == VR_UI_TYPE_VIVE ? VR_Widget_Layout::BUTTONBITS_DPADANY : VR_Widget_Layout::BUTTONBITS_STICKANY)) != 0);
				static bool press_init[VR_SIDES] = { true, true };
				if (press_init[c.side] && pressed) {
					press_init[c.side] = false;
				}
				else if (!press_init[c.side] && !pressed) {
					/* Stop drag (execute menu operation) when dpad was pressed and released. */
					menu->drag_stop(c);
					if (c.trigger) {
						VR_UI::pie_menu_active[c.side] = true;
					}
					pie_menu_init[c.side] = true;
					press_init[c.side] = true;
				}
			}
			if (pie_menu_init[c.side]) {
				if (stick_pressed) {
					//
				}
				else if (touched) {
					/* First interaction since the menu was opened. */
					menu->drag_start(c);
					pie_menu_init[c.side] = false;
				}
			}
			else {
				if (VR_UI::ui_type == VR_UI_TYPE_VIVE) {
					bool stick_touched = ((buttons_touched & (VR_UI::ui_type == VR_UI_TYPE_OCULUS ? VR_Widget_Layout::BUTTONBITS_STICKS : VR_Widget_Layout::BUTTONBITS_DPADS)) != 0);
					if (stick_pressed) {
						//
					}
					else if (touched || stick_touched) {
						menu->drag_contd(c);
					}
					else {
						if (!c.trigger) {
							/* Stop drag (execute menu operation) on stick release. */
							menu->drag_stop(c);
							if (c.trigger) {
								VR_UI::pie_menu_active[c.side] = true;
							}
							pie_menu_init[c.side] = true;
						}
						else {
							/* Turn off highlight index for action settings */
							Widget_Menu::highlight_index[c.side] = -1;
						}
					}
				}
				else {
					if (stick_pressed) {
						//
					}
					else if (touched) {
						menu->drag_contd(c);
					}
					else {
						/* Stop drag (execute menu operation) on stick release. */
						menu->drag_stop(c);
						if (c.trigger) {
							VR_UI::pie_menu_active[c.side] = true;
						}
						pie_menu_init[c.side] = true;
					}
				}
			}

			/* If a pie menu is active, invalidate other widgets mapped to the stick. */
			buttons &= ~(VR_UI::ui_type == VR_UI_TYPE_VIVE ? VR_Widget_Layout::BUTTONBITS_DPADANY : VR_Widget_Layout::BUTTONBITS_STICKANY);
			buttons &= ~(VR_UI::ui_type == VR_UI_TYPE_OCULUS ? VR_Widget_Layout::BUTTONBITS_STICKS : VR_Widget_Layout::BUTTONBITS_DPADS);
		}
		else {
			VR_UI::pie_menu_active[c.side] = false;
		}
	}

	if (c.interaction_state == VR_UI::BUTTONSTATE_IDLE) {
		/* No button in interaction (initial state). */
		VR_Widget_Layout::ButtonBit dead_button_bits = VR_Widget_Layout::ButtonBit(VR_Widget_Layout::current_layout->ctrl_button_bits[c.side][VR_UI::alt_key] | VR_Widget_Layout::current_layout->shift_button_bits[c.side][VR_UI::alt_key]);// | VR_Widget_Layout::current_layout->alt_button_bits[c.side]);
		if ((buttons & (~dead_button_bits)) == 0) { /* no buttons pressed except shift/alt: nothing to do */
			return ERROR_NONE;
		}
		/* else: button hit */
		VR_Widget_Layout::ButtonID button_id = VR_Widget_Layout::buttonBitToID(VR_Widget_Layout::ButtonBit(buttons)); /* will get the main/dominant button */
		if (button_id == VR_Widget_Layout::BUTTONID_UNKNOWN) {
			return ERROR_NONE;
		}
		/* Checked for widget. Buttons with no widgets attached are ignored. */
		if (VR_Widget_Layout::current_layout->m[c.side][button_id][VR_UI::alt_key] == 0) {
			return ERROR_NONE;
		}
		c.interaction_button = VR_Widget_Layout::buttonIDToBit(button_id); /* <- *only* the bits for the active button */
		c.interaction_widget = VR_Widget_Layout::current_layout->m[c.side][button_id][VR_UI::alt_key];
		c.interaction_state = VR_UI::BUTTONSTATE_DOWN;
		c.interaction_time = now;
		c.interaction_position = c.position;
		c.interaction_ctrl = VR_UI::ctrl_key;
		c.interaction_shift = VR_UI::shift_key;
		c.interaction_alt = VR_UI::alt_key;
	}

	if (c.interaction_state == VR_UI::BUTTONSTATE_DOWN) {
		/* Button is pressed, but no action was triggered yet.
		 * If the interaction button was released: */
		if ((buttons & c.interaction_button) == 0) {
			c.interaction_state = VR_UI::BUTTONSTATE_RELEASE;
			return ERROR_NONE;
		}
		/* If an additional button was pressed linked to a widget that steals focus */
		VR_Widget_Layout::ButtonBit new_button = VR_Widget_Layout::ButtonBit(buttons & ~c.last_buttons);
		if (new_button) {
			VR_Widget_Layout::ButtonID new_button_id = VR_Widget_Layout::buttonBitToID(new_button);
			if (new_button_id != VR_Widget_Layout::BUTTONID_UNKNOWN) {
				VR_Widget* new_button_widget = VR_Widget_Layout::current_layout->m[c.side][new_button_id][VR_UI::alt_key];
				if (new_button_widget) {
					if (!c.interaction_widget
						|| (c.interaction_widget->allows_focus_steal(new_button_widget->type()) && new_button_widget->steals_focus(c.interaction_widget->type()))) {
						/* Focus steal */
						c.interaction_widget = VR_Widget_Layout::current_layout->m[c.side][new_button_id][VR_UI::alt_key];
						c.interaction_state = VR_UI::BUTTONSTATE_DOWN;
						c.interaction_time = now;
						c.interaction_position = c.position;
						c.interaction_ctrl = VR_UI::ctrl_key;
						c.interaction_shift = VR_UI::shift_key;
						c.interaction_alt = VR_UI::alt_key;
					}
				}
			}
		}
		if (!c.interaction_widget) {
			return ERROR_NONE; /* nothing to do */
		}
		/* If the other hand is already dragging with the same widget
		 * -or- if the widget does not support clicking, start dragging immediately. */
		if ((c.other_hand->interaction_state == VR_UI::BUTTONSTATE_DRAG && c.interaction_widget == c.other_hand->interaction_widget)
			|| !c.interaction_widget->has_click(c)) {
			c.interaction_state = VR_UI::BUTTONSTATE_DRAG;
			if (c.interaction_widget == c.other_hand->interaction_widget) {
				/* the other hand is also interacting with the same widget */
				if (c.other_hand->interaction_state == VR_UI::BUTTONSTATE_DRAG) { /* the other hand is already dragging */
					c.bimanual = VR_UI::Cursor::BIMANUAL_SECOND; /* I come in second */
					c.other_hand->bimanual = VR_UI::Cursor::BIMANUAL_FIRST;
				}
				else { /* the other hand is not dragging yet */
					c.bimanual = VR_UI::Cursor::BIMANUAL_FIRST; /* I come in first */
					c.other_hand->bimanual = VR_UI::Cursor::BIMANUAL_SECOND;
				}
			}
			c.interaction_widget->drag_start(c);
			return ERROR_NONE;
		}
		/* Check if enough time has passed to allow for dragging. */
		if (c.interaction_widget->has_drag(c) && (now - c.interaction_time >= VR_UI::drag_threshold_time)) {
			/* Check for motion that would indicate a drag. */
			const Mat44f& pi = c.interaction_position.get();
			const Mat44f& pc = c.position.get();
			float d;
			if (c.interaction_widget->type() == VR_Widget::TYPE_ANNOTATE || VR_Widget::TYPE_TRANSFORM || /* For annotation and transform widget, start dragging immediately. */
				((d = VR_Math::matrix_distance(pi, pc)) >= VR_UI::drag_threshold_distance) ||
				((d = VR_Math::matrix_rotation(pi, pc)) >= VR_UI::drag_threshold_rotation)) {
				c.interaction_state = VR_UI::BUTTONSTATE_DRAG;
				if (c.interaction_widget == c.other_hand->interaction_widget) {
					/* the other hand is also interacting with the same widget */
					if (c.other_hand->interaction_state == VR_UI::BUTTONSTATE_DRAG) { /* the other hand is already dragging */
						c.bimanual = VR_UI::Cursor::BIMANUAL_SECOND; /* I come in second */
						c.other_hand->bimanual = VR_UI::Cursor::BIMANUAL_FIRST;
					}
					else { /* the other hand is not dragging yet */
						c.bimanual = VR_UI::Cursor::BIMANUAL_FIRST; /* I come in first */
						c.other_hand->bimanual = VR_UI::Cursor::BIMANUAL_SECOND;
					}
				}
				c.interaction_widget->drag_start(c);
			}
		}
		return ERROR_NONE;
	}
	if (c.interaction_state == VR_UI::BUTTONSTATE_RELEASE) {
		/* Button was recently released (no click action triggered yet). */
		if (c.interaction_widget) {
			if (c.interaction_widget == c.other_hand->interaction_widget) {
				/* the other hand is already dragging with the same widget - let this widget know */
				c.bimanual = VR_UI::Cursor::BIMANUAL_SECOND;
			}
			c.interaction_widget->click(c);
			c.bimanual = VR_UI::Cursor::BIMANUAL_OFF; /* in case I just set it */
		}
		c.interaction_state = VR_UI::BUTTONSTATE_IDLE;
		c.interaction_widget = 0;
		c.interaction_button = 0;
		return ERROR_NONE;
	}
	if (c.interaction_state == VR_UI::BUTTONSTATE_DRAG) {
		/* Button in holding/dragging action. */
		/* If an additional button was pressed linked to a widget that steals focus */
		VR_Widget_Layout::ButtonBit new_button = VR_Widget_Layout::ButtonBit(buttons & ~c.last_buttons);
		VR_Widget_Layout::ButtonID new_button_id;
		if (new_button && (new_button_id = VR_Widget_Layout::buttonBitToID(new_button)) != VR_Widget_Layout::BUTTONID_UNKNOWN) {
			VR_Widget* new_button_widget = VR_Widget_Layout::current_layout->m[c.side][new_button_id][VR_UI::alt_key];
			if (new_button_widget) {
				if (!c.interaction_widget) {
					/* Was an empty interaction anyway : just take over focus */
					c.interaction_widget = VR_Widget_Layout::current_layout->m[c.side][new_button_id][VR_UI::alt_key];
					c.interaction_state = VR_UI::BUTTONSTATE_DOWN;
					c.interaction_time = now;
					c.interaction_position = c.position;
					c.interaction_ctrl = VR_UI::ctrl_key;
					c.interaction_shift = VR_UI::shift_key;
					c.interaction_alt = VR_UI::alt_key;
					return ERROR_NONE;
				}
				else if (c.interaction_widget->allows_focus_steal(new_button_widget->type()) && new_button_widget->steals_focus(c.interaction_widget->type())) {
					/* Focus steal
					 * old widget must first be allowed to finish its operation. */
					c.interaction_widget->drag_stop(c);
					/* now switch to new widget */
					c.interaction_widget = VR_Widget_Layout::current_layout->m[c.side][new_button_id][VR_UI::alt_key];
					c.interaction_state = VR_UI::BUTTONSTATE_DOWN;
					c.interaction_time = now;
					c.interaction_position = c.position;
					c.interaction_ctrl = VR_UI::ctrl_key;
					c.interaction_shift = VR_UI::shift_key;
					c.interaction_alt = VR_UI::alt_key;
					return ERROR_NONE;
				}
			}
		}

		/* Coninue dragging action
		 * If the interaction button was released, */
		if ((buttons & c.interaction_button) == 0) {
			/* assume VR module already did de-bouncing of input, we can end the dragging immediately */
			if (c.interaction_widget) {
				c.interaction_widget->drag_stop(c);
			}
			if (c.bimanual) {
				c.bimanual = VR_UI::Cursor::BIMANUAL_OFF;
				c.other_hand->bimanual = VR_UI::Cursor::BIMANUAL_OFF;
			}
			c.interaction_state = VR_UI::BUTTONSTATE_IDLE;
			c.interaction_widget = 0;
			c.interaction_button = 0;
			return ERROR_NONE;
		}
		if (c.interaction_widget) {
			c.interaction_widget->drag_contd(c);
		}
		return ERROR_NONE;
	}

	return ERROR_NONE;
}

VR_UI::Error VR_UI::update_menus()
{
	/* TODO_XR */

	return ERROR_NONE;
}

VR_UI::Error VR_UI::execute_post_render_operations()
{
	if (undo_count == 0 && redo_count == 0) {
		return ERROR_NONE;
	}

	/* Execute undo/redo operations. */
	bContext *C = vr_get_obj()->ctx;
	for (int i = 0; i < undo_count; ++i) {
		ED_undo_pop(C);
	}
	undo_count = 0;
	
	for (int i = 0; i < redo_count; ++i) {
		ED_undo_redo(C);
	}
	redo_count = 0;

	/* Update manipulators */
	Widget_Transform::update_manipulator();

	return ERROR_NONE;
}

VR_UI::Error VR_UI::pre_render(VR_Side side)
{
	return ERROR_NONE;
}

VR_UI::Error VR_UI::post_render(VR_Side side)
{
	/* Apply widget render functions (if any). */
	execute_widget_renders(side);

	VR* vr = vr_get_obj();

	if (VR_UI::ui_type == VR_UI_TYPE_FOVE) {
		/* Render box for eye cursor (convergence) position. */
		VR_Draw::update_modelview_matrix(&VR_Math::identity_f, 0);

		const Mat44f& t_controller = VR_UI::cursor_position_get(VR_SPACE_REAL, VR_SIDE_MONO);
		VR_Draw::set_color(1, 0, 0.5f, 0.5f);
		VR_Draw::render_box(*(Coord3Df*)(t_controller.m[3]) + Coord3Df(1, 1, 1) * 0.02f,
							*(Coord3Df*)(t_controller.m[3]) + Coord3Df(-1, -1, -1) * 0.02f);

		const Mat44f& t_hmd = VR_UI::hmd_position_get(VR_SPACE_REAL);
		VR_UI::render_widget_icons(VR_SIDE_MONO, t_hmd);
	}
	else {
		/* Create controllers if they haven't already been created. */
		if (!VR_Draw::controller_model[VR_SIDE_LEFT] || !VR_Draw::controller_model[VR_SIDE_RIGHT]) {
			VR_Draw::create_controller_models(VR_UI::type());
		}

		/* Render controllers, cursors, and widgets. */
		bool render_left = VR_UI::cursor_active_get(VR_SIDE_LEFT) && VR_UI::cursor_visible_get(VR_SIDE_LEFT);
		bool render_right = VR_UI::cursor_active_get(VR_SIDE_RIGHT) && VR_UI::cursor_visible_get(VR_SIDE_RIGHT);
		if (render_left && render_right) {
			VR_UI::render_controller(VR_SIDE_BOTH);
		}
		else if (render_left) {
			VR_UI::render_controller(VR_SIDE_LEFT);
		}
		else if (render_right) {
			VR_UI::render_controller(VR_SIDE_RIGHT);
		}
	}

	if (VR_UI::mouse_cursor_enabled && side == VR_UI::eye_dominance) {
		/* Render mouse cursor. */
		const Mat44f& prior_model_matrix = VR_Draw::get_model_matrix();
		const Mat44f& prior_view_matrix = VR_Draw::get_view_matrix();
		const Mat44f prior_projection_matrix = VR_Draw::get_projection_matrix();

		VR_Draw::update_modelview_matrix(&VR_Math::identity_f, &VR_Math::identity_f);
		VR_Draw::update_projection_matrix(VR_Math::identity_f.m);
		VR_Draw::set_color(1.0f, 1.0f, 1.0f, 1.0f);

		const rcti& rect = VR_UI::viewport_bounds;
		int win_width_half = (int)((float)(rect.xmax - rect.xmin) / 2.0f);
		int win_height_half = (int)((float)(rect.ymax - rect.ymin) / 2.0f);
		int x, y;
		wm_get_cursor_position(vr_get_obj()->window, &x, &y);
		static int r = 20;
		static float x0, x1, y0, y1;
		x0 = ((float)(x - r - win_width_half - rect.xmin) / (float)win_width_half);
		x1 = ((float)(x + r - win_width_half - rect.xmin) / (float)win_width_half);
		y0 = ((float)(y - r - win_height_half - rect.ymin) / (float)win_height_half);
		y1 = ((float)(y + r - win_height_half - rect.ymin) / (float)win_height_half);

		VR_Draw::set_depth_test(false, false);
		VR_Draw::render_rect(x0, x1, y1, y0, 0.001f, 1.0f, 1.0f, VR_Draw::mouse_cursor_tex);
		VR_Draw::set_depth_test(true, true);

		VR_Draw::update_modelview_matrix(&prior_model_matrix, &prior_view_matrix);
		VR_Draw::update_projection_matrix(prior_projection_matrix.m);
	}

	/* Render warning if VR isn't tracking. */
	if (!vr->tracking) {
		/* Render a big warning over the screen. */
		VR_Draw::update_projection_matrix(VR_Math::identity_f.m);
		VR_Draw::update_view_matrix(VR_Math::identity_f.m);
		VR_Draw::update_modelview_matrix(&VR_Math::identity_f, 0);
		VR_Draw::set_color(0.8f, 0.0f, 0.0f, 1.0f);
		static const char* tracking_lost_str = std::string("TRACKING LOST").c_str();
		VR_Draw::render_string(tracking_lost_str, 0.03f, 0.03f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.18f, 0.001f);

		return ERROR_INTERNALFAILURE;
	}

	/* Render menus. */
	//render_menus(0, 0);

	return ERROR_NONE;
}

VR_UI::Error VR_UI::render_controller(VR_Side controller_side)
{
	if (controller_side == VR_SIDE_BOTH) { /* Render both controllers in one function call (optimized). */
		const Mat44f& t_controller_left = VR_UI::cursor[VR_SIDE_LEFT].position.position->mat;
		const Mat44f& t_controller_right = VR_UI::cursor[VR_SIDE_RIGHT].position.position->mat;

		if (VR_UI::ui_type == VR_UI_TYPE_MICROSOFT) {
			/* Render controller models black until we get proper textures. */
			VR_Draw::set_depth_test(false, false);
			for (int i = 0; i < 2; ++i) {
				VR_Draw::set_color(0.211f, 0.219f, 0.223f, 0.2f);
				if (i == VR_SIDE_LEFT) {
					VR_Draw::controller_model[i]->render(t_controller_left);
				}
				else { /* VR_SIDE_RIGHT */
					VR_Draw::controller_model[i]->render(t_controller_right);
				}
				VR_Draw::set_color(1.0f, 1.0f, 1.0f, 0.2f);
				VR_Draw::cursor_model->render();
			}

			VR_Draw::set_depth_test(true, true);
			for (int i = 0; i < 2; ++i) {
				VR_Draw::set_color(0.211f, 0.219f, 0.223f, 1.0f);
				if (i == VR_SIDE_LEFT) {
					VR_Draw::controller_model[i]->render(t_controller_left);
				}
				else { /* VR_SIDE_RIGHT */
					VR_Draw::controller_model[i]->render(t_controller_right);
				}
				VR_Draw::set_color(1.0f, 1.0f, 1.0f, 1.0f);
				VR_Draw::cursor_model->render();
				VR_Draw::set_depth_test(true, false);
				/* Render crosshair cursor */
				VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f, VR_Draw::crosshair_cursor_tex);
				VR_Draw::set_depth_test(true, true);
			}
		}
		else {
			VR_Draw::set_depth_test(false, false);
			VR_Draw::set_color(1, 1, 1, 0.2f);
			for (int i = 0; i < 2; ++i) {
				if (i == VR_SIDE_LEFT) {
					VR_Draw::controller_model[i]->render(t_controller_left);
				}
				else { /* VR_SIDE_RIGHT */
					VR_Draw::controller_model[i]->render(t_controller_right);
				}
				VR_Draw::cursor_model->render();
			}

			VR_Draw::set_depth_test(true, true);
			VR_Draw::set_color(1, 1, 1, 1);
			for (int i = 0; i < 2; ++i) {
				if (i == VR_SIDE_LEFT) {
					VR_Draw::controller_model[i]->render(t_controller_left);
				}
				else { /* VR_SIDE_RIGHT */
					VR_Draw::controller_model[i]->render(t_controller_right);
				}
				VR_Draw::cursor_model->render();
				VR_Draw::set_depth_test(true, false);
				/* Render crosshair cursor */
				VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f, VR_Draw::crosshair_cursor_tex);
				VR_Draw::set_depth_test(true, true);
			}
		}
		render_widget_icons(VR_SIDE_LEFT, t_controller_left);
		render_widget_icons(VR_SIDE_RIGHT, t_controller_right);

		return ERROR_NONE;
	}

	/* else: Render specified controller */
	const Mat44f& t_controller = VR_UI::cursor[controller_side].position.position->mat;

	VR_Draw::set_depth_test(false, false);
	VR_Draw::set_color(1, 1, 1, 0.2f);
	VR_Draw::controller_model[controller_side]->render(t_controller);
	VR_Draw::cursor_model->render();
	
	VR_Draw::set_depth_test(true, true);
	VR_Draw::set_color(1, 1, 1, 1);
	VR_Draw::controller_model[controller_side]->render();
	VR_Draw::cursor_model->render();
	VR_Draw::set_depth_test(true, false);
	/* Render crosshair cursor */
	VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f, VR_Draw::crosshair_cursor_tex);
	VR_Draw::set_depth_test(true, true);

	render_widget_icons(controller_side, t_controller);

	return ERROR_NONE;
}

VR_UI::Error VR_UI::render_widget_icons(VR_Side controller_side, const Mat44f& t_controller)
{
	if (!VR_Widget_Layout::current_layout) {
		return ERROR_NOTINITIALIZED;
	}

	static Mat44f t_icon = VR_Math::identity_f;
	AltState alt = VR_UI::alt_key;

	VR *vr = vr_get_obj();
	ui64 buttons = vr->controller[controller_side]->buttons;
	ui64 buttons_touched = vr->controller[controller_side]->buttons_touched;

	/* Handle pie menu rendering first. */
	if (VR_UI::pie_menu_active[controller_side]) {
		VR_Widget *menu = (VR_Widget*)VR_UI::pie_menu[controller_side];
		if (!menu) {
			return VR_UI::ERROR_INTERNALFAILURE;
		}
		/* Render pie menu. */
		VR_Widget_Layout::ButtonID btn = (VR_UI::ui_type == VR_UI_TYPE_VIVE ? VR_Widget_Layout::BUTTONID_DPAD : VR_Widget_Layout::BUTTONID_STICK);
		VR_Widget_Layout::ButtonBit btnbit = (VR_UI::ui_type == VR_UI_TYPE_VIVE ? VR_Widget_Layout::BUTTONBITS_DPADANY : VR_Widget_Layout::BUTTONBITS_STICKANY);
		*((Coord3Df*)t_icon.m[3]) = VR_Widget_Layout::button_positions[VR_UI::ui_type][controller_side][btn];
		VR_Draw::set_depth_test(true, false);
		if (VR_UI::ui_type == VR_UI_TYPE_MICROSOFT && !Widget_Menu::action_settings[controller_side]) {
			static Mat44f temp;
			/* Need to rotate the menu up */
			t_icon.m[1][1] = t_icon.m[2][2] = -(float)cos(7.0f * PI / 8.0f);
			t_icon.m[1][2] = -(t_icon.m[2][1] = (float)sin(7.0f * PI / 8.0f));
			temp = t_controller;
			*(Coord3Df*)temp.m[3] += ((*(Coord3Df*)t_controller.m[1]).normalize() + (*(Coord3Df*)t_controller.m[2]).normalize()) * 0.01f;
			menu->render_icon(t_icon * temp,
				controller_side,
				((buttons & btnbit) != 0),
				((buttons_touched & btnbit) != 0));
		}
		else {
			menu->render_icon(t_icon * t_controller,
				controller_side,
				((buttons & btnbit) != 0),
				((buttons_touched & btnbit) != 0));
		}
		VR_Draw::set_depth_test(true, true);
		/* If a pie menu is active, invalidate other widgets mapped to the stick. */
		buttons &= ~(VR_UI::ui_type == VR_UI_TYPE_VIVE ? VR_Widget_Layout::BUTTONBITS_DPADANY : VR_Widget_Layout::BUTTONBITS_STICKANY);
		buttons &= ~(VR_UI::ui_type == VR_UI_TYPE_OCULUS ? VR_Widget_Layout::BUTTONBITS_STICKS : VR_Widget_Layout::BUTTONBITS_DPADS);
		buttons_touched &= ~(VR_UI::ui_type == VR_UI_TYPE_VIVE ? VR_Widget_Layout::BUTTONBITS_DPADANY : VR_Widget_Layout::BUTTONBITS_STICKANY);
		buttons_touched &= ~(VR_UI::ui_type == VR_UI_TYPE_OCULUS ? VR_Widget_Layout::BUTTONBITS_STICKS : VR_Widget_Layout::BUTTONBITS_DPADS);

		/* Render other enabled widget icons. */
		if (VR_UI::ui_type == VR_UI_TYPE_MICROSOFT) {
			/* Render the icons on top.
			 * Need to rotate icons 45deg */
			t_icon.m[1][1] = t_icon.m[2][2] = (float)cos(QUARTPI);
			t_icon.m[1][2] = -(t_icon.m[2][1] = (float)sin(QUARTPI));
		}
		if (Widget_Menu::action_settings[controller_side]) {
			for (VR_Widget_Layout::ButtonID btn = VR_Widget_Layout::ButtonID(0); btn < VR_Widget_Layout::BUTTONIDS; btn = VR_Widget_Layout::ButtonID(btn + 1)) {
				VR_Widget *w = VR_Widget_Layout::current_layout->m[controller_side][btn][alt];
				if (w) {
					switch (w->type()) {
					case VR_Widget::TYPE_CTRL:
					case VR_Widget::TYPE_SHIFT: {
						VR_Widget_Layout::ButtonBit btnbit = VR_Widget_Layout::buttonIDToBit(btn);
						*((Coord3Df*)t_icon.m[3]) = VR_Widget_Layout::button_positions[VR_UI::ui_type][controller_side][btn];
						VR_Widget_Layout::current_layout->m[controller_side][btn][alt]->render_icon(
							t_icon * t_controller,
							controller_side,
							((buttons & btnbit) != 0),
							((buttons_touched & btnbit) != 0)
						);
						break;
					}
					default: {
						break;
					}
					}
				}
			}
			return ERROR_NONE;
		}
		else {
			for (VR_Widget_Layout::ButtonID btn = VR_Widget_Layout::ButtonID(0); btn < VR_Widget_Layout::BUTTONIDS; btn = VR_Widget_Layout::ButtonID(btn + 1)) {
				if (btn == VR_Widget_Layout::BUTTONID_DPADLEFT && VR_UI::ui_type == VR_UI_TYPE_VIVE) {
					btn = VR_Widget_Layout::BUTTONID_DPAD;
					continue;
				}
				else if (btn == VR_Widget_Layout::BUTTONID_STICKLEFT && VR_UI::ui_type != VR_UI_TYPE_VIVE) {
					btn = VR_Widget_Layout::BUTTONID_STICK;
					continue;
				}
				if (VR_Widget_Layout::current_layout->m[controller_side][btn][alt]) {
					VR_Widget_Layout::ButtonBit btnbit = VR_Widget_Layout::buttonIDToBit(btn);
					*((Coord3Df*)t_icon.m[3]) = VR_Widget_Layout::button_positions[VR_UI::ui_type][controller_side][btn];
					VR_Widget_Layout::current_layout->m[controller_side][btn][alt]->render_icon(
						t_icon * t_controller,
						controller_side,
						((buttons & btnbit) != 0),
						((buttons_touched & btnbit) != 0)
					);
				}
			}
			return ERROR_NONE;
		}
	}

	if (VR_UI::ui_type == VR_UI_TYPE_MICROSOFT) {
		/* Render the icons on top.
		 * Need to rotate icons 45deg */
		t_icon.m[1][1] = t_icon.m[2][2] = (float)cos(QUARTPI);
		t_icon.m[1][2] = -(t_icon.m[2][1] = (float)sin(QUARTPI));
	}

	for (VR_Widget_Layout::ButtonID btn = VR_Widget_Layout::ButtonID(0); btn < VR_Widget_Layout::BUTTONIDS; btn = VR_Widget_Layout::ButtonID(btn + 1)) {
		if (VR_Widget_Layout::current_layout->m[controller_side][btn][alt]) {
			VR_Widget_Layout::ButtonBit btnbit = VR_Widget_Layout::buttonIDToBit(btn);
			*((Coord3Df*)t_icon.m[3]) = VR_Widget_Layout::button_positions[VR_UI::ui_type][controller_side][btn];
			VR_Widget_Layout::current_layout->m[controller_side][btn][alt]->render_icon(
				t_icon * t_controller,
				controller_side,
				((buttons & btnbit) != 0),
				((buttons_touched & btnbit) != 0)
			);
		}
	}

	return ERROR_NONE;
}

VR_UI::Error VR_UI::execute_widget_renders(VR_Side side)
{
	if (VR_UI::cursor_offset_enabled) {
		VR_Draw::set_depth_test(false, false);
		VR_Draw::set_color(0.3f, 0.5f, 0.3f, 0.2f);

		VR *vr = vr_get_obj();
		if (VR_UI::ui_type == VR_UI_TYPE_FOVE) {
			VR_Draw::update_modelview_matrix(&VR_Math::identity_f, 0);
			const Mat44f& t_controller = VR_UI::controller_position_get(VR_SPACE_REAL, VR_SIDE_MONO);
			VR_Draw::render_box(*(Coord3Df*)(t_controller.m[3]) + Coord3Df(1, 1, 1) * 0.02f,
								*(Coord3Df*)(t_controller.m[3]) + Coord3Df(-1, -1, -1) * 0.02f);
		}
		else {
			for (int controller_side = 0; controller_side < VR_SIDES; ++controller_side) {
				const Mat44f& t_controller = (Mat44f)vr->t_controller[VR_SPACE_REAL][controller_side];
				VR_Draw::controller_model[controller_side]->render(t_controller);
			}
		}
	}

	/* Apply widget render functions (if any). */
	AltState alt = VR_UI::alt_key;
	/* TODO_XR: Refactor this. */
	bool manip_rendered = false;
	for (VR_Widget_Layout::ButtonID btn = VR_Widget_Layout::ButtonID(0); btn < VR_Widget_Layout::BUTTONIDS; btn = VR_Widget_Layout::ButtonID(btn + 1)) {
		for (int controller_side = 0; controller_side < VR_SIDES; ++controller_side) {
			VR_Widget* widget = VR_Widget_Layout::current_layout->m[controller_side][btn][alt];
			if (widget && widget->do_render[side]) {
				if (widget->type() == VR_Widget::TYPE_TRANSFORM && !manip_rendered) {
					/* Manipulator */
					VR_Draw::set_depth_test(false, false);
					VR_Draw::set_color(1, 1, 1, 0.2f);
					widget->render(side);
					VR_Draw::set_depth_test(true, true);
					VR_Draw::set_color(1, 1, 1, 1);
					widget->render(side);
					/* Prevent rendering from duplicate widgets. */
					manip_rendered = true;
				}
				else {
					widget->render(side);
				}
			}
		}
	}

	return ERROR_NONE;
}

VR_UI::Error VR_UI::render_menus(const Mat44f* _model, const Mat44f* _view)
{
	if (_model || _view) {
		VR_Draw::update_modelview_matrix(_model, _view);
	}

	/* Background */
	VR_Draw::set_color(0.2f, 0.2f, 0.2f, 1.0f);
	VR_Draw::set_blend(false);
	VR_Draw::render_rect(-0.2f, 0.2f, 0.2f, -0.2f, 0.0f);

	/* Frame */
	VR_Draw::set_color(0.0f, 0.7f, 1.0f, 1.0f);
	VR_Draw::render_frame(-0.2f, 0.2f, 0.2f, -0.2f, 0.01f, 0.0f);
	VR_Draw::set_blend(true);

	/* Fps counter */
	VR_Draw::set_color(1.0f, 0.7f, 0.0f, 1.0f);
	static int counter = 0;
	static std::string fps_str = std::to_string(VR_UI::fps_render);
	if (++counter > 60) {
		counter = 0;
		fps_str = std::to_string(VR_UI::fps_render);
	}
	VR_Draw::render_string(fps_str.c_str(), 0.03f, 0.03f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.18f, 0.001f);

	/* Zoom and close icons */
	VR_Draw::set_color(0.0f, 1.0f, 0.7f, 1.0f);
	VR_Draw::render_rect(0.1f, 0.13f, 0.187f, 0.157f, 0.001f, 1.0f, 1.0f, VR_Draw::zoom_tex);
	VR_Draw::set_color(0.7f, 0.0f, 1.0f, 1.0f);
	VR_Draw::render_rect(0.14f, 0.17f, 0.187f, 0.157f, 0.001f, 1.0f, 1.0f, VR_Draw::close_tex);

	return ERROR_NONE;
}

/***********************************************************************************************//**
 *											 vr_api									   
 ***************************************************************************************************
/* Create an object internally. Must be called before the functions below. */
int vr_api_create_ui()
{
	VR* vr = vr_get_obj();
	VR_UI::set_ui(vr->ui_type);
	return 0;
}

/* Initialize the internal object (OpenGL). */
#ifdef WIN32
int vr_api_init_ui(void* device, void* context)
{
	int error = VR_Draw::init(device, context);
	if (!error) {
		/* Will automatically assign widget layout based on ui_type */
		VR_Widget_Layout::reset_to_default_layouts();
	}
	return error;
}
#else
int vr_api_init_ui(void* display, void* drawable, void* context)
{
	int error = VR_Draw::init(display, drawable, context);
	if (!error) {
		/* Will automatically assign widget layout based on ui_type */
		VR_Widget_Layout::reset_to_default_layouts();
	}
	return error;
}
#endif

/* Update VR tracking including UI button states. */
int vr_api_update_tracking_ui()
{
	VR_UI::update_tracking();
	return 0;
}

/* Execute UI operations. */
int vr_api_execute_operations()
{
	VR_UI::execute_operations();
	return 0;
}

/* Execute post-scene UI operations. */
int vr_api_execute_post_render_operations()
{
	VR_UI::execute_post_render_operations();
	return 0;
}

/* Get the navigation matrix (or inverse navigation matrix) from the UI module. */
const float *vr_api_get_navigation_matrix(int inverse)
{
	if (inverse) {
		return (float*)VR_UI::navigation_inverse_get().m;
	}
	else {
		return (float*)VR_UI::navigation_matrix_get().m;
	}
	return 0;
}

/* Update the OpenGL view matrix for the UI module. */
int vr_api_update_view_matrix(const float _view[4][4])
{
	VR_Draw::update_view_matrix(_view);
	return 0;
}

/* Update the OpenGL projection matrix for the UI module. */
int vr_api_update_projection_matrix(int side, const float _projection[4][4])
{
	VR_UI::viewport_projection[side] = _projection;
	VR_Draw::update_projection_matrix(_projection);
	return 0;
}

/* Update viewport (window) bounds for the UI module. */
int vr_api_update_viewport_bounds(const rcti *bounds)
{
	memcpy(&VR_UI::viewport_bounds, bounds, sizeof(rcti));
	return 0;
}

/* Pre-render UI elements. */
int vr_api_pre_render(int side)
{
	VR_UI::pre_render((VR_Side)side);
	return 0;
}

/* Post-render UI elements. */
int vr_api_post_render(int side)
{
	VR_UI::post_render((VR_Side)side);
	return 0;
}

/* Un-initialize the internal object. */
int vr_api_uninit_ui()
{
	VR_Draw::uninit();
	VR_UI::shutdown();
	return 0;
}
