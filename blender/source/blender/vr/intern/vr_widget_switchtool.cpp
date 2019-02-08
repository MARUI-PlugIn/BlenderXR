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

/** \file blender/vr/intern/vr_widget_switchtool.cpp
*   \ingroup vr
* 
* Main module for the VR widget UI.
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_addprimitive.h"
#include "vr_widget_extrude.h"
#include "vr_widget_layout.h"
#include "vr_widget_menu.h"
#include "vr_widget_select.h"
#include "vr_widget_switchtool.h"
#include "vr_widget_transform.h"

#include "vr_draw.h"

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
			VR_UI::SelectionMode& mode = VR_UI::selection_mode;
			if (mode == VR_UI::SELECTIONMODE_RAYCAST) {
				VR_Draw::render_rect(-0.011f, 0.011f, 0.011f, -0.011f, 0.001f, 1.0f, 1.0f, VR_Draw::select_raycast_tex);
			}
			else {
				VR_Draw::render_rect(-0.011f, 0.011f, 0.011f, -0.011f, 0.001f, 1.0f, 1.0f, VR_Draw::select_proximity_tex);
			}
			break;
		}
		case TYPE_CURSOR: {
			VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::cursor_tex);
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
		case TYPE_ADDPRIMITIVE: {
			switch (Widget_AddPrimitive::primitive) {
			case Widget_AddPrimitive::PRIMITIVE_PLANE: {
				VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_plane_tex);
				break;
			}
			case Widget_AddPrimitive::PRIMITIVE_CUBE: {
				VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_cube_tex);
				break;
			}
			case Widget_AddPrimitive::PRIMITIVE_CIRCLE: {
				VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_circle_tex);
				break;
			}
			case Widget_AddPrimitive::PRIMITIVE_CYLINDER: {
				VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_cylinder_tex);
				break;
			}
			case Widget_AddPrimitive::PRIMITIVE_CONE: {
				VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_cone_tex);
				break;
			}
			case Widget_AddPrimitive::PRIMITIVE_GRID: {
				VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_grid_tex);
				break;
			}
			case Widget_AddPrimitive::PRIMITIVE_MONKEY: {
				VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_monkey_tex);
				break;
			}
			case Widget_AddPrimitive::PRIMITIVE_UVSPHERE: {
				VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_uvsphere_tex);
				break;
			}
			case Widget_AddPrimitive::PRIMITIVE_ICOSPHERE: {
				VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::mesh_icosphere_tex);
				break;
			}
			default: {
				break;
			}
			}
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
		case TYPE_INSETFACES: {
			VR_Draw::render_rect(-0.011f, 0.011f, 0.011f, -0.011f, 0.001f, 1.0f, 1.0f, VR_Draw::insetfaces_tex);
			break;
		}
		case TYPE_BEVEL: {
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::bevel_tex);
			break;
		}
		case TYPE_LOOPCUT: {
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::loopcut_tex);
			break;
		}
		case TYPE_KNIFE: {
			VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::knife_tex);
			break;
		}
		default: {
			break;
		}
	}
}
