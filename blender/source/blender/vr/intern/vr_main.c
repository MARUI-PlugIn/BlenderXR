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
 * The Original Code is Copyright (C) 2007 Blender Foundation but based
 * on ghostwinlay.c (C) 2001-2002 by NaN Holding BV
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2008
 *
 * ***** END GPL LICENSE BLOCK *****
 */

 /** \file blender/vr/intern/vr_main.c
  *  \ingroup vr
  *
  * Main VR module. Also handles loading/unloading of VR device shared libraries.
  */

#include "vr_build.h"

#include "vr_main.h"

#include "BLI_math.h"

#include "DNA_camera_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_camera.h"

#include "GPU_framebuffer.h"
#include "GPU_viewport.h"

#include "draw_manager.h"
#include "wm_draw.h"

#ifdef WIN32
#include "BLI_winstuff.h"
#else
#include <dlfcn.h>
#include <GL/glxew.h>
#endif

#include "vr_api.h"

#ifndef WIN32
/* Remove __stdcall for the dll imports. */
#define __stdcall
#endif

/* VR shared library functions. */
typedef int(__stdcall *c_createVR)(void);	/* Create the internal VR object. Must be called before the functions below. */
#ifdef WIN32
typedef int(__stdcall *c_initVR)(void* device, void* context);	/*Initialize the internal VR object (OpenGL). */
#else
typedef int(__stdcall *c_initVR)(void* display, void* drawable, void* context);	/* Initialize the internal object (OpenGL). */
#endif
typedef int(__stdcall *c_getHMDType)(int* type);	/* Get the type of HMD used for VR. */
typedef int(__stdcall *c_setEyeParams)(int side, float fx, float fy, float cx, float cy);	/* Set rendering parameters. */
typedef int(__stdcall *c_getDefaultEyeParams)(int side, float* fx, float* fy, float* cx, float* cy);	/* Get the HMD's default parameters. */
typedef int(__stdcall *c_getDefaultEyeTexSize)(int* w, int* h, int side);	/* Get the default eye texture size. */
typedef int(__stdcall *c_updateTrackingVR)(void);	/* Update the t_eye positions based on latest tracking data. */
typedef int(__stdcall *c_getEyePositions)(float t_eye[VR_SIDES][4][4]);	/* Last tracked position of the eyes. */
typedef int(__stdcall *c_getHMDPosition)(float t_hmd[4][4]);	/* Last tracked position of the HMD. */
typedef int(__stdcall *c_getControllerPositions)(float t_controller[VR_MAX_CONTROLLERS][4][4]);	/* Last tracked position of the controllers. */
typedef int(__stdcall *c_getControllerStates)(void* controller_states[VR_MAX_CONTROLLERS]);	/* Last tracked button states of the controllers. */
typedef int(__stdcall *c_blitEye)(int side, void* texture_resource, const float* aperture_u, const float* aperture_v);	/* Blit a rendered image into the internal eye texture. */
typedef int(__stdcall *c_blitEyes)(void* texture_resource_left, void* texture_resource_right, const float* aperture_u, const float* aperture_v);	/* Blit rendered images into the internal eye textures. */
typedef int(__stdcall *c_submitFrame)(void);	/* Submit frame to the HMD. */
typedef int(__stdcall *c_uninitVR)(void);	/* Un-initialize the internal object. */

static c_createVR vr_dll_create_vr;
static c_initVR vr_dll_init_vr;
static c_getHMDType vr_dll_get_hmd_type;
static c_setEyeParams vr_dll_set_eye_params;
static c_getDefaultEyeParams vr_dll_get_default_eye_params;
static c_getDefaultEyeTexSize vr_dll_get_default_eye_tex_size;
static c_updateTrackingVR vr_dll_update_tracking_vr;
static c_getEyePositions vr_dll_get_eye_positions;
static c_getHMDPosition vr_dll_get_hmd_position;
static c_getControllerPositions vr_dll_get_controller_positions;
static c_getControllerStates vr_dll_get_controller_states;
static c_blitEye vr_dll_blit_eye;
static c_blitEyes vr_dll_blit_eyes;
static c_submitFrame vr_dll_submit_frame;
static c_uninitVR vr_dll_uninit_vr;

/* VR module object (singleton). */
static VR vr;
VR *vr_get_obj() { return &vr; }

