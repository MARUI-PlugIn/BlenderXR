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

/** \file blender/vr/intern/vr_api.h
*   \ingroup vr
*/

#ifndef __VR_API_H__
#define __VR_API_H__

#ifdef __cplusplus
extern "C" {
#endif

int vr_api_create_ui();	/* Create a object internally. Must be called before the functions below. */
#ifdef WIN32
int vr_api_init_ui(void* device, void* context);	/* Initialize the internal object (OpenGL). */
#else
int vr_api_init_ui(void* display, void* drawable, void* context);	/* Initialize the internal object (OpenGL). */
#endif
int vr_api_update_tracking_ui();	/* Update VR tracking including UI button states. */
int vr_api_execute_operations();	/* Execute UI operations. */
int vr_api_execute_post_render_operations();	/* Execute post-render UI operations. */
const float *vr_api_get_navigation_matrix(int inverse);	/* Get the navigation matrix (or inverse navigation matrix) from the UI module. */
float vr_api_get_navigation_scale(); /* Get the scale factor between real-world units and Blender units from the UI module. */
int vr_api_update_view_matrix(const float _view[4][4]);	/* Update the OpenGL view matrix for the UI module. */
int vr_api_update_projection_matrix(int side, const float _projection[4][4]);	/* Update the OpenGL projection matrix for the UI module. */

struct rcti;

int vr_api_update_viewport_bounds(const struct rcti *bounds);	/* Update viewport (window) bounds for the UI module. */

int vr_api_pre_render(int side);	/* Pre-render UI elements. */
int vr_api_post_render(int side);/* Post-render UI elements. */
int vr_api_uninit_ui();	/* Un-initialize the internal object. */

#ifdef __cplusplus
}
#endif

#endif /* __VR_API_H__ */
