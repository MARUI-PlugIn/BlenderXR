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
* The Original Code is Copyright (C) 2019 by Blender Foundation.
* All rights reserved.
*
* Contributor(s): MARUI-PlugIn, Multiplexed Reality
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/vr/intern/vr_widget_switchcomponent.cpp
*   \ingroup vr
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_switchcomponent.h"
#include "vr_widget_transform.h"
#include "vr_widget_sculpt.h"

#include "vr_math.h"
#include "vr_draw.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"

#include "DEG_depsgraph.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_undo.h"

#include "WM_api.h"
#include "WM_types.h"

/***************************************************************************************************
 * \class                               Widget_SwitchComponent
 ***************************************************************************************************
 * Interaction widget for switching the currently active component mode.
 *
 **************************************************************************************************/
Widget_SwitchComponent Widget_SwitchComponent::obj;

bool Widget_SwitchComponent::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_SwitchComponent::click(VR_UI::Cursor& c)
{
	if (Widget_Transform::is_dragging || Widget_Sculpt::is_dragging) {
		/* Don't switch component modes if object data is currently being modified. */
		return;
	}

	bContext *C = vr_get_obj()->ctx;
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	short& select_mode = ts->selectmode;
	Object *ob_edit = CTX_data_edit_object(C);

	/* Cycle through component modes. */
	if (ob_edit) {
		if (select_mode == SCE_SELECT_VERTEX) {
			select_mode = SCE_SELECT_EDGE;
		}
		else if (select_mode == SCE_SELECT_EDGE) {
			select_mode = SCE_SELECT_FACE;
		}
		else if (select_mode == SCE_SELECT_FACE) {
			/* Exit edit mode */
			/* Execute operation and update manipulator on post-render */
			VR_UI::editmode_exit = true;
			// ED_object_editmode_exit(C, EM_FREEDATA);
			Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_LOCAL;
		    return;
		}
		else { /* Multi mode */
			/* TODO_XR */
		}
	}
	else {
		/* Enter edit mode */
		ED_object_editmode_enter(C, EM_NO_CONTEXT);
		/* Set transform space to normal by default. */
		Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_NORMAL;
		select_mode = SCE_SELECT_VERTEX;
		ob_edit = CTX_data_edit_object(C);
	}

	if (ob_edit) {
		BMEditMesh *em = BKE_editmesh_from_object(ob_edit);
		if (em) {
			em->selectmode = select_mode;
			EDBM_selectmode_set(em);
			DEG_id_tag_update((ID *)ob_edit->data, ID_RECALC_COPY_ON_WRITE | ID_RECALC_SELECT);
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob_edit->data);
		}
	}

	/* Update manipulators. */
	Widget_Transform::update_manipulator();

	WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, NULL);
	DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
	ED_undo_push(C, "Selectmode");
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
	ToolSettings *ts = CTX_data_scene(C)->toolsettings;
	if (CTX_data_edit_object(C)) {
		switch (ts->selectmode) {
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
