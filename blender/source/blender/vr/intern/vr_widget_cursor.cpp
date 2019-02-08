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

/** \file blender/vr/intern/vr_widget_cursor.cpp
*   \ingroup vr
* 
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_cursor.h"

#include "vr_math.h"

#include "BLI_math.h"

#include "BKE_context.h"

#include "DEG_depsgraph.h"

#include "DNA_scene_types.h"

//#include "ED_undo.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

/**************************************************************************************************\
 * \class                               Widget_Cursor											   *
 ***************************************************************************************************
 * Interaction widget for the Blender cursor.													   *
 **************************************************************************************************/

Widget_Cursor Widget_Cursor::obj;

void Widget_Cursor::cursor_teleport()
{
	/* Convert cursor position from Blender to real space. */
	Scene *scene = CTX_data_scene(vr_get_obj()->ctx);
	static Mat44f m_blender;
	ED_view3d_cursor3d_calc_mat4(scene, m_blender.m);
	static Mat44f m_real = VR_Math::identity_f;
	memcpy(m_real.m[3],	VR_UI::convert_space(m_blender, VR_SPACE_BLENDER, VR_SPACE_REAL).m[3], sizeof(float) * 3);

	/* Apply navigation transformation to move from the HMD to the cursor. */
	static Mat44f ref = VR_Math::identity_f;
	memcpy(ref.m[3], VR_UI::hmd_position_get(VR_SPACE_REAL).m[3], sizeof(float) * 3);

	VR_UI::navigation_apply_transformation(m_real.inverse() * ref, VR_SPACE_REAL);
}

void Widget_Cursor::cursor_set_to_world_origin()
{
	/* Update the Blender 3D cursor */
	bContext *C = vr_get_obj()->ctx;
	Scene *scene = CTX_data_scene(C);
	mat4_to_quat(scene->cursor.rotation, (float(*)[4])VR_Math::identity_f.m);
	memset(scene->cursor.location, 0, sizeof(float) * 3);

	WM_event_add_notifier(C, NC_SCENE | NA_EDITED, scene);
	DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
	//ED_undo_push(C, "Cursor");
}

void Widget_Cursor::cursor_set_to_object_origin()
{
	bContext *C = vr_get_obj()->ctx;
	if (CTX_data_edit_object(C)) {
		return;
	}

	ListBase ctx_data_list;
	Object *obact = NULL;
	int num_objects = 0;
	CTX_data_selected_objects(C, &ctx_data_list);
	CollectionPointerLink *ctx_link = (CollectionPointerLink *)ctx_data_list.first;

	static Mat44f center;
	memset(center.m, 0, sizeof(float) * 4 * 4);

	for (; ctx_link; ctx_link = ctx_link->next) {
		obact = (Object*)ctx_link->ptr.data;
		if (!obact) {
			continue;
		}
		/* Average object rotations (z-axis). */
		*(Coord3Df*)center.m[2] += *(Coord3Df*)obact->obmat[2];
		/* Average object positions. */
		*(Coord3Df*)center.m[3] += *(Coord3Df*)obact->obmat[3];
		++num_objects;
	}
	*(Coord3Df*)center.m[2] /= num_objects;
	(*(Coord3Df*)center.m[2]).normalize_in_place();
	static float rot[3][3];
	static float z_axis[3] = { 0.0f, 0.0f, 1.0f };
	rotation_between_vecs_to_mat3(rot, z_axis, center.m[2]);
	*(Coord3Df*)center.m[3] /= num_objects;

	/* Update the Blender 3D cursor */
	Scene *scene = CTX_data_scene(C);
	mat3_to_quat(scene->cursor.rotation, rot);
	memcpy(scene->cursor.location, &center.m[3], sizeof(float) * 3);

	WM_event_add_notifier(C, NC_SCENE | NA_EDITED, scene);
	DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
	//ED_undo_push(C, "Cursor");
}

bool Widget_Cursor::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Cursor::click(VR_UI::Cursor& c)
{
	/* Update the Blender 3D cursor */
	const Mat44f& m = c.position.get(VR_SPACE_BLENDER);

	bContext *C = vr_get_obj()->ctx;
	Scene *scene = CTX_data_scene(C);
	mat4_to_quat(scene->cursor.rotation, (float(*)[4])m.m);
	memcpy(scene->cursor.location, &m.m[3], sizeof(float) * 3);

	WM_event_add_notifier(C, NC_SCENE | NA_EDITED, scene);
	DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
	//ED_undo_push(C, "Cursor");
}

void Widget_Cursor::drag_start(VR_UI::Cursor& c)
{
	/* Update the Blender 3D cursor */
	const Mat44f& m = c.position.get(VR_SPACE_BLENDER);

	bContext *C = vr_get_obj()->ctx;
	Scene *scene = CTX_data_scene(C);
	mat4_to_quat(scene->cursor.rotation, (float(*)[4])m.m);
	memcpy(scene->cursor.location, &m.m[3], sizeof(float) * 3);

	DEG_id_tag_update(&scene->id, 0);
}

void Widget_Cursor::drag_contd(VR_UI::Cursor& c)
{
	/* Update the Blender 3D cursor */
	const Mat44f& m = c.position.get(VR_SPACE_BLENDER);

	bContext *C = vr_get_obj()->ctx;
	Scene *scene = CTX_data_scene(C);
	mat4_to_quat(scene->cursor.rotation, (float(*)[4])m.m);
	memcpy(scene->cursor.location, &m.m[3], sizeof(float) * 3);

	DEG_id_tag_update(&scene->id, 0);
}

void Widget_Cursor::drag_stop(VR_UI::Cursor& c)
{
	/* Update the Blender 3D cursor */
	const Mat44f& m = c.position.get(VR_SPACE_BLENDER);

	bContext *C = vr_get_obj()->ctx;
	Scene *scene = CTX_data_scene(C);
	mat4_to_quat(scene->cursor.rotation, (float(*)[4])m.m);
	memcpy(scene->cursor.location, &m.m[3], sizeof(float) * 3);

	WM_event_add_notifier(C, NC_SCENE | NA_EDITED, scene);
	DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
	//ED_undo_push(C, "Cursor");
}