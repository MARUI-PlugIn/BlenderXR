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

#include "vr_draw.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"

#include "DNA_gpencil_types.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

/***********************************************************************************************//**
* \class                               Widget_Measure
***************************************************************************************************
* Interaction widget for the gpencil measure tool.
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
float Widget_Measure::color[4] = { 1.0f, 0.3f, 0.3f, 1.0f };

float Widget_Measure::angle(0.0f);

VR_Side Widget_Measure::cursor_side;

void Widget_Measure::drag_start(VR_UI::Cursor& c)
{
	cursor_side = c.side;
	c.reference = c.position.get();

	memcpy(&measure_points[0], c.position.get(VR_SPACE_BLENDER).m[3], sizeof(float) * 3);
}

void Widget_Measure::drag_contd(VR_UI::Cursor& c)
{
	//if (measure_state == VR_UI::CTRLSTATE_OFF) {
		memcpy(&measure_points[1], c.position.get(VR_SPACE_BLENDER).m[3], sizeof(float) * 3);
	//}
	/*else {
		measure_points[2] = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];
		Coord3Df dir_a = (measure_points[0] - measure_points[1]).normalize();
		Coord3Df dir_b = (measure_points[2] - measure_points[1]).normalize();
		angle = angle_normalized_v3v3((float*)&dir_a, (float*)&dir_b) * (180.0f / PI);
		angle = (float)((int)(angle) % 180);
	}
	if (VR_UI::ctrl_key_get()) {
		if (++measure_ctrl_count == 1) {
			draw_line(c, measure_points[0], measure_points[1]);
		}
		measure_ctrl_state = VR_UI::CTRLSTATE_ON;
	}*/

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Measure::obj.do_render[i] = true;
	}
}

void Widget_Measure::drag_stop(VR_UI::Cursor& c)
{
	//if (measure_ctrl_state == VR_UI::CTRLSTATE_OFF) {
		draw_line(c, measure_points[0], measure_points[1]);
	/*}
	else {
		draw_line(c, measure_points[1], measure_points[2]);
	}*/

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Measure::obj.do_render[i] = false;
	}

	//Coord3Df p = *(Coord3Df*)&current_stroke_points[0];
	//render_GPFont(1, 5, p);

	measure_state = Widget_Measure::Measure_State::INIT;
	measure_ctrl_state = VR_UI::CTRLSTATE_OFF;
	measure_ctrl_count = 0;

	for (int i = 0; i < 3; ++i) {
		memset(&measure_points[i], 0, sizeof(float) * 3);
	}
}

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

	/* Get active drawing layer */
	uint active_layer = Widget_Annotate::num_layers - 1;

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

	current_stroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 3, line_thickness * 1.6f);
	memcpy(current_stroke->points, current_stroke_points, sizeof(bGPDspoint) * 3);

	memcpy(Widget_Annotate::gpl[active_layer]->color, color, sizeof(float) * 4);
	BKE_gpencil_layer_setactive(Widget_Annotate::gpd, Widget_Annotate::gpl[active_layer]);
}

