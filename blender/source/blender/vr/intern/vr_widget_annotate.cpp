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

/** \file blender/vr/intern/vr_widget_annotate.cpp
*   \ingroup vr
* 
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_annotate.h"

#include "vr_draw.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"

#include "DNA_gpencil_types.h"

#include "ED_screen.h"

#include "gpencil_intern.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

/***********************************************************************************************//**
 * \class                               Widget_Annotate
 ***************************************************************************************************
 * Interaction widget for the gpencil annotation tool.
 *
 **************************************************************************************************/
Widget_Annotate Widget_Annotate::obj;

bGPdata *Widget_Annotate::gpd(0);
std::vector<bGPDlayer *> Widget_Annotate::gpl;
std::vector<bGPDframe *> Widget_Annotate::gpf;
Main *Widget_Annotate::main(0);

uint Widget_Annotate::num_layers(13); 
uint Widget_Annotate::active_layer(0);

std::vector<bGPDspoint> Widget_Annotate::points;

//float Widget_Annotate::point_thickness(40.0f);
float Widget_Annotate::line_thickness(10.0f);
float Widget_Annotate::color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

bool Widget_Annotate::eraser(false);
VR_Side Widget_Annotate::cursor_side;
float Widget_Annotate::eraser_radius(0.05f);

int Widget_Annotate::init(bool new_scene)
{
	/* Allocate gpencil data/layer/frame and set to active. */
	bContext *C = vr_get_obj()->ctx;
	if (new_scene) {
		gpl.clear();
		gpf.clear();
		/* TODO_XR: This causes memory access errors... */
		/*if (gpd) {
			BKE_gpencil_free(gpd, 1);
		}*/

		gpd = BKE_gpencil_data_addnew(CTX_data_main(C), "VR_Annotate");
		if (!gpd) {
			return -1;
		}
		gpd->flag |= (GP_DATA_ANNOTATIONS); //| GP_DATA_STROKE_EDITMODE);
		//gpd->xray_mode = GP_XRAY_3DSPACE;
		//ED_gpencil_add_defaults(C);
	}

	/* The last layer is the measure tool layer. 
	 * TODO_XR: Refactor this / use a std::map. */
	for (uint i = 0; i < num_layers; ++i) {
		bGPDlayer *gp_layer = BKE_gpencil_layer_addnew(gpd, "VR_Annotate", true);
		if (!gp_layer) {
			if (gpl.size() > 0) {
				BKE_gpencil_free(gpd, 1);
			}
			else {
				BKE_gpencil_free(gpd, 0);
			}
			return -1;
		}
		memcpy(gp_layer->color, color, sizeof(float) * 4);
		gp_layer->thickness = line_thickness / 1.15f;

		bGPDframe *gp_frame = BKE_gpencil_frame_addnew(gp_layer, 0);
		if (!gp_frame) {
			BKE_gpencil_free(gpd, 1);
			return -1;
		}

		gpl.push_back(gp_layer);
		gpf.push_back(gp_frame);
	}

	/* TODO_XR: Find a way to "coexist" with any existing scene gpd */
	Scene *scene = CTX_data_scene(C);
	scene->gpd = gpd;

	return 0;
}

