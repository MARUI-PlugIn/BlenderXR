/***********************************************************************************************//**
 * \file      oculus.cpp
 *            Oculus Rift HMD module.
 *            This file contains code related to using Oculus Rift HMDs.
 *            Both tracking and rendering are implemented.
 ***************************************************************************************************
 * \brief     Oculus Rift HMD module.
 * \copyright MARUI-PlugIn (inc.)
 **************************************************************************************************/

#include "vr_oculus.h"

#include <cstdlib>
#include <cmath>

/***********************************************************************************************//**
* \class                                  VR_Oculus
***************************************************************************************************
* Oculus HMD module for tracking and rendering.
* NOT THREAD-SAFE!
**************************************************************************************************/

//																		____________________________
//_____________________________________________________________________/   D3D::gle_context
/**
* OpenGL extension management for Oculus.
* -> temporarily made static to avoid crashes.
*/
OVR::GLEContext VR_Oculus::gle_context;

//																		____________________________
//_____________________________________________________________________/   GL::vshader_source
/**
* Primitive pass-through vertex shader source code.
*/
const char* const VR_Oculus::GL::vshader_source(STRING(#version 120\n
	attribute vec2 position;
	attribute vec2 uv;
	varying vec2 texcoord;
void main()
{
	gl_Position = vec4(position, 0.0, 1.0);
	texcoord = uv;
}
));

//																		____________________________
//_____________________________________________________________________/    GL::fshader_source
/**
 * Primitive texture look-up shader source code.
 */
const char* const VR_Oculus::GL::fshader_source(STRING(#version 120\n
	varying vec2 texcoord;
	uniform sampler2D tex;
	uniform vec4 param;
void main()
{
	gl_FragColor = pow(texture2D(tex, texcoord), param.zzzz); // GAMMA CORRECTION (1/gamma should be passed as param.z)
}
));

//                                                                          ________________________
//_________________________________________________________________________/       Oculus()
/**
 * Class constructor
 */
VR_Oculus::VR_Oculus()
	: VR()
	, hmd(0)
	, hmd_type(HMDType_Oculus)
	, frame_index(0)
	, initialized(false)
{
	this->eye[0] = Eye();
	this->eye[1] = Eye();
	memset(&this->gl, 0, sizeof(GL));
}

//                                                                          ________________________
//_________________________________________________________________________/     ~Oculus()
/**
 * Class destructor
 */
VR_Oculus::~VR_Oculus()
{
	if (this->initialized)
		this->uninit();
}

//                                                                          ________________________
//_________________________________________________________________________/		type()
/**
 * Get which API was used in this implementation.
 */
VR::Type VR_Oculus::type()
{
	return VR::Type_Oculus;
};

//                                                                          ________________________
//_________________________________________________________________________/		hmdType()
/**
 * Get which HMD was used in this implementation.
 */
VR::HMDType VR_Oculus::hmdType()
{
	return this->hmd_type;
};

//                                                                          ________________________
//_________________________________________________________________________/      acquireHMD()
/**
 * Initialize basic OVR operation and acquire the HMD object.
 * \return  Zero on success, and error code on failue.
 */