void Widget_Measure::render_GPFont(const uint num, const uint numPoint, const Coord3Df& o)
{
	uint active_layer = Widget_Annotate::num_layers - 1;

	bContext *C = vr_get_obj()->ctx;
	Main *curr_main = CTX_data_main(C);
	if (Widget_Annotate::gpl.size() < 1 || Widget_Annotate::main != curr_main) {
		int error = Widget_Annotate::init(Widget_Annotate::main != curr_main ? true : false);
		Widget_Annotate::main = curr_main;
		if (error) {
			return;
		}
	}

	bGPDstroke *GPFStroke = NULL;

	/* Based on the number and (o)rigin passed, we fill our stroke with points related to the requested number. */
	switch (num) {
	case 0: {
		static bGPDspoint GPpoints[9];
		GPpoints[0].x = -0.01f;	GPpoints[0].y = +0.01f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = +0.00f;	GPpoints[1].y = +0.02f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = +0.01f;	GPpoints[2].y = +0.02f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = +0.02f;	GPpoints[3].y = +0.01f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = +0.02f;	GPpoints[4].y = -0.01f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;
		GPpoints[5].x = +0.01f;	GPpoints[5].y = -0.02f;	GPpoints[5].z = 0.0f; GPpoints[5].pressure = 1.0f; GPpoints[5].strength = 1.0f;
		GPpoints[6].x = +0.00f;	GPpoints[6].y = -0.02f;	GPpoints[6].z = 0.0f; GPpoints[6].pressure = 1.0f; GPpoints[6].strength = 1.0f;
		GPpoints[7].x = -0.01f;	GPpoints[7].y = -0.01f;	GPpoints[7].z = 0.0f; GPpoints[7].pressure = 1.0f; GPpoints[7].strength = 1.0f;
		GPpoints[8].x = -0.01f;	GPpoints[8].y = +0.01f;	GPpoints[8].z = 0.0f; GPpoints[8].pressure = 1.0f; GPpoints[8].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 9, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 9);
		break;
	}
	case 1: {
		static bGPDspoint GPpoints[5];
		GPpoints[0].x = -0.01f;	GPpoints[0].y = -0.01f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = 0.00f;	GPpoints[1].y = +0.02f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = 0.00f;	GPpoints[2].y = -0.02f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = -0.01f;	GPpoints[3].y = -0.02f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = +0.01f;	GPpoints[4].y = -0.02f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 5, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 5);
		break;
	}
	case 2: {
		static bGPDspoint GPpoints[6];
		GPpoints[0].x = -0.02f;	GPpoints[0].y = +0.01f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = -0.01f;	GPpoints[1].y = +0.02f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = 0.00f;	GPpoints[2].y = +0.02f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = -0.01f;	GPpoints[3].y = +0.01f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = +0.02f;	GPpoints[4].y = -0.02f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;
		GPpoints[5].x = -0.01f;	GPpoints[5].y = -0.02f;	GPpoints[5].z = 0.0f; GPpoints[5].pressure = 1.0f; GPpoints[5].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 6, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 6);
		break;
	}
	case 3: {
		static bGPDspoint GPpoints[9];
		GPpoints[0].x = -0.01f;	GPpoints[0].y = +0.02f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = +0.01f;	GPpoints[1].y = +0.02f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = +0.02f;	GPpoints[2].y = +0.01f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = +0.01f;	GPpoints[3].y = 0.00f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = 0.00f;	GPpoints[4].y = 0.00f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;
		GPpoints[5].x = +0.01f;	GPpoints[5].y = 0.00f;	GPpoints[5].z = 0.0f; GPpoints[5].pressure = 1.0f; GPpoints[5].strength = 1.0f;
		GPpoints[6].x = +0.02f;	GPpoints[6].y = -0.01f;	GPpoints[6].z = 0.0f; GPpoints[6].pressure = 1.0f; GPpoints[6].strength = 1.0f;
		GPpoints[7].x = +0.01f;	GPpoints[7].y = -0.02f;	GPpoints[7].z = 0.0f; GPpoints[7].pressure = 1.0f; GPpoints[7].strength = 1.0f;
		GPpoints[8].x = -0.01f;	GPpoints[8].y = -0.02f;	GPpoints[8].z = 0.0f; GPpoints[8].pressure = 1.0f; GPpoints[8].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 9, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 9);
		break;
	}
	case 4: {
		static bGPDspoint GPpoints[8];
		GPpoints[0].x = -0.02f;	GPpoints[0].y = 0.00f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = 0.00f;	GPpoints[1].y = +0.02f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = +0.01f;	GPpoints[2].y = +0.02f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = +0.01f;	GPpoints[3].y = -0.01f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = +0.01f;	GPpoints[4].y = -0.02f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;
		GPpoints[5].x = +0.01f;	GPpoints[5].y = -0.01f;	GPpoints[5].z = 0.0f; GPpoints[5].pressure = 1.0f; GPpoints[5].strength = 1.0f;
		GPpoints[6].x = -0.02f;	GPpoints[6].y = -0.01f;	GPpoints[6].z = 0.0f; GPpoints[6].pressure = 1.0f; GPpoints[6].strength = 1.0f;
		GPpoints[7].x = -0.02f;	GPpoints[7].y = -0.001;	GPpoints[7].z = 0.0f; GPpoints[7].pressure = 1.0f; GPpoints[7].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 8, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 8);
		break;
	}
	case 5: {
		static bGPDspoint GPpoints[7];
		GPpoints[0].x = +0.02f;	GPpoints[0].y = +0.02f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = -0.01f;	GPpoints[1].y = +0.02f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = -0.01f;	GPpoints[2].y = 0.00f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = +0.02f;	GPpoints[3].y = 0.00f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = +0.02f;	GPpoints[4].y = -0.01f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;
		GPpoints[5].x = +0.01f;	GPpoints[5].y = -0.02f;	GPpoints[5].z = 0.0f; GPpoints[5].pressure = 1.0f; GPpoints[5].strength = 1.0f;
		GPpoints[6].x = -0.01f;	GPpoints[6].y = -0.02f;	GPpoints[6].z = 0.0f; GPpoints[6].pressure = 1.0f; GPpoints[6].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 7, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 7);
		break;
	}
	case 6: {
		static bGPDspoint GPpoints[9];
		GPpoints[0].x = +0.02f;	GPpoints[0].y = +0.02f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = 0.00f;	GPpoints[1].y = +0.02f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = -0.01f;	GPpoints[2].y = +0.01f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = -0.01f;	GPpoints[3].y = -0.01f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = 0.00f;	GPpoints[4].y = -0.02f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;
		GPpoints[5].x = +0.01f;	GPpoints[5].y = -0.02f;	GPpoints[5].z = 0.0f; GPpoints[5].pressure = 1.0f; GPpoints[5].strength = 1.0f;
		GPpoints[6].x = +0.02f;	GPpoints[6].y = -0.01f;	GPpoints[6].z = 0.0f; GPpoints[6].pressure = 1.0f; GPpoints[6].strength = 1.0f;
		GPpoints[7].x = +0.01f;	GPpoints[7].y = 0.00f;	GPpoints[7].z = 0.0f; GPpoints[7].pressure = 1.0f; GPpoints[7].strength = 1.0f;
		GPpoints[8].x = -0.01f;	GPpoints[8].y = 0.00f;	GPpoints[8].z = 0.0f; GPpoints[8].pressure = 1.0f; GPpoints[8].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 9, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 9);
		break;
	}
	case 7: {
		static bGPDspoint GPpoints[5];
		GPpoints[0].x = -0.01f;	GPpoints[0].y = +0.02f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = +0.02f;	GPpoints[1].y = +0.02f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = +0.02f;	GPpoints[2].y = +0.01f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = 0.00f;	GPpoints[3].y = -0.01f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = 0.00f;	GPpoints[4].y = -0.02f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 5, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 5);
		break;
	}
	case 8: {
		static bGPDspoint GPpoints[11];
		GPpoints[0].x = 0.00f;	GPpoints[0].y = 0.00f;	GPpoints[0].z = 0.0f;	GPpoints[0].pressure = 1.0f;	GPpoints[0].strength = 1.0f;
		GPpoints[1].x = -0.01f;	GPpoints[1].y = +0.01f;	GPpoints[1].z = 0.0f;	GPpoints[1].pressure = 1.0f;	GPpoints[1].strength = 1.0f;
		GPpoints[2].x = 0.00f;	GPpoints[2].y = +0.02f;	GPpoints[2].z = 0.0f;	GPpoints[2].pressure = 1.0f;	GPpoints[2].strength = 1.0f;
		GPpoints[3].x = +0.01f;	GPpoints[3].y = +0.02f;	GPpoints[3].z = 0.0f;	GPpoints[3].pressure = 1.0f;	GPpoints[3].strength = 1.0f;
		GPpoints[4].x = +0.02f;	GPpoints[4].y = +0.01f;	GPpoints[4].z = 0.0f;	GPpoints[4].pressure = 1.0f;	GPpoints[4].strength = 1.0f;
		GPpoints[5].x = +0.01f;	GPpoints[5].y = 0.00f;	GPpoints[5].z = 0.0f;	GPpoints[5].pressure = 1.0f;	GPpoints[5].strength = 1.0f;
		GPpoints[6].x = +0.02f;	GPpoints[6].y = -0.01f;	GPpoints[6].z = 0.0f;	GPpoints[6].pressure = 1.0f;	GPpoints[6].strength = 1.0f;
		GPpoints[7].x = +0.01f;	GPpoints[7].y = -0.02f;	GPpoints[7].z = 0.0f;	GPpoints[7].pressure = 1.0f;	GPpoints[7].strength = 1.0f;
		GPpoints[8].x = 0.00f;	GPpoints[8].y = -0.02f;	GPpoints[8].z = 0.0f;	GPpoints[8].pressure = 1.0f;	GPpoints[8].strength = 1.0f;
		GPpoints[9].x = -0.01f;	GPpoints[9].y = -0.01f;	GPpoints[9].z = 0.0f;	GPpoints[9].pressure = 1.0f;	GPpoints[9].strength = 1.0f;
		GPpoints[10].x = 0.00f;	GPpoints[10].y = 0.00f;	GPpoints[10].z = 0.0f;	GPpoints[10].pressure = 1.0f;	GPpoints[10].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 11, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 11);
		break;
	}
	case 9: {
		static bGPDspoint GPpoints[9];
		GPpoints[0].x = +0.01f;	GPpoints[0].y = 0.00f;	GPpoints[0].z = 0.0f; GPpoints[0].pressure = 1.0f; GPpoints[0].strength = 1.0f;
		GPpoints[1].x = -0.01f;	GPpoints[1].y = 0.00f;	GPpoints[1].z = 0.0f; GPpoints[1].pressure = 1.0f; GPpoints[1].strength = 1.0f;
		GPpoints[2].x = -0.02f;	GPpoints[2].y = +0.01f;	GPpoints[2].z = 0.0f; GPpoints[2].pressure = 1.0f; GPpoints[2].strength = 1.0f;
		GPpoints[3].x = -0.01f;	GPpoints[3].y = +0.02f;	GPpoints[3].z = 0.0f; GPpoints[3].pressure = 1.0f; GPpoints[3].strength = 1.0f;
		GPpoints[4].x = 0.00f;	GPpoints[4].y = +0.02f;	GPpoints[4].z = 0.0f; GPpoints[4].pressure = 1.0f; GPpoints[4].strength = 1.0f;
		GPpoints[5].x = +0.01f;	GPpoints[5].y = +0.01f;	GPpoints[5].z = 0.0f; GPpoints[5].pressure = 1.0f; GPpoints[5].strength = 1.0f;
		GPpoints[6].x = +0.01f;	GPpoints[6].y = -0.01f;	GPpoints[6].z = 0.0f; GPpoints[6].pressure = 1.0f; GPpoints[6].strength = 1.0f;
		GPpoints[7].x = 0.00f;	GPpoints[7].y = -0.02f;	GPpoints[7].z = 0.0f; GPpoints[7].pressure = 1.0f; GPpoints[7].strength = 1.0f;
		GPpoints[8].x = -0.02f;	GPpoints[8].y = -0.02f;	GPpoints[8].z = 0.0f; GPpoints[8].pressure = 1.0f; GPpoints[8].strength = 1.0f;

		GPFStroke = BKE_gpencil_add_stroke(Widget_Annotate::gpf[active_layer], 0, 9, line_thickness * 1.6f);
		memcpy(GPFStroke->points, GPpoints, sizeof(bGPDspoint) * 9);
		break;
	}
	default: {
		return;
	}
	}

	/*static Mat44f hmd;
	hmd = VR_UI::hmd_position_get(VR_SPACE_REAL);
	memset(hmd.m[3], 0, sizeof(float) * 3);
	static Mat44f hmd_temp;
	hmd_temp = hmd;
	*(Coord3Df*)hmd.m[1] = *(Coord3Df*)hmd_temp.m[2];
	*(Coord3Df*)hmd.m[2] = *(Coord3Df*)hmd_temp.m[1];*/

	//static Mat44f hmd2;
	//hmd2 = VR_UI::hmd_position_get(VR_SPACE_BLENDER);
	//memset(hmd2.m[3], 0, sizeof(float) * 3);

	/* Rotation */
	//static Coord3Df temp;
	//for (int p = 0; p < numPoint; ++p) {
	//	Coord3Df &curSP = *(Coord3Df*)&GPFStroke->points[p];
	//	/* Rotate numbers to point upright */
	//	temp = curSP;
	//	VR_Math::multiply_mat44_coord3D(curSP, rot_matrix_x, temp);
	//	/* Rotate numbers to match hmd local rotation */
	//	temp = curSP;
	//	VR_Math::multiply_mat44_coord3D(curSP, hmd, temp);
	//	/* TODO: Rotate numbers around world axis to face hmd */
	//	//temp = curSP;
	//	//VR_Math::multiply_mat44_coord3D(curSP, hmd2, temp);
	//}

	//static float temp[4] = { 0, 0, 0, 1.0f };
	//for (int p = 0; p < numPoint; p++)
	//{
	//	Coord3Df hmdX = { hmd.m[0][0], hmd.m[0][1], hmd.m[0][2] };
	//	Coord3Df hmdY = { hmd.m[1][0], hmd.m[1][1], hmd.m[1][2] };
	//	Coord3Df hmdZ = { hmd.m[2][0], hmd.m[2][1], hmd.m[2][2] };
	//	Coord3Df spaceorigin = { 0.0f, 0.0f, 0.0f };
	//	memcpy(temp, &GPFStroke->points[p], sizeof(float) * 3);
	//	hmdX *= temp[0];
	//	hmdY *= temp[1];
	//	hmdZ *= temp[2];
	//	spaceorigin += hmdX + hmdY + hmdZ;

	//	memcpy(&GPFStroke->points[p], &spaceorigin, sizeof(float) * 3);
	//}

	/* Translation */
	for (int p = 0; p < numPoint; p++)
	{
		bGPDspoint &curSP = GPFStroke->points[p];
		curSP.x += o.x;
		curSP.y += o.y;
		curSP.z += o.z;
	}

	if (!GPFStroke) {
		return;
	}

	memcpy(Widget_Annotate::gpl[active_layer]->color, color, sizeof(float) * 4);
	BKE_gpencil_layer_setactive(Widget_Annotate::gpd, Widget_Annotate::gpl[active_layer]);
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
	static std::string distance, degrees;
	//if (measure_ctrl_state == VR_UI::CTRLSTATE_ON) {
		/* Angle measurement */
		//sprintf((char*)degrees.data(), "%.f", angle);
		//VR_Draw::render_string(degrees.c_str(), 0.02f, 0.02f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.08f, 0.001f);
	//}
	//else {
		/* Line measurement */
		sprintf((char*)distance.data(), "%.3f", (measure_points[1] - measure_points[0]).length());
		VR_Draw::render_string(distance.c_str(), 0.02f, 0.02f, VR_HALIGN_CENTER, VR_VALIGN_TOP, 0.0f, 0.08f, 0.001f);
	//}
	VR_Draw::set_depth_test(true, true);
	VR_Draw::update_modelview_matrix(&prior_model_matrix, 0);

	/* Instead of working with multiple points that make up a whole line, we work with just p0/p1. */
	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

	GPU_line_width(10.0f);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	//immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);
	if (measure_ctrl_state == VR_UI::CTRLSTATE_OFF && measure_state == Widget_Measure::Measure_State::INIT) {
		immBeginAtMost(GPU_PRIM_LINES, 2);
		immUniformColor3fvAlpha(color, color[3]);
		//immUniform1f("dash_width", 6.0f);

		immVertex3fv(pos, (float*)&measure_points[0]);
		immVertex3fv(pos, (float*)&measure_points[1]);

		if (measure_points[0] == measure_points[1]) {
			/* cyclic */
			immVertex3fv(pos, (float*)&measure_points[0]);
		}
		immEnd();
	}
	//else {
	//	immBeginAtMost(GPU_PRIM_LINES, 2);
	//	immUniformColor3fvAlpha(color, color[3]);
	//	immVertex3fv(pos, (float*)&measure_points[1]);
	//	immVertex3fv(pos, (float*)&measure_points[2]);

	//	if (measure_points[1] == measure_points[2]) {
	//		/* cyclic */
	//		immVertex3fv(pos, (float*)&measure_points[2]);
	//	}
	//	immEnd();
	//	immUnbindProgram();

	//	static Mat44f m_circle = VR_Math::identity_f;
	//	static float temp[3][3];
	//	/* Set arc rotation and position. */
	//	Coord3Df dir_a = (measure_points[0] - measure_points[1]).normalize();
	//	Coord3Df dir_b = (measure_points[2] - measure_points[1]).normalize();
	//	rotation_between_vecs_to_mat3(temp, (float*)&dir_a, (float*)&dir_b);
	//	for (int i = 0; i < 3; ++i) {
	//		memcpy(m_circle.m[i], temp[i], sizeof(float) * 3);
	//	}
	//	*(Coord3Df*)m_circle.m[3] = measure_points[1];

	//	GPU_matrix_push();
	//	GPU_matrix_mul(m_circle.m);
	//	GPU_blend(true);

	//	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	//	immUniformColor3fvAlpha(color, color[3]);
	//	int nsegments = 100;
	//	float angle_start = PI;// 0.0f;
	//	float angle_end = PI - (DEG2RADF(angle));
	//	float rad = ((measure_points[0] - measure_points[1]) / 2.0f).length();
	//	immBegin(GPU_PRIM_LINE_STRIP, nsegments);
	//	static Coord3Df p(0.0f, 0.0f, 0.0f);
	//	for (int i = 0; i < nsegments; ++i) {
	//		const float angle_in = interpf(angle_start, angle_end, ((float)i / (float)(nsegments - 1)));
	//		const float angle_sin = sinf(angle_in);
	//		const float angle_cos = cosf(angle_in);
	//		p.x = rad * angle_cos;
	//		p.y = rad * angle_sin;
	//		immVertex3fv(pos, (float*)&p);
	//	}
	//	immEnd();
	//	immUnbindProgram();

	//	GPU_blend(false);
	//	GPU_matrix_pop();
	//}

	Widget_Measure::obj.do_render[side] = false;
}
