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

/** \file blender/vr/intern/vr_widget_measure.cpp
*   \ingroup vr
* 
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_measure.h"
#include "vr_widget_annotate.h"

#include "vr_math.h"
#include "vr_draw.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"

#include "DNA_gpencil_types.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

/***************************************************************************************************
* \class                               Widget_Measure
***************************************************************************************************
* Interaction widget for the Measure tool.
*
**************************************************************************************************/

Widget_Measure Widget_Measure::obj;

Coord3Df Widget_Measure::measure_points[3];
bGPDstroke *Widget_Measure::current_stroke(NULL);
bGPDspoint Widget_Measure::current_stroke_points[3];

Widget_Measure::Measure_State Widget_Measure::measure_state(Widget_Measure::Measure_State::INIT);
VR_UI::CtrlState Widget_Measure::measure_ctrl_state(VR_UI::CTRLSTATE_OFF);
int Widget_Measure::measure_ctrl_count(0);

float Widget_Measure::line_thickness(10.0f);

float Widget_Measure::angle(0.0f);
VR_Side Widget_Measure::cursor_side;

#define WIDGET_MEASURE_ARC_STEPS 100

void Widget_Measure::draw_line(VR_UI::Cursor& c, Coord3Df& localP0, Coord3Df& localP1) {
	switch (measure_state) {
	case Widget_Measure::Measure_State::INIT: {
		measure_state = Widget_Measure::Measure_State::DRAW;
		break;
	}
	case Widget_Measure::Measure_State::DRAW: {
		measure_state = Widget_Measure::Measure_State::MEASURE;
		break;
	}
	case Widget_Measure::Measure_State::MEASURE: {
		measure_state = Widget_Measure::Measure_State::DONE;
		break;
	}
	default: {
		break;
	}
	}

	if (measure_state == Widget_Measure::Measure_State::DRAW) {
		bContext *C = vr_get_obj()->ctx;
		Main* curr_main = CTX_data_main(C);
		if (Widget_Annotate::gpl.size() < 1 || Widget_Annotate::main != curr_main) {
			int error = Widget_Annotate::init(Widget_Annotate::main != curr_main ? true : false);
			Widget_Annotate::main = curr_main;
			if (error) {
				return;
			}
		}

		/* Create and parse our previous points into bGPDspoint structures */
		memcpy(&current_stroke_points[0], &localP0, sizeof(float) * 3);
		memcpy(&current_stroke_points[1], &localP1, sizeof(float) * 3);
		memcpy(&current_stroke_points[2], &localP1, sizeof(float) * 3);

		/* Set the pressure and strength for proper display */
		for (int i = 0; i < 3; ++i) {
			current_stroke_points[i].strength = 1.0f;
			current_stroke_points[i].pressure = 1.0f;
		}
	}
	if (measure_state == Widget_Measure::Measure_State::MEASURE) {
		/* Current state is MEASURE. This is our last draw, so add the arc for degree display. */
		memcpy(&current_stroke_points[2], &localP1, sizeof(float) * 3);
	}

	current_stroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[WIDGET_ANNOTATE_MEASURE_LAYER], 0, 3, line_thickness * 1.6f);
	memcpy(current_stroke->points, current_stroke_points, sizeof(bGPDspoint) * 3);

	BKE_gpencil_layer_setactive(Widget_Annotate::gpd, Widget_Annotate::gpl[WIDGET_ANNOTATE_MEASURE_LAYER]);
}

void Widget_Measure::drag_start(VR_UI::Cursor& c)
{
	cursor_side = c.side;
	c.reference = c.position.get();

	memcpy(&measure_points[0], c.position.get(VR_SPACE_BLENDER).m[3], sizeof(float) * 3);
}

void Widget_Measure::drag_contd(VR_UI::Cursor& c)
{
	if (measure_ctrl_state == VR_UI::CTRLSTATE_OFF) {
		/* Line measurement */
		memcpy(&measure_points[1], c.position.get(VR_SPACE_BLENDER).m[3], sizeof(float) * 3);
	}
	else {
		/* Angle measurement */
		measure_points[2] = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];
		Coord3Df dir_a = (measure_points[0] - measure_points[1]).normalize();
		Coord3Df dir_b = (measure_points[2] - measure_points[1]).normalize();
		angle = angle_normalized_v3v3((float*)&dir_a, (float*)&dir_b) * (180.0f / PI);
	}
	if (VR_UI::ctrl_key_get() && measure_ctrl_state == VR_UI::CTRLSTATE_OFF) {
		draw_line(c, measure_points[0], measure_points[1]);
		measure_points[2] = measure_points[1];
		measure_ctrl_state = VR_UI::CTRLSTATE_ON;
	}

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Measure::obj.do_render[i] = true;
	}
}

