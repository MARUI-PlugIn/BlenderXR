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

/** \file blender/vr/intern/vr_widget_join.cpp
*   \ingroup vr
*
* Main module for the VR widget UI.
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_join.h"
#include "vr_widget_transform.h"

#include "vr_draw.h"

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "DNA_gpencil_types.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_gpencil.h"
#include "ED_mesh.h"
#include "ED_undo.h"

/***********************************************************************************************//**
 * \class                               Widget_Join
 ***************************************************************************************************
 * Interaction widget for performing a 'join' operation.
 *
 **************************************************************************************************/
Widget_Join Widget_Join::obj;

/* Dummy op to pass to join_x_exec() functions */
static wmOperator join_dummy_op;

/* From bject_add.c */
static int join_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);

	if (ob->mode & OB_MODE_EDIT) {
		BKE_report(op->reports, RPT_ERROR, "This data does not support joining in edit mode");
		return OPERATOR_CANCELLED;
	}
	else if (BKE_object_obdata_is_libdata(ob)) {
		BKE_report(op->reports, RPT_ERROR, "Cannot edit external libdata");
		return OPERATOR_CANCELLED;
	}
	else if (ob->type == OB_GPENCIL) {
		bGPdata *gpd = (bGPdata *)ob->data;
		if ((!gpd) || GPENCIL_ANY_MODE(gpd)) {
			BKE_report(op->reports, RPT_ERROR, "This data does not support joining in this mode");
			return OPERATOR_CANCELLED;
		}
	}

	if (ob->type == OB_MESH)
		return join_mesh_exec(C, op);
	else if (ELEM(ob->type, OB_CURVE, OB_SURF))
		return join_curve_exec(C, op);
	else if (ob->type == OB_ARMATURE)
		return join_armature_exec(C, op);
	else if (ob->type == OB_GPENCIL)
		return ED_gpencil_join_objects_exec(C, op);

	return OPERATOR_CANCELLED;
}

bool Widget_Join::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Join::click(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	if (join_exec(C, &join_dummy_op) == OPERATOR_FINISHED) {
		/* Update manipulators */
		Widget_Transform::update_manipulator();

		ED_undo_push(C, "Join");
	}
}

bool Widget_Join::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_Join::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
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
	VR_Draw::render_rect(-0.007f, 0.007f, 0.007f, -0.007f, 0.001f, 1.0f, 1.0f, VR_Draw::join_tex);
}