void Widget_Annotate::erase_stroke(bGPDstroke *gps, bGPDframe *gp_frame) {

	/* Adapted from gp_stroke_eraser_do_stroke() in annotate_paint.c */

	bGPDspoint *pt1, *pt2;
	int i;

	if (gps->totpoints == 0) {
		/* just free stroke */
		BKE_gpencil_free_stroke(gps);
	}
	else if (gps->totpoints == 1) {
		/* only process if it hasn't been masked out... */
		//if (!(gps->points->flag & GP_SPOINT_SELECT)) {		
			const Mat44f& c = VR_UI::cursor_position_get(VR_SPACE_BLENDER, cursor_side);
			const Coord3Df& c_pos = *(Coord3Df*)c.m[3];
			const Coord3Df& pt_pos = *(Coord3Df*)gps->points;
			if ((pt_pos - c_pos).length() <= eraser_radius * VR_UI::navigation_scale_get()) {
				gps->points->flag |= GP_SPOINT_TAG;
				gp_stroke_delete_tagged_points(gp_frame, gps, gps->next, GP_SPOINT_TAG, false, 0);
			}
		//}
	}
	else {
		bool inside_sphere = false;

		/* Clear Tags
		 *
		 * Note: It's better this way, as we are sure that
		 * we don't miss anything, though things will be
		 * slightly slower as a result
		 */
		for (i = 0; i < gps->totpoints; ++i) {
			bGPDspoint *pt = &gps->points[i];
			pt->flag &= ~GP_SPOINT_TAG;
		}

		/* First Pass: Loop over the points in the stroke
		 *   1) Thin out parts of the stroke under the brush
		 *   2) Tag "too thin" parts for removal (in second pass)
		 */
		for (i = 0; (i + 1) < gps->totpoints; ++i) {
			/* get points to work with */
			pt1 = gps->points + i;
			pt2 = gps->points + i + 1;

			/* only process if it hasn't been masked out... */
			//if (!(gps->points->flag & GP_SPOINT_SELECT))
			//	continue;

			/* Check if point segment of stroke had anything to do with
			 * eraser region (either within stroke painted, or on its lines)
			 *  - this assumes that linewidth is irrelevant */
			const Mat44f& c = VR_UI::cursor_position_get(VR_SPACE_BLENDER, cursor_side);
			const Coord3Df& c_pos = *(Coord3Df*)c.m[3];
			const Coord3Df& pt1_pos = *(Coord3Df*)pt1;
			const Coord3Df& pt2_pos = *(Coord3Df*)pt2;
			if ((pt1_pos - c_pos).length() <= eraser_radius * VR_UI::navigation_scale_get()) {
				pt1->flag |= GP_SPOINT_TAG;
				inside_sphere = true;
			}
			if ((pt2_pos - c_pos).length() <= eraser_radius * VR_UI::navigation_scale_get()) {
				pt2->flag |= GP_SPOINT_TAG;
				inside_sphere = true;
			}
		}

		/* Second Pass: Remove any points that are tagged */
		if (inside_sphere) {
			gp_stroke_delete_tagged_points(gp_frame, gps, gps->next, GP_SPOINT_TAG, false, 0);
		}
	} 
}

//bool Widget_Annotate::has_click(VR_UI::Cursor& c) const
//{
//	return true;
//}
//
//void Widget_Annotate::click(VR_UI::Cursor& c)
//{
//	/* Eraser */
//	if (VR_UI::ctrl_key_get() == VR_UI::CTRLSTATE_ON) {
//		eraser = true;
//		cursor_side = c.side;
//		if (gpf) {
//			/* Loop over VR strokes and check if they should be erased. */
//			bGPDstroke *gpn;
//			for (bGPDstroke *gps = (bGPDstroke*)gpf->strokes.first; gps; gps = gpn) {
//				gpn = gps->next;
//				Widget_Annotate::erase_stroke(gps);
//			}
//		}
//	}
//	else {
//		eraser = false;
//
//		/* Draw a single point. */
//		points.clear();
//
//		bGPDspoint pt;
//
//		const Mat44f& cursor = c.position.get(VR_SPACE_BLENDER);
//		memcpy(&pt, cursor.m[3], sizeof(float) * 3);
//		pt.pressure = 1.0f; //(vr->controller[c.side]->trigger_pressure);
//		pt.strength = 1.0f;
//		//pt.flag = GP_SPOINT_SELECT;
//
//		points.push_back(pt);
//
//		if (!gpf) {
//			int error = Widget_Annotate::init();
//			if (error) {
//				return;
//			}
//		}
//
//		/* TODO_XR: Find a way to "coexist" with any existing scene gpd. */
//		//BKE_gpencil_layer_setactive(gpd, gpl);
//
//		/* Add new stroke. */
//		bGPDstroke *gps = BKE_gpencil_add_stroke(gpf, 0, 1, point_thickness /*/ 25.0f */);
//		gps->points[0] = points[0];
//	}
//
//	for (int i = 0; i < VR_SIDES; ++i) {
//		Widget_Annotate::obj.do_render[i] = true;
//	}
//}