/* The active VR dll (if any). */
#ifdef WIN32
static HINSTANCE vr_dll;
#else
static void *vr_dll;
#endif

/* Unload shared library functions. */
static int vr_unload_dll_functions(void)
{
#ifdef WIN32
	int success = FreeLibrary(vr_dll);
	if (success) {
		vr_dll = 0;
		return 0;
	}
#else
	int error = dlclose(vr_dll);
	if (!error) {
		vr_dll = 0;
		return 0;
	}
#endif

	return -1;
}

/* Load shared library functions and set VR type. */
static int vr_load_dll_functions(void)
{
	if (vr_dll) {
		vr_unload_dll_functions();
	}

	/* The shared library must be in the folder that contains the Blender executable.
	 * "BlenderXR_SteamVR.dll/.so" also requires "openvr_api.dll/.so" to be present.
	 * "BlenderXR_Fove.dll" also requires "FoveClient.dll". */
	for (int i = 1; i < VR_TYPES; ++i) {
		if (i == VR_TYPE_STEAM) {
			#ifdef WIN32
			vr_dll = LoadLibrary("BlenderXR_SteamVR.dll");
			#elif defined __linux__
			vr_dll = dlopen("libBlenderXR_SteamVR.so", RTLD_NOW | RTLD_LOCAL);
			#elif defined __APPLE__
			vr_dll = dlopen("BlenderXR_SteamVR.bundle", RTLD_NOW | RTLD_LOCAL);
			#else
			return -1;
			#endif
			if (vr_dll) {
				vr.type = VR_TYPE_STEAM;
				break;
			}
		}
#ifdef WIN32
		else if (i == VR_TYPE_OCULUS) {
			vr_dll = LoadLibrary("BlenderXR_Oculus.dll");
			if (vr_dll) {
				vr.type = VR_TYPE_OCULUS;
				break;
			}
		}
		else if (i == VR_TYPE_FOVE) {
			vr_dll = LoadLibrary("BlenderXR_Fove.dll");
			if (vr_dll) {
				vr.type = VR_TYPE_FOVE;
				break;
			}
		}
#endif
	}
	if (!vr_dll) {
		return -1;
	}

#ifdef WIN32
	vr_dll_create_vr = (c_createVR)GetProcAddress(vr_dll, "c_createVR");
	if (!vr_dll_create_vr) {
		return -1;
	}
	vr_dll_init_vr = (c_initVR)GetProcAddress(vr_dll, "c_initVR");
	if (!vr_dll_init_vr) {
		return -1;
	}
	vr_dll_get_hmd_type = (c_getHMDType)GetProcAddress(vr_dll, "c_getHMDType");
	if (!vr_dll_get_hmd_type) {
		return -1;
	}
	vr_dll_set_eye_params = (c_setEyeParams)GetProcAddress(vr_dll, "c_setEyeParams");
	if (!vr_dll_set_eye_params) {
		return -1;
	}
	vr_dll_get_default_eye_params = (c_getDefaultEyeParams)GetProcAddress(vr_dll, "c_getDefaultEyeParams");
	if (!vr_dll_get_default_eye_params) {
		return -1;
	}
	vr_dll_get_default_eye_tex_size = (c_getDefaultEyeTexSize)GetProcAddress(vr_dll, "c_getDefaultEyeTexSize");
	if (!vr_dll_get_default_eye_tex_size) {
		return -1;
	}
	vr_dll_update_tracking_vr = (c_updateTrackingVR)GetProcAddress(vr_dll, "c_updateTrackingVR");
	if (!vr_dll_update_tracking_vr) {
		return -1;
	}
	vr_dll_get_eye_positions = (c_getEyePositions)GetProcAddress(vr_dll, "c_getEyePositions");
	if (!vr_dll_get_eye_positions) {
		return -1;
	}
	vr_dll_get_hmd_position = (c_getHMDPosition)GetProcAddress(vr_dll, "c_getHMDPosition");
	if (!vr_dll_get_hmd_position) {
		return -1;
	}
	vr_dll_get_controller_positions = (c_getControllerPositions)GetProcAddress(vr_dll, "c_getControllerPositions");
	if (!vr_dll_get_controller_positions) {
		return -1;
	}
	vr_dll_get_controller_states = (c_getControllerStates)GetProcAddress(vr_dll, "c_getControllerStates");
	if (!vr_dll_get_controller_states) {
		return -1;
	}
	vr_dll_blit_eye = (c_blitEye)GetProcAddress(vr_dll, "c_blitEye");
	if (!vr_dll_blit_eye) {
		return -1;
	}
	vr_dll_blit_eyes = (c_blitEyes)GetProcAddress(vr_dll, "c_blitEyes");
	if (!vr_dll_blit_eyes) {
		return -1;
	}
	vr_dll_submit_frame = (c_submitFrame)GetProcAddress(vr_dll, "c_submitFrame");
	if (!vr_dll_submit_frame) {
		return -1;
	}
	vr_dll_uninit_vr = (c_uninitVR)GetProcAddress(vr_dll, "c_uninitVR");
	if (!vr_dll_uninit_vr) {
		return -1;
	}
#else
	vr_dll_create_vr = (c_createVR)dlsym(vr_dll, "c_createVR");
	if (!vr_dll_create_vr) {
		return -1;
	}
	vr_dll_init_vr = (c_initVR)dlsym(vr_dll, "c_initVR");
	if (!vr_dll_init_vr) {
		return -1;
	}
	vr_dll_get_hmd_type = (c_getHMDType)dlsym(vr_dll, "c_getHMDType");
	if (!vr_dll_get_hmd_type) {
		return -1;
	}
	vr_dll_set_eye_params = (c_setEyeParams)dlsym(vr_dll, "c_setEyeParams");
	if (!vr_dll_set_eye_params) {
		return -1;
	}
	vr_dll_get_default_eye_params = (c_getDefaultEyeParams)dlsym(vr_dll, "c_getDefaultEyeParams");
	if (!vr_dll_get_default_eye_params) {
		return -1;
	}
	vr_dll_get_default_eye_tex_size = (c_getDefaultEyeTexSize)dlsym(vr_dll, "c_getDefaultEyeTexSize");
	if (!vr_dll_get_default_eye_tex_size) {
		return -1;
	}
	vr_dll_update_tracking_vr = (c_updateTrackingVR)dlsym(vr_dll, "c_updateTrackingVR");
	if (!vr_dll_update_tracking_vr) {
		return -1;
	}
	vr_dll_get_eye_positions = (c_getEyePositions)dlsym(vr_dll, "c_getEyePositions");
	if (!vr_dll_get_eye_positions) {
		return -1;
	}
	vr_dll_get_hmd_position = (c_getHMDPosition)dlsym(vr_dll, "c_getHMDPosition");
	if (!vr_dll_get_hmd_position) {
		return -1;
	}
	vr_dll_get_controller_positions = (c_getControllerPositions)dlsym(vr_dll, "c_getControllerPositions");
	if (!vr_dll_get_controller_positions) {
		return -1;
	}
	vr_dll_get_controller_states = (c_getControllerStates)dlsym(vr_dll, "c_getControllerStates");
	if (!vr_dll_get_controller_states) {
		return -1;
	}
	vr_dll_blit_eye = (c_blitEye)dlsym(vr_dll, "c_blitEye");
	if (!vr_dll_blit_eye) {
		return -1;
	}
	vr_dll_blit_eyes = (c_blitEyes)dlsym(vr_dll, "c_blitEyes");
	if (!vr_dll_blit_eyes) {
		return -1;
	}
	vr_dll_submit_frame = (c_submitFrame)dlsym(vr_dll, "c_submitFrame");
	if (!vr_dll_submit_frame) {
		return -1;
	}
	vr_dll_uninit_vr = (c_uninitVR)dlsym(vr_dll, "c_uninitVR");
	if (!vr_dll_uninit_vr) {
		return -1;
	}
#endif

	return 0;
}

