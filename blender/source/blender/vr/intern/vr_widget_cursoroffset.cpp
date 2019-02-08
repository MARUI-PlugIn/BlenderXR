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

/** \file blender/vr/intern/vr_widget_cursoroffset.cpp
*   \ingroup vr
* 
* Main module for the VR widget UI.
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_cursoroffset.h"

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
