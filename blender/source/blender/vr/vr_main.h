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

/** \file blender/vr/vr_main.h
*   \ingroup vr
*/

#ifndef __VR_MAIN_H__
#define __VR_MAIN_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define VR_MAX_CONTROLLERS 3 /* Maximum number of controllers that can be simultaneously supported. */

typedef enum VR_Space
{
	VR_SPACE_REAL		= 0	/* Real-world coordinates, units are meters, origin is dependent on VR device set-up (usually on the floor). */
	,
	VR_SPACE_BLENDER	= 1 /* Blender coordinates. */
	,
	VR_SPACES			= 2 /* Number of coordinate systems. */
} VR_Space;

typedef enum VR_Side
{
	VR_SIDE_MONO		= 0	/* The only available option in a mono rig. */
	,
	VR_SIDE_LEFT		= 0 /* Left side. */
	,
	VR_SIDE_RIGHT		= 1 /* Right side. */
	,
	VR_SIDES			= 2 /* Number of actual (non-symbolic) sides. */
	,
	VR_SIDE_AUX			= 2	/* Auxilliary third "side" (where applicable). */
	,
	VR_SIDE_BOTH		=-1 /* Both sides (where applicable). */
	,
	VR_SIDE_DOMINANT	=-2 /* The side of the dominant eye (where applicable). */
} VR_Side;

/* Enum defining VR device types/APIs. */
typedef enum VR_Type 
{
	VR_TYPE_NULL	= 0	/* Empty null-implementation. */
	,
	VR_TYPE_OCULUS	= 1	/* Oculus OVR API was used for implementation. */
	,
	VR_TYPE_STEAM	= 2	/* SteamVR (Valve OpenVR) was used for implementation. */
	,
	VR_TYPE_FOVE	= 3	/* Fove API was used for implementation. */
	,
	VR_TYPES		= 4	/* Number of VR types. */
} VR_Type; 

/* Enum defining VR UIs. */
typedef enum VR_UI_Type 
{
	VR_UI_TYPE_NULL		= 0 /* No UI processing. */
	,
	VR_UI_TYPE_OCULUS	= 1 /* Oculus Touch UI. */
	,
	VR_UI_TYPE_VIVE		= 2 /* HTC Vive controller UI. */
	,
	VR_UI_TYPE_MICROSOFT= 3 /* Windows MR UI. */
	,
	VR_UI_TYPE_FOVE		= 4 /* Fove eye-tracking UI. */
	,
	VR_UI_TYPES			= 5 /* Number of VR UI types. */
} VR_UI_Type;

/* Simple struct for 3D input device information. */
typedef struct VR_Controller {
	VR_Side	side;		/* Side of the controller.  */
	int		available;  /* Whether the controller are (currently) available.  */
	unsigned long long	buttons;	/* Buttons currently pressed on the controller. */
	unsigned long long	buttons_touched;	/* Buttons currently touched on the controller (if available). */
	float	dpad[2];	/* Dpad / touchpad position (u,v). */
	float	stick[2];	/* Joystick / thumbstick position (u,v). */
	float	trigger_pressure;	/* Analog trigger pressure (0~1) (if available). */
} VR_Controller;

struct GPUOffscreen;
struct GPUViewport;
struct wmWindow;
struct bContext;

/* VR module struct. */
typedef struct VR {
	VR_Type	type;	/* Type of API used for the VR device. */
	VR_UI_Type ui_type;	/* Type of VR UI used. */

	int initialized;	/* Whether the base VR module was successfully initialized and currently active. */
	int ui_initialized; /* Whether the VR UI module was successfully initialized and currently active. */

	int tracking;		/* Whether the VR tracking state is currently active/valid. */

	float fx[VR_SIDES]; /* Horizontal focal length, in "image-width" - units(1 = image width). */
	float fy[VR_SIDES]; /* Vertical focal length, in "image-height" - units(1 = image height). */
	float cx[VR_SIDES]; /* Horizontal principal point, in "image-width" - units(0.5 = image center). */
	float cy[VR_SIDES]; /* Vertical principal point, in "image-height" - units(0.5 = image center). */

	int tex_width;		/* Default eye texture width. */
	int tex_height;		/* Default eye texture height. */

	float aperture_u;	/* The aperture of the texture(0~u) that contains the rendering. */
	float aperture_v;	/* The aperture of the texture(0~v) that contains the rendering. */

	float t_hmd[VR_SPACES][4][4];	/* Last tracked position of the HMD. */
	float t_hmd_inv[VR_SPACES][4][4];	/* Inverses of t_hmd. */
	float t_eye[VR_SPACES][VR_SIDES][4][4];	/* Last tracked position of the eyes. */
	float t_eye_inv[VR_SPACES][VR_SIDES][4][4]; /* Inverses of t_eye. */

	struct VR_Controller *controller[VR_MAX_CONTROLLERS];		/* Controllers associated with the HMD device. */
	float t_controller[VR_SPACES][VR_MAX_CONTROLLERS][4][4];	/* Last tracked positions of the controllers. */
	float t_controller_inv[VR_SPACES][VR_MAX_CONTROLLERS][4][4];/* Inverses of t_controller. */

	struct GPUOffScreen *offscreen[VR_SIDES];	/* Offscreen render buffers (one per eye). */
	struct GPUViewport *viewport[VR_SIDES];		/* Viewports corresponding to offscreen buffers. */
	struct wmWindow *window;	/* The window that contains the VR viewports. */

	struct bContext *ctx; /* The Blender context associated with the VR module. */
} VR;
VR *vr_get_obj(void); /* Getter function for VR module singleton. */

int	vr_init(struct bContext *C);	/* Initialize VR operations. Returns 0 on success, -1 on failure. */
int vr_init_ui(void);	/* Initialize VR UI operations. Returns 0 on success, -1 on failure. */
int vr_uninit(void);	/* Un-intialize VR operations. Returns 0 on success, -1 on failure. */

struct ARegion;

int vr_create_viewports(struct ARegion *ar);	/* Create VR offscreen buffers and viewports. */
void vr_free_viewports(struct ARegion *ar);		/* Free VR offscreen buffers and viewports. */
void vr_draw_region_bind(struct ARegion *ar, int side);	/* Bind the VR offscreen buffer for rendering. */
void vr_draw_region_unbind(struct ARegion *ar, int side);	/* Unbind the VR offscreen buffer. */

/* VR module functions. */
int vr_update_tracking(void);	/* Update tracking. */
int vr_blit(void);	/* Blit the hmd. */

/* Interaction/execution function. */
void vr_do_interaction(void);	/* Interaction update/execution where the VR module may alter scene data. */
void vr_do_post_render_interaction(void);	/* Interaction update/execution for special operations (i.e. undo/redo) that need to be called after the scene is rendered.	*/

/* Drawing functions. */
void vr_pre_scene_render(int side);	/* Pre-scene rendering call. */
void vr_post_scene_render(int side);/* Post-scene rendering call. */

void vr_update_view_matrix(int side, const float view[4][4]);	/* Update OpenGL view matrix for VR module. */
void vr_update_projection_matrix(const float projection[4][4]);	/* Update OpenGL projection matrix for VR module. */

struct CameraParams;

/* Utility functions. */
void vr_compute_viewplane(int side, struct CameraParams *params, int winx, int winy);	/* Compute the VR camera viewplane. */
void vr_compute_viewmat(int side, float viewmat_out[4][4]);	/* Compute the VR camera viewmat. */

#ifdef __cplusplus
}
#endif

#endif /* __VR_MAIN_H__ */