int VR_Oculus::acquireHMD()
{
	if (ovr_Initialize(nullptr) != ovrSuccess) {
		this->releaseHMD();
		return Error_InternalFailure;
	}

	ovrResult result;
	result = ovr_Create(&this->hmd, &this->luid);
	if (result != ovrSuccess || !this->hmd) {
		return Error_InternalFailure;
	}

	this->hmd_desc = ovr_GetHmdDesc(this->hmd);

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/    releaseHMD()
/**
 * Delete the HMD object and uninitialize basic OVR operation.
 * \return  Zero on success, and error code on failue.
 */
int VR_Oculus::releaseHMD()
{
	if (this->hmd) {
		ovr_Destroy(this->hmd);
		this->hmd = 0;
	}

	ovr_Shutdown();

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/        init()
/**
 * Initialize the VR device.
 * \param   device          The graphics device used by Blender (HDC)
 * \param   context         The rendering context used by Blender (HGLRC)
 * \return  Zero on success, an error code on failure.
 */
int VR_Oculus::init(void* device, void* context)
{
	ovrResult result;

	if (this->initialized) {
		this->uninit();
	}

	// Get the Blender viewport window / context IDs.
	this->gl.device = (HDC)device;
	this->gl.context = (HGLRC)context; // save old context so that we can share
	BOOL r = wglMakeCurrent(this->gl.device, this->gl.context); // just to be sure...

	if (!this->hmd) {
		int e = this->acquireHMD();
		if (e || !this->hmd) {
			return VR::Error_InternalFailure;
		}
	}

	// Setup Oculus GL context stuff
	if (!gle_context.IsInitialized()) {
		gle_context.Init();
	}
	OVR::GLEContext::SetCurrentContext(&gle_context);

	// Calculate required FOV:
	if (this->eye[Side_Left].fx <= 0) { // not yet loaded or overridden
		// Load the default eye parameters of the hmd
		this->getDefaultEyeParams(Side_Left,
			this->eye[Side_Left].fx,
			this->eye[Side_Left].fy,
			this->eye[Side_Left].cx,
			this->eye[Side_Left].cy);
	}
	if (this->eye[Side_Right].fx <= 0) { // not yet loaded or overridden
		// Load the default eye parameters of the hmd
		this->getDefaultEyeParams(Side_Right,
			this->eye[Side_Right].fx,
			this->eye[Side_Right].fy,
			this->eye[Side_Right].cx,
			this->eye[Side_Right].cy);
	}
	this->eye[Side_Left].fov.UpTan = this->eye[Side_Left].cy / this->eye[Side_Left].fy;
	this->eye[Side_Left].fov.DownTan = (1.0f - this->eye[Side_Left].cy) / this->eye[Side_Left].fy;
	this->eye[Side_Left].fov.LeftTan = this->eye[Side_Left].cx / this->eye[Side_Left].fx;
	this->eye[Side_Left].fov.RightTan = (1.0f - this->eye[Side_Left].cx) / this->eye[Side_Left].fx;
	this->eye[Side_Right].fov.UpTan = this->eye[Side_Right].cy / this->eye[Side_Right].fy;
	this->eye[Side_Right].fov.DownTan = (1.0f - this->eye[Side_Right].cy) / this->eye[Side_Right].fy;
	this->eye[Side_Right].fov.LeftTan = this->eye[Side_Right].cx / this->eye[Side_Right].fx;
	this->eye[Side_Right].fov.RightTan = (1.0f - this->eye[Side_Right].cx) / this->eye[Side_Right].fx;

	// Make eye render buffers
	for (int i = 0; i < Sides; ++i) {
		// Swap texture (color)
		ovrEyeType ovr_eye = (i == Side_Left) ? ovrEye_Left : ovrEye_Right;
		this->eye[i].texsize = ovr_GetFovTextureSize(this->hmd, ovr_eye, this->eye[i].fov, 1);
		ovrTextureSwapChainDesc desc;
		memset(&desc, 0, sizeof(desc));
		desc.Type = ovrTexture_2D;
		desc.ArraySize = 1;
		desc.Width = this->eye[i].texsize.w;
		desc.Height = this->eye[i].texsize.h;
		desc.MipLevels = 1;
		desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.SampleCount = 1;
		desc.StaticImage = ovrFalse;

		ovr_CreateTextureSwapChainGL(hmd, &desc, &this->eye[i].swap_texture_set);
		if (!this->eye[i].swap_texture_set) {
			this->uninit();
			return VR_Oculus::Error_InternalFailure;
		}
		// Depth texture:
		glGenFramebuffers(1, &this->eye[i].framebuffer);
	}

	// Create vertex buffer
	static const GLfloat vertex_data[] = {
		-1.0f, -1.0f,
		1.0f, -1.0f,
		-1.0f,  1.0f,
		1.0f,  1.0f,
	};
	glGenBuffers(1, &this->gl.verts);
	glBindBuffer(GL_ARRAY_BUFFER, this->gl.verts);
	glBufferData(GL_ARRAY_BUFFER, 2 * 4 * sizeof(float), vertex_data, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Create uv buffer
	static const GLfloat uv_data[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,
	};
	glGenBuffers(1, &this->gl.uvs);
	glBindBuffer(GL_ARRAY_BUFFER, this->gl.uvs);
	glBufferData(GL_ARRAY_BUFFER, 2 * 4 * sizeof(float), uv_data, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	this->eye[Side_Left].render_desc = ovr_GetRenderDesc(this->hmd, ovrEye_Left, this->eye[Side_Left].fov);
	this->eye[Side_Right].render_desc = ovr_GetRenderDesc(this->hmd, ovrEye_Right, this->eye[Side_Right].fov);
	this->eye[Side_Left].offset = this->eye[Side_Left].render_desc.HmdToEyePose;
	t_hmd2eye[Side_Left][0][0] = 1;   t_hmd2eye[Side_Left][0][1] = 0;   t_hmd2eye[Side_Left][0][2] = 0;   t_hmd2eye[Side_Left][0][3] = 0;
	t_hmd2eye[Side_Left][1][0] = 0;   t_hmd2eye[Side_Left][1][1] = 1;   t_hmd2eye[Side_Left][1][2] = 0;   t_hmd2eye[Side_Left][1][3] = 0;
	t_hmd2eye[Side_Left][2][0] = 0;   t_hmd2eye[Side_Left][2][1] = 0;   t_hmd2eye[Side_Left][2][2] = 1;   t_hmd2eye[Side_Left][2][3] = 0;
	t_hmd2eye[Side_Left][3][0] = this->eye[Side_Left].offset.Position.x;
	t_hmd2eye[Side_Left][3][1] = this->eye[Side_Left].offset.Position.y;
	t_hmd2eye[Side_Left][3][2] = this->eye[Side_Left].offset.Position.z;
	t_hmd2eye[Side_Left][3][3] = 1;
	this->eye[Side_Right].offset = this->eye[Side_Right].render_desc.HmdToEyePose;
	t_hmd2eye[Side_Right][0][0] = 1;   t_hmd2eye[Side_Right][0][1] = 0;   t_hmd2eye[Side_Right][0][2] = 0;   t_hmd2eye[Side_Right][0][3] = 0;
	t_hmd2eye[Side_Right][1][0] = 0;   t_hmd2eye[Side_Right][1][1] = 1;   t_hmd2eye[Side_Right][1][2] = 0;   t_hmd2eye[Side_Right][1][3] = 0;
	t_hmd2eye[Side_Right][2][0] = 0;   t_hmd2eye[Side_Right][2][1] = 0;   t_hmd2eye[Side_Right][2][2] = 1;   t_hmd2eye[Side_Right][2][3] = 0;
	t_hmd2eye[Side_Right][3][0] = this->eye[Side_Right].offset.Position.x;
	t_hmd2eye[Side_Right][3][1] = this->eye[Side_Right].offset.Position.y;
	t_hmd2eye[Side_Right][3][2] = this->eye[Side_Right].offset.Position.z;
	t_hmd2eye[Side_Right][3][3] = 1;

	// FloorLevel will give tracking poses where the floor height is 0
	ovr_SetTrackingOriginType(this->hmd, ovrTrackingOrigin_FloorLevel);

	// Turn off vsync to let the compositor do its magic
	wglSwapIntervalEXT(0);
	// Create required structured for texture blitting
	this->gl.program = glCreateProgram();
	this->gl.vshader = glCreateShader(GL_VERTEX_SHADER);
	this->gl.fshader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(this->gl.vshader, 1, &GL::vshader_source, 0);
	glShaderSource(this->gl.fshader, 1, &GL::fshader_source, 0);

	GLint ret;

	glCompileShader(this->gl.vshader);
	glGetShaderiv(this->gl.vshader, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		GLchar error[256];
		GLsizei len;
		glGetShaderInfoLog(this->gl.vshader, 256, &len, error);
		error[255] = 0;
	}
	glAttachShader(this->gl.program, this->gl.vshader);

	glCompileShader(this->gl.fshader);
	glGetShaderiv(this->gl.fshader, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		GLchar error[256];
		GLsizei len;
		glGetShaderInfoLog(this->gl.fshader, 256, &len, error);
		error[255] = 0;
	}
	glAttachShader(this->gl.program, this->gl.fshader);

	glLinkProgram(this->gl.program);
	glGetProgramiv(this->gl.program, GL_LINK_STATUS, &ret);
	if (!ret) {
		GLchar error[256];
		GLsizei len;
		glGetProgramInfoLog(this->gl.program, 256, &len, error);
		error[255] = 0;
	}

	this->gl.position_location = glGetAttribLocation(this->gl.program, "position");
	this->gl.uv_location = glGetAttribLocation(this->gl.program, "uv");
	this->gl.sampler_location = glGetUniformLocation(this->gl.program, "tex");
	glUniform1i(this->gl.sampler_location, 0);
	this->gl.param_location = glGetUniformLocation(this->gl.program, "param");

	// Create vertex array
	glGenVertexArrays(1, &this->gl.vertex_array);
	glBindVertexArray(this->gl.vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, this->gl.verts);
	glVertexAttribPointer(this->gl.position_location, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
	glBindBuffer(GL_ARRAY_BUFFER, this->gl.uvs);
	glVertexAttribPointer(this->gl.uv_location, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	this->initialized = true;

	return Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/  uninit()
/**
 * Un-initialize the module.
 */
int VR_Oculus::uninit()
{
	OVR::GLEContext::SetCurrentContext(&gle_context);

	// Delete eye render buffers
	for (int i = 0; i < Sides; ++i) {
		if (this->eye[i].swap_texture_set) {
			ovr_DestroyTextureSwapChain(this->hmd, this->eye[i].swap_texture_set);
			this->eye[i].swap_texture_set = 0;
		}
		if (this->eye[i].framebuffer) {
			glDeleteFramebuffers(1, &this->eye[i].framebuffer);
			this->eye[i].framebuffer = 0;
		}
	}

	if (this->gl.program) {
		glDeleteProgram(this->gl.program);
		this->gl.program = 0;
	}
	if (this->gl.vshader) {
		glDeleteShader(this->gl.vshader);
		this->gl.vshader = 0;
	}
	if (this->gl.fshader) {
		glDeleteShader(this->gl.fshader);
		this->gl.fshader = 0;
	}
	if (gle_context.IsInitialized()) {
		gle_context.Shutdown();
	}

	wglMakeCurrent(this->gl.device, this->gl.context);

	if (this->hmd) {
		this->releaseHMD();
	}

	this->initialized = false;
	return Error_None;
}

//                                                                      ____________________________
//_____________________________________________________________________/ transferHMDTranformation()
/**
 * Helper function to transform Oculus transformations into BlenderXR transformation matrices.
 * \param   pos     Pose / tracking information.
 * \param   m       [OUT] Transformation matrix (both translation and rotation).
 */
static void transferHMDTranformation(const ovrPosef& pos, float m[4][4])
{
	const ovrVector3f& p = pos.Position;
	const ovrQuatf& q = pos.Orientation;
	m[0][0] = 1 - 2 * q.y*q.y - 2 * q.z*q.z;
	m[1][0] = 2 * q.x*q.y - 2 * q.z*q.w;
	m[2][0] = 2 * q.x*q.z + 2 * q.y*q.w;
	m[3][0] = p.x;
	m[0][1] = -(2 * q.x*q.z - 2 * q.y*q.w);
	m[1][1] = -(2 * q.y*q.z + 2 * q.x*q.w);
	m[2][1] = -(1 - 2 * q.x*q.x - 2 * q.y*q.y);
	m[3][1] = -p.z;
	m[0][2] = 2 * q.x*q.y + 2 * q.z*q.w;
	m[1][2] = 1 - 2 * q.x*q.x - 2 * q.z*q.z;
	m[2][2] = 2 * q.y*q.z - 2 * q.x*q.w;
	m[3][2] = p.y;
	m[0][3] = 0;
	m[1][3] = 0;
	m[2][3] = 0;
	m[3][3] = 1;
}

//                                                                      ____________________________
//_____________________________________________________________________/ transferHMDTranformation()
/**
 * Helper function to transform Oculus transformations into BlenderXR transformation matrices.
 * \param   pos     Pose / tracking information.
 * \param   m       [OUT] Transformation matrix (both translation and rotation).
 */
static void transferControllerTranformation(const ovrPosef& pos, float m[4][4])
{
	const ovrVector3f& p = pos.Position;
	const ovrQuatf& q = pos.Orientation;
	// x-axis
	m[0][0] = 1 - 2 * q.y*q.y - 2 * q.z*q.z;
	m[0][1] = -(2 * q.x*q.z - 2 * q.y*q.w);
	m[0][2] = 2 * q.x*q.y + 2 * q.z*q.w;
	// y-axis
	m[1][0] = -(2 * q.x*q.z + 2 * q.y*q.w);
	m[1][1] = (1 - 2 * q.x*q.x - 2 * q.y*q.y);
	m[1][2] = -(2 * q.y*q.z - 2 * q.x*q.w);
	// z-axis
	m[2][0] = 2 * q.x*q.y - 2 * q.z*q.w;
	m[2][1] = -(2 * q.y*q.z + 2 * q.x*q.w);
	m[2][2] = 1 - 2 * q.x*q.x - 2 * q.z*q.z;
	// translation (moved ahead 50mm)
	m[3][0] = p.x + (0.05f * m[1][0]);
	m[3][1] = -p.z + (0.05f * m[1][1]);
	m[3][2] = p.y + (0.05f * m[1][2]);
	m[0][3] = 0;
	m[1][3] = 0;
	m[2][3] = 0;
	m[3][3] = 1;
}

//                                                                          ________________________
//_________________________________________________________________________/  updateTracking()
/**
 * Update the t_eye positions based on latest tracking data.
 * \return      Zero on success, an error code on failure.
 */
int VR_Oculus::updateTracking()
{
	if (!this->initialized) {
		return VR_Oculus::Error_NotInitialized;
	}
	float ftiming = ovr_GetPredictedDisplayTime(this->hmd, 0);
	ovrTrackingState tracking_state = ovr_GetTrackingState(this->hmd, ftiming, ovrTrue);
	ovrPosef offset[2];
	offset[ovrEye_Left] = this->eye[Side_Left].offset;
	offset[ovrEye_Right] = this->eye[Side_Right].offset;
	ovrPosef pose[2];
	ovr_GetEyePoses(this->hmd, this->frame_index, ovrTrue, offset, pose, &this->sensor_sample_time);
	this->eye[Side_Left].pose = pose[ovrEye_Left];
	this->eye[Side_Right].pose = pose[ovrEye_Right];

	// Save the HMD position as matrices
	transferHMDTranformation(tracking_state.HeadPose.ThePose, this->t_hmd);
	transferHMDTranformation(this->eye[Side_Left].pose, this->t_eye[Side_Left]);
	transferHMDTranformation(this->eye[Side_Right].pose, this->t_eye[Side_Right]);

	if (tracking_state.HandStatusFlags[ovrHand_Left] & ovrStatus_PositionTracked) {
		this->controller[Side_Left].available = true;
		transferControllerTranformation(tracking_state.HandPoses[ovrHand_Left].ThePose, this->t_controller[Side_Left]);
		ovrInputState input_state;
		ovr_GetInputState(this->hmd, ovrControllerType_LTouch, &input_state);

		ui64& btn_press = this->controller[Side_Left].buttons;
		ui64& btn_touch = this->controller[Side_Left].buttons_touched;

		// Convert Oculus button bits to Widget_Layout button bits.
		btn_press = btn_touch = 0;
		if (input_state.Buttons & ovrButton_X) {
			btn_press |= VR_OCULUS_BTNBIT_X;
		}
		if (input_state.Buttons & ovrButton_Y) {
			btn_press |= VR_OCULUS_BTNBIT_Y;
		}
		if (input_state.Buttons & ovrButton_Enter) {
			btn_press |= VR_OCULUS_BTNBIT_E;
		}
		if (input_state.Buttons & ovrButton_LThumb) {
			btn_press |= VR_OCULUS_BTNBIT_LEFTSTICK;
		}
		btn_touch = btn_press;

		// Special treatment for the stick
		const ovrVector2f& t = input_state.Thumbstick[ovrHand_Left];
		if (t.x != 0 || t.y != 0) {
			memcpy(this->controller[Side_Left].stick, &t, sizeof(ovrVector2f));
		}
		if (std::abs(t.x) > std::abs(t.y)) { // LEFT or RIGHT
			if (t.x > VR_OCULUS_TOUCHTHRESHOLD_STICKDIRECTION) { // RIGHT
				btn_touch |= VR_OCULUS_BTNBIT_STICKRIGHT;
				if (t.x > VR_OCULUS_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
					btn_press |= VR_OCULUS_BTNBIT_STICKRIGHT;
				}
			}
			else if (t.x < -VR_OCULUS_TOUCHTHRESHOLD_STICKDIRECTION) { // LEFT
				btn_touch |= VR_OCULUS_BTNBIT_STICKLEFT;
				if (t.x < -VR_OCULUS_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
					btn_press |= VR_OCULUS_BTNBIT_STICKLEFT;
				}
			} // else: center
		}
		else { // UP or DOWN
			if (t.y > VR_OCULUS_TOUCHTHRESHOLD_STICKDIRECTION*0.7f) { // UP (reduced threshold, because it's hard to hit)
				btn_touch |= VR_OCULUS_BTNBIT_STICKUP;
				if (t.y > VR_OCULUS_PRESSTHRESHOLD_STICKDIRECTION*0.7f) { // "PRESS"
					btn_press |= VR_OCULUS_BTNBIT_STICKUP;
				}
			}
			else if (t.y < -VR_OCULUS_TOUCHTHRESHOLD_STICKDIRECTION) { // DOWN
				btn_touch |= VR_OCULUS_BTNBIT_STICKDOWN;
				if (t.y < -VR_OCULUS_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
					btn_press |= VR_OCULUS_BTNBIT_STICKDOWN;
				}
			} // else: center
		}
		this->controller[Side_Left].trigger_pressure = 0.0f;
		if (input_state.IndexTrigger[ovrHand_Left] > VR_OCULUS_TOUCHTHRESHOLD_INDEXTRIGGER) {
			btn_touch |= VR_OCULUS_BTNBIT_LEFTTRIGGER;
			if (input_state.IndexTrigger[ovrHand_Left] > VR_OCULUS_PRESSTHRESHOLD_INDEXTRIGGER) {
				btn_press |= VR_OCULUS_BTNBIT_LEFTTRIGGER;
				this->controller[Side_Left].trigger_pressure = (input_state.IndexTrigger[ovrHand_Left] - VR_OCULUS_PRESSTHRESHOLD_INDEXTRIGGER) / (1.0 - VR_OCULUS_PRESSTHRESHOLD_INDEXTRIGGER);
			}
		}
		this->controller[Side_Left].grip_pressure = 0.0f;
		if (input_state.HandTrigger[ovrHand_Left] > VR_OCULUS_TOUCHTHRESHOLD_SHOULDERGRIP) {
			btn_touch |= VR_OCULUS_BTNBIT_LEFTGRIP;
			if (input_state.HandTrigger[ovrHand_Left] > VR_OCULUS_PRESSTHRESHOLD_SHOULDERGRIP) {
				btn_press |= VR_OCULUS_BTNBIT_LEFTGRIP;
				this->controller[Side_Left].grip_pressure = (input_state.HandTrigger[ovrHand_Left] - VR_OCULUS_PRESSTHRESHOLD_SHOULDERGRIP) / (1.0 - VR_OCULUS_PRESSTHRESHOLD_SHOULDERGRIP);
			}
		}
		// add touch information
		if (input_state.Touches & ovrTouch_X) {
			btn_touch |= VR_OCULUS_BTNBIT_X;
		}
		if (input_state.Touches & ovrTouch_Y) {
			btn_touch |= VR_OCULUS_BTNBIT_Y;
		}
		if (input_state.Touches & ovrTouch_LThumb) {
			btn_touch |= VR_OCULUS_BTNBIT_LEFTSTICK;
		}
		if (input_state.Touches & ovrTouch_LThumbRest) {
			btn_touch |= VR_OCULUS_BTNBIT_LEFTTHUMBREST;
			btn_press |= VR_OCULUS_BTNBIT_LEFTTHUMBREST;
		}
	}
	else {
		this->controller[Side_Left].available = false;
	}

	if (tracking_state.HandStatusFlags[ovrHand_Right] & ovrStatus_PositionTracked) {
		this->controller[Side_Right].available = true;
		transferControllerTranformation(tracking_state.HandPoses[ovrHand_Right].ThePose, this->t_controller[Side_Right]);
		ovrInputState input_state;
		ovr_GetInputState(this->hmd, ovrControllerType_RTouch, &input_state);

		ui64& btn_press = this->controller[Side_Right].buttons;
		ui64& btn_touch = this->controller[Side_Right].buttons_touched;

		// Convert Oculus button bits to Widget_Layout button bits.
		btn_press = btn_touch = 0;
		if (input_state.Buttons & ovrButton_A) {
			btn_press |= VR_OCULUS_BTNBIT_A;
		}
		if (input_state.Buttons & ovrButton_B) {
			btn_press |= VR_OCULUS_BTNBIT_B;
		}
		if (input_state.Buttons & ovrButton_RThumb) {
			btn_press |= VR_OCULUS_BTNBIT_RIGHTSTICK;
		}
		btn_touch = btn_press;

		const ovrVector2f& t = input_state.Thumbstick[ovrHand_Right];
		if (t.x != 0 || t.y != 0) {
			memcpy(this->controller[Side_Right].stick, &t, sizeof(ovrVector2f));
		}
		if (std::abs(t.x) > std::abs(t.y)) { // LEFT or RIGHT
			if (t.x > VR_OCULUS_TOUCHTHRESHOLD_STICKDIRECTION) { // RIGHT
				btn_touch |= VR_OCULUS_BTNBIT_STICKRIGHT;
				if (t.x > VR_OCULUS_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
					btn_press |= VR_OCULUS_BTNBIT_STICKRIGHT;
				}
			}
			else if (t.x < -VR_OCULUS_TOUCHTHRESHOLD_STICKDIRECTION) { // LEFT
				btn_touch |= VR_OCULUS_BTNBIT_STICKLEFT;
				if (t.x < -VR_OCULUS_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
					btn_press |= VR_OCULUS_BTNBIT_STICKLEFT;
				}
			} // else: center
		}
		else { // UP or DOWN
			if (t.y > VR_OCULUS_TOUCHTHRESHOLD_STICKDIRECTION*0.7f) { // UP (reduced threshold, because it's hard to hit)
				btn_touch |= VR_OCULUS_BTNBIT_STICKUP;
				if (t.y > VR_OCULUS_PRESSTHRESHOLD_STICKDIRECTION*0.7f) { // "PRESS"
					btn_press |= VR_OCULUS_BTNBIT_STICKUP;
				}
			}
			else if (t.y < -VR_OCULUS_TOUCHTHRESHOLD_STICKDIRECTION) { // DOWN
				btn_touch |= VR_OCULUS_BTNBIT_STICKDOWN;
				if (t.y < -VR_OCULUS_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
					btn_press |= VR_OCULUS_BTNBIT_STICKDOWN;
				}
			} // else: center
		}
		this->controller[Side_Right].trigger_pressure = 0.0f;
		if (input_state.IndexTrigger[ovrHand_Right] > VR_OCULUS_TOUCHTHRESHOLD_INDEXTRIGGER) {
			btn_touch |= VR_OCULUS_BTNBIT_RIGHTTRIGGER;
			if (input_state.IndexTrigger[ovrHand_Right] > VR_OCULUS_PRESSTHRESHOLD_INDEXTRIGGER) {
				btn_press |= VR_OCULUS_BTNBIT_RIGHTTRIGGER;
				this->controller[Side_Right].trigger_pressure = (input_state.IndexTrigger[ovrHand_Right] - VR_OCULUS_PRESSTHRESHOLD_INDEXTRIGGER) / (1.0 - VR_OCULUS_PRESSTHRESHOLD_INDEXTRIGGER);
			}
		}
		this->controller[Side_Right].grip_pressure = 0.0f;
		if (input_state.HandTrigger[ovrHand_Right] > VR_OCULUS_TOUCHTHRESHOLD_SHOULDERGRIP) {
			btn_touch |= VR_OCULUS_BTNBIT_RIGHTGRIP;
			if (input_state.HandTrigger[ovrHand_Right] > VR_OCULUS_PRESSTHRESHOLD_SHOULDERGRIP) {
				btn_press |= VR_OCULUS_BTNBIT_RIGHTGRIP;
				this->controller[Side_Right].grip_pressure = (input_state.HandTrigger[ovrHand_Right] - VR_OCULUS_PRESSTHRESHOLD_SHOULDERGRIP) / (1.0 - VR_OCULUS_PRESSTHRESHOLD_SHOULDERGRIP);
			}
		}
		// add touch information
		if (input_state.Touches & ovrTouch_A) {
			btn_touch |= VR_OCULUS_BTNBIT_A;
		}
		if (input_state.Touches & ovrTouch_B) {
			btn_touch |= VR_OCULUS_BTNBIT_B;
		}
		if (input_state.Touches & ovrTouch_RThumb) {
			btn_touch |= VR_OCULUS_BTNBIT_RIGHTSTICK;
		}
		if (input_state.Touches & ovrTouch_RThumbRest) {
			btn_touch |= VR_OCULUS_BTNBIT_RIGHTTHUMBREST;
			btn_press |= VR_OCULUS_BTNBIT_RIGHTTHUMBREST;
		}
	}
	else {
		this->controller[Side_Right].available = false;
	}

	this->tracking = true;
	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/     blitEye()
/**
 * Blit a rendered image into the internal eye texture.
 * TODO_MARUI: aperture_u and aperture_v currently don't do anything in the shader.
 */
int VR_Oculus::blitEye(Side side, void* texture_resource, const float& aperture_u, const float& aperture_v)
{
	if (!this->initialized) {
		return VR_Oculus::Error_NotInitialized;
	}

	// Increment to use next texture, just before writing
	int current_index;
	ovr_GetTextureSwapChainCurrentIndex(this->hmd, eye[side].swap_texture_set, &current_index);

	uint texture_id = *((uint*)texture_resource);
	
	// 2: Save previous OpenGL state
	GLint prior_framebuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prior_framebuffer);
	GLint prior_program;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prior_program);
	GLboolean prior_backface_culling = glIsEnabled(GL_CULL_FACE);
	GLboolean prior_blend_enabled = glIsEnabled(GL_BLEND);
	GLboolean prior_depth_test = glIsEnabled(GL_DEPTH_TEST);
	GLboolean prior_texture_enabled = glIsEnabled(GL_TEXTURE_2D);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);

	glUseProgram(this->gl.program);

	// Bind the eye as render target eye render target
	unsigned int tex_id;
	ovr_GetTextureSwapChainBufferGL(this->hmd, eye[side].swap_texture_set, current_index, &tex_id);
	glBindFramebuffer(GL_FRAMEBUFFER, this->eye[side].framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0);
	glViewport(0, 0, this->eye[side].texsize.w, this->eye[side].texsize.h);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glUniform4f(this->gl.param_location, aperture_u, aperture_v, 1.0 / this->gamma, 0);

	// Render the provided texture into the Oculus eye texture.
	glBindTexture(GL_TEXTURE_2D, texture_id);

	glBindVertexArray(this->gl.vertex_array);
	glEnableVertexAttribArray(this->gl.position_location);
	glEnableVertexAttribArray(this->gl.uv_location);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(this->gl.position_location);
	glDisableVertexAttribArray(this->gl.uv_location);

	// Restore previous OpenGL state
	glUseProgram(prior_program);
	prior_backface_culling ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
	prior_blend_enabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
	prior_depth_test ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
	prior_texture_enabled ? glEnable(GL_TEXTURE_2D) : glDisable(GL_TEXTURE_2D);
	glBindFramebuffer(GL_FRAMEBUFFER, prior_framebuffer);

	ovrResult result = ovr_CommitTextureSwapChain(this->hmd, this->eye[side].swap_texture_set);

	return Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/     blitEyes()
/**
 * Blit rendered images into the internal eye textures.
 * TODO_MARUI: aperture_u and aperture_v currently don't do anything in the shader.
 */
int VR_Oculus::blitEyes(void* texture_resource_left, void* texture_resource_right, const float& aperture_u, const float& aperture_v)
{
	if (!this->initialized) {
		return VR_Oculus::Error_NotInitialized;
	}

	uint texture_id_left = *((uint*)texture_resource_left);
	uint texture_id_right = *((uint*)texture_resource_right);

	// Save previous OpenGL state
	GLint prior_framebuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prior_framebuffer);
	GLint prior_program;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prior_program);
	GLboolean prior_backface_culling = glIsEnabled(GL_CULL_FACE);
	GLboolean prior_blend_enabled = glIsEnabled(GL_BLEND);
	GLboolean prior_depth_test = glIsEnabled(GL_DEPTH_TEST);
	GLboolean prior_texture_enabled = glIsEnabled(GL_TEXTURE_2D);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);

	glUseProgram(this->gl.program);

	// Load shader params
	glUniform4f(this->gl.param_location, aperture_u, aperture_v, 1.0 / this->gamma, 0);
	glBindVertexArray(this->gl.vertex_array);
	glEnableVertexAttribArray(this->gl.position_location);
	glEnableVertexAttribArray(this->gl.uv_location);

	// Increment to use next texture, just before writing
	int current_index;
	for (int i = 0; i < 2; ++i) {
		ovr_GetTextureSwapChainCurrentIndex(this->hmd, eye[i].swap_texture_set, &current_index);
		
		// Bind the eye as render target eye render target
		unsigned int tex_id;
		ovr_GetTextureSwapChainBufferGL(this->hmd, eye[i].swap_texture_set, current_index, &tex_id);
		glBindFramebuffer(GL_FRAMEBUFFER, this->eye[i].framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0);
		glViewport(0, 0, this->eye[i].texsize.w, this->eye[i].texsize.h);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);

		// Render the provided texture into the Oculus eye texture.
		if (i == Side_Left) {
			glBindTexture(GL_TEXTURE_2D, texture_id_left);
		}
		else {
			glBindTexture(GL_TEXTURE_2D, texture_id_right);
		}

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		ovrResult result = ovr_CommitTextureSwapChain(this->hmd, this->eye[i].swap_texture_set);
	}

	glDisableVertexAttribArray(this->gl.position_location);
	glDisableVertexAttribArray(this->gl.uv_location);

	// Restore previous OpenGL state
	glUseProgram(prior_program);
	prior_backface_culling ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
	prior_blend_enabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
	prior_depth_test ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
	prior_texture_enabled ? glEnable(GL_TEXTURE_2D) : glDisable(GL_TEXTURE_2D);
	glBindFramebuffer(GL_FRAMEBUFFER, prior_framebuffer);

	return Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/        frame()
/**
 * Submit frame to oculus.
 */
int VR_Oculus::submitFrame()
{
	ovrResult result;

	if (!VR_Oculus::initialized) {
		return VR::Error_NotInitialized;
	}

	ovrLayerEyeFov ld;
	ld.Header.Type = ovrLayerType_EyeFov;
	ld.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;

	for (int side = 0; side < Sides; ++side) {
		ld.ColorTexture[side] = this->eye[side].swap_texture_set;
		ovrTextureSwapChainDesc desc;
		ovr_GetTextureSwapChainDesc(this->hmd, this->eye[side].swap_texture_set, &desc);
		ld.Viewport[side] = OVR::Recti(0, 0, desc.Width, desc.Height);
		ld.Fov[side] = this->hmd_desc.DefaultEyeFov[side];
		ld.RenderPose[side] = this->eye[side].pose;
		ld.SensorSampleTime = this->sensor_sample_time;
	}

	ovrLayerHeader* layers = &ld.Header;
	result = ovr_SubmitFrame(this->hmd, frame_index++, nullptr, &layers, 1);

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/    setEyeOffset
/**
 * Override the offset of the eyes (camera positions) relative to the HMD.
 */
int VR_Oculus::setEyeOffset(Side side, float x, float y, float z)
{
	if (side != Side_Left && side != Side_Right) {
		return VR_Oculus::Error_InvalidParameter;
	}
	this->eye[side].offset.Position.x = x;
	this->eye[side].offset.Position.y = y;
	this->eye[side].offset.Position.z = z;

	t_hmd2eye[side][0][0] = 1;   t_hmd2eye[side][0][1] = 0;   t_hmd2eye[side][0][2] = 0;   t_hmd2eye[side][0][3] = 0;
	t_hmd2eye[side][1][0] = 0;   t_hmd2eye[side][1][1] = 1;   t_hmd2eye[side][1][2] = 0;   t_hmd2eye[side][1][3] = 0;
	t_hmd2eye[side][2][0] = 0;   t_hmd2eye[side][2][1] = 0;   t_hmd2eye[side][2][2] = 1;   t_hmd2eye[side][2][3] = 0;
	t_hmd2eye[side][3][0] = x;   t_hmd2eye[side][3][1] = y;   t_hmd2eye[side][3][2] = z;   t_hmd2eye[side][3][3] = 1;

	return VR_Oculus::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/  getDefaultEyeTexSize
/**
 * Get the default eye texture size.
 */
int VR_Oculus::getDefaultEyeTexSize(uint& w, uint& h, Side side)
{
	if (!this->hmd) {
		int e = this->acquireHMD();
		if (e || !this->hmd) {
			return VR::Error_InternalFailure;
		}
	}
	w = 0;
	h = 0;
	if (side == Side_Left || side == Side_Both) {
		ovrSizei s = ovr_GetFovTextureSize(this->hmd, ovrEye_Left, this->hmd_desc.DefaultEyeFov[ovrEye_Left], 1);
		if ((int)w < s.w) w = (uint)s.w;
		if ((int)h < s.h) h = (uint)s.h;
	}
	if (side == Side_Right || side == Side_Both) {
		ovrSizei s = ovr_GetFovTextureSize(this->hmd, ovrEye_Right, this->hmd_desc.DefaultEyeFov[ovrEye_Right], 1);
		if ((int)w < s.w) w = (uint)s.w;
		if ((int)h < s.h) h = (uint)s.h;
	}
	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/  getDefaultEyeParams
/**
 * Get the HMD's default parameters.
 */
int VR_Oculus::getDefaultEyeParams(Side side, float& fx, float& fy, float& cx, float& cy)
{
	if (!this->hmd) {
		int e = this->acquireHMD();
		if (e || !this->hmd) {
			return VR::Error_InternalFailure;
		}
	}

	ovrFovPort fov = (side == Side_Left)
		? this->hmd_desc.DefaultEyeFov[ovrEye_Left]
		: this->hmd_desc.DefaultEyeFov[ovrEye_Right];

	// fov.UpTan   =     cy    / fy;
	// fov.DownTan = (1.0f-cy) / fy;
	// fov.LeftTan =     cx    / fx;
	// fov.RightTan= (1.0f-cx) / fx;

	// => fy == cy / fov.UpTan == (1-cy) / fov.DownTan
	// => (1-cy) / cy  ==  fov.DownTan / fov.UpTan  ==  1/cy - cy/cy  ==  1/cy - 1 
	// => 1/cy = (fov.DownTan / fov.UpTan) + 1
	cy = 1.0f / ((fov.DownTan / fov.UpTan) + 1.0f);
	fy = cy / fov.UpTan;

	// => fx  ==  cx / fov.LeftTan  ==  (1-cx) / fov.RightTan
	// => (1-cx) / cx  ==  fov.RightTan / fov.LeftTan  ==  1/cx - cx/cx  ==  1/cx - 1
	// => 1/cx  ==  (fov.RightTan / fov.LeftTan) + 1
	cx = 1.0f / ((fov.RightTan / fov.LeftTan) + 1.0f);
	fx = cx / fov.LeftTan;

	return VR_Oculus::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/    setEyeParams()
/**
 * Set the HMD's projection parameters.
 * For correct distortion rendering and possibly other internal things,
 * the HMD might need to know these.
 *
 * \todo   setEyeParams() currently only has effect when called before rendering is intialized.
 *         Need a work-around to update params after rendering has already started.
 *
 * \param   side    Side of the eye which to set.ccess
 * \param   fx      Horizontal focal length, in "image-width"-units (1=image width).
 * \param   fy      Vertical focal length, in "image-height"-units (1=image height).
 * \param   cx      Horizontal principal point, in "image-width"-units (0.5=image center).
 * \param   cy      Vertical principal point, in "image-height"-units (0.5=image center).
 * \return          Zero on success, an error code on failure.
 */
int VR_Oculus::setEyeParams(Side side, float fx, float fy, float cx, float cy)
{
	if (side != Side_Left && side != Side_Right) {
		return VR::Error_InvalidParameter;
	}
	this->eye[side].fx = fx;
	this->eye[side].fy = fy;
	this->eye[side].cx = cx;
	this->eye[side].cy = cy;
	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/  getTrackerPosition()
/**
 * Get the position of a tracking camera / device (if available).
 * \i           Index of the camera tracking device to query.
 * \t           [OUT] Matrix to receive the transformation.
 * \return      Zero on success, an error code on failure.
 */
int VR_Oculus::getTrackerPosition(uint i, float t[4][4]) const
{
	if (!this->hmd) {
		return VR::Error_NotInitialized;
	}
	if (i >= ovr_GetTrackerCount(this->hmd)) {
		return VR::Error_InvalidParameter;
	}
	ovrTrackerPose p = ovr_GetTrackerPose(this->hmd, i);
	transferHMDTranformation(p.Pose, t);
	return VR::Error_None;
}

/***********************************************************************************************//**
*								Exported shared library functions.								   *
***************************************************************************************************/
VR_Oculus* c_obj(0);

/**
 * Create an object internally. Must be called before the functions below.
 */
int c_createVR()
{
	c_obj = new VR_Oculus();
	return 0;
}

/**
 * Initialize the internal object (OpenGL).
 */
int c_initVR(void* device, void* context)
{
	return c_obj->init(device, context);
}

/**
 * Get the type of HMD used for VR.
 */
int c_getHMDType(int* type)
{
	*type = c_obj->hmdType();
	return 0;
}

/**
 * Get the default eye texture size.
 * \param side      Zero for left, one for right, -1 for both eyes (default).
 */
int c_getDefaultEyeTexSize(int* w, int* h, int side)
{
	return c_obj->getDefaultEyeTexSize(*(uint*)w, *(uint*)h, (VR::Side)side);
}

/**
 * Get the HMD's default parameters.
 * \param side       Zero for left, one for right.
 */
int c_getDefaultEyeParams(int side, float* fx, float* fy, float* cx, float* cy)
{
	return c_obj->getDefaultEyeParams((VR::Side)side, *fx, *fy, *cx, *cy);
}

/**
 * Set rendering parameters.
 * \param side      Zero for left, one for right.
 */
int c_setEyeParams(int side, float fx, float fy, float cx, float cy)
{
	return c_obj->setEyeParams((VR::Side)side, fx, fy, cx, cy);
}

/**
 * Update the t_eye positions based on latest tracking data.
 */
int c_updateTrackingVR()
{
	return c_obj->updateTracking();
}

/**
 * Last tracked position of the eyes.
 */
int c_getEyePositions(float t_eye[VR::Sides][4][4])
{
	memcpy(t_eye, c_obj->t_eye, sizeof(float) * VR::Sides * 4 * 4);
	return 0;
}

/**
 *  Last tracked position of the HMD.
 */
int c_getHMDPosition(float t_hmd[4][4])
{
	memcpy(t_hmd, c_obj->t_hmd, sizeof(float) * 4 * 4);
	return 0;
}

/**
 * Last tracked position of the controllers.
 */
int c_getControllerPositions(float t_controller[VR_MAX_CONTROLLERS][4][4])
{
	for (int i = 0; i < VR_MAX_CONTROLLERS; ++i) {
		if (c_obj->controller[i].available) {
			memcpy(t_controller[i], c_obj->t_controller[i], sizeof(float) * 4 * 4);
		}
	}
	return 0;
}

/**
 * Last tracked button state of the controller.
 */
int c_getControllerStates(void* controller_states[VR_MAX_CONTROLLERS])
{
	for (int i = 0; i < VR_MAX_CONTROLLERS; ++i) {
		if (c_obj->controller[i].available) {
			memcpy(controller_states[i], &c_obj->controller[i], sizeof(VR::Controller));
		}
		else {
			// Just copy side and availability information.
			memcpy(controller_states[i], &c_obj->controller[i], sizeof(VR::Side) + sizeof(bool));
		}
	}

	return 0;
}

/**
 * Blit a rendered image into the internal eye texture.
 * \param side      Zero for left, one for right.
 */
int c_blitEye(int side, void* texture_resource, const float* aperture_u, const float* aperture_v)
{
	return c_obj->blitEye((VR::Side)side, texture_resource, *aperture_u, *aperture_v);
}

/**
 * Blit rendered images into the internal eye textures.
 */
int c_blitEyes(void* texture_resource_left, void* texture_resource_right, const float* aperture_u, const float* aperture_v)
{
	return c_obj->blitEyes(texture_resource_left, texture_resource_right, *aperture_u, *aperture_v);
}

/**
 * Submit frame to the HMD.
 */
int c_submitFrame()
{
	return c_obj->submitFrame();
}

/**
 * Un-initialize the internal object.
 */
int c_uninitVR()
{
	return c_obj->uninit();
}
