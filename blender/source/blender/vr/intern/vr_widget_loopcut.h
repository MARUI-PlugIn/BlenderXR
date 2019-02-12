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

/** \file blender/vr/intern/vr_widget_loopcut.h
*   \ingroup vr
*/

#ifndef __VR_WIDGET_LOOPCUT_H__
#define __VR_WIDGET_LOOPCUT_H__

#include "vr_widget.h"

/* Interaction widget for the Loop Cut tool. */
class Widget_LoopCut : public VR_Widget
{
	friend class Widget_Menu;

	static bool edge_slide;	/* Whether the loop cut tool is in edge slide mode. */
public:
	static Coord3Df	p0;	/* Start / interaction point of the loop cut. */
	static Coord3Df	p1;	/* Current / end point of the loop cut. */
	static Coord3Df p0_b;	/* Start / interaction point of the loop cut (Blender coordinates). */
	static Coord3Df p1_b;	/* Current / end point of the loop cut (Blender coordinates). */
	static bool selection_empty;	/* Whether the current selection is empty. */
public:
	static int object_index;	/* The object index for the loop cut. */
	static int edge_index;	/* the edge index for the loop cut. */

	static float percent;	/* The loop cut percent (offset). */
	static int cuts;	/* The number of loop cuts to perform. */
	static bool double_side; /* Whether to do a double side edge slide. */
	static bool even;	/* Whether to use even offsets when edge sliding. */
	static bool flipped;	/* Whether to flip edges when edge sliding. */
	static bool clamp;	/* Whether to clamp to face bounds when edge sliding. */

	static Widget_LoopCut obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "LOOPCUT"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_LOOPCUT; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
	virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
	virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

	virtual void render(VR_Side side) override;	/* Apply the widget's custom render function (if any). */
};

#endif /* __VR_WIDGET_LOOPCUT_H__ */

