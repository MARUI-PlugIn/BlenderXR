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

/** \file blender/vr/intern/vr_widget_annotate.h
*   \ingroup vr
*/

#ifndef __VR_WIDGET_ANNOTATE_H__
#define __VR_WIDGET_ANNOTATE_H__

#include "vr_widget.h"

struct bGPDspoint;
struct bGPdata;
struct bGPDlayer;
struct bGPDframe;
struct bGPDstroke;
struct Main;
class Widget_Measure;

/* Interaction widget for the Annotate tool. */
class Widget_Annotate : public VR_Widget
{
	friend class Widget_Measure;
	friend class Widget_Menu;

	static struct bGPdata *gpd;	/* The VR gpencil data .*/
	static std::vector<struct bGPDlayer *>gpl;	/* The VR gpencil layer. */
	static std::vector<struct bGPDframe *>gpf;	/* The VR gpencil frame. */
	static struct Main *main;	/* The current scene data. */

	static uint num_layers;	/* The number of VR gpencil layers (one layer for each color + measure tool layer). */
	static uint active_layer;	/* The currently active VR gpencil layer. */
	static int init(bool new_scene); /* Initialize the VR gpencil structs. */

	static std::vector<bGPDspoint> points;	/* The 3D points associated with the current stroke. */

	//static float point_thickness;	/* Stroke thickness for points. */
	static float line_thickness;	/* Stroke thickness for lines. */
	static float color[4];	/* Stroke color. */

	static bool eraser;	/* Whether the annotate widget is in eraser mode. */
	static VR_Side cursor_side;	/* Side of the current interaction cursor. */
	static float eraser_radius;	/* Radius of the eraser ball. */
	static void erase_stroke(bGPDstroke *gps, bGPDframe *gp_frame);	/*	Helper function to erase a stroke. */
public:
	static Widget_Annotate obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "ANNOTATE"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_ANNOTATE; };	/* Type of Widget. */

	//virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	//virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
	virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
	virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

	virtual void render(VR_Side side) override;	/* Apply the widget's custom render function (if any). */
};

#endif /* __VR_WIDGET_ANNOTATE_H__ */
