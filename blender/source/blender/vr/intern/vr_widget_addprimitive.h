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

/** \file blender/vr/intern/vr_widget_addprimitive.h
*   \ingroup vr
*/

#ifndef __VR_WIDGET_ADDPRIMITIVE_H__
#define __VR_WIDGET_ADDPRIMITIVE_H__

#include "vr_widget.h"

/* Interaction widget for adding (mesh) primitives. */
class Widget_AddPrimitive : public VR_Widget
{
	friend class Widget_SwitchTool;
	friend class Widget_Menu;

	typedef enum Primitive {
		PRIMITIVE_PLANE = 0	/* Plane primitive. */
		,
		PRIMITIVE_CUBE = 1	/* Cube primitive. */
		,
		PRIMITIVE_CIRCLE = 2	/* Circle primitive. */
		,
		PRIMITIVE_CYLINDER = 3	/* Cylinder primitive. */
		,
		PRIMITIVE_CONE = 4	/* Cone primitive. */
		,
		PRIMITIVE_GRID = 5	/* Grid primitive. */
		,
		PRIMITIVE_MONKEY = 6	/* Monkey primitive. */
		,
		PRIMITIVE_UVSPHERE = 7	/* UV sphere primitive. */
		,
		PRIMITIVE_ICOSPHERE = 8	/* Icosphere primitive. */
		,
		PRIMITIVES /* Number of primitives. */
	} Primitive;

	static Primitive primitive;	/* The current primitive creation mode. */
public:
	static bool calc_uvs;	/* Whether to calculate uvs upon primitive creation. */
	static float size;	/* The size for planes / cubes / grids / monkeys. */
	static int end_fill_type;	/* The fill type for circles / cylinders / cones. */
	static int circle_vertices;	/* The number of vertices for circles / cylinders / cones. */
	static float radius;	/* The radius for circles / cylinders / spheres. */
	static float depth;	/* The depth for cylinders / cones. */
	static float cone_radius1;	/* The first radius for cones. */
	static float cone_radius2;	/* The second radius for cones. */
	static int grid_subdivx;	/* The number of x subdivisions for grids. */
	static int grid_subdivy;	/* The number of y subdivisions for grids. */
	static int sphere_segments; /* The number of segments for uv spheres. */
	static int sphere_rings; /* The number of rings for uv spheres. */
	static int sphere_subdiv; /* The number of segments for icospheres. */

	static Widget_AddPrimitive obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "ADDPRIMITIVE"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_ADDPRIMITIVE; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual bool has_drag(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "dragging". */
};

#endif /* __VR_WIDGET_ADDPRIMITIVE_H__ */