/* Copy-pasted from wm_draw_offscreen_texture_parameters(). */
static void vr_draw_offscreen_texture_parameters(GPUOffScreen *offscreen)
{
	/* Setup offscreen color texture for drawing. */
	GPUTexture *texture = GPU_offscreen_color_texture(offscreen);

	/* We don't support multisample textures here. */
	BLI_assert(GPU_texture_target(texture) == GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, GPU_texture_opengl_bindcode(texture));

	/* No mipmaps or filtering. */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	/* GL_TEXTURE_BASE_LEVEL = 0 by default */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBindTexture(GL_TEXTURE_2D, 0);
}

int vr_init(bContext *C)
{
	memset(&vr, 0, sizeof(vr));
	int error = vr_load_dll_functions();

	if (!error) {
		vr_dll_create_vr();
#ifdef WIN32
		HDC device = wglGetCurrentDC();
		HGLRC context = wglGetCurrentContext();
		error = vr_dll_init_vr((void*)device, (void*)context);
#else
		if (vr.type == VR_TYPE_STEAM) {
			Display* display = glXGetCurrentDisplay();
			GLXDrawable drawable = glXGetCurrentDrawable();
			GLXContext context = glXGetCurrentContext();
			error = vr_dll_init_vr((void*)display, (void*)&drawable, (void*)&context);
		}
		else {
			return -1;
		}
#endif
		if (!error) {
			/* Get VR params. */
			vr_dll_get_default_eye_params(0, &vr.fx[0], &vr.fy[0], &vr.cx[0], &vr.cy[0]);
			vr_dll_get_default_eye_params(1, &vr.fx[1], &vr.fy[1], &vr.cx[1], &vr.cy[1]);
			vr_dll_get_default_eye_tex_size(&vr.tex_width, &vr.tex_height, 0);
			vr.aperture_u = 1.0f;
			vr.aperture_v = 1.0f;
			vr_dll_get_eye_positions(vr.t_eye[VR_SPACE_REAL]);
			vr_dll_get_hmd_position(vr.t_hmd[VR_SPACE_REAL]);
			vr_dll_get_controller_positions(vr.t_controller[VR_SPACE_REAL]);

			vr.ctx = C;
			vr.initialized = 1;
		}
	}

	if (!vr.initialized) {
		return -1;
		printf("vr_init() : Failed to initialize VR.");
	}

	return 0;
}

