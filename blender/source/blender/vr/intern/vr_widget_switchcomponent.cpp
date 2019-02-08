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

/** \file blender/vr/intern/vr_widget_switchcomponent.cpp
*   \ingroup vr
* 
* Main module for the VR widget UI.
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_switchcomponent.h"
#include "vr_widget_transform.h"
#include "vr_widget_layout.h"

#include "vr_math.h"
#include "vr_draw.h"

#include "BKE_context.h"

#include "ED_object.h"
#include "ED_screen.h"

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
