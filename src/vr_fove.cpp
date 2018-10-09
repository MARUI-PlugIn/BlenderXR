/***********************************************************************************************//**
* \file      vr_fove.cpp
*            Fove HMD module.
*            This file contains code related to using Fove HMDs.
*            Both tracking and rendering are implemented.
***************************************************************************************************
* \brief     Fove HMD module.
* \copyright MARUI-PlugIn (inc.)
**************************************************************************************************/

#define GLEW_STATIC
#include <glew.h>

#include <Windows.h> // For mouse input

#include "vr_fove.h"

#include <chrono>
#include <xmmintrin.h>

/***********************************************************************************************//**
 * \class                                  VR_Fove
 ***************************************************************************************************
 * Fove HMD module for tracking and rendering.
 * NOT THREAD-SAFE!
 **************************************************************************************************/

 //																		____________________________
 //_____________________________________________________________________/   GL::vshader_source
 /**
 * Primitive pass-through vertex shader source code.
 */
const char* const VR_Fove::GL::vshader_source(STRING(#version 120\n
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
const char* const VR_Fove::GL::fshader_source(STRING(#version 120\n
	varying vec2 texcoord;
	uniform sampler2D tex;
	uniform vec4 param;
void main()
{
	gl_FragColor = pow(texture2D(tex, texcoord), param.zzzz); // GAMMA CORRECTION (1/gamma should be passed as param.z)
}
));

//                                                                          ________________________
//_________________________________________________________________________/       VR_Fove()
/**
 * Class constructor
 */
VR_Fove::VR_Fove()
	: VR()
	, hmd(0)
	, hmd_type(HMDType_Fove)
	, compositor(0)
	, compositor_layer(Fove::SFVR_CompositorLayer())
	, compositor_create_info(Fove::SFVR_CompositorLayerCreateInfo())
	, compositor_submit_info(Fove::SFVR_CompositorLayerSubmitInfo())
	, initialized(false)
	, eye_tracking_enabled(true)
{
	this->eye[0] = Eye();
	this->eye[1] = Eye();
	memset(&this->gl, 0, sizeof(GL));
}

//                                                                          ________________________
//_________________________________________________________________________/     ~VR_Fove()
/**
 * Class destructor.
 */
VR_Fove::~VR_Fove()
{
	if (this->initialized) {
		this->uninit();
	}
}

//                                                                          ________________________
//_________________________________________________________________________/		type()
/**
 * Get which API was used in this implementation.
 */
VR::Type VR_Fove::type()
{
	return VR::Type_Fove;
};

//                                                                          ________________________
//_________________________________________________________________________/		hmdType()
/**
 * Get which HMD was used in this implementation.
 */
VR::HMDType VR_Fove::hmdType()
{
	return this->hmd_type;
};

//                                                                          ________________________
//_________________________________________________________________________/     GL::create()
/**
* Create required OpenGL objects.
*/
bool VR_Fove::GL::create(uint width, uint height)
{
	bool success = true; // assume success

	// Create texture targets / frame buffers
	for (int i = 0; i < Sides; ++i) {
		glGenFramebuffers(1, &this->framebuffer[i]);
		glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer[i]);

		glGenTextures(1, &this->texture[i]);
		glBindTexture(GL_TEXTURE_2D, this->texture[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->texture[i], 0);

		// check FBO status
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			success = false;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// Create vertex buffer
	static const GLfloat vertex_data[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		-1.0f,  1.0f,
		 1.0f,  1.0f,
	};
	glGenBuffers(1, &this->verts);
	glBindBuffer(GL_ARRAY_BUFFER, this->verts);
	glBufferData(GL_ARRAY_BUFFER, 2 * 4 * sizeof(float), vertex_data, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Create uv buffer
	static const GLfloat uv_data[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,
	};
	glGenBuffers(1, &this->uvs);
	glBindBuffer(GL_ARRAY_BUFFER, this->uvs);
	glBufferData(GL_ARRAY_BUFFER, 2 * 4 * sizeof(float), uv_data, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Create shaders required for texture blitting
	this->program = glCreateProgram();
	this->vshader = glCreateShader(GL_VERTEX_SHADER);
	this->fshader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(this->vshader, 1, &GL::vshader_source, 0);
	glShaderSource(this->fshader, 1, &GL::fshader_source, 0);

	GLint ret;

	glCompileShader(this->vshader);
	glGetShaderiv(this->vshader, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		char error[256];
		GLsizei len;
		glGetShaderInfoLog(this->vshader, 256, &len, error);
		error[255] = 0;
		success = false;
	}
	glAttachShader(this->program, this->vshader);

	glCompileShader(this->fshader);
	glGetShaderiv(this->fshader, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		char error[256];
		GLsizei len;
		glGetShaderInfoLog(this->fshader, 256, &len, error);
		error[255] = 0;
		success = false;
	}
	glAttachShader(this->program, this->fshader);

	glLinkProgram(this->program);
	glGetProgramiv(this->program, GL_LINK_STATUS, &ret);
	if (!ret) {
		char error[256];
		GLsizei len;
		glGetProgramInfoLog(this->program, 256, &len, error);
		error[255] = 0;
		success = false;
	}

	this->position_location = glGetAttribLocation(this->program, "position");
	this->uv_location = glGetAttribLocation(this->program, "uv");
	this->sampler_location = glGetUniformLocation(this->program, "tex");
	glUniform1i(this->sampler_location, 0);
	this->param_location = glGetUniformLocation(this->program, "param");

	// Create vertex array
	glGenVertexArrays(1, &this->vertex_array);
	glBindVertexArray(this->vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, this->verts);
	glVertexAttribPointer(this->position_location, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
	glBindBuffer(GL_ARRAY_BUFFER, this->uvs);
	glVertexAttribPointer(this->uv_location, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return success;
}

//                                                                          ________________________
//_________________________________________________________________________/   GL::release()
/**
* Release OpenGL objects.
*/
void VR_Fove::GL::release()
{
	for (int i = 0; i < Sides; ++i) {
		if (this->framebuffer[i]) {
			glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
			glDeleteFramebuffers(1, &this->framebuffer[i]);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			this->framebuffer[i] = 0;
		}
		if (this->texture) {
			glDeleteTextures(1, &this->texture[i]);
			this->texture[i] = 0;
		}
	}

	if (this->program) {
		glDeleteProgram(this->program);
		this->program = 0;
	}
	if (this->vshader) {
		glDeleteShader(this->vshader);
		this->vshader = 0;
	}
	if (this->fshader) {
		glDeleteShader(this->fshader);
		this->fshader = 0;
	}
}

//                                                                          ________________________
//_________________________________________________________________________/      acquireHMD()
/**
 * Initialize basic FoveVR operation and acquire the HMD object.
 * \return  Zero on success, and error code on failue.
 */
int VR_Fove::acquireHMD()
{
	// Connect to headset
	hmd = Fove::GetFVRHeadset();
	if (!hmd) {
		return VR::Error_InternalFailure;
	}

	Fove::EFVR_ErrorCode error = Fove::EFVR_ErrorCode::None;
	if (eye_tracking_enabled) {
		error = hmd->Initialise(Fove::EFVR_ClientCapabilities::Orientation | Fove::EFVR_ClientCapabilities::Position | Fove::EFVR_ClientCapabilities::Gaze);
	}
	else {
		// Initialize without eye tracking capabilities
		error = hmd->Initialise(Fove::EFVR_ClientCapabilities::Orientation | Fove::EFVR_ClientCapabilities::Position);
	}
	if (error != Fove::EFVR_ErrorCode::None) {
		return VR::Error_InternalFailure;
	}

	// Connect to compositor
	compositor = Fove::GetFVRCompositor();
	if (!compositor) {
		return VR::Error_InternalFailure;
	}

	// Create a compositor layer, which we will use for submission
	/*compositor_create_info.type = Fove::EFVR_ClientType::Base;
	compositor_create_info.alphaMode = Fove::EFVR_AlphaMode::Sample;
	compositor_create_info.disableDistortion = false;
	compositor_create_info.disableFading = false;
	compositor_create_info.disableTimeWarp = false;*/
	error = compositor->CreateLayer(compositor_create_info, &compositor_layer);
	if (error != Fove::EFVR_ErrorCode::None) {
		return VR::Error_InternalFailure;
	}

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/    releaseHMD()
/**
 * Delete the HMD object and uninitialize basic FoveVR operation.
 * \return  Zero on success, and error code on failue.
 */
int VR_Fove::releaseHMD()
{
	if (hmd) {
		delete hmd;
	}
	//if (compositor) {
	//	delete compositor;
	//}

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/        init()
/**
 * Initialize the module (initialize rendering)
 * \param   device          The graphics device used by Blender (HDC)
 * \param   context         The rendering context used by Blender (HGLRC)
 * \return  Zero on success, an error code on failure.
 */
int VR_Fove::init(void* device, void* context)
{
	if (this->initialized) {
		this->uninit();
	}

	// Get the Blender viewport window / context IDs.
	this->gl.device = (HDC)device;
	this->gl.context = (HGLRC)context; // save old context so that we can share
	BOOL r = wglMakeCurrent(this->gl.device, this->gl.context); // just to be sure...

	if (!hmd) {
		int e = this->acquireHMD();
		if (e || !hmd) {
			return VR::Error_InternalFailure;
		}
	}

	// Initialize glew
	glewExperimental = GL_TRUE; // This is required for the glGenFramebuffers call
	if (glewInit() != GLEW_OK) {
		return VR::Error_InternalFailure;
	}

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

	// Get eye offsets
	hmd->GetEyeToHeadMatrices(&this->eye[Side_Left].offset, &this->eye[Side_Right].offset);
	t_hmd2eye[Side_Left][0][0] = 1;   t_hmd2eye[Side_Left][0][1] = 0;   t_hmd2eye[Side_Left][0][2] = 0;   t_hmd2eye[Side_Left][0][3] = 0;
	t_hmd2eye[Side_Left][1][0] = 0;   t_hmd2eye[Side_Left][1][1] = 1;   t_hmd2eye[Side_Left][1][2] = 0;   t_hmd2eye[Side_Left][1][3] = 0;
	t_hmd2eye[Side_Left][2][0] = 0;   t_hmd2eye[Side_Left][2][1] = 0;   t_hmd2eye[Side_Left][2][2] = 1;   t_hmd2eye[Side_Left][2][3] = 0;
	t_hmd2eye[Side_Left][3][0] = this->eye[Side_Left].offset.mat[3][0];
	t_hmd2eye[Side_Left][3][1] = this->eye[Side_Left].offset.mat[3][1];
	t_hmd2eye[Side_Left][3][2] = this->eye[Side_Left].offset.mat[3][2];
	t_hmd2eye[Side_Left][3][3] = 1;
	t_hmd2eye[Side_Right][0][0] = 1;   t_hmd2eye[Side_Right][0][1] = 0;   t_hmd2eye[Side_Right][0][2] = 0;   t_hmd2eye[Side_Right][0][3] = 0;
	t_hmd2eye[Side_Right][1][0] = 0;   t_hmd2eye[Side_Right][1][1] = 1;   t_hmd2eye[Side_Right][1][2] = 0;   t_hmd2eye[Side_Right][1][3] = 0;
	t_hmd2eye[Side_Right][2][0] = 0;   t_hmd2eye[Side_Right][2][1] = 0;   t_hmd2eye[Side_Right][2][2] = 1;   t_hmd2eye[Side_Right][2][3] = 0;
	t_hmd2eye[Side_Right][3][0] = this->eye[Side_Right].offset.mat[3][0];
	t_hmd2eye[Side_Right][3][1] = this->eye[Side_Right].offset.mat[3][1];
	t_hmd2eye[Side_Right][3][2] = this->eye[Side_Right].offset.mat[3][2];
	t_hmd2eye[Side_Right][3][3] = 1;

	// Get texture resolutions and initialize rendering
	Fove::SFVR_Vec2i tex_size = compositor_layer.idealResolutionPerEye;
	texture_width = tex_size.x;
	texture_height = tex_size.y;
	this->gl.create(this->texture_width, this->texture_height);

	this->initialized = true;

	return Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/  uninit()
/**
 * Un-initialize the module (un-initialize rendering).
 */
int VR_Fove::uninit()
{
	if (!this->initialized) {
		return VR::Error_NotInitialized;
	}

	// Save current context so that we can return
	HDC   dc = wglGetCurrentDC();
	HGLRC rc = wglGetCurrentContext();
	// Switch to the context we had when initializing
	if (rc != this->gl.context) {
		wglMakeCurrent(this->gl.device, this->gl.context);
	}

	// Destroy render buffers
	this->gl.release();

	this->releaseHMD();

	// Return to previous context
	if (rc != this->gl.context) {
		wglMakeCurrent(dc, rc);
	}

	this->initialized = false;
	return Error_None;
}

//                                                                      ____________________________
//_____________________________________________________________________/ transferHMDTranformation()
/**
* Helper function to transform FoveVR transformations into BlenderXR transformation matrices.
* \param   pos     Pose / tracking information.
* \param   m       [OUT] Transformation matrix (both translation and rotation).
*/
static void transferHMDTranformation(const Fove::SFVR_Pose& pos, float m[4][4])
{
	const Fove::SFVR_Vec3& p = pos.position;
	const Fove::SFVR_Quaternion& q = pos.orientation;

	m[0][0] = 1 - 2 * q.y*q.y - 2 * q.z*q.z;
	m[1][0] = 2 * q.x*q.y - 2 * q.z*q.w;
	m[2][0] = 2 * -q.x*q.z + 2 * -q.y*q.w;
	m[3][0] = p.x;
	m[0][1] = -(2 * -q.x*q.z - 2 * -q.y*q.w);
	m[1][1] = -(2 * -q.y*q.z + 2 * -q.x*q.w);
	m[2][1] = -(1 - 2 * q.x*q.x - 2 * q.y*q.y);
	m[3][1] = p.z;
	m[0][2] = 2 * q.x*q.y + 2 * q.z*q.w;
	m[1][2] = 1 - 2 * q.x*q.x - 2 * q.z*q.z;
	m[2][2] = 2 * -q.y*q.z - 2 * -q.x*q.w;
	m[3][2] = p.y;
	m[0][3] = 0;
	m[1][3] = 0;
	m[2][3] = 0;
	m[3][3] = 1;
}

//                                                                      ____________________________
//_____________________________________________________________________/ transferHMDTranformation()
/**
* Helper function to transform FoveVR transformations into BlenderXR transformation matrices.
* \param   pos     Pose / tracking information.
* \param   m       [OUT] Transformation matrix (both translation and rotation).
*/
static void transferControllerTranformation(const Fove::SFVR_Pose& pos, float m[4][4])
{
	const Fove::SFVR_Vec3& p = pos.position;
	const Fove::SFVR_Quaternion& q = pos.orientation;

	// x-axis
	m[0][0] = 1 - 2 * q.y*q.y - 2 * q.z*q.z;
	m[1][0] = 2 * q.x*q.y - 2 * q.z*q.w;
	m[2][0] = 2 * -q.x*q.z + 2 * -q.y*q.w;
	// y-axis
	m[0][1] = -(2 * -q.x*q.z - 2 * -q.y*q.w);
	m[1][1] = -(2 * -q.y*q.z + 2 * -q.x*q.w);
	m[2][1] = -(1 - 2 * q.x*q.x - 2 * q.y*q.y);
	// z-axis
	m[0][2] = 2 * q.x*q.y + 2 * q.z*q.w;
	m[1][2] = 1 - 2 * q.x*q.x - 2 * q.z*q.z;
	m[2][2] = 2 * -q.y*q.z - 2 * -q.x*q.w;
	// translation (moved ahead 120mm)
	m[3][0] = p.x - (0.12f * m[2][0]);
	m[3][1] = p.z - (0.12f * m[2][1]);
	m[3][2] = p.y - (0.12f * m[2][2]);
	m[0][3] = 0;
	m[1][3] = 0;
	m[2][3] = 0;
	m[3][3] = 1;
}

//                                                                          ________________________
//_________________________________________________________________________/  
/**
 * Helper functions for Fove structs (from Fove SDK examples).
 */
static Fove::SFVR_Vec3 transformPoint(const Fove::SFVR_Matrix44& transform, Fove::SFVR_Vec3 point, float w)
{
	return {
		transform.mat[0][0] * point.x + transform.mat[0][1] * point.y + transform.mat[0][2] * point.z + transform.mat[0][3] * w,
		transform.mat[1][0] * point.x + transform.mat[1][1] * point.y + transform.mat[1][2] * point.z + transform.mat[1][3] * w,
		transform.mat[2][0] * point.x + transform.mat[2][1] * point.y + transform.mat[2][2] * point.z + transform.mat[2][3] * w,
	};
}
static Fove::SFVR_Matrix44 quatToMatrix(const Fove::SFVR_Quaternion q)
{
	Fove::SFVR_Matrix44 ret;
	ret.mat[0][0] = 1 - 2 * q.y*q.y - 2 * q.z*q.z;
	ret.mat[0][1] = 2 * q.x*q.y - 2 * q.z*q.w;
	ret.mat[0][2] = 2 * q.x*q.z + 2 * q.y*q.w;
	ret.mat[0][3] = 0;
	ret.mat[1][0] = 2 * q.x*q.y + 2 * q.z*q.w;
	ret.mat[1][1] = 1 - 2 * q.x*q.x - 2 * q.z*q.z;
	ret.mat[1][2] = 2 * q.y*q.z - 2 * q.x*q.w;
	ret.mat[1][3] = 0;
	ret.mat[2][0] = 2 * q.x*q.z - 2 * q.y*q.w;
	ret.mat[2][1] = 2 * q.y*q.z + 2 * q.x*q.w;
	ret.mat[2][2] = 1 - 2 * q.x*q.x - 2 * q.y*q.y;
	ret.mat[2][3] = 0;
	ret.mat[3][0] = 0;
	ret.mat[3][1] = 0;
	ret.mat[3][2] = 0;
	ret.mat[3][3] = 1;
	return ret;
}
static Fove::SFVR_Matrix44 translationMatrix(const float x, const float y, const float z)
{
	Fove::SFVR_Matrix44 ret;
	ret.mat[0][0] = 1;
	ret.mat[0][1] = 0;
	ret.mat[0][2] = 0;
	ret.mat[0][3] = x;
	ret.mat[1][0] = 0;
	ret.mat[1][1] = 1;
	ret.mat[1][2] = 0;
	ret.mat[1][3] = y;
	ret.mat[2][0] = 0;
	ret.mat[2][1] = 0;
	ret.mat[2][2] = 1;
	ret.mat[2][3] = z;
	ret.mat[3][0] = 0;
	ret.mat[3][1] = 0;
	ret.mat[3][2] = 0;
	ret.mat[3][3] = 1;
	return ret;
}

//                                                                          ________________________
//_________________________________________________________________________/   mat44_multiply()
/**
 * Helper function to multiply VR matrices (from Blender BLI_math).
 */
static void mat44_multiply_unique(float R[4][4], const float A[4][4], const float B[4][4])
{
	/* matrix product: R[j][k] = A[j][i] . B[i][k] */
	__m128 A0 = _mm_loadu_ps(A[0]);
	__m128 A1 = _mm_loadu_ps(A[1]);
	__m128 A2 = _mm_loadu_ps(A[2]);
	__m128 A3 = _mm_loadu_ps(A[3]);

	for (int i = 0; i < 4; i++) {
		__m128 B0 = _mm_set1_ps(B[i][0]);
		__m128 B1 = _mm_set1_ps(B[i][1]);
		__m128 B2 = _mm_set1_ps(B[i][2]);
		__m128 B3 = _mm_set1_ps(B[i][3]);

		__m128 sum = _mm_add_ps(
			_mm_add_ps(_mm_mul_ps(B0, A0), _mm_mul_ps(B1, A1)),
			_mm_add_ps(_mm_mul_ps(B2, A2), _mm_mul_ps(B3, A3)));

		_mm_storeu_ps(R[i], sum);
	}
}
static void mat44_pre_multiply(float R[4][4], const float A[4][4])
{
	float B[4][4];
	memcpy(B, R, sizeof(float) * 4 * 4);
	mat44_multiply_unique(R, A, B);
}
static void mat44_post_multiply(float R[4][4], const float B[4][4])
{
	float A[4][4];
	memcpy(A, R, sizeof(float) * 4 * 4);
	mat44_multiply_unique(R, A, B);
}
static void mat44_multiply(float R[4][4], const float A[4][4], const float B[4][4])
{
	if (A == R) {
		mat44_post_multiply(R, B);
	}
	else if (B == R) {
		mat44_pre_multiply(R, A);
	}
	else {
		mat44_multiply_unique(R, A, B);
	}
}

//                                                                          ________________________
//_________________________________________________________________________/  updateTracking()
/**
 * Update the t_eye positions based on latest tracking data.
 * \return	Zero on success, an error code on failure.
 */
int VR_Fove::updateTracking()
{
	if (!this->initialized) {
		return VR_Fove::Error_NotInitialized;
	}

	// Update gaze information
	const Fove::EFVR_ErrorCode gaze_error = hmd->GetGazeConvergence(&this->convergence);
	if (gaze_error == Fove::EFVR_ErrorCode::None) {
		Fove::SFVR_Ray ray = convergence.ray;
		ray.origin = transformPoint(camera_matrix, ray.origin, 1);
		ray.direction = transformPoint(camera_matrix, ray.direction, 0);

		Fove::SFVR_Pose controller;
		if (VR_FOVE_USE_CONVERGENCE_DEPTH) {
			float ray_length = convergence.distance / 1000.0f;
			controller.position.x = ray.origin.x + ray.direction.x * ray_length;
			controller.position.y = ray.origin.y + ray.direction.y * ray_length;
			controller.position.z = ray.origin.z + ray.direction.z * ray_length;
			controller.orientation = hmd_pose.orientation;
			transferControllerTranformation(controller, this->t_controller[Side_Mono]);
		}
		else {
			/* Fix convergence depth at one meter ahead. */
			controller.position.x = ray.origin.x + ray.direction.x;
			controller.position.y = ray.origin.y + ray.direction.y;
			controller.position.z = ray.origin.z + ray.direction.z;
			controller.orientation = hmd_pose.orientation;
			transferHMDTranformation(controller, this->t_controller[Side_Mono]);
		}
		this->controller[Side_Mono].available = true;
	}
	else {
		convergence.attention = false;
		convergence.pupilDilation = 1.0f;
		this->controller[Side_Mono].available = false;
	}

	// Get emulated button presses from eye behavior
	ui64& btn_press = this->controller[Side_Mono].buttons;
	ui64& btn_touch = this->controller[Side_Mono].buttons_touched;

	static ui64 eye_closed_left = 0; // Start time when the left eye was closed.
	static ui64 eye_closed_right = 0; // Start time when the right eye was closed.
	static bool persist_wink_left = false; // Whether to continue simulating a WinkLeft press. 
	static bool persist_wink_right = false; // Whether to continue simulating a WinkRight press.
	static bool prev_wink_left = false; // Whether the left eye was closed the previous frame (used to prevent continuous toggling when the eye is closed).
	static bool prev_wink_right = false; // Whether the right eye was closed the previous frame (used to prevent continuous toggling when the eye is closed).

	btn_touch = btn_press = 0;

	// Mouse events
#if VR_FOVE_USE_MOUSE
	if ((GetKeyState(VK_LBUTTON) & 0x100) != 0) {
		btn_touch |= VR_FOVE_BTNBITS_TRIGGERS;
		btn_press |= VR_FOVE_BTNBITS_TRIGGERS;
	}
	if ((GetKeyState(VK_MBUTTON) & 0x100) != 0) {
		btn_touch |= VR_FOVE_BTNBITS_GRIPS;
		btn_press |= VR_FOVE_BTNBITS_GRIPS;
	}
	if ((GetKeyState(VK_RBUTTON) & 0x100) != 0) {
		btn_touch |= VR_FOVE_BTNBIT_STICKLEFT;
		btn_press |= VR_FOVE_BTNBIT_STICKLEFT;
	}
#else
	// Just use the mouse for the grips (navigation)
	if ((GetKeyState(VK_LBUTTON) & 0x100) != 0 ||
		(GetKeyState(VK_MBUTTON) & 0x100) != 0 ||
		(GetKeyState(VK_RBUTTON) & 0x100) != 0) {
		btn_touch |= VR_FOVE_BTNBITS_GRIPS;
		btn_press |= VR_FOVE_BTNBITS_GRIPS;
	}
	// Eye events
	//if (convergence.pupilDilation < VR_FOVE_SQUINT_SIZE_THRESHOLD) {
	//	btn_touch |= VR_FOVE_BTNBITS_TRIGGERS;
	//	btn_press |= VR_FOVE_BTNBITS_TRIGGERS;
	//}
	//else if (convergence.pupilDilation > VR_FOVE_DILATE_SIZE_THRESHOLD) {
	//	btn_touch |= VR_FOVE_BTNBIT_STICKLEFT;
	//	btn_press |= VR_FOVE_BTNBIT_STICKLEFT;
	//}
#endif

	Fove::EFVR_Eye eyes_closed;
	if (hmd->CheckEyesClosed(&eyes_closed) == Fove::EFVR_ErrorCode::None) {
		switch (eyes_closed) {
		case Fove::EFVR_Eye::Neither: {
			// Both eyes closed (no interaction)
			eye_closed_left = 0;
			eye_closed_right = 0;
			break;
		}
		case Fove::EFVR_Eye::Left: {
			// Only left eye closed so right must be open (no interaction)
			eye_closed_right = 0;

			if (eye_closed_left == 0) { // First time eye was closed
				eye_closed_left = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
				prev_wink_left = false;
			}
			ui64 duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() - eye_closed_left;
			if (duration >= VR_FOVE_WINK_THRESHOLD) {
				if (!prev_wink_left) {
					persist_wink_left = !persist_wink_left; // Toggle the persistent press
					prev_wink_left = true;
				}
			}
			break;
		}
		case Fove::EFVR_Eye::Right: {
			// Only right eye closed so left must be open (no interaction)
			eye_closed_left = 0;

			if (eye_closed_right == 0) { // First time eye was closed
				eye_closed_right = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
				prev_wink_right = false;
			}
			ui64 duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() - eye_closed_right;
			if (duration >= VR_FOVE_WINK_THRESHOLD) {
				if (!prev_wink_right) {
					persist_wink_right = !persist_wink_right; // Toggle the persistent press
					prev_wink_right = true;
				}
			}
			break;
		}
		case Fove::EFVR_Eye::Both: {
			//
			break;
		}
		}
	}
	if (persist_wink_left) {
		btn_touch |= VR_FOVE_BTNBITS_XA;
		btn_press |= VR_FOVE_BTNBITS_XA;
	}
	if (persist_wink_right) {
		btn_touch |= VR_FOVE_BTNBITS_YB;
		btn_press |= VR_FOVE_BTNBITS_YB;
	}

	// Wait until the compositor is ready for rendering 
	const Fove::EFVR_ErrorCode pose_error = compositor->WaitForRenderPose(&hmd_pose);
	if (pose_error != Fove::EFVR_ErrorCode::None) {
		Sleep(10);
	}

	// Compute the camera matrix
	mat44_multiply(camera_matrix.mat, quatToMatrix(hmd_pose.orientation).mat, translationMatrix(hmd_pose.position.x, hmd_pose.position.y, hmd_pose.position.z).mat);

	// Update eye poses
	this->eye[Side_Left].pose = hmd_pose;
	transformPoint(this->eye[Side_Left].offset, this->eye[Side_Left].pose.position, 1);
	this->eye[Side_Right].pose = hmd_pose;
	transformPoint(this->eye[Side_Left].offset, this->eye[Side_Right].pose.position, 1);

	// Save the HMD position as matrices
	transferHMDTranformation(hmd_pose, this->t_hmd);
	transferHMDTranformation(this->eye[Side_Left].pose, this->t_eye[Side_Left]);
	transferHMDTranformation(this->eye[Side_Right].pose, this->t_eye[Side_Right]);

	this->tracking = true;
	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/     blitEye()
/**
 * Blit a rendered image into the internal eye texture.
 * TODO_MARUI: aperture_u and aperture_v currently don't do anything in the shader.
 */
int VR_Fove::blitEye(Side side, void* texture_resource, const float& aperture_u, const float& aperture_v)
{
	if (!this->initialized) {
		return VR_Fove::Error_NotInitialized;
	}

	uint texture_id = *((uint*)texture_resource);

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

	// Bind my render buffer as render target eye render target
	glBindFramebuffer(GL_FRAMEBUFFER, this->gl.framebuffer[side]);
	glViewport(0, 0, this->texture_width, this->texture_height);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glUniform4f(this->gl.param_location, aperture_u, aperture_v, 1.0 / this->gamma, 0);

	// Render the provided texture into the Fove eye texture.
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

	// Update compositor texture submission info
	Fove::SFVR_TextureBounds bounds;
	bounds.top = 0;
	bounds.bottom = 1;
	bounds.left = 0;
	bounds.right = 1;

	Fove::SFVR_GLTexture tex{ texture_id, NULL };
	compositor_submit_info.layerId = compositor_layer.layerId;
	compositor_submit_info.pose = hmd_pose;
	if (side == Side_Left) {
		compositor_submit_info.left.texInfo = &tex;
		compositor_submit_info.left.bounds = bounds;
	}
	else { // Side_Right 
		compositor_submit_info.right.texInfo = &tex;
		compositor_submit_info.right.bounds = bounds;
	}

	// Present rendered results to compositor
	if (side == Side_Right && compositor) {
		compositor->Submit(compositor_submit_info);
	}

	return Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/     blitEyes()
/**
 * Blit rendered images into the internal eye textures.
 * TODO_MARUI: aperture_u and aperture_v currently don't do anything in the shader.
 */
int VR_Fove::blitEyes(void* texture_resource_left, void* texture_resource_right, const float& aperture_u, const float& aperture_v)
{
	if (!this->initialized) {
		return VR_Fove::Error_NotInitialized;
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

	for (int i = 0; i < 2; ++i) {
		// Bind my render buffer as render target eye render target
		glBindFramebuffer(GL_FRAMEBUFFER, this->gl.framebuffer[i]);
		glViewport(0, 0, this->texture_width, this->texture_height);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		// Render the provided texture into the Fove eye texture.
		if (i == Side_Left) {
			glBindTexture(GL_TEXTURE_2D, texture_id_left);
		}
		else {
			glBindTexture(GL_TEXTURE_2D, texture_id_right);
		}

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
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

	// Update compositor texture submission info
	Fove::SFVR_TextureBounds bounds;
	bounds.top = 0;
	bounds.bottom = 1;
	bounds.left = 0;
	bounds.right = 1;

	compositor_submit_info.layerId = compositor_layer.layerId;
	compositor_submit_info.pose = hmd_pose;
	Fove::SFVR_GLTexture tex_left{ texture_id_left, NULL };
	compositor_submit_info.left.texInfo = &tex_left;
	compositor_submit_info.left.bounds = bounds;
	Fove::SFVR_GLTexture tex_right{ texture_id_right, NULL };
	compositor_submit_info.right.texInfo = &tex_right;
	compositor_submit_info.right.bounds = bounds;

	// Present rendered results to compositor
	if (compositor) {
		compositor->Submit(compositor_submit_info);
	}

	return Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/        frame()
/**
 * Submit frame to Fove compositor.
 */
int VR_Fove::submitFrame()
{
	//if (!this->initialized) {
	//	return VR::Error_NotInitialized;
	//}

	// Present rendered results to compositor
	//if (compositor) {
	//	Fove::EFVR_ErrorCode error = compositor->Submit(compositor_submit_info);
	//}

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/  getDefaultEyeTexSize
/**
 * Get the default eye texture size.
 */
int VR_Fove::getDefaultEyeTexSize(uint& w, uint& h, Side side)
{
	if (!hmd) {
		int e = this->acquireHMD();
		if (e || !hmd) {
			return VR::Error_InternalFailure;
		}
	}

	Fove::SFVR_Vec2i tex_size = compositor_layer.idealResolutionPerEye;
	w = tex_size.x;
	h = tex_size.y;

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/  getDefaultEyeParams
/**
 * Get the HMD's default parameters.
 */
int VR_Fove::getDefaultEyeParams(Side side, float& fx, float& fy, float& cx, float& cy)
{
	if (!hmd) {
		int e = this->acquireHMD();
		if (e || !hmd) {
			return VR::Error_InternalFailure;
		}
	}

	float left, right, top, bottom;
	Fove::SFVR_ProjectionParams eye_left;
	Fove::SFVR_ProjectionParams eye_right;
	hmd->GetRawProjectionValues(&eye_left, &eye_right);

	if (side == Side_Left) {
		left = eye_left.left;
		right = eye_left.right;
		top = eye_left.top;
		bottom = eye_left.bottom;
	}
	else { // Side_Right
		left = eye_right.left;
		right = eye_right.right;
		top = eye_right.top;
		bottom = eye_right.bottom;
	}

	// FoveVR may consider the y-axis pointing down
	if (top < bottom) {
		top = -top;
		bottom = -bottom;
	}

	float width = right - left;
	float height = top - bottom;
	cx = -left / width;
	cy = -bottom / height;
	fx = 1.0 / width;
	fy = 1.0 / height;

	return VR_Fove::Error_None;
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
 * \param   side    Side of the eye which to set.
 * \param   fx      Horizontal focal length, in "image-width"-units (1=image width).
 * \param   fy      Vertical focal length, in "image-height"-units (1=image height).
 * \param   cx      Horizontal principal point, in "image-width"-units (0.5=image center).
 * \param   cy      Vertical principal point, in "image-height"-units (0.5=image center).
 * \return          Zero on success, an error code on failure.
 */
int VR_Fove::setEyeParams(Side side, float fx, float fy, float cx, float cy)
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
//_________________________________________________________________________/    setEyeOffset
/**
 * Override the offset of the eyes (camera positions) relative to the HMD.
 */
int VR_Fove::setEyeOffset(Side side, float x, float y, float z)
{
	if (side != Side_Left && side != Side_Right) {
		return VR_Fove::Error_InvalidParameter;
	}
	this->eye[side].offset.mat[3][0] = x;
	this->eye[side].offset.mat[3][1] = y;
	this->eye[side].offset.mat[3][2] = z;

	t_hmd2eye[side][0][0] = 1;   t_hmd2eye[side][0][1] = 0;   t_hmd2eye[side][0][2] = 0;   t_hmd2eye[side][0][3] = 0;
	t_hmd2eye[side][1][0] = 0;   t_hmd2eye[side][1][1] = 1;   t_hmd2eye[side][1][2] = 0;   t_hmd2eye[side][1][3] = 0;
	t_hmd2eye[side][2][0] = 0;   t_hmd2eye[side][2][1] = 0;   t_hmd2eye[side][2][2] = 1;   t_hmd2eye[side][2][3] = 0;
	t_hmd2eye[side][3][0] = x;   t_hmd2eye[side][3][1] = y;   t_hmd2eye[side][3][2] = z;   t_hmd2eye[side][3][3] = 1;

	return VR_Fove::Error_None;
}

/***********************************************************************************************//**
*								Exported shared library functions.								   *
***************************************************************************************************/
VR_Fove* c_obj(0);

/**
 * Create a object internally. Must be called before the functions below.
 */
int c_createVR()
{
	c_obj = new VR_Fove();
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
 * \param side      Zero for left, one for right., -1 for both eyes (default).
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
 * Last tracked button state of the controllers.
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
 * \param side  Zero for left, one for right.
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