int vr_init_ui(void)
{
	BLI_assert(vr.initialized);

	int error;

	/* Assign the UI type based on the HMD type.
	 * This is important when the VR type differs from
	 * the HMD type (i.e. running WindowsMR through SteamVR. */
	vr_dll_get_hmd_type((int*)&vr.ui_type);

	vr_api_create_ui();
#ifdef WIN32
	HDC device = wglGetCurrentDC();
	HGLRC context = wglGetCurrentContext();
	error = vr_api_init_ui((void*)device, (void*)context);
#else
	if (vr.type == VR_TYPE_STEAM) {
		Display* display = glXGetCurrentDisplay();
		GLXDrawable drawable = glXGetCurrentDrawable();
		GLXContext context = glXGetCurrentContext();
		error = vr_api_init_ui((void*)display, (void*)&drawable, (void*)&context);
	}
	else {
		return -1;
	}
#endif
	if (!error) {
		/* Allocate controller structs. */
		for (int i = 0; i < VR_MAX_CONTROLLERS; ++i) {
			vr.controller[i] = MEM_callocN(sizeof(VR_Controller), "VR_Controller");
		}
		vr_dll_get_controller_states(vr.controller);

		vr.ui_initialized = 1;
	}

	if (!vr.ui_initialized) {
		return -1;
		printf("vr_init_ui() : Failed to initialize VR UI.");
	}

	return 0;
}

int vr_uninit(void)
{
	BLI_assert(vr.initialized);

	if (vr.ui_initialized) {
		vr_api_uninit_ui();
		/* Free controller structs. */
		for (int i = 0; i < VR_MAX_CONTROLLERS; ++i) {
			if (vr.controller[i]) {
				MEM_freeN(vr.controller[i]);
				vr.controller[i] = NULL;
			}
		}

		vr.ui_initialized = 0;
	}
	vr_dll_uninit_vr();

	vr.ctx = 0;
	vr.initialized = 0;

	//vr_free_viewports();

	int error = vr_unload_dll_functions();
	if (error) {
		return -1;
	}

	return 0;
}

