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

/** \file blender/vr/intern/vr_widget_undo.cpp
*   \ingroup vr
* 
* Main module for the VR widget UI.
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_undo.h"

#include "vr_draw.h"

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
