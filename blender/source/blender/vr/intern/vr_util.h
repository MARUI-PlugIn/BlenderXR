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

/** \file blender/vr/intern/vr_util.h
*   \ingroup vr
*/

#ifndef __VR_UTIL_H__
#define __VR_UTIL_H__

#include "ED_view3d.h"
struct BMesh;

/* Modified from view3d_project.c */
#define WIDGET_SELECT_RAYCAST_NEAR_CLIP 0.0001f
#define WIDGET_SELECT_RAYCAST_ZERO_CLIP 0.0001f

class VR_Util
{
public:
    /* From view3d_select.c */
    static void object_deselect_all_visible(ViewLayer *view_layer, View3D *v3d);

    /* From view3d_select.c */
    static void deselectall_except(ViewLayer *view_layer, Base *b);

    static eV3DProjStatus view3d_project(const ARegion *ar,
	    const float perspmat[4][4], const bool is_local,  /* normally hidden */
	    const float co[3], float r_co[2], const eV3DProjTest flag);

    static void deselectall_edit(BMesh *bm, int mode);

    /* Adapted from view3d_select.c */
    static void raycast_select_single_vertex(const Coord3Df& p, ViewContext *vc, bool extend, bool deselect);

    static void raycast_select_single_edge(const Coord3Df& p, ViewContext *vc, bool extend, bool deselect);

    static void raycast_select_single_face(const Coord3Df& p, ViewContext *vc, bool extend, bool deselect);

    static void raycast_select_single_edit(
	    const Coord3Df& p,
	    bool extend,
	    bool deselect,
	    bool toggle = false,
	    bool enumerate = false);

    static void raycast_select_single(
	    const Coord3Df& p,
	    bool extend,
	    bool deselect,
	    bool toggle = false,
	    bool enumerate = false,
	    bool object = true,
	    bool obcenter = true);
};

#endif /* __VR_UTIL_H__ */