int vr_create_viewports(struct ARegion* ar)
{
	BLI_assert(vr.initialized);

	if (!ar->draw_buffer) {
		ar->draw_buffer = MEM_callocN(sizeof(wmDrawBuffer), "wmDrawBuffer");

		for (int i = 0; i < 2; ++i) {
			GPUOffScreen *offscreen = GPU_offscreen_create(vr.tex_width, vr.tex_height, 0, true, true, NULL);
			if (!offscreen) {
				printf("vr_create_viewports() : Could not create offscreen buffers.");
				return -1;
			}
			vr_draw_offscreen_texture_parameters(offscreen);

			vr.offscreen[i] = ar->draw_buffer->offscreen[i] = offscreen;
			vr.viewport[i] = ar->draw_buffer->viewport[i] = GPU_viewport_create_from_offscreen(vr.offscreen[i]);
		}

		RegionView3D *rv3d = ar->regiondata;
		if (!rv3d) {
			return -1;
		}
#if WITH_VR
		rv3d->rflag |= RV3D_IS_VR;
#endif
	}

	return 0;
}

void vr_free_viewports(ARegion *ar)
{
	if (ar->draw_buffer) {
		for (int side = 0; side < 2; ++side) {
			if (vr.offscreen[side]) {
				GPU_offscreen_free(vr.offscreen[side]);
				vr.offscreen[side] = NULL;
			}
			if (vr.viewport[side]) {
				GPU_viewport_free(vr.viewport[side]);
				vr.viewport[side] = NULL;
			}
		}

		MEM_freeN(ar->draw_buffer);
		ar->draw_buffer = NULL;
	}
}

void vr_draw_region_bind(ARegion *ar, int side)
{
	BLI_assert(vr.initialized);

	if (!vr.viewport[side]) {
		return;
	}

	/* Render with VR dimensions, regardless of window size. */
	rcti rect;
	rect.xmin = 0;
	rect.xmax = vr.tex_width;
	rect.ymin = 0;
	rect.ymax = vr.tex_height;

	GPU_viewport_bind(vr.viewport[side], &rect);

	ar->draw_buffer->bound_view = side;
}

void vr_draw_region_unbind(ARegion *ar, int side)
{
	BLI_assert(vr.initialized);

	if (!vr.viewport[side]) {
		return;
	}

	ar->draw_buffer->bound_view = -1;

	GPU_viewport_unbind(vr.viewport[side]);
}

int vr_update_tracking(void)
{
	BLI_assert(vr.initialized);

	int error = vr_dll_update_tracking_vr();

	/* Get hmd and eye positions. */
	vr_dll_get_hmd_position(vr.t_hmd[VR_SPACE_REAL]);
	vr_dll_get_eye_positions(vr.t_eye[VR_SPACE_REAL]);

	/* Get controller positions. */
	vr_dll_get_controller_positions(vr.t_controller[VR_SPACE_REAL]);

	if (vr.ui_initialized) {
		/* Get controller button states. */
		vr_dll_get_controller_states(vr.controller);

		/* Update the UI. */
		error = vr_api_update_tracking_ui();
	}

	if (error) {
		vr.tracking = 0;
	}
	else {
		vr.tracking = 1;
	}

	return error;
}

int vr_blit(void)
{
	BLI_assert(vr.initialized);

	int error;

#if WITH_VR
	vr_dll_blit_eyes((void*)(&(vr.viewport[VR_SIDE_LEFT]->fbl->default_fb->attachments[2].tex->bindcode)),
			     	 (void*)(&(vr.viewport[VR_SIDE_RIGHT]->fbl->default_fb->attachments[2].tex->bindcode)),
					 &vr.aperture_u, &vr.aperture_v);
#endif
	error = vr_dll_submit_frame();

	return error;
}

void vr_pre_scene_render(int side)
{
	BLI_assert(vr.ui_initialized);

	vr_api_pre_render(side);
}
void vr_post_scene_render(int side)
{
	BLI_assert(vr.ui_initialized);

	vr_api_post_render(side);
}

void vr_do_interaction(void)
{
	BLI_assert(vr.ui_initialized);

	vr_api_execute_operations();
}

void vr_update_view_matrix(int side, const float view[4][4])
{
	BLI_assert(vr.ui_initialized);

	/* Take navigation into account. */
	static float(*navinv)[4];
	navinv = vr_api_get_navigation_matrix(1);
	_va_mul_m4_series_3(vr.t_eye[VR_SPACE_REAL][side], navinv, view);
	invert_m4_m4(vr.t_eye_inv[VR_SPACE_REAL][side], vr.t_eye[VR_SPACE_REAL][side]);
	vr_api_update_view_matrix(vr.t_eye_inv[VR_SPACE_REAL][side]);
}

