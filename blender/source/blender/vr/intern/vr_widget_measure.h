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

/** \file blender/vr/intern/vr_widget_measure.h
*   \ingroup vr
*/

#ifndef __VR_WIDGET_MEASURE_H__
#define __VR_WIDGET_MEASURE_H__

#include "vr_widget.h"

struct bGPDstroke;
struct bGPDspoint;

/* Interaction widget for the Measure tool. */
class Widget_Measure : public VR_Widget
{
public:
	/* Measure states. */
	typedef enum Measure_State {
		INIT,
		DRAW,
		MEASURE,
		DONE
	} Measure_State;
protected:
	static Coord3Df measure_points[3];	/* The current measure points. */
	static bGPDstroke *current_stroke;	/* The current gpencil stroke. */
	static bGPDspoint current_stroke_points[3];	/* The current gpencil points. */

	static Measure_State measure_state;	/* The current measure state. */
	static VR_UI::CtrlState measure_ctrl_state;	/* The current ctrl state. */
	static int measure_ctrl_count;	/* The current measure ctrl count. */

	static float line_thickness;	/* Stroke thickness for lines. */
	static float color[4];	/* Stroke color. */

	static float angle;	/* The current measured angle. */
	static VR_Side cursor_side;	/* Side of the current interaction cursor. */

	static void draw_line(VR_UI::Cursor& c, Coord3Df& p1, Coord3Df& p2);
public:
	static Widget_Measure obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "MEASURE"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_MEASURE; };	/* Type of Widget. */

	virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
	virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
	virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

	virtual void render(VR_Side side) override;	/* Apply the widget's custom render function (if any). */
};

#endif /* __VR_WIDGET_MEAURE_H__ */