void Widget_Annotate::drag_start(VR_UI::Cursor& c)
{	
	/* Eraser */
	if (VR_UI::ctrl_key_get() == VR_UI::CTRLSTATE_ON) {
		eraser = true;
		cursor_side = c.side;

		Main *curr_main = CTX_data_main(vr_get_obj()->ctx);
		if (gpf.size() < 1 || main != curr_main) {
			int error = Widget_Annotate::init(main != curr_main ? true : false);
			main = curr_main;
			if (error) {
				return;
			}
		}

		uint tot_layers = gpl.size();
		if (tot_layers > 0) {
			/* Loop over VR strokes and check if they should be erased. */
			bGPDstroke *gpn;
			for (int i = 0; i < tot_layers; ++i) {
				if (gpf[i]) {
					for (bGPDstroke *gps = (bGPDstroke*)gpf[i]->strokes.first; gps; gps = gpn) {
						gpn = gps->next;
						Widget_Annotate::erase_stroke(gps, gpf[i]);
					}
				}
			}
		}
	}
	else {
		eraser = false;

		points.clear();

		bGPDspoint pt;

		const Mat44f& cursor = c.position.get(VR_SPACE_BLENDER);
		memcpy(&pt, cursor.m[3], sizeof(float) * 3);
		VR *vr = vr_get_obj();
		pt.pressure = vr->controller[c.side]->trigger_pressure;
		pt.strength = 1.0f;
		//pt.flag = GP_SPOINT_SELECT;

		points.push_back(pt);
	}

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Annotate::obj.do_render[i] = true;
	}
}

void Widget_Annotate::drag_contd(VR_UI::Cursor& c)
{
	/* Eraser */
	if (eraser) {
		uint tot_layers = gpl.size();
		if (tot_layers > 0) {
			/* Loop over VR strokes and check if they should be erased.
			 * Maybe there's a better way to do this? */
			bGPDstroke *gpn;
			for (int i = 0; i < tot_layers; ++i) {
				if (gpf[i]) {
					for (bGPDstroke *gps = (bGPDstroke*)gpf[i]->strokes.first; gps; gps = gpn) {
						gpn = gps->next;
						Widget_Annotate::erase_stroke(gps, gpf[i]);
					}
				}
			}
		}
	}
	else {
		bGPDspoint pt;

		const Mat44f& cursor = c.position.get(VR_SPACE_BLENDER);
		memcpy(&pt, cursor.m[3], sizeof(float) * 3);
		VR *vr = vr_get_obj();
		pt.pressure = vr->controller[c.side]->trigger_pressure;
		pt.strength = 1.0f;
		//pt.flag = GP_SPOINT_SELECT;

		points.push_back(pt);
	}

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Annotate::obj.do_render[i] = true;
	}
}

void Widget_Annotate::drag_stop(VR_UI::Cursor& c)
{
	if (c.bimanual) {
		VR_UI::Cursor *other = c.other_hand;
		c.bimanual = VR_UI::Cursor::BIMANUAL_OFF;
		other->bimanual = VR_UI::Cursor::BIMANUAL_OFF;
		return; /* calculations are only performed by the second hand */
	}
	
	bContext *C = vr_get_obj()->ctx;

	/* Eraser */
	if (eraser) {
		return;
	}

	/* Finalize curve (save space data) */

	Main *curr_main = CTX_data_main(C);
	if (gpf.size() < 1 || main != curr_main) {
		int error = Widget_Annotate::init(main != curr_main ? true : false);
		main = curr_main;
		if (error) {
			return;
		}
	}

	/* TODO_XR: Find a way to "coexist" with any existing scene gpd. */
	//BKE_gpencil_layer_setactive(gpd, gpl);

	/* Add new stroke. */
	int tot_points = points.size();
	bGPDstroke *gps = BKE_gpencil_add_stroke(gpf[active_layer], 0, tot_points, line_thickness);

	/* Could probably avoid the memcpy by allocating the stroke in drag_start()
	 * but it's nice to store the points in a vector. */
	memcpy(gps->points, &points[0], sizeof(bGPDspoint) * tot_points);

	memcpy(gpl[active_layer]->color, color, sizeof(float) * 4);
	BKE_gpencil_layer_setactive(gpd, gpl[active_layer]);

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Annotate::obj.do_render[i] = false;
	}
}

