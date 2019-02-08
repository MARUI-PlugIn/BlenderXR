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

/** \file blender/vr/intern/vr_widget_switchspace.cpp
*   \ingroup vr
* 
* Main module for the VR widget UI.
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_switchspace.h"
#include "vr_widget_transform.h"
#include "vr_widget_layout.h"

#include "vr_draw.h"

#include "BKE_context.h"

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
