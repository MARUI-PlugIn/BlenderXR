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
* Contributor(s): Multiplexed Reality
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/vr/intern/vr_widget_animation.cpp
*   \ingroup vr
*
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_animation.h"

#include "vr_math.h"
#include "vr_draw.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_layer.h"

#include "DEG_depsgraph.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

//#include "WM_api.h"
//#include "WM_types.h"

#include "vr_util.h"

/***************************************************************************************************
 * \class									Widget_Animation
 ***************************************************************************************************
 * Interaction widget for the Animation tool.
 *
 **************************************************************************************************/
Widget_Animation Widget_Animation::obj;

Widget_Animation::BindType Widget_Animation::bind_type(Widget_Animation::BINDTYPE_NONE);
std::vector<Object*> Widget_Animation::bindings;
bool Widget_Animation::binding_paused(false);

int Widget_Animation::constraint_flag[3][3] = { 0 };
VR_UI::TransformSpace Widget_Animation::transform_space(VR_UI::TRANSFORMSPACE_GLOBAL);

/* Manipulator colors. */
static const float c_manip[4][4] = { 1.0f, 0.2f, 0.322f, 0.4f,
									 0.545f, 0.863f, 0.0f, 0.4f,
									 0.157f, 0.565f, 1.0f, 0.4f,
									 1.0f, 1.0f, 1.0f, 0.4f };
static const float c_manip_select[4][4] = { 1.0f, 0.2f, 0.322f, 1.0f,
											0.545f, 0.863f, 0.0f, 1.0f,
											0.157f, 0.565f, 1.0f, 1.0f,
											1.0f, 1.0f, 1.0f, 1.0f };
/* Scale factors for manipulator rendering. */
#define WIDGET_ANIMATION_ARROW_SCALE_FACTOR	0.1f
#define WIDGET_ANIMATION_DIAL_RESOLUTION 100

bool Widget_Animation::manipulator(false);
Mat44f Widget_Animation::manip_t = VR_Math::identity_f;
Coord3Df Widget_Animation::manip_angle[2];
float Widget_Animation::manip_scale_factor(4.0f/*2.0f*/);