void Widget_Annotate::render(VR_Side side)
{
	int tot_points = points.size();

	/* Eraser */
	if (eraser) {
		const Mat44f& prior_model_matrix = VR_Draw::get_model_matrix();

		VR_Draw::update_modelview_matrix(&VR_UI::cursor_position_get(VR_SPACE_REAL, cursor_side), 0);
		VR_Draw::set_depth_test(false, false);
		VR_Draw::set_color(1.0f, 0.2f, 0.6f, 0.1f);
		VR_Draw::render_ball(eraser_radius);
		VR_Draw::set_depth_test(true, false);
		VR_Draw::set_color(1.0f, 0.2f, 0.6f, 0.4f);
		VR_Draw::render_ball(eraser_radius);
		VR_Draw::set_depth_test(true, true);

		VR_Draw::update_modelview_matrix(&prior_model_matrix, 0);

		Widget_Annotate::obj.do_render[side] = false;
		return;
	}

	/* Adapted from gp_draw_stroke_3d() in annotate_draw.c. */
	if (tot_points <= 1) { 
		/* If click, point will already be finalized and drawn. 
		 * If drag, need at least two points to draw a line. */
	}
	else { 
		/* if cyclic needs one vertex more */
		bool cyclic = false;
		if ((*(Coord3Df*)&points[0] == *(Coord3Df*)&points[tot_points - 1])) {
			cyclic = true;
		}
		int cyclic_add = 0;
		if (cyclic) {
			++cyclic_add;
		}

		float cyclic_fpt[3];
		int draw_points = 0;

		float cur_pressure = points[0].pressure;

		GPUVertFormat *format = immVertexFormat();
		uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformColor3fvAlpha(color, color[3]);

		/* TODO: implement this with a geometry shader to draw one continuous tapered stroke */

		/* draw stroke curve */
		GPU_line_width(max_ff(cur_pressure * line_thickness, 1.0f));
		immBeginAtMost(GPU_PRIM_LINE_STRIP, tot_points + cyclic_add);
		for (int i = 0; i < tot_points; ++i) {
			/* if there was a significant pressure change, stop the curve, change the thickness of the stroke,
			 * and continue drawing again (since line-width cannot change in middle of GL_LINE_STRIP)
			 * Note: we want more visible levels of pressures when thickness is bigger.
			 */
			if (fabsf(points[i].pressure - cur_pressure) > 0.2f / (float)line_thickness) {
				/* if the pressure changes before get at least 2 vertices, need to repeat last point to avoid assert in immEnd() */
				if (draw_points < 2) {
					immVertex3fv(pos, &points[i - 1].x);
				}
				immEnd();
				draw_points = 0;

				cur_pressure = points[i].pressure;
				GPU_line_width(max_ff(cur_pressure * line_thickness, 1.0f));
				immBeginAtMost(GPU_PRIM_LINE_STRIP, tot_points - i + 1 + cyclic_add);

				/* need to roll-back one point to ensure that there are no gaps in the stroke */
				if (i != 0) {
					immVertex3fv(pos, &points[i - 1].x);
					++draw_points;
				}
			}

			/* now the point we want */
			immVertex3fv(pos, &points[i].x);
			++draw_points;

			if (cyclic && i == 0) {
				/* save first point to use in cyclic */
				copy_v3_v3(cyclic_fpt, &points[i].x);
			}
		}

		if (cyclic) {
			/* draw line to first point to complete the cycle */
			immVertex3fv(pos, cyclic_fpt);
			++draw_points;
		}

		/* if less of two points, need to repeat last point to avoid assert in immEnd() */
		if (draw_points < 2) {
			immVertex3fv(pos, &points[tot_points - 1].x);
		}

		immEnd();
		immUnbindProgram();
	}

	Widget_Annotate::obj.do_render[side] = false;
}
