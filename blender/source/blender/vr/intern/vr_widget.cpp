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
#include "vr_widget_alt.h"
#include "vr_widget_addprimitive.h"
#include "vr_widget_annotate.h"
#include "vr_widget_bevel.h"
#include "vr_widget_ctrl.h"
#include "vr_widget_cursor.h"
#include "vr_widget_cursoroffset.h"
#include "vr_widget_delete.h"
#include "vr_widget_duplicate.h"
#include "vr_widget_extrude.h"
#include "vr_widget_insetfaces.h"
#include "vr_widget_join.h"
#include "vr_widget_knife.h"
#include "vr_widget_layout.h"
#include "vr_widget_loopcut.h"
#include "vr_widget_measure.h"
#include "vr_widget_menu.h"
#include "vr_widget_navi.h"
#include "vr_widget_redo.h"
#include "vr_widget_shift.h"
#include "vr_widget_select.h"
#include "vr_widget_separate.h"
#include "vr_widget_switchcomponent.h"
#include "vr_widget_switchlayout.h"
#include "vr_widget_switchspace.h"
#include "vr_widget_switchtool.h"
#include "vr_widget_transform.h"
#include "vr_widget_undo.h"

#include "vr_math.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_undo.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

/* Multiplier for one and two-handed scaling transformations. */
#define WIDGET_TRANSFORM_SCALING_SENSITIVITY 0.5f

/* Precision multipliers. */
#define WIDGET_TRANSFORM_TRANS_PRECISION 0.1f
#define WIDGET_TRANSFORM_ROT_PRECISION (PI/36.0f)
#define WIDGET_TRANSFORM_SCALE_PRECISION 0.005f

#include "vr_util.h"

/* Static transformation matrix for rendering touched widgets. */
float m_wt[4][4] = { 1.5f, 0.0f, 0.0f, 0.0f,
					 0.0f, 1.5f, 0.0f, 0.0f,
					 0.0f, 0.0f, 1.5f, 0.0f,
					 0.0f, 0.0f, 0.003f, 1.0f };
const Mat44f VR_Widget::m_widget_touched = m_wt;

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
	case TYPE_ADDPRIMITIVE:
		return &Widget_AddPrimitive::obj;
	case TYPE_EXTRUDE:
		return &Widget_Extrude::obj;
	case TYPE_INSETFACES:
		return &Widget_InsetFaces::obj;
	case TYPE_BEVEL:
		return &Widget_Bevel::obj;
	case TYPE_LOOPCUT:
		return &Widget_LoopCut::obj;
	case TYPE_KNIFE:
		return &Widget_Knife::obj;
	case TYPE_CURSOROFFSET:
		return &Widget_CursorOffset::obj;
	case TYPE_DELETE:
		return &Widget_Delete::obj;
	case TYPE_DUPLICATE:
		return &Widget_Duplicate::obj;
	case TYPE_JOIN:
		return &Widget_Join::obj;
	case TYPE_SEPARATE:
		return &Widget_Separate::obj;
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
	if (str == "ADDPRIMITIVE") {
		return TYPE_ADDPRIMITIVE;
	}
	if (str == "EXTRUDE") {
		return TYPE_EXTRUDE;
	}
	if (str == "INSETFACES") {
		return TYPE_INSETFACES;
	}
	if (str == "BEVEL") {
		return TYPE_BEVEL;
	}
	if (str == "LOOPCUT") {
		return TYPE_LOOPCUT;
	}
	if (str == "KNIFE") {
		return TYPE_KNIFE;
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
	if (str == "JOIN") {
		return TYPE_JOIN;
	}
	if (str == "SEPARATE") {
		return TYPE_SEPARATE;
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
	if (str == "ADDPRIMITIVE") {
		return &Widget_AddPrimitive::obj;
	}
	if (str == "EXTRUDE") {
		return &Widget_Extrude::obj;
	}
	if (str == "INSETFACES") {
		return &Widget_InsetFaces::obj;
	}
	if (str == "BEVEL") {
		return &Widget_Bevel::obj;
	}
	if (str == "LOOPCUT") {
		return &Widget_LoopCut::obj;
	}
	if (str == "KNIFE") {
		return &Widget_Knife::obj;
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
	if (str == "JOIN") {
		return &Widget_Join::obj;
	}
	if (str == "SEPARATE") {
		return &Widget_Separate::obj;
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
	ret.push_back("ADDPRIMITIVE");
	ret.push_back("EXTRUDE");
	ret.push_back("INSETFACES");
	ret.push_back("BEVEL");
	ret.push_back("LOOPCUT");
	ret.push_back("KNIFE");
	ret.push_back("CURSOROFFSET");
	ret.push_back("DELETE");
	ret.push_back("DUPLICATE");
	ret.push_back("JOIN");
	ret.push_back("SEPARATE");
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
	case TYPE_ADDPRIMITIVE:
		return "ADDPRIMITIVE";
	case TYPE_EXTRUDE:
		return "EXTRUDE";
	case TYPE_INSETFACES:
		return "INSETFACES";
	case TYPE_BEVEL:
		return "BEVEL";
	case TYPE_LOOPCUT:
		return "LOOPCUT";
	case TYPE_KNIFE:
		return "KNIFE";
	case TYPE_CURSOROFFSET:
		return "CURSOROFFSET";
	case TYPE_DELETE:
		return "DELETE";
	case TYPE_DUPLICATE:
		return "DUPLICATE";
	case TYPE_JOIN:
		return "JOIN";
	case TYPE_SEPARATE:
		return "SEPARATE";
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