void Widget_Measure::drag_stop(VR_UI::Cursor& c)
{
	if (measure_ctrl_state == VR_UI::CTRLSTATE_OFF) {
		draw_line(c, measure_points[0], measure_points[1]);
	}
	else {
		draw_line(c, measure_points[1], measure_points[2]);

		/* Arc */
		float dir_tmp[3];
		float dir_a[3];
		float dir_b[3];
		float quat[4];
		float axis[3];
		float angle;

		sub_v3_v3v3(dir_a, &measure_points[0].x, &measure_points[1].x);
		sub_v3_v3v3(dir_b, &measure_points[2].x, &measure_points[1].x);
		normalize_v3(dir_a);
		normalize_v3(dir_b);

		cross_v3_v3v3(axis, dir_a, dir_b);
		angle = angle_normalized_v3v3(dir_a, dir_b);
		axis_angle_to_quat(quat, axis, angle / (float)WIDGET_MEASURE_ARC_STEPS);
		copy_v3_v3(dir_tmp, dir_a);
		float rad = ((measure_points[0] - measure_points[1]) / 2.0f).length();

		bGPDstroke *stroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[WIDGET_ANNOTATE_MEASURE_LAYER], 0, WIDGET_MEASURE_ARC_STEPS + 1, line_thickness * 1.6f);
		if (stroke && stroke->points)
		{
			bGPDspoint *points = stroke->points;
			for (int i = 0; i <= WIDGET_MEASURE_ARC_STEPS; ++i) {
				bGPDspoint& point = points[i];
				madd_v3_v3v3fl(&point.x, &measure_points[1].x, dir_tmp, rad);
				point.strength = 1.0f;
				point.pressure = 1.0f;
				mul_qt_v3(quat, dir_tmp);
			}

			BKE_gpencil_layer_setactive(Widget_Annotate::gpd, Widget_Annotate::gpl[WIDGET_ANNOTATE_MEASURE_LAYER]);
		}
	}

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Measure::obj.do_render[i] = false;
	}

	measure_state = Widget_Measure::Measure_State::INIT;
	measure_ctrl_state = VR_UI::CTRLSTATE_OFF;
	measure_ctrl_count = 0;

	for (int i = 0; i < 3; ++i) {
		memset(&measure_points[i], 0, sizeof(float) * 3);
	}
}

void Widget_Measure::render(VR_Side side)
{
	/* Render measurement text. */
	const Mat44f& prior_model_matrix = VR_Draw::get_model_matrix();
	static Mat44f m;
	m = VR_UI::hmd_position_get(VR_SPACE_REAL);
	const Mat44f& c = VR_UI::cursor_position_get(VR_SPACE_REAL, cursor_side);
	memcpy(m.m[3], c.m[3], sizeof(float) * 3);
	VR_Draw::update_modelview_matrix(&m, 0);

	VR_Draw::set_depth_test(false, false);
	VR_Draw::set_color(0.8f, 0.8f, 0.8f, 1.0f);
	static std::string measure_str;
	if (measure_ctrl_state == VR_UI::CTRLSTATE_OFF) {
		/* Line measurement */
		sprintf((char*)measure_str.data(), "%.3f", (measure_points[1] - measure_points[0]).length());
		VR_Draw::render_string(measure_str.c_str(), 0.02f, 0.02f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.08f, 0.001f);
	}
	else {
		/* Angle measurement */
		sprintf((char*)measure_str.data(), "%5.1fdeg", angle);
		VR_Draw::render_string(measure_str.c_str(), 0.02f, 0.02f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.08f, 0.001f);
	}
	VR_Draw::set_depth_test(true, true);
	VR_Draw::update_modelview_matrix(&prior_model_matrix, 0);

	/* Render measurement lines. */
	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	GPU_line_width(10.0f);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	if (measure_ctrl_state == VR_UI::CTRLSTATE_OFF && measure_state == Widget_Measure::Measure_State::INIT) {
		immBeginAtMost(GPU_PRIM_LINES, 2);
		immUniformColor4fv(Widget_Annotate::colors[WIDGET_ANNOTATE_MEASURE_LAYER]);

		immVertex3fv(pos, (float*)&measure_points[0]);
		immVertex3fv(pos, (float*)&measure_points[1]);

		if (measure_points[0] == measure_points[1]) {
			/* cyclic */
			immVertex3fv(pos, (float*)&measure_points[0]);
		}
		immEnd();
		immUnbindProgram();
	}
	else {
		immBeginAtMost(GPU_PRIM_LINES, 2);
		immUniformColor4fv(Widget_Annotate::colors[WIDGET_ANNOTATE_MEASURE_LAYER]);
		immVertex3fv(pos, (float*)&measure_points[1]);
		immVertex3fv(pos, (float*)&measure_points[2]);

		if (measure_points[1] == measure_points[2]) {
			/* cyclic */
			immVertex3fv(pos, (float*)&measure_points[2]);
		}
		immEnd();
		immUnbindProgram();

		/* Arc */
		float dir_tmp[3];
		float co_tmp[3];
		float dir_a[3];
		float dir_b[3];
		float quat[4];
		float axis[3];
		float angle;

		sub_v3_v3v3(dir_a, &measure_points[0].x, &measure_points[1].x);
		sub_v3_v3v3(dir_b, &measure_points[2].x, &measure_points[1].x);
		normalize_v3(dir_a);
		normalize_v3(dir_b);

		cross_v3_v3v3(axis, dir_a, dir_b);
		angle = angle_normalized_v3v3(dir_a, dir_b);
		axis_angle_to_quat(quat, axis, angle / (float)WIDGET_MEASURE_ARC_STEPS);
		copy_v3_v3(dir_tmp, dir_a);
		float rad = ((measure_points[0] - measure_points[1]) / 2.0f).length();

		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformColor4fv(Widget_Annotate::colors[WIDGET_ANNOTATE_MEASURE_LAYER]);
		immBegin(GPU_PRIM_LINE_STRIP, WIDGET_MEASURE_ARC_STEPS + 1);

		for (int i = 0; i <= WIDGET_MEASURE_ARC_STEPS; ++i) {
			madd_v3_v3v3fl(co_tmp, &measure_points[1].x, dir_tmp, rad);
			mul_qt_v3(quat, dir_tmp);
			immVertex3fv(pos, co_tmp);
		}

		immEnd();
		immUnbindProgram();
	}

	Widget_Measure::obj.do_render[side] = false;
}