void vr_update_projection_matrix(const float projection[4][4])
{
	BLI_assert(vr.ui_initialized);
	vr_api_update_projection_matrix(projection);
}

void vr_compute_viewplane(int side, CameraParams *params, int winx, int winy)
{
	BLI_assert(vr.initialized);
	BLI_assert(params);

	rctf viewplane;
	float xasp, yasp, pixsize, viewfac, sensor_size, dx, dy;

	params->clipsta = 0.0001f;
	params->clipend = 10000.0f;
	//params->zoom = 2.0f;

	xasp = vr.aperture_u;
	yasp = vr.aperture_v;
	params->ycor = xasp / yasp;

	if (params->is_ortho) {
		/* orthographic camera */
		/* scale == 1.0 means exact 1 to 1 mapping */
		pixsize = params->ortho_scale;
	}
	else {
		/* perspective camera */
		switch (params->sensor_fit) {
			case CAMERA_SENSOR_FIT_AUTO:
			case CAMERA_SENSOR_FIT_HOR: {
				sensor_size = params->sensor_x;
				params->lens = vr.fx[side] * params->zoom * params->sensor_x;
				break;
			}
			case CAMERA_SENSOR_FIT_VERT: {
				sensor_size = params->sensor_y;
				params->lens = vr.fy[side] * params->zoom * params->sensor_y;
				break;
			}
		}
		pixsize = (sensor_size * params->clipsta) / params->lens;
	}

	switch (params->sensor_fit) {
		case CAMERA_SENSOR_FIT_AUTO: {
			if (xasp * vr.tex_width >= yasp *vr.tex_height) {
				viewfac = vr.tex_width;
			}
			else {
				viewfac = params->ycor * vr.tex_height;
			}
			break;
		}
		case CAMERA_SENSOR_FIT_HOR: {
			viewfac = vr.tex_width;
			break;
		}
		case CAMERA_SENSOR_FIT_VERT: {
			viewfac = params->ycor * vr.tex_height;
			break;
		}
	}
	pixsize /= viewfac;

	/* extra zoom factor */
	pixsize *= params->zoom;

	/* lens shift and offset */
	params->offsetx = (vr.cx[side] - 0.5f)*2.0f * xasp;
	params->offsety = (vr.cy[side] - 0.5f)*2.0f * yasp;

	dx = params->shiftx * viewfac + vr.tex_width * params->offsetx;
	dy = params->shifty * viewfac + vr.tex_height * params->offsety;

	/* Compute view plane:
     * centered and at distance 1.0: */
	float res_x = (float)vr.tex_width;
	float res_y = (float)vr.tex_height;
	float pfx = vr.fx[side] * res_x;
	float pfy = vr.fy[side] * res_y;
	float pcx = vr.cx[side] * res_x;
	float pcy = (1.0f - vr.cy[side]) * res_y;
	viewplane.xmax = ((res_x - pcx) / pfx) * params->clipsta;
	viewplane.xmin = (-pcx / pfx) * params->clipsta;
	viewplane.ymax = ((res_y - pcy) / pfy) * params->clipsta;
	viewplane.ymin = (-pcy / pfy) * params->clipsta;

	/* Used for rendering (offset by near-clip with perspective views), passed to RE_SetPixelSize.
	 * For viewport drawing 'RegionView3D.pixsize'. */
	params->viewdx = pixsize;
	params->viewdy = params->ycor * pixsize;
	params->viewplane = viewplane;
}

void vr_compute_viewmat(int side, float viewmat_out[4][4])
{
	BLI_assert(vr.initialized);

	if (vr.ui_initialized) {
		/* Take navigation into account. */
		static float (*navmat)[4];
		navmat = vr_api_get_navigation_matrix(0);
		_va_mul_m4_series_3(vr.t_eye[VR_SPACE_BLENDER][side], navmat, vr.t_eye[VR_SPACE_REAL][side]);
		invert_m4_m4(viewmat_out, vr.t_eye[VR_SPACE_BLENDER][side]);
	}
	else {
		invert_m4_m4(viewmat_out, vr.t_eye[VR_SPACE_REAL][side]);
	}
}
