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

/** \file blender/vr/intern/vr_widget_insetfaces.h
*   \ingroup vr
*/

#ifndef __VR_WIDGET_INSETFACES_H__
#define __VR_WIDGET_INSETFACES_H__

#include "vr_widget.h"

/* Interaction widget for the Inset Faces tool. */
class Widget_InsetFaces : public VR_Widget
{
	friend class Widget_Menu;

	static Coord3Df  p0;	/* Start / interaction point of the bevel. */
	static Coord3Df  p1;	/* Current / end point of the bevel. */
	static VR_Side cursor_side;	/* Side of the current interaction cursor. */
public:
	static float thickness;	/* The inset thickness. */
	static float depth;	/* The inset depth. */

	static bool	use_individual;	/* Whether to perform individual inset faces for multiple selection. */
	static bool use_boundary;	/* Whether to inset face boundaries. */
	static bool use_even_offset;	/* Whether to scale the offset to give more even thickness. */;
	static bool use_relative_offset;	/* Whether to scale the offset by surrounding geometry. */
	static bool use_outset;	/* Whether to outset rather than inset. */

	static Widget_InsetFaces obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "INSETFACES"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_INSETFACES; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
	virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
	virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

	virtual void render(VR_Side side) override;	/* Apply the widget's custom render function (if any). */
};

#endif /* __VR_WIDGET_INSETFACES_H__ */
