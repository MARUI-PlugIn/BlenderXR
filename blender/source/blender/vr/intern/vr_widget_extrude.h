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

/** \file blender/vr/intern/vr_widget_extrude.h
*   \ingroup vr
*/

#ifndef __VR_WIDGET_EXTRUDE_H__
#define __VR_WIDGET_EXTRUDE_H__

#include "vr_widget.h"

/* Interaction widget for the Extrude tool. */
class Widget_Extrude : public VR_Widget
{
	friend class Widget_Transform;
	friend class Widget_SwitchTool;
	friend class Widget_Menu;

	typedef enum ExtrudeMode {
		EXTRUDEMODE_REGION = 0	/* Region extrude mode. */
		,
		EXTRUDEMODE_INDIVIDUAL = 1	/* Individual extrude mode. */
		,
		EXTRUDEMODE_NORMALS = 2	/* Normals extrude mode. */
		,
		EXTRUDEMODES /* Number of extrude modes. */
	} ExtrudeMode;

	static ExtrudeMode extrude_mode;	/* The current extrude mode for the Extrude tool. */
	static bool extrude; /* Whether the current interaction is an extrude operation. */
	static bool flip_normals;	/* Whether to flip normals when extruding edges. */
	//static bool offset_even;	/* Whether to use even offsets when extruding along normals. */
	static bool transform;	/* Whether Transform tool behavior is enabled. */
public:
	static Widget_Extrude obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "EXTRUDE"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_EXTRUDE; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
	virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
	virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

	virtual void render(VR_Side side) override;	/* Apply the widget's custom render function (if any). */
};

#endif /* __VR_WIDGET_EXTRUDE_H__ */
