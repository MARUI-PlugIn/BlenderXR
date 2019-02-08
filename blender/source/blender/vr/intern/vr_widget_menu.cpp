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

/** \file blender/vr/intern/vr_widget_menu.cpp
*   \ingroup vr
* 
* Main module for the VR widget UI.
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_menu.h"
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
#include "vr_widget_select.h"
#include "vr_widget_separate.h"
#include "vr_widget_switchcomponent.h"
#include "vr_widget_switchlayout.h"
#include "vr_widget_switchspace.h"
#include "vr_widget_switchtool.h"
#include "vr_widget_transform.h"
#include "vr_widget_undo.h"

#include "vr_math.h"
#include "vr_draw.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_object.h"

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
static const Coord3Df p12_0(0.0f, 0.06f, 0.0f);
static const Coord3Df p12_1(-0.06f, 0.0f, 0.0f);
static const Coord3Df p12_2(0.06f, 0.0f, 0.0f);
static const Coord3Df p12_3(-0.032f, 0.052f, 0.0f);
static const Coord3Df p12_4(0.032f, 0.052f, 0.0f);
static const Coord3Df p12_5(-0.054f, 0.028f, 0.0f);
static const Coord3Df p12_6(0.054f, 0.028f, 0.0f);
static const Coord3Df p12_7(-0.054f, -0.028f, 0.0f);
static const Coord3Df p12_8(0.054f, -0.028f, 0.0f);
static const Coord3Df p12_9(-0.032f, -0.052f, 0.0f);
static const Coord3Df p12_10(0.032f, -0.052f, 0.0f);
static const Coord3Df p12_11(0.0f, -0.06f, 0.0f);
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
		break;
	}
	case TYPE_CURSOR: {
		menu_type[c.side] = MENUTYPE_TS_CURSOR;
		break;
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
		break;
	}
	case TYPE_ADDPRIMITIVE: {
		menu_type[c.side] = MENUTYPE_TS_ADDPRIMITIVE;
		break;
	}
	case TYPE_EXTRUDE: {
		menu_type[c.side] = MENUTYPE_TS_EXTRUDE;
		break;
	}
	case TYPE_INSETFACES: {
		menu_type[c.side] = MENUTYPE_TS_INSETFACES;
		break;
	}
	case TYPE_BEVEL: {
		menu_type[c.side] = MENUTYPE_TS_BEVEL;
		break;
	}
	case TYPE_LOOPCUT: {
		menu_type[c.side] = MENUTYPE_TS_LOOPCUT;
		break;
	}
	case TYPE_KNIFE: {
		menu_type[c.side] = MENUTYPE_TS_KNIFE;
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
	if (!VR_UI::pie_menu_active[c.side]) {
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
				break;
			}
			case TYPE_CURSOR: {
				menu_type[c.side] = MENUTYPE_TS_CURSOR;
				break;
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
				break;
			}
			case TYPE_ADDPRIMITIVE: {
				menu_type[c.side] = MENUTYPE_TS_ADDPRIMITIVE;
				break;
			}
			case TYPE_EXTRUDE: {
				menu_type[c.side] = MENUTYPE_TS_EXTRUDE;
				break;
			}
			case TYPE_INSETFACES: {
				menu_type[c.side] = MENUTYPE_TS_INSETFACES;
				break;
			}
			case TYPE_BEVEL: {
				menu_type[c.side] = MENUTYPE_TS_BEVEL;
				break;
			}
			case TYPE_LOOPCUT: {
				menu_type[c.side] = MENUTYPE_TS_LOOPCUT;
				break;
			}
			case TYPE_KNIFE: {
				menu_type[c.side] = MENUTYPE_TS_KNIFE;
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
		menu_items.push_back(&Widget_SwitchComponent::obj);
		menu_items.push_back(&Widget_SwitchSpace::obj);
		menu_items.push_back(&Widget_Delete::obj);
		menu_items.push_back(&Widget_Duplicate::obj);
		num_items[c.side] = 7;
		break;
	}
	case MENUTYPE_MAIN_12: {
		menu_items.push_back(&Widget_Menu::obj);
		menu_items.push_back(&Widget_Undo::obj);
		menu_items.push_back(&Widget_Redo::obj);
		menu_items.push_back(&Widget_SwitchComponent::obj);
		menu_items.push_back(&Widget_SwitchSpace::obj);
		menu_items.push_back(&Widget_Delete::obj);
		menu_items.push_back(&Widget_Duplicate::obj);
		menu_items.push_back(&Widget_Delete::obj);
		menu_items.push_back(&Widget_Duplicate::obj);
		menu_items.push_back(&Widget_SwitchComponent::obj);
		menu_items.push_back(&Widget_SwitchSpace::obj);
		num_items[c.side] = 11;
		break;
	}
	case MENUTYPE_SWITCHTOOL: {
		// Transform
		// Add primitive
		// Extrude
		// Cursor 
		// Annotate
		// Select
		// Measure
		// Annotate
		// Inset faces
		// Bevel
		// Loop cut
		// Knife
		num_items[c.side] = 11;
		break;
	}
	case MENUTYPE_TS_SELECT: {
		// Mouse cursor
		// Raycast
		// Proximity
		num_items[c.side] = 3;
		break;
	}
	case MENUTYPE_TS_CURSOR: {
		// Teleport to cursor
		// Set cursor to world origin
		// Set cursor to object origin
		num_items[c.side] = 3;
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
	case MENUTYPE_TS_ANNOTATE: {
		// Colorwheel
		num_items[c.side] = 11;
		break;
	}
	case MENUTYPE_TS_MEASURE: {
		// Default clip
		// Decrease far clip
		// Increase far clip
		num_items[c.side] = 3;
		break;
	}
	case MENUTYPE_TS_ADDPRIMITIVE: {
		// Plane
		// Cube
		// Circle
		// Cylinder
		// Cone
		// Grid
		// Monkey
		// UV sphere
		// Icosphere
		// Split
		// Join
		num_items[c.side] = 11;
		break;
	}
	case MENUTYPE_TS_EXTRUDE: {
		// Normals
		// Region
		// Individual
		// Flip normals
		// Transform
		num_items[c.side] = 5;
		break;
	}
	case MENUTYPE_TS_INSETFACES: {
		// Individual
		// Even offset
		// Relative offset
		// Boundary
		// Outset
		num_items[c.side] = 5;
		break;
	}
	case MENUTYPE_TS_BEVEL: {
		// Vertex only
		// Decrease segments
		// Increase segments
		num_items[c.side] = 3;
		break;
	}
	case MENUTYPE_TS_LOOPCUT: {
		// Edge slide
		// Decrease cuts
		// Increase cuts
		// Flipped
		// Clamp
		num_items[c.side] = 5;
		break;
	}
	case MENUTYPE_TS_KNIFE: {
		// Occlude geometry
		// Decrease cuts
		// Increase cuts
		num_items[c.side] = 3;
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
	if (!VR_UI::pie_menu_active[c.side]) {
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
		}
		else if (angle2 >= PI && angle2 < 2 * PI) {
			/* Set/lock up-axis to Blender up-axis */
			Widget_Navi::obj.nav_lock[1] = ((Widget_Navi::obj.nav_lock[1] == VR_UI::NAVLOCK_ROT_UP) ? VR_UI::NAVLOCK_NONE : VR_UI::NAVLOCK_ROT_UP);
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			/* Lock scale */
			Widget_Navi::obj.nav_lock[2] = ((Widget_Navi::obj.nav_lock[2] == VR_UI::NAVLOCK_SCALE) ? VR_UI::NAVLOCK_NONE : VR_UI::NAVLOCK_SCALE);
		}
		else if (angle2 >= 3 * PI && angle2 < 4 * PI) {
			/* Set/lock scale 1:1 with real world */
			Widget_Navi::obj.nav_lock[2] = ((Widget_Navi::obj.nav_lock[2] == VR_UI::NAVLOCK_SCALE_REAL) ? VR_UI::NAVLOCK_NONE : VR_UI::NAVLOCK_SCALE_REAL);
		}
		else if (angle2 >= 4 * PI || (angle2 < -3 * PI && angle2 >= -4 * PI)) {
			/* exit region */
		}
		else if (angle2 < -2 * PI && angle2 >= -3 * PI) {
			/* Off */
			memset(Widget_Navi::nav_lock, 0, sizeof(int) * 3);
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			/* Lock translation */
			Widget_Navi::obj.nav_lock[0] = ((Widget_Navi::obj.nav_lock[0] == VR_UI::NAVLOCK_TRANS) ? VR_UI::NAVLOCK_NONE : VR_UI::NAVLOCK_TRANS);
		}
		else if (angle2 < 0 && angle2 >= -PI) {
			/* Lock up-translation */
			Widget_Navi::obj.nav_lock[0] = ((Widget_Navi::obj.nav_lock[0] == VR_UI::NAVLOCK_TRANS_UP) ? VR_UI::NAVLOCK_NONE : VR_UI::NAVLOCK_TRANS_UP);
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

		if (angle2 >= 0 && angle2 < PI) {
			/* Y */
			Widget_Transform::constraint_flag[0] = 0; Widget_Transform::constraint_flag[1] = 1; Widget_Transform::constraint_flag[2] = 0;
			memcpy(Widget_Transform::snap_flag, Widget_Transform::constraint_flag, sizeof(int) * 3);
			switch (Widget_Transform::transform_mode) {
			case Widget_Transform::TRANSFORMMODE_OMNI: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_Y;
				Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_MOVE;
				break;
			}
			case Widget_Transform::TRANSFORMMODE_MOVE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_Y;
				break;
			}
			case Widget_Transform::TRANSFORMMODE_ROTATE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_ROT_Y;
				Widget_Transform::update_manipulator();
				break;
			}
			case Widget_Transform::TRANSFORMMODE_SCALE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_Y;
				break;
			}
			default: {
				break;
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
				break;
			}
			case Widget_Transform::TRANSFORMMODE_SCALE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_YZ;
				break;
			}
			default: {
				break;
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
				break;
			}
			case Widget_Transform::TRANSFORMMODE_ROTATE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_ROT_Z;
				Widget_Transform::update_manipulator();
				break;
			}
			case Widget_Transform::TRANSFORMMODE_SCALE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_Z;
				break;
			}
			default: {
				break;
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
				break;
			}
			case Widget_Transform::TRANSFORMMODE_SCALE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_ZX;
				break;
			}
			default: {
				break;
			}
			}
		}
		else if (angle2 >= 4 * PI || (angle2 < -3 * PI && angle2 >= -4 * PI)) {
			/* exit region */
		}
		else if (angle2 < -2 * PI && angle2 >= -3 * PI) {
			/* Off */
			memset(Widget_Transform::constraint_flag, 0, sizeof(int) * 3);
			memset(Widget_Transform::snap_flag, 1, sizeof(int) * 3);
			Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_NONE;
			if (Widget_Transform::omni) {
				Widget_Transform::transform_mode = Widget_Transform::TRANSFORMMODE_OMNI;
			}
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
				break;
			}
			case Widget_Transform::TRANSFORMMODE_ROTATE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_ROT_X;
				Widget_Transform::update_manipulator();
				break;
			}
			case Widget_Transform::TRANSFORMMODE_SCALE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_X;
				break;
			}
			default: {
				break;
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
				break;
			}
			case Widget_Transform::TRANSFORMMODE_SCALE: {
				Widget_Transform::constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_XY;
				break;
			}
			default: {
				break;
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
	case MENUTYPE_TS_SELECT: {
		if (angle2 >= 0 && angle2 < PI) {
			/* Mouse cursor */
			VR_UI::mouse_cursor_enabled = !VR_UI::mouse_cursor_enabled;
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			/* Proximity */
			VR_UI::selection_mode = VR_UI::SELECTIONMODE_PROXIMITY;
		}
		else if (angle2 >= 3 * PI || (angle2 < -2 * PI && angle2 >= -3 * PI)) {
			/* exit region */
			if (depth[c.side] > 0) {
				--depth[c.side];
			}
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			/* Raycast */
			VR_UI::selection_mode = VR_UI::SELECTIONMODE_RAYCAST;
		}
		return;
	}
	case MENUTYPE_TS_CURSOR: {
		if (angle2 >= 0 && angle2 < PI) {
			/* Teleport to cursor */
			Widget_Cursor::cursor_teleport();
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			/* Set cursor to object origin */
			Widget_Cursor::cursor_set_to_object_origin();
		}
		else if (angle2 >= 3 * PI || (angle2 < -2 * PI && angle2 >= -3 * PI)) {
			/* exit region */
			if (depth[c.side] > 0) {
				--depth[c.side];
			}
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			/* Set cursor to world origin */
			Widget_Cursor::cursor_set_to_world_origin();
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
	case MENUTYPE_TS_MEASURE: {
		if (angle2 >= 0 && angle2 < PI) {
			/* Default clip */
			vr->clip_sta = VR_CLIP_NEAR;
			vr->clip_end = VR_CLIP_FAR;
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			/* Increase far clip */
			if (vr->clip_end < VR_CLIP_FAR * 10000.0f) {
				vr->clip_end *= 10.0f;
			}
		}
		else if (angle2 >= 3 * PI || (angle2 < -2 * PI && angle2 >= -3 * PI)) {
			/* exit region */
			if (depth[c.side] > 0) {
				--depth[c.side];
			}
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			/* Decrease far clip */
			if (vr->clip_end > 1.0f) {
				vr->clip_end /= 10.0f;
			}
		}
		return;
	}
	case MENUTYPE_TS_ADDPRIMITIVE: {
		if (angle2 >= 0 && angle2 < PI) {
			/* Plane */
			Widget_AddPrimitive::primitive = Widget_AddPrimitive::PRIMITIVE_PLANE;
		}
		else if (angle2 >= PI && angle2 < 2 * PI) {
			/* Cone */
			Widget_AddPrimitive::primitive = Widget_AddPrimitive::PRIMITIVE_CONE;
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			/* Monkey */
			Widget_AddPrimitive::primitive = Widget_AddPrimitive::PRIMITIVE_MONKEY;
		}
		else if (angle2 >= 3 * PI && angle2 < 4 * PI) {
			/* Circle */
			Widget_AddPrimitive::primitive = Widget_AddPrimitive::PRIMITIVE_CIRCLE;
		}
		else if (angle2 >= 4 * PI && angle2 < 5 * PI) {
			/* Icosphere */
			Widget_AddPrimitive::primitive = Widget_AddPrimitive::PRIMITIVE_ICOSPHERE;
		}
		else if (angle2 >= 5 * PI && angle2 < 6 * PI) {
			/* Join */
			Widget_Join::obj.click(c);
		}
		else if (angle2 >= 6 * PI || (angle2 < -5 * PI && angle2 >= -6 * PI)) {
			/* exit region */
			if (depth[c.side] > 0) {
				--depth[c.side];
			}
		}
		else if (angle2 < -4 * PI && angle2 >= -5 * PI) {
			/* Separate */
			Widget_Separate::obj.click(c);
		}
		else if (angle2 < -3 * PI && angle2 >= -4 * PI) {
			/* UV sphere */
			Widget_AddPrimitive::primitive = Widget_AddPrimitive::PRIMITIVE_UVSPHERE;
		}
		else if (angle2 < -2 * PI && angle2 >= -3 * PI) {
			/* Cube */
			Widget_AddPrimitive::primitive = Widget_AddPrimitive::PRIMITIVE_CUBE;
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			/* Grid */
			Widget_AddPrimitive::primitive = Widget_AddPrimitive::PRIMITIVE_GRID;
		}
		else if (angle2 < 0 && angle2 >= -PI) {
			/* Cylinder */
			Widget_AddPrimitive::primitive = Widget_AddPrimitive::PRIMITIVE_CYLINDER;
		}
		return;
	}
	case MENUTYPE_TS_EXTRUDE: {
		if (angle2 >= 0 && angle2 < PI) {
			/* Normals */
			Widget_Extrude::extrude_mode = Widget_Extrude::EXTRUDEMODE_NORMALS;
		}
		else if (angle2 >= PI && angle2 < 2 * PI) {
			/* Transform */
			Widget_Extrude::transform = !Widget_Extrude::transform;
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
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			/* Region */
			Widget_Extrude::extrude_mode = Widget_Extrude::EXTRUDEMODE_REGION;
		}
		else if (angle2 < 0 && angle2 >= -PI) {
			/* Flip normals */
			Widget_Extrude::flip_normals = !Widget_Extrude::flip_normals;
		}
		return;
	}
	case MENUTYPE_TS_INSETFACES: {
		if (angle2 >= 0 && angle2 < PI) {
			/* Individual */
			Widget_InsetFaces::use_individual = !Widget_InsetFaces::use_individual;
		}
		else if (angle2 >= PI && angle2 < 2 * PI) {
			/* Outset */
			Widget_InsetFaces::use_outset = !Widget_InsetFaces::use_outset;
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			/* Relative offset */
			Widget_InsetFaces::use_relative_offset = !Widget_InsetFaces::use_relative_offset;
		}
		else if (angle2 >= 3 * PI || (angle2 < -2 * PI && angle2 >= -3 * PI)) {
			/* exit region */
			if (depth[c.side] > 0) {
				--depth[c.side];
			}
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			/* Even offset */
			Widget_InsetFaces::use_even_offset = !Widget_InsetFaces::use_even_offset;
		}
		else if (angle2 < 0 && angle2 >= -PI) {
			/* Boundary */
			Widget_InsetFaces::use_boundary = !Widget_InsetFaces::use_boundary;
		}
		return;
	}
	case MENUTYPE_TS_BEVEL: {
		if (angle2 >= 0 && angle2 < PI) {
			/* Vertex only */
			Widget_Bevel::vertex_only = !Widget_Bevel::vertex_only;
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			/* Increase segments */
			if (Widget_Bevel::segments < 100) {
				++Widget_Bevel::segments;
			}
		}
		else if (angle2 >= 3 * PI || (angle2 < -2 * PI && angle2 >= -3 * PI)) {
			/* exit region */
			if (depth[c.side] > 0) {
				--depth[c.side];
			}
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			/* Decrease segments */
			if (Widget_Bevel::segments > 1) {
				--Widget_Bevel::segments;
			}
		}
		return;
	}
	case MENUTYPE_TS_LOOPCUT: {
		if (angle2 >= 0 && angle2 < PI) {
			/* Edge slide */
			Widget_LoopCut::edge_slide = !Widget_LoopCut::edge_slide;
		}
		else if (angle2 >= PI && angle2 < 2 * PI) {
			/* Clamp */
			Widget_LoopCut::clamp = !Widget_LoopCut::clamp;
		}
		else if (angle2 >= 2 * PI && angle2 < 3 * PI) {
			/* Increase cuts */
			if (Widget_LoopCut::cuts < 100) {
				++Widget_LoopCut::cuts;
			}
		}
		else if (angle2 >= 3 * PI || (angle2 < -2 * PI && angle2 >= -3 * PI)) {
			/* exit region */
			if (depth[c.side] > 0) {
				--depth[c.side];
			}
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			/* Decrease cuts */
			if (Widget_LoopCut::cuts > 1) {
				--Widget_LoopCut::cuts;
			}
		}
		else if (angle2 < 0 && angle2 >= -PI) {
			/* Flipped */
			Widget_LoopCut::flipped = !Widget_LoopCut::flipped;
		}
		return;
	}
	case MENUTYPE_TS_KNIFE: {
		/* TODO_XR */
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
		else if (angle2 >= 3 * PI && angle2 < 4 * PI) {
			/* Extrude */
			VR_UI::set_current_tool(&Widget_Extrude::obj, c.side);
			Widget_SwitchTool::obj.curr_tool[c.side] = &Widget_Extrude::obj;
		}
		else if (angle2 >= 4 * PI && angle2 < 5 * PI) {
			/* Bevel */
			VR_UI::set_current_tool(&Widget_Bevel::obj, c.side);
			Widget_SwitchTool::obj.curr_tool[c.side] = &Widget_Bevel::obj;
		}
		else if (angle2 >= 5 * PI && angle2 < 6 * PI) {
			/* Knife */
			VR_UI::set_current_tool(&Widget_Knife::obj, c.side);
			Widget_SwitchTool::obj.curr_tool[c.side] = &Widget_Knife::obj;
		}
		else if (angle2 >= 6 * PI || (angle2 < -5 * PI && angle2 >= -6 * PI)) {
			/* exit region */
			if (depth[c.side] > 0) {
				--depth[c.side];
			}
			return;
		}
		else if (angle2 < -4 * PI && angle2 >= -5 * PI) {
			/* Loop cut */
			VR_UI::set_current_tool(&Widget_LoopCut::obj, c.side);
			Widget_SwitchTool::obj.curr_tool[c.side] = &Widget_LoopCut::obj;
		}
		else if (angle2 < -3 * PI && angle2 >= -4 * PI) {
			/* Inset faces */
			VR_UI::set_current_tool(&Widget_InsetFaces::obj, c.side);
			Widget_SwitchTool::obj.curr_tool[c.side] = &Widget_InsetFaces::obj;
		}
		else if (angle2 < -2 * PI && angle2 >= -3 * PI) {
			/* Add primitive */
			VR_UI::set_current_tool(&Widget_AddPrimitive::obj, c.side);
			Widget_SwitchTool::obj.curr_tool[c.side] = &Widget_AddPrimitive::obj;
		}
		else if (angle2 < -PI && angle2 >= -2 * PI) {
			/* Select */
			VR_UI::set_current_tool(&Widget_Select::obj, c.side);
			Widget_SwitchTool::obj.curr_tool[c.side] = &Widget_Select::obj;
		}
		else if (angle2 < 0 && angle2 >= -PI) {
			/* Cursor */
			VR_UI::set_current_tool(&Widget_Cursor::obj, c.side);
			Widget_SwitchTool::obj.curr_tool[c.side] = &Widget_Cursor::obj;
		}

		VR_Widget *tool = VR_UI::get_current_tool(c.side);
		if (!tool) {
			menu_type[c.side] = MENUTYPE_MAIN_12;
			return;
		}
		switch (tool->type()) {
		case TYPE_SELECT: {
			menu_type[c.side] = MENUTYPE_TS_SELECT;
			break;
		}
		case TYPE_CURSOR: {
			menu_type[c.side] = MENUTYPE_TS_CURSOR;
			break;
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
			break;
		}
		case TYPE_ADDPRIMITIVE: {
			menu_type[c.side] = MENUTYPE_TS_ADDPRIMITIVE;
			break;
		}
		case TYPE_EXTRUDE: {
			menu_type[c.side] = MENUTYPE_TS_EXTRUDE;
			break;
		}
		case TYPE_INSETFACES: {
			menu_type[c.side] = MENUTYPE_TS_INSETFACES;
			break;
		}
		case TYPE_BEVEL: {
			menu_type[c.side] = MENUTYPE_TS_BEVEL;
			break;
		}
		case TYPE_LOOPCUT: {
			menu_type[c.side] = MENUTYPE_TS_LOOPCUT;
			break;
		}
		case TYPE_KNIFE: {
			menu_type[c.side] = MENUTYPE_TS_KNIFE;
			break;
		}
		default: {
			menu_type[c.side] = MENUTYPE_MAIN_12;
			break;
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
	if (!VR_UI::pie_menu_active[controller_side]) {
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

		VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::toolsettings_tex);
		return;
	}

	const MenuType& type = menu_type[controller_side];

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
		case MENUTYPE_TS_SELECT: {
			/* index = 0 */
			if (VR_UI::mouse_cursor_enabled) {
				VR_Draw::set_color(c_menu_red);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_0;
			if (menu_highlight_index == 0) {
				menu_str = "MOUSE CURSOR";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f,
				VR_UI::mouse_cursor_enabled ? VR_Draw::box_filled_tex : VR_Draw::box_empty_tex);
			if (VR_UI::mouse_cursor_enabled) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 4 */
			/* index = 2 */
			if (VR_UI::selection_mode == VR_UI::SELECTIONMODE_PROXIMITY) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_2;
			if (menu_highlight_index == 2) {
				menu_str = "PROXIMITY";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.011f, 0.011f, 0.011f, -0.011f, 0.001f, 1.0f, 1.0f, VR_Draw::select_proximity_tex);
			if (VR_UI::selection_mode == VR_UI::SELECTIONMODE_PROXIMITY) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 6 */
			/* index = 7 (exit region) */
			/* index = 5 */
			/* index = 1 */
			if (VR_UI::selection_mode == VR_UI::SELECTIONMODE_RAYCAST) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_1;
			if (menu_highlight_index == 1) {
				menu_str = "RAYCAST";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.011f, 0.011f, 0.011f, -0.011f, 0.001f, 1.0f, 1.0f, VR_Draw::select_raycast_tex);
			if (VR_UI::selection_mode == VR_UI::SELECTIONMODE_RAYCAST) {
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
		case MENUTYPE_TS_CURSOR: {
			/* index = 0 */
			if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_0;
			if (menu_highlight_index == 0) {
				menu_str = "TELEPORT TO CURSOR";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::cursor_teleport_tex);
			if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 4 */
			/* index = 2 */
			if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_2;
			if (menu_highlight_index == 2) {
				menu_str = "OBJECT ORIGIN";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::cursor_objorigin_tex);
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
			*((Coord3Df*)t_icon.m[3]) = p8_1;
			if (menu_highlight_index == 1) {
				menu_str = "WORLD ORIGIN";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::cursor_worldorigin_tex);
			if (menu_highlight_index == 1) {
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
			VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f,
				Widget_Transform::manipulator ? VR_Draw::box_filled_tex : VR_Draw::box_empty_tex);
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
		case MENUTYPE_TS_ANNOTATE: {
			return;
		}
		case MENUTYPE_TS_MEASURE: {
			/* index = 0 */
			if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_0;
			if (menu_highlight_index == 0) {
				menu_str = "DEFAULT CLIP";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::reset_tex);
			if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 4 */
			/* index = 2 */
			if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_2;
			if (menu_highlight_index == 2) {
				menu_str = "INCREASE CLIP";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f, VR_Draw::plus_tex);
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
			*((Coord3Df*)t_icon.m[3]) = p8_1;
			if (menu_highlight_index == 1) {
				menu_str = "DECREASE CLIP";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::minus_tex);
			if (menu_highlight_index == 1) {
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
		case MENUTYPE_TS_ADDPRIMITIVE: {
			/* index = 0 */
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_PLANE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_0;
			if (menu_highlight_index == 0) {
				menu_str = "PLANE";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_plane_tex);
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_PLANE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 4 */
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_CONE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_4;
			if (menu_highlight_index == 4) {
				menu_str = "CONE";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_cone_tex);
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_CONE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 6 */
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_MONKEY) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 6) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_6;
			if (menu_highlight_index == 6) {
				menu_str = "MONKEY";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_monkey_tex);
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_MONKEY) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 6) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 2 */
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_CIRCLE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_2;
			if (menu_highlight_index == 2) {
				menu_str = "CIRCLE";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_circle_tex);
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_CIRCLE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 8 */
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_ICOSPHERE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 8) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_8;
			if (menu_highlight_index == 8) {
				menu_str = "ICOSPHERE";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_icosphere_tex);
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_ICOSPHERE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 8) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 10 */
			if (menu_highlight_index == 10) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_10;
			if (menu_highlight_index == 10) {
				menu_str = "JOIN";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::join_tex);
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
				menu_str = "SEPARATE";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::separate_tex);
			if (menu_highlight_index == 9) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 7 */
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_UVSPHERE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 7) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_7;
			if (menu_highlight_index == 7) {
				menu_str = "UV SPHERE";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_uvsphere_tex);
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_UVSPHERE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 7) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 1 */
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_CUBE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_1;
			if (menu_highlight_index == 1) {
				menu_str = "CUBE";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_cube_tex);
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_CUBE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 5 */
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_GRID) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 5) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_5;
			if (menu_highlight_index == 5) {
				menu_str = "GRID";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_grid_tex);
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_GRID) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 5) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 3 */
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_CYLINDER) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_3;
			if (menu_highlight_index == 3) {
				menu_str = "CYLINDER";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_cylinder_tex);
			if (Widget_AddPrimitive::primitive == Widget_AddPrimitive::PRIMITIVE_CYLINDER) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_white);
			}
			/* Center */
			*((Coord3Df*)t_icon.m[3]) = p12_stick;
			m = t_icon * t;
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_string(menu_str.c_str(), 0.009f, 0.012f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.0f, 0.001f);
			break;
		}
		case MENUTYPE_TS_EXTRUDE: {
			/* index = 0 */
			if (Widget_Extrude::extrude_mode == Widget_Extrude::EXTRUDEMODE_NORMALS) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_0;
			if (menu_highlight_index == 0) {
				menu_str = "NORMALS";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::extrude_normals_tex);
			if (Widget_Extrude::extrude_mode == Widget_Extrude::EXTRUDEMODE_NORMALS) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 4 */
			if (Widget_Extrude::transform) {
				VR_Draw::set_color(c_menu_red);
			}
			else if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_4;
			if (menu_highlight_index == 4) {
				menu_str = "TRANSFORM";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f,
				Widget_Extrude::transform ? VR_Draw::box_filled_tex : VR_Draw::box_empty_tex);
			if (Widget_Extrude::transform) {
				VR_Draw::set_color(c_menu_white);
			}
			if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_white);
			}
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
			if (Widget_Extrude::flip_normals) {
				VR_Draw::set_color(c_menu_red);
			}
			else if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_3;
			if (menu_highlight_index == 3) {
				menu_str = "FLIP EDGES";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f, 
				Widget_Extrude::flip_normals ? VR_Draw::box_filled_tex : VR_Draw::box_empty_tex);
			if (Widget_Extrude::flip_normals) {
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
		case MENUTYPE_TS_INSETFACES: {
			/* index = 0 */
			if (Widget_InsetFaces::use_individual) {
				VR_Draw::set_color(c_menu_red);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_0;
			if (menu_highlight_index == 0) {
				menu_str = "INDIVIDUAL";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f,
				Widget_InsetFaces::use_individual ? VR_Draw::box_filled_tex : VR_Draw::box_empty_tex);
			if (Widget_InsetFaces::use_individual) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 4 */
			if (Widget_InsetFaces::use_outset) {
				VR_Draw::set_color(c_menu_red);
			}
			else if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_4;
			if (menu_highlight_index == 4) {
				menu_str = "OUTSET";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f,
				Widget_InsetFaces::use_outset ? VR_Draw::box_filled_tex : VR_Draw::box_empty_tex);
			if (Widget_InsetFaces::use_outset) {
				VR_Draw::set_color(c_menu_white);
			}
			if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 2 */
			if (Widget_InsetFaces::use_relative_offset) {
				VR_Draw::set_color(c_menu_red);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_2;
			if (menu_highlight_index == 2) {
				menu_str = "RELATIVE OFFSET";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f,
				Widget_InsetFaces::use_relative_offset ? VR_Draw::box_filled_tex : VR_Draw::box_empty_tex);
			if (Widget_InsetFaces::use_relative_offset) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 6 */
			/* index = 7 (exit region) */
			/* index = 5 */
			/* index = 1 */
			if (Widget_InsetFaces::use_even_offset) {
				VR_Draw::set_color(c_menu_red);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_1;
			if (menu_highlight_index == 1) {
				menu_str = "EVEN OFFSET";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f,
				Widget_InsetFaces::use_even_offset ? VR_Draw::box_filled_tex : VR_Draw::box_empty_tex);
			if (Widget_InsetFaces::use_even_offset) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 3 */
			if (Widget_InsetFaces::use_boundary) {
				VR_Draw::set_color(c_menu_red);
			}
			else if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_3;
			if (menu_highlight_index == 3) {
				menu_str = "BOUNDARY";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f,
				Widget_InsetFaces::use_boundary ? VR_Draw::box_filled_tex : VR_Draw::box_empty_tex);
			if (Widget_InsetFaces::use_boundary) {
				VR_Draw::set_color(c_menu_white);
			}
			if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_white);
			}
			/* Center */
			*((Coord3Df*)t_icon.m[3]) = p8_stick;
			m = t_icon * t;
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_string(menu_str.c_str(), 0.009f, 0.012f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.005f, 0.001f);
			break;
		}
		case MENUTYPE_TS_BEVEL: {
			/* index = 0 */
			if (Widget_Bevel::vertex_only) {
				VR_Draw::set_color(c_menu_red);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_0;
			if (menu_highlight_index == 0) {
				menu_str = "VERTEX ONLY";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f,
				Widget_Bevel::vertex_only ? VR_Draw::box_filled_tex : VR_Draw::box_empty_tex);
			if (Widget_Bevel::vertex_only) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 4 */
			/* index = 2 */
			if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_2;
			if (menu_highlight_index == 2) {
				menu_str = "INCREASE SEGMENTS";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f, VR_Draw::plus_tex);
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
			*((Coord3Df*)t_icon.m[3]) = p8_1;
			if (menu_highlight_index == 1) {
				menu_str = "DECREASE SEGMENTS";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::minus_tex);
			if (menu_highlight_index == 1) {
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
		case MENUTYPE_TS_LOOPCUT: {
			/* index = 0 */
			if (Widget_LoopCut::edge_slide) {
				VR_Draw::set_color(c_menu_red);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_0;
			if (menu_highlight_index == 0) {
				menu_str = "EDGE SLIDE";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f,
				Widget_LoopCut::edge_slide ? VR_Draw::box_filled_tex : VR_Draw::box_empty_tex);
			if (Widget_LoopCut::edge_slide) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 0) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 4 */
			if (Widget_LoopCut::clamp) {
				VR_Draw::set_color(c_menu_red);
			}
			else if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_4;
			if (menu_highlight_index == 4) {
				menu_str = "CLAMP";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f,
				Widget_LoopCut::clamp ? VR_Draw::box_filled_tex : VR_Draw::box_empty_tex);
			if (Widget_LoopCut::clamp) {
				VR_Draw::set_color(c_menu_white);
			}
			if (menu_highlight_index == 4) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 2 */
			if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_2;
			if (menu_highlight_index == 2) {
				menu_str = "INCREASE CUTS";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f, VR_Draw::plus_tex);
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
			*((Coord3Df*)t_icon.m[3]) = p8_1;
			if (menu_highlight_index == 1) {
				menu_str = "DECREASE CUTS";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::minus_tex);
			if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 3 */
			if (Widget_LoopCut::flipped) {
				VR_Draw::set_color(c_menu_red);
			}
			else if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p8_3;
			if (menu_highlight_index == 3) {
				menu_str = "FLIP EDGES";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f,
				Widget_LoopCut::flipped ? VR_Draw::box_filled_tex : VR_Draw::box_empty_tex);
			if (Widget_LoopCut::flipped) {
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
		case MENUTYPE_TS_KNIFE: {
			/* TODO_XR */
			/* index = 0 */
			//if (menu_highlight_index == 0) {
			//	VR_Draw::set_color(c_menu_blue);
			//}
			//*((Coord3Df*)t_icon.m[3]) = p8_0;
			//if (menu_highlight_index == 0) {
			//	menu_str = "OCCLUDE GEOM";
			//	m = m_widget_touched * t_icon * t;
			//}
			//else {
			//	m = t_icon * t;
			//}
			//VR_Draw::update_modelview_matrix(&m, 0);
			//VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f, VR_Draw::box_empty_tex);
			//VR_Draw::set_color(c_menu_white);
			///* index = 4 */
			///* index = 2 */
			//if (menu_highlight_index == 2) {
			//	VR_Draw::set_color(c_menu_blue);
			//}
			//*((Coord3Df*)t_icon.m[3]) = p8_2;
			//if (menu_highlight_index == 2) {
			//	menu_str = "INCREASE CUTS";
			//	m = m_widget_touched * t_icon * t;
			//}
			//else {
			//	m = t_icon * t;
			//}
			//VR_Draw::update_modelview_matrix(&m, 0);
			//VR_Draw::render_rect(-0.005f, 0.005f, 0.005f, -0.005f, 0.001f, 1.0f, 1.0f, VR_Draw::plus_tex);
			//if (menu_highlight_index == 2) {
			//	VR_Draw::set_color(c_menu_white);
			//}
			///* index = 6 */
			///* index = 7 (exit region) */
			///* index = 5 */
			///* index = 1 */
			//if (menu_highlight_index == 1) {
			//	VR_Draw::set_color(c_menu_blue);
			//}
			//*((Coord3Df*)t_icon.m[3]) = p8_1;
			//if (menu_highlight_index == 1) {
			//	menu_str = "DECREASE CUTS";
			//	m = m_widget_touched * t_icon * t;
			//}
			//else {
			//	m = t_icon * t;
			//}
			//VR_Draw::update_modelview_matrix(&m, 0);
			//VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::minus_tex);
			//if (menu_highlight_index == 1) {
			//	VR_Draw::set_color(c_menu_white);
			//}
			///* index = 3 */
			///* Center */
			//*((Coord3Df*)t_icon.m[3]) = p8_stick;
			//m = t_icon * t;
			//VR_Draw::update_modelview_matrix(&m, 0);
			//VR_Draw::render_string(menu_str.c_str(), 0.009f, 0.012f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.005f, 0.001f);
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
			*((Coord3Df*)t_icon.m[3]) = p12_0;
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
			*((Coord3Df*)t_icon.m[3]) = p12_4;
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
			/* index = 6 */
			if (type == TYPE_MEASURE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 6) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_6;
			if (menu_highlight_index == 6) {
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
			else if (menu_highlight_index == 6) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 2 */
			if (type == TYPE_EXTRUDE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_2;
			if (menu_highlight_index == 2) {
				menu_str = "EXTRUDE";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::extrude_tex);
			if (type == TYPE_EXTRUDE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 2) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 8 */
			if (type == TYPE_BEVEL) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 8) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_8;
			if (menu_highlight_index == 8) {
				menu_str = "BEVEL";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::bevel_tex);
			if (type == TYPE_BEVEL) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 8) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 10 */
			if (type == TYPE_KNIFE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 10) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_10;
			if (menu_highlight_index == 10) {
				menu_str = "KNIFE";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::knife_tex);
			if (type == TYPE_KNIFE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 10) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 11 (exit region) */
			/* index = 9 */
			if (type == TYPE_LOOPCUT) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 9) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_9;
			if (menu_highlight_index == 9) {
				menu_str = "LOOP CUT";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::loopcut_tex);
			if (type == TYPE_LOOPCUT) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 9) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 7 */
			if (type == TYPE_INSETFACES) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 7) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_7;
			if (menu_highlight_index == 7) {
				menu_str = "INSET FACES";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.011f, 0.011f, 0.011f, -0.011f, 0.001f, 1.0f, 1.0f, VR_Draw::insetfaces_tex);
			if (type == TYPE_INSETFACES) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 7) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 1 */
			if (type == TYPE_ADDPRIMITIVE) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_1;
			if (menu_highlight_index == 1) {
				menu_str = "ADD PRIMITIVE";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.011f, 0.011f, 0.011f, -0.011f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_tex);
			if (type == TYPE_ADDPRIMITIVE) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 1) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 5 */
			if (type == TYPE_SELECT) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 5) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_5;
			if (menu_highlight_index == 5) {
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
			else if (menu_highlight_index == 5) {
				VR_Draw::set_color(c_menu_white);
			}
			/* index = 3 */
			if (type == TYPE_CURSOR) {
				VR_Draw::set_color(c_menu_green);
			}
			else if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_blue);
			}
			*((Coord3Df*)t_icon.m[3]) = p12_3;
			if (menu_highlight_index == 3) {
				menu_str = "CURSOR";
				m = m_widget_touched * t_icon * t;
			}
			else {
				m = t_icon * t;
			}
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::cursor_tex);
			if (type == TYPE_CURSOR) {
				VR_Draw::set_color(c_menu_white);
			}
			else if (menu_highlight_index == 3) {
				VR_Draw::set_color(c_menu_white);
			}
			/* Center */
			*((Coord3Df*)t_icon.m[3]) = p12_stick;
			m = t_icon * t;
			VR_Draw::update_modelview_matrix(&m, 0);
			VR_Draw::render_string(menu_str.c_str(), 0.009f, 0.012f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.0f, 0.001f);
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
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_global_tex);
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
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::object_tex);
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
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_global_tex);
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
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::object_tex);
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
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::manip_global_tex);
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