void Widget_Animation::update_bindings()
{
	if (bindings.size() < 1 || binding_paused) {
		return;
	}

	/* Bind type */
	Mat44f m;
	switch (bind_type) {
	case BINDTYPE_HMD: {
		m = VR_UI::hmd_position_get(VR_SPACE_BLENDER);
		break;
	}
	case BINDTYPE_CONTROLLER_LEFT: {
		m = VR_UI::controller_position_get(VR_SPACE_BLENDER, VR_SIDE_LEFT);
		break;
	}
	case BINDTYPE_CONTROLLER_RIGHT: {
		m = VR_UI::controller_position_get(VR_SPACE_BLENDER, VR_SIDE_RIGHT);
		break;
	}
	case BINDTYPE_TRACKER: {
		if (!vr_get_obj()->controller[VR_SIDE_AUX]->available) {
			return;
		}
		m = VR_UI::controller_position_get(VR_SPACE_BLENDER, VR_SIDE_AUX);
		break;
	}
	case BINDTYPE_NONE:
	default: {
		clear_bindings();
		return;
	}
	}

	for (auto it = bindings.begin(); it != bindings.end(); ++it) {
		Object *ob = *it;
		if (!ob) {
			bindings.erase(it);
			continue;
		}
		Mat44f& obmat = *(Mat44f*)ob->obmat;
		Mat44f obmat_orig = obmat;

		float scale;
		for (int i = 0; i < 3; ++i) {
			scale = len_v3(obmat.m[i]);
			*(Coord3Df*)obmat.m[i] = (*(Coord3Df*)m.m[i]).normalize_in_place() * scale;
		}
		*(Coord3Df*)obmat.m[3] = *(Coord3Df*)m.m[3];

		/* Contraints */
		float rot[3][3];
		float temp1[3], temp2[3];
		switch (transform_space) {
		case VR_UI::TRANSFORMSPACE_GLOBAL: {
			if (constraint_flag[0][0]) {
				//obmat.m[3][0] = obmat_orig.m[3][0];
				obmat.m[3][1] = obmat_orig.m[3][1];
				obmat.m[3][2] = obmat_orig.m[3][2];
			}
			if (constraint_flag[0][1]) {
				//obmat.m[3][1] = obmat_orig.m[3][1];
				obmat.m[3][0] = obmat_orig.m[3][0];
				obmat.m[3][2] = obmat_orig.m[3][2];
			}
			if (constraint_flag[0][2]) {
				//obmat.m[3][2] = obmat_orig.m[3][2];
				obmat.m[3][0] = obmat_orig.m[3][0];
				obmat.m[3][1] = obmat_orig.m[3][1];
			}
			if (constraint_flag[1][0]) {
				normalize_v3_v3(temp1, obmat.m[0]);
				rotation_between_vecs_to_mat3(rot, temp1, VR_Math::identity_f.m[0]);
				mul_m4_m3m4(obmat.m, rot, obmat.m);
			}
			if (constraint_flag[1][1]) {
				normalize_v3_v3(temp1, obmat.m[1]);
				rotation_between_vecs_to_mat3(rot, temp1, VR_Math::identity_f.m[1]);
				mul_m4_m3m4(obmat.m, rot, obmat.m);
			}
			if (constraint_flag[1][2]) {
				normalize_v3_v3(temp1, obmat.m[2]);
				rotation_between_vecs_to_mat3(rot, temp1, VR_Math::identity_f.m[2]);
				mul_m4_m3m4(obmat.m, rot, obmat.m);
			}
			//if (constraint_scale & VR_AXIS_X) {
			//	//
			//}
			//if (constraint_scale &VR_AXIS_Y) {
			//	//
			//}
			//if (constraint_scale & VR_AXIS_Z) {
			//	//
			//}
			break;
		}
		case VR_UI::TRANSFORMSPACE_LOCAL: {
			if (constraint_flag[0][0]) {
				//project_v3_plane(obmat.m[3], obmat_orig.m[0], obmat_orig.m[3]);
				project_v3_v3v3(obmat.m[3], obmat.m[3], obmat_orig.m[0]);
			}
			if (constraint_flag[0][1]) {
				//project_v3_plane(obmat.m[3], obmat_orig.m[1], obmat_orig.m[3]);
				project_v3_v3v3(obmat.m[3], obmat.m[3], obmat_orig.m[1]);
			}
			if (constraint_flag[0][2]) {
				//project_v3_plane(obmat.m[3], obmat_orig.m[2], obmat_orig.m[3]);
				project_v3_v3v3(obmat.m[3], obmat.m[3], obmat_orig.m[2]);
			}
			if (constraint_flag[1][0]) {
				normalize_v3_v3(temp1, obmat.m[0]);
				normalize_v3_v3(temp2, obmat_orig.m[0]);
				rotation_between_vecs_to_mat3(rot, temp1, temp2);
				mul_m4_m3m4(obmat.m, rot, obmat.m);
			}
			if (constraint_flag[1][1]) {
				normalize_v3_v3(temp1, obmat.m[1]);
				normalize_v3_v3(temp2, obmat_orig.m[1]);
				rotation_between_vecs_to_mat3(rot, temp1, temp2);
				mul_m4_m3m4(obmat.m, rot, obmat.m);
			}
			if (constraint_flag[1][2]) {
				normalize_v3_v3(temp1, obmat.m[2]);
				normalize_v3_v3(temp2, obmat_orig.m[2]);
				rotation_between_vecs_to_mat3(rot, temp1, temp2);
				mul_m4_m3m4(obmat.m, rot, obmat.m);
			}
			//if (constraint_scale & VR_AXIS_X) {
			//	//
			//}
			//if (constraint_scale &VR_AXIS_Y) {
			//	//
			//}
			//if (constraint_scale & VR_AXIS_Z) {
			//	//
			//}
			break;
		}
		case VR_UI::TRANSFORMSPACE_NORMAL:
		default: {
			break;
		}
		}

		DEG_id_tag_update((ID*)ob->data, 0);

		/* Translation */
		//Mat44f& t = *(Mat44f*)ob->obmat;
		//memcpy(ob->loc, t.m[3], sizeof(float) * 3);
		///* Rotation */
		//mat4_to_eul(ob->rot, t.m);
		///* Scale */
		//ob->scale[0] = (*(Coord3Df*)(t.m[0])).length();
		//ob->scale[1] = (*(Coord3Df*)(t.m[1])).length();
		//ob->scale[2] = (*(Coord3Df*)(t.m[2])).length();

		//bContext *C = vr_get_obj()->ctx;
		//Scene *scene = CTX_data_scene(C);
		//DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
		//WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	}

	/* Update mamipulators. */
	update_manipulator();
}

void Widget_Animation::clear_bindings()
{
	if (bindings.size() < 1) {
		return;
	}

	bindings.clear();

	/* Update mamipulators. */
	manipulator = false;
	memset(manip_t.m, 0, sizeof(float) * 4 * 4);
}

void Widget_Animation::update_manipulator()
{
	if (bindings.size() < 1) {
		manipulator = false;
		memset(manip_t.m, 0, sizeof(float) * 4 * 4);
		return;
	}
	else {
		manipulator = true;
	}

	manip_t.set_to_identity();
	if (transform_space == VR_UI::TRANSFORMSPACE_LOCAL) {
		manip_t.m[0][0] = manip_t.m[1][1] = manip_t.m[2][2] = 0.0f;
	}
	float manip_length = 0.0f;
	int num_objects = 0;
	for (auto it = bindings.begin(); it != bindings.end(); ++it) {
		Object *ob = *it;
		if (!ob) {
			bindings.erase(it);
			continue;
		}

		if (transform_space == VR_UI::TRANSFORMSPACE_LOCAL) {
			/* Average object rotations (z-axis). */
			*(Coord3Df*)manip_t.m[2] += *(Coord3Df*)ob->obmat[2];
		}
		/* Average object positions for manipulator location */
		*(Coord3Df*)manip_t.m[3] += *(Coord3Df*)ob->obmat[3];
		/* Use largest axis size (across all objects) for manipulator size */
		for (int i = 0; i < 3; ++i) {
			const float& len = (*(Coord3Df*)ob->obmat[i]).length();
			if (len > manip_length) {
				manip_length = len;
			}
		}
		++num_objects;
	}

	*(Coord3Df*)manip_t.m[3] /= num_objects;
	if (transform_space == VR_UI::TRANSFORMSPACE_LOCAL) {
		*(Coord3Df*)manip_t.m[2] /= num_objects;
		(*(Coord3Df*)manip_t.m[2]).normalize_in_place();
		static float rot[3][3];
		static float z_axis[3] = { 0.0f, 0.0f, 1.0f };
		rotation_between_vecs_to_mat3(rot, z_axis, manip_t.m[2]);
		for (int i = 0; i < 3; ++i) {
			memcpy(manip_t.m[i], rot[i], sizeof(float) * 3);
		}
		/* Apply uniform scaling to manipulator */
		for (int i = 0; i < 3; ++i) {
			*(Coord3Df*)manip_t.m[i] *= manip_length;
		}
	}
	else {
		/* Apply uniform scaling to manipulator */
		for (int i = 0; i < 3; ++i) {
			(*(Coord3Df*)manip_t.m[i]).normalize_in_place() *= manip_length;
		}
	}
}

bool Widget_Animation::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Animation::click(VR_UI::Cursor& c)
{
	if (VR_UI::ctrl_key_get()) {
		/* Remove from bindings */
		if (bindings.size() > 0) {
			bindings.pop_back();

			/* Update manipulator transform. */
			Widget_Animation::update_manipulator();
		}
	}
	else {
		const Mat44f& m = c.position.get();
		Base *base = NULL;
		if (CTX_data_edit_object(vr_get_obj()->ctx)) {
			VR_Util::raycast_select_single_edit(*(Coord3Df*)m.m[3], false, false); //VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
		}
		else {
			base = VR_Util::raycast_select_single(*(Coord3Df*)m.m[3], false, false); //VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
		}

		if (base) {
			if (VR_UI::shift_key_get()) {
				/* Add to bindings */
				bool is_duplicate = false;
				for (auto it = bindings.begin(); it != bindings.end(); ++it) {
					if (base->object == *it) {
						is_duplicate = true;
						break;
					}
				}
				if (!is_duplicate) {
					bindings.push_back(base->object);
				}
			}
			else {
				/* Replace bindings */
				bindings.clear();
				bindings.push_back(base->object);
			}

			/* Update manipulator transform. */
			Widget_Animation::update_manipulator();
		}
	}

	if (manipulator) {
		for (int i = 0; i < VR_SIDES; ++i) {
			Widget_Animation::obj.do_render[i] = true;
		}
	}
}

void Widget_Animation::drag_start(VR_UI::Cursor& c)
{
	if (VR_UI::ctrl_key_get()) {
		binding_paused = true;
	}
	else if (VR_UI::shift_key_get()) {
		binding_paused = false;
	}
}

void Widget_Animation::drag_contd(VR_UI::Cursor& c)
{
	if (VR_UI::ctrl_key_get()) {
		binding_paused = true;
	}
	else if (VR_UI::shift_key_get()) {
		binding_paused = false;
	}
}

void Widget_Animation::drag_stop(VR_UI::Cursor& c)
{
	if (VR_UI::ctrl_key_get()) {
		binding_paused = true;
	}
	else if (VR_UI::shift_key_get()) {
		binding_paused = false;
	}
}

void Widget_Animation::render_axes(const float length[3], int draw_style)
{
	/* Adapted from arrow_draw_geom() in arrow3d_gizmo.c */
	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	bool unbind_shader = true;

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	/* Axes */
	GPU_line_width(1.0f);
	for (int i = 0; i < 3; ++i) {
		if (constraint_flag[0][i]) {
			immUniformColor4fv(c_manip_select[i]);
			immBegin(GPU_PRIM_LINES, 2);
			switch (i) {
			case 0: { /* x-axis */
				immVertex3f(pos, -length[i], 0.0f, 0.0f);
				immVertex3f(pos, length[i], 0.0f, 0.0f);
				break;
			}
			case 1: { /* y-axis */
				immVertex3f(pos, 0.0f, -length[i], 0.0f);
				immVertex3f(pos, 0.0f, length[i], 0.0f);
				break;
			}
			case 2: { /* z-axis */
				immVertex3f(pos, 0.0f, 0.0f, -length[i]);
				immVertex3f(pos, 0.0f, 0.0f, length[i]);
				break;
			}
			}
			immEnd();
		}
	}

	/* *** draw arrow head *** */
	GPU_matrix_push();

	switch (draw_style) {
	case 0:
	default: { /* Arrow */
		for (int i = 0; i < 3; ++i) {
			if (constraint_flag[0][i]) {
				float len = length[i] * WIDGET_ANIMATION_ARROW_SCALE_FACTOR;
				float width = length[i] * 0.04f;
				switch (i) {
				case 0: { /* x-axis */
					immUniformColor4fv(c_manip_select[i]);
	
					GPU_matrix_translate_3f(length[i], 0.0f, 0.0f);
					GPU_matrix_rotate_axis(90.0f, 'Y');

					imm_draw_circle_fill_3d(pos, 0.0, 0.0, width, 8);
					imm_draw_cylinder_fill_3d(pos, width, 0.0, len, 8, 1);

					GPU_matrix_rotate_axis(-90.0f, 'Y');
					GPU_matrix_translate_3f(-length[i], 0.0f, 0.0f);
					break;
				}
				case 1: { /* y-axis */
					immUniformColor4fv(c_manip_select[i]);
			
					GPU_matrix_translate_3f(0.0f, length[i], 0.0f);
					GPU_matrix_rotate_axis(-90.0f, 'X');

					imm_draw_circle_fill_3d(pos, 0.0, 0.0, width, 8);
					imm_draw_cylinder_fill_3d(pos, width, 0.0, len, 8, 1);

					GPU_matrix_rotate_axis(90.0f, 'X');
					GPU_matrix_translate_3f(0.0f, -length[i], 0.0f);
					break;
				}
				case 2: { /* z-axis */
					immUniformColor4fv(c_manip_select[i]);

					GPU_matrix_translate_3f(0.0f, 0.0f, length[i]);

					imm_draw_circle_fill_3d(pos, 0.0, 0.0, width, 8);
					imm_draw_cylinder_fill_3d(pos, width, 0.0, len, 8, 1);

					GPU_matrix_translate_3f(0.0f, 0.0f, -length[i]);
					break;
				}
				}
			}
		}
		break;
	}
	}

	GPU_matrix_pop();

	if (unbind_shader) {
		immUnbindProgram();
	}
}

void Widget_Animation::render_gimbal(
	const float radius[3],
	const bool filled,
	const float axis_modal_mat[4][4], const float clip_plane[4],
	const float arc_partial_angle, const float arc_inner_factor)
{
	/* Adapted from dial_geom_draw() in dial3d_gizmo.c */
	GPU_line_width(1.0f);
	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

	if (clip_plane) {
		immBindBuiltinProgram(GPU_SHADER_3D_CLIPPED_UNIFORM_COLOR);
		immUniform4fv("ClipPlane", clip_plane);
		immUniformMatrix4fv("ModelMatrix", axis_modal_mat);
		glEnable(GL_CLIP_DISTANCE0);
	}
	else {
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	}

	float rad = 0.0f;
	for (int i = 0; i < 3; ++i) {
		if (constraint_flag[1][i]) {
			immUniformColor4fv(c_manip_select[i]);

			switch (i) { /* x-axis */
			case 0: {
				GPU_matrix_rotate_axis(-90.0f, 'Y');
				break;
			}
			case 1: { /* y-axis */
				GPU_matrix_rotate_axis(90.0f, 'X');
				break;
			}
			case 2: { /* z-axis */
				break;
			}
			}

			rad = radius[i] / 4.0f;

			if (filled) {
				imm_draw_circle_fill_2d(pos, 0, 0, rad, WIDGET_ANIMATION_DIAL_RESOLUTION);
			}
			else {
				if (arc_partial_angle == 0.0f) {
					imm_draw_circle_wire_2d(pos, 0, 0, rad, WIDGET_ANIMATION_DIAL_RESOLUTION);
					if (arc_inner_factor != 0.0f) {
						imm_draw_circle_wire_2d(pos, 0, 0, arc_inner_factor, WIDGET_ANIMATION_DIAL_RESOLUTION);
					}
				}
				else {
					float arc_partial_deg = RAD2DEGF((M_PI * 2) - arc_partial_angle);
					imm_draw_circle_partial_wire_2d(
						pos, 0, 0, rad, WIDGET_ANIMATION_DIAL_RESOLUTION,
						0.0f, arc_partial_deg);
				}
			}

			switch (i) { /* x-axis */
			case 0: {
				GPU_matrix_rotate_axis(90.0f, 'Y');
				break;
			}
			case 1: { /* y-axis */
				GPU_matrix_rotate_axis(-90.0f, 'X');
				break;
			}
			case 2: { /* z-axis */
				break;
			}
			}
		}
	}

	immUnbindProgram();

	if (clip_plane) {
		glDisable(GL_CLIP_DISTANCE0);
	}
}

/* From dial3d_gizmo.c. */
static void dial_ghostarc_draw(
	const float angle_ofs, const float angle_delta,
	const float arc_inner_factor, const float color[4], const float radius)
{
	const float width_inner = radius;
	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	if (arc_inner_factor != 0.0) {
		float color_dark[4] = { 0 };
		color_dark[3] = color[3] / 2;
		immUniformColor4fv(color_dark);
		imm_draw_disk_partial_fill_2d(
			pos, 0, 0, arc_inner_factor, width_inner, WIDGET_ANIMATION_DIAL_RESOLUTION, RAD2DEGF(angle_ofs), RAD2DEGF(M_PI * 2));
	}

	immUniformColor4fv(color);
	imm_draw_disk_partial_fill_2d(
		pos, 0, 0, arc_inner_factor, width_inner, WIDGET_ANIMATION_DIAL_RESOLUTION, RAD2DEGF(angle_ofs), RAD2DEGF(angle_delta));
	immUnbindProgram();
}

static void dial_ghostarc_draw_helpline(
	const float angle, const float co_outer[3], const float color[4])
{
	GPU_matrix_push();
	GPU_matrix_rotate_3f(RAD2DEGF(angle), 0.0f, 0.0f, -1.0f);

	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	immUniformColor4fv(color);

	immBegin(GPU_PRIM_LINE_STRIP, 2);
	immVertex3f(pos, 0.0f, 0, 0.0f);
	immVertex3fv(pos, co_outer);
	immEnd();

	immUnbindProgram();

	GPU_matrix_pop();
}

void Widget_Animation::render_dial(const int index,
	const float angle_ofs, const float angle_delta,
	const float arc_inner_factor, const float radius)
{
	/* From dial_ghostarc_draw_with_helplines() in dial3d_gizmo.c */

	/* Coordinate at which the arc drawing will be started. */
	const float co_outer[4] = { 0.0f, radius, 0.0f };
	const float color[4] = { 0.8f, 0.8f, 0.8f, 0.4f };
	dial_ghostarc_draw(angle_ofs, angle_delta, arc_inner_factor, color, radius);
	GPU_line_width(1.0f);

	dial_ghostarc_draw_helpline(angle_ofs, co_outer, c_manip_select[index]);
	dial_ghostarc_draw_helpline(angle_ofs + angle_delta, co_outer, c_manip_select[index]);
}

void Widget_Animation::render(VR_Side side)
{
	if (!manipulator) {
		Widget_Animation::obj.do_render[side] = false;
	}

	if (is_zero_m3((float(*)[3])&Widget_Animation::constraint_flag)) {
		return;
	}

	static float manip_length[3];
	for (int i = 0; i < 3; ++i) {
		manip_length[i] = manip_scale_factor * 2.0f;
	}
	static float clip_plane[4] = { 0.0f };
	
	GPU_blend(true);
	GPU_matrix_push();
	GPU_matrix_mul(manip_t.m);
	GPU_polygon_smooth(false);

	if (!is_zero_v3((float*)&Widget_Animation::constraint_flag[1])) {
		/* Dial and Gimbal */
		if (constraint_flag[1][0]) {
			GPU_matrix_rotate_axis(-90.0f, 'Y');
			render_dial(0, PI / 4.0f, manip_angle[transform_space].x, 0.0f, manip_length[0] / 4.0f);
			GPU_matrix_rotate_axis(90.0f, 'Y');
		}
		if (constraint_flag[1][1]) {
			GPU_matrix_rotate_axis(90.0f, 'X');
			render_dial(1, PI / 4.0f, manip_angle[transform_space].y, 0.0f, manip_length[1] / 4.0f);
			GPU_matrix_rotate_axis(-90.0f, 'X');
		}
		if (constraint_flag[1][2]) {
			GPU_matrix_rotate_axis(-90.0f, 'Z');
			render_dial(2, -PI / 4.0f, -manip_angle[transform_space].z, 0.0f, manip_length[2] / 4.0f);
			GPU_matrix_rotate_axis(90.0f, 'Z');
		}

		render_gimbal(manip_length, false, manip_t.m, clip_plane, 0.0f, 0.0f);
	}
	if (!is_zero_v3((float*)&Widget_Animation::constraint_flag[0])) {
		/* Arrow */
		*((Coord3Df*)manip_length) /= 2.0f;
		render_axes(manip_length, 0);
	}

	GPU_blend(false);
	GPU_matrix_pop();
}
