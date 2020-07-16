/***********************************************************************************************//**
 * \file      vr_steam.cpp
 *            Valve openvr HMD / VR module for use with SteamVR.
 *            This file contains code related to using Valve's openvr / SteamVR API for HMDs and controllers.
 *            Both tracking and rendering are implemented.
 ***************************************************************************************************
 * \brief     Valve openvr API module for use with SteamVR.
 * \copyright MARUI-PlugIn (inc.)
 * \contributors Multiplexed Reality
 **************************************************************************************************/

#include "openvr.h"

#define GLEW_STATIC
#include "glew.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <GL/glx.h>
#endif

#include "vr_steam.h"

#include <ctime>
#include <xmmintrin.h>
#ifdef _WIN32
#include <cstdlib>
#else
#include <string.h>
#endif
#include <cmath>

#define BITSET1(flagword, bits) {flagword=(decltype(flagword))(flagword|((decltype(flagword))bits));}    //!< Set specified bits in flagword to 1.
#define BITSET0(flagword, bits) {flagword=(decltype(flagword))(flagword&(~((decltype(flagword))bits)));} //!< Set specified bits in flagword to 0.
#define BITSET(flagword, bits, value) {if(value){BITSET1(flagword, bits)}else{BITSET0(flagword, bits)}}  //!< Set specified bits to specified value.

/***********************************************************************************************//**
 * \class                                  VR_Steam
 ***************************************************************************************************
 * VR module for using Valve openvr / SteamVR API.
 **************************************************************************************************/

 //																		____________________________
 //_____________________________________________________________________/   GL::vshader_source
 /**
  * Primitive pass-through vertex shader source code.
  */
const char* const VR_Steam::GL::vshader_source(STRING(#version 120\n
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
const char* const VR_Steam::GL::fshader_source(STRING(#version 120\n
	varying vec2 texcoord;
	uniform sampler2D tex;
	uniform vec4 param;
void main()
{
	gl_FragColor = pow(texture2D(tex, texcoord), param.zzzz); // GAMMA CORRECTION (1/gamma should be passed as param.z)
}
));

//                                                                          ________________________
//_________________________________________________________________________/       VR_Steam()
/**
 * Class constructor
 */
VR_Steam::VR_Steam()
	: VR()
	, hmd(0)
	, hmd_type(HMDType_Null)
	, initialized(false)
{
	eye_offset_override[Side_Left] = false;
	eye_offset_override[Side_Right] = false;

	memset(&this->gl, 0, sizeof(this->gl));

	SET4X4IDENTITY(t_basestation[0]);
	SET4X4IDENTITY(t_basestation[1]);
}

//                                                                          ________________________
//_________________________________________________________________________/     ~VR_Steam()
/**
 * Class destructor
 */
VR_Steam::~VR_Steam()
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
VR::Type VR_Steam::type()
{
	return VR::Type_Steam;
};

//                                                                          ________________________
//_________________________________________________________________________/		hmdType()
/**
 * Get which HMD was used in this implementation.
 */
VR::HMDType VR_Steam::hmdType()
{
	return this->hmd_type;
};

//                                                                          ________________________
//_________________________________________________________________________/     GL::create()
/**
 * Create required OpenGL objects.
 */
bool VR_Steam::GL::create(uint width, uint height)
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
		GLchar error[256];
		GLsizei len;
		glGetShaderInfoLog(this->vshader, 256, &len, error);
		error[255] = 0;
		success = false;
	}
	glAttachShader(this->program, this->vshader);

	glCompileShader(this->fshader);
	glGetShaderiv(this->fshader, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		GLchar error[256];
		GLsizei len;
		glGetShaderInfoLog(this->fshader, 256, &len, error);
		error[255] = 0;
		success = false;
	}
	glAttachShader(this->program, this->fshader);

	glLinkProgram(this->program);
	glGetProgramiv(this->program, GL_LINK_STATUS, &ret);
	if (!ret) {
		GLchar error[256];
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
void VR_Steam::GL::release()
{
	for (int i = 0; i < Sides; ++i) {
		if (this->framebuffer[i]) {
			glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
			glDeleteFramebuffers(1, &this->framebuffer[i]);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			this->framebuffer[i] = 0;
		}
		if (this->texture[i]) {
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
 * Initialize basic OVR operation and acquire the HMD object.
 * \return  Zero on success, and error code on failue.
 */
int VR_Steam::acquireHMD()
{
	if (this->hmd) {
		this->releaseHMD();
	}

	// Create HMD object and initialize whatever necessary
	vr::HmdError error = vr::HmdError::VRInitError_None;
	this->hmd = vr::VR_Init(&error, vr::VRApplication_Scene);
	if (error != vr::VRInitError_None || !this->hmd) {
		this->hmd = 0;
		return VR::Error_InternalFailure;
	}

	// Figure out which HMD it is
	for (int i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i) {
		if (!this->hmd->IsTrackedDeviceConnected(i)) {
			continue;
		}
		if (this->hmd->GetTrackedDeviceClass(i) != vr::TrackedDeviceClass_HMD) {
			continue;
		}

		char str_ManufacturerName[128] = { 0 };
		char str_TrackingSystemName[128] = { 0 };
		char str_ModelNumber[128] = { 0 };
		const size_t str_max = 128 * sizeof(char);
		this->hmd->GetStringTrackedDeviceProperty(i, vr::ETrackedDeviceProperty::Prop_ManufacturerName_String,   str_ManufacturerName, str_max);
		this->hmd->GetStringTrackedDeviceProperty(i, vr::ETrackedDeviceProperty::Prop_TrackingSystemName_String, str_TrackingSystemName, str_max);
		this->hmd->GetStringTrackedDeviceProperty(i, vr::ETrackedDeviceProperty::Prop_ModelNumber_String,        str_ModelNumber, str_max);
		str_ManufacturerName[127] = 0;
		str_TrackingSystemName[127] = 0;
		str_ModelNumber[127] = 0;
		FILE* fp = 0;
		fopen_s(&fp, "DetectedDevices.log", "a");
		if (fp) {
			fprintf_s(fp, "ManufacturerName=%s\n", str_ManufacturerName);
			fprintf_s(fp, "TrackingSystemName=%s\n", str_TrackingSystemName);
			fprintf_s(fp, "ModelNumber=%s\n\n", str_ModelNumber);
			fclose(fp);
		}

		// For Oculus Rift:
		// Prop_ManufacturerName_String   == "Oculus"
		// Prop_TrackingSystemName_String == "oculus"
		// Prop_ModelNumber_String        == "Oculus Rift CV1"
		if (strncmp(str_ManufacturerName, "Oculus", str_max) == 0) {
#if 1
			// we do not handle Oculus HMDs via steam
			continue;
#else
			this->hmd_type = VR::HMDType_Oculus;
			return VR::Error_None;
#endif
		}

		// For Fove0:
		// Prop_ManufacturerName_String   == "FOV
		// Prop_TrackingSystemName_String == "fove"
		// Prop_ModelNumber_String        == "FOVE0001"
		if (strncmp(str_ManufacturerName, "FOV", str_max) == 0) {
#if 1
			// we do not handle Fove HMDs via steam
			continue;
#else
			this->hmd_type = VR::HMDType_Fove;
			return VR::Error_None;
#endif
		}

		// For HTC Vive / Cosmos:
		// Prop_ManufacturerName_String   == "HTC"
		// Prop_TrackingSystemName_String == "lighthouse" | "vive_eyes"
		// Prop_ModelNumber_String        == "Vive. MV" | "vive_cosmos"
		if (strncmp(str_ManufacturerName, "HTC", str_max) == 0) {
			this->hmd_type = strncmp(str_ModelNumber, "vive_cosmos", str_max) == 0 ? VR::HMDType_Cosmos : VR::HMDType_Vive;
			return VR::Error_None;
		}

		// For WindowsMR / DELL:
		// Prop_ManufacturerName_String   == "WindowsMR"
		// Prop_TrackingSystemName_String == "holographic"
		// Prop_ModelNumber_String        == "DELL VISOR VR118"
		if (strncmp(str_ManufacturerName, "WindowsMR", str_max) == 0) {
			this->hmd_type = VR::HMDType_WindowsMR;
			return VR::Error_None;
		}

		// For Pimax VR:
        // Prop_ManufacturerName_String   == "Pimax VR, Inc."
        // Prop_TrackingSystemName_String == "aapvr"
        // Prop_ModelNumber_String        == "Pimax 5K Plus"
        if (strncmp(str_ManufacturerName, "Pimax VR, Inc.", str_max) == 0) {
            this->hmd_type = VR::HMDType_Pimax;
            return VR::Error_None;
        }

        // For Valve Index:
        // Prop_ManufacturerName_String   == "Valve"
        // Prop_TrackingSystemName_String == "lighthouse"?
        // Prop_ModelNumber_String        == "Index"?
        if (strncmp(str_ManufacturerName, "Valve", str_max) == 0) {
            this->hmd_type = VR::HMDType_Index;
            return VR::Error_None;
        }

        // For RiftCat:
        // Prop_ManufacturerName_String   == "Riftcat"
        // Prop_TrackingSystemName_String == "vridge"
        // Prop_ModelNumber_String        == "Vridge"
        if (strncmp(str_ManufacturerName, "Riftcat", str_max) == 0) {
            this->hmd_type = VR::HMDType_Oculus;
            return VR::Error_None;
        }

		// Other SteamVR compatible device:
		this->hmd_type = VR::HMDType_Vive;
		return VR::Error_None;
	}

	// If we arrive here, we could not find any supported HMD
	vr::VR_Shutdown();
	this->hmd = 0;
	return VR::Error_InternalFailure;
}

//                                                                          ________________________
//_________________________________________________________________________/    releaseHMD()
/**
 * Delete the HMD object and uninitialize basic OVR operation.
 * \return  Zero on success, and error code on failue.
 */
int VR_Steam::releaseHMD()
{
	if (!this->hmd) {
		return VR::Error_NotInitialized;
	}

	// Shutdown VR
	vr::VR_Shutdown();
	this->hmd = 0;

	return VR::Error_None;
}

#ifdef _WIN32
//                                                                          ________________________
//_________________________________________________________________________/        init()
/**
 * Initialize the VR device.
 * \param   device          The graphics device used by Blender (HDC)
 * \param   context         The rendering context used by Blennder (HGLRC)
 * \return  Zero on success, an error code on failure.
 */
int VR_Steam::init(void* device, void* context)
#else
//                                                                          ________________________
//_________________________________________________________________________/        init()
/**
 * Initialize the VR device.
 * \param   display     The connection to the X server (Display*).
 * \param   drawable    Pointer to the GLX drawable (GLXDrawable*). Either an X window ID or a GLX pixmap ID.
 * \param   context     Pointer to the GLX rendering context (GLXContext*) attached to drawable.
 * \return  Zero on success, an error code on failure.
 */
int VR_Steam::init(void* display, void* drawable, void* context)
#endif
{
	if (this->initialized) {
		this->uninit();
	}

	// Get the Blender viewport window / context IDs.
#ifdef _WIN32
	this->gl.device = (HDC)device;
	this->gl.context = (HGLRC)context; // save old context so that we can share
#else
	this->gl.display = (Display*)display;
	this->gl.drawable = *(GLXDrawable*)drawable;
	this->gl.context = *(GLXContext*)context;
#endif

	if (!this->hmd) {
		int e = this->acquireHMD();
		if (e || !this->hmd) {
			return VR::Error_InternalFailure;
		}
	}

	// Initialize compositor
	if (!vr::VRCompositor()) {
		return VR::Error_InternalFailure;
	}

	// Initialize glew
	glewExperimental = GL_TRUE; // This is required for the glGenFramebuffers call
	if (glewInit() != GLEW_OK) {
		return VR::Error_InternalFailure;
	}

	// Get head position data if not set manually
	if (!this->eye_offset_override[Side_Left]) {
		vr::HmdMatrix34_t m = this->hmd->GetEyeToHeadTransform(vr::Eye_Left);
		t_hmd2eye[Side_Left][0][0] = m.m[0][0];  t_hmd2eye[Side_Left][1][0] = m.m[0][1];  t_hmd2eye[Side_Left][2][0] = m.m[0][2];  t_hmd2eye[Side_Left][3][0] = m.m[0][3];
		t_hmd2eye[Side_Left][0][1] = m.m[1][0];  t_hmd2eye[Side_Left][1][1] = m.m[1][1];  t_hmd2eye[Side_Left][2][1] = m.m[1][2];  t_hmd2eye[Side_Left][3][1] = m.m[1][3];
		t_hmd2eye[Side_Left][0][2] = m.m[2][0];  t_hmd2eye[Side_Left][1][2] = m.m[2][1];  t_hmd2eye[Side_Left][2][2] = m.m[2][2];  t_hmd2eye[Side_Left][3][2] = m.m[2][3];
		t_hmd2eye[Side_Left][0][3] = 0;          t_hmd2eye[Side_Left][1][3] = 0;          t_hmd2eye[Side_Left][2][3] = 0;          t_hmd2eye[Side_Left][3][3] = 1;
	}
	if (!this->eye_offset_override[Side_Right]) {
		vr::HmdMatrix34_t m = this->hmd->GetEyeToHeadTransform(vr::Eye_Right);
		t_hmd2eye[Side_Right][0][0] = m.m[0][0];  t_hmd2eye[Side_Right][1][0] = m.m[0][1];  t_hmd2eye[Side_Right][2][0] = m.m[0][2];  t_hmd2eye[Side_Right][3][0] = m.m[0][3];
		t_hmd2eye[Side_Right][0][1] = m.m[1][0];  t_hmd2eye[Side_Right][1][1] = m.m[1][1];  t_hmd2eye[Side_Right][2][1] = m.m[1][2];  t_hmd2eye[Side_Right][3][1] = m.m[1][3];
		t_hmd2eye[Side_Right][0][2] = m.m[2][0];  t_hmd2eye[Side_Right][1][2] = m.m[2][1];  t_hmd2eye[Side_Right][2][2] = m.m[2][2];  t_hmd2eye[Side_Right][3][2] = m.m[2][3];
		t_hmd2eye[Side_Right][0][3] = 0;          t_hmd2eye[Side_Right][1][3] = 0;          t_hmd2eye[Side_Right][2][3] = 0;          t_hmd2eye[Side_Right][3][3] = 1;
	}

	// Create the render buffers and textures
	this->hmd->GetRecommendedRenderTargetSize(&this->texture_width, &this->texture_height);
	this->gl.create(this->texture_width, this->texture_height);

	// Initialize new input system:
	std::string tmp_folder;
#ifdef _WIN32
	char temp_path[128];
	DWORD temp_path_len = GetTempPath(126, temp_path);
	if (temp_path_len == 0) {
		// Fallback option
		tmp_folder = "C:/Temp/";
	}
	else {
		for (DWORD i = 0; i < temp_path_len; i++) {
			if (temp_path[i] == '\\') {
				temp_path[i] = '/';
			}
		}
		temp_path[temp_path_len] = '/';
		temp_path[temp_path_len + 1] = 0;
		tmp_folder = temp_path;
	}
#else
	// On Unix OSs, we can use /tmp/
	tmp_folder = "/tmp/";
#endif

	std::string action_manifest_path = tmp_folder + std::string("action_manifest.json");
	const char* action_manifest_path_str = action_manifest_path.c_str();
	std::string binding_vive_path = tmp_folder + std::string("binding_vive.json");
	const char* binding_vive_path_str = binding_vive_path.c_str();
	std::string binding_windowsmr_path = tmp_folder + std::string("binding_windowsmr.json");
	const char* binding_windowsmr_path_str = binding_windowsmr_path.c_str();
	std::string binding_index_path = tmp_folder + std::string("binding_index.json");
	const char* binding_index_path_str = binding_index_path.c_str();
	std::string binding_cosmos_path = tmp_folder + std::string("binding_cosmos.json");
	const char* binding_cosmos_path_str = binding_cosmos_path.c_str();
	std::string binding_logitechink_path = tmp_folder + std::string("binding_logitechink.json");
	const char* binding_logitechink_path_str = binding_logitechink_path.c_str();

	FILE* fp;
	fp = fopen(action_manifest_path_str, "w");
	fwrite(Input::action_manifest, sizeof(char), strlen(Input::action_manifest), fp);
	fflush(fp);
	fclose(fp);
	fp = fopen(binding_vive_path_str, "w");
	fwrite(Input::binding_vive, sizeof(char), strlen(Input::binding_vive), fp);
	fflush(fp);
	fclose(fp);
	fp = fopen(binding_windowsmr_path_str, "w");
	fwrite(Input::binding_windowsmr, sizeof(char), strlen(Input::binding_windowsmr), fp);
	fflush(fp);
	fclose(fp);
	fp = fopen(binding_index_path_str, "w");
	fwrite(Input::binding_index, sizeof(char), strlen(Input::binding_index), fp);
	fflush(fp);
	fclose(fp);
	fp = fopen(binding_cosmos_path_str, "w");
	fwrite(Input::binding_cosmos, sizeof(char), strlen(Input::binding_cosmos), fp);
	fflush(fp);
	fclose(fp);
	fp = fopen(binding_logitechink_path_str, "w");
	fwrite(Input::binding_logitechink, sizeof(char), strlen(Input::binding_logitechink), fp);
	fflush(fp);
	fclose(fp);

	vr::EVRInputError input_error;
	vr::IVRInput* vr_input = vr::VRInput();
	input_error = vr_input->SetActionManifestPath(action_manifest_path_str);

	input_error = vr_input->GetActionHandle("/actions/main/in/pos_left", &this->input.action_handles[Side_Left].pos);
	input_error = vr_input->GetActionHandle("/actions/main/in/pos_right", &this->input.action_handles[Side_Right].pos);
	input_error = vr_input->GetActionHandle("/actions/main/in/trigger_left", &this->input.action_handles[Side_Left].trigger);
	input_error = vr_input->GetActionHandle("/actions/main/in/trigger_right", &this->input.action_handles[Side_Right].trigger);
	input_error = vr_input->GetActionHandle("/actions/main/in/grip_left", &this->input.action_handles[Side_Left].grip);
	input_error = vr_input->GetActionHandle("/actions/main/in/grip_right", &this->input.action_handles[Side_Right].grip);
	input_error = vr_input->GetActionHandle("/actions/main/in/grip_touch_left", &this->input.action_handles[Side_Left].grip_touch);
	input_error = vr_input->GetActionHandle("/actions/main/in/grip_touch_right", &this->input.action_handles[Side_Right].grip_touch);
	input_error = vr_input->GetActionHandle("/actions/main/in/grip_force_left", &this->input.action_handles[Side_Left].grip_force);
	input_error = vr_input->GetActionHandle("/actions/main/in/grip_force_right", &this->input.action_handles[Side_Right].grip_force);
	input_error = vr_input->GetActionHandle("/actions/main/in/touchpad_left", &this->input.action_handles[Side_Left].touchpad);
	input_error = vr_input->GetActionHandle("/actions/main/in/touchpad_right", &this->input.action_handles[Side_Right].touchpad);
	input_error = vr_input->GetActionHandle("/actions/main/in/touchpad_press_left", &this->input.action_handles[Side_Left].touchpad_press);
	input_error = vr_input->GetActionHandle("/actions/main/in/touchpad_press_right", &this->input.action_handles[Side_Right].touchpad_press);
	input_error = vr_input->GetActionHandle("/actions/main/in/touchpad_touch_left", &this->input.action_handles[Side_Left].touchpad_touch);
	input_error = vr_input->GetActionHandle("/actions/main/in/touchpad_touch_right", &this->input.action_handles[Side_Right].touchpad_touch);
	input_error = vr_input->GetActionHandle("/actions/main/in/thumbstick_left", &this->input.action_handles[Side_Left].thumbstick);
	input_error = vr_input->GetActionHandle("/actions/main/in/thumbstick_right", &this->input.action_handles[Side_Right].thumbstick);
	input_error = vr_input->GetActionHandle("/actions/main/in/thumbstick_press_left", &this->input.action_handles[Side_Left].thumbstick_press);
	input_error = vr_input->GetActionHandle("/actions/main/in/thumbstick_press_right", &this->input.action_handles[Side_Right].thumbstick_press);
	input_error = vr_input->GetActionHandle("/actions/main/in/button_a_left", &this->input.action_handles[Side_Left].button_a);
	input_error = vr_input->GetActionHandle("/actions/main/in/button_a_right", &this->input.action_handles[Side_Right].button_a);
	input_error = vr_input->GetActionHandle("/actions/main/in/button_a_touch_left", &this->input.action_handles[Side_Left].button_a_touch);
	input_error = vr_input->GetActionHandle("/actions/main/in/button_a_touch_right", &this->input.action_handles[Side_Right].button_a_touch);
	input_error = vr_input->GetActionHandle("/actions/main/in/button_b_left", &this->input.action_handles[Side_Left].button_b);
	input_error = vr_input->GetActionHandle("/actions/main/in/button_b_right", &this->input.action_handles[Side_Right].button_b);
	input_error = vr_input->GetActionHandle("/actions/main/in/button_b_touch_left", &this->input.action_handles[Side_Left].button_b_touch);
	input_error = vr_input->GetActionHandle("/actions/main/in/button_b_touch_right", &this->input.action_handles[Side_Right].button_b_touch);
	input_error = vr_input->GetActionHandle("/actions/main/in/button_menu_left", &this->input.action_handles[Side_Left].button_menu);
	input_error = vr_input->GetActionHandle("/actions/main/in/button_menu_right", &this->input.action_handles[Side_Right].button_menu);
	input_error = vr_input->GetActionHandle("/actions/main/in/button_menu_touch_left", &this->input.action_handles[Side_Left].button_menu_touch);
	input_error = vr_input->GetActionHandle("/actions/main/in/button_menu_touch_right", &this->input.action_handles[Side_Right].button_menu_touch);
	input_error = vr_input->GetActionSetHandle("/actions/main", &this->input.action_set_handle);
	this->input.active_action_set.ulActionSet = this->input.action_set_handle;
	this->input.active_action_set.ulRestrictedToDevice = vr::k_ulInvalidInputValueHandle; // for all devices
	this->input.active_action_set.nPriority = 1;
	this->input.active_action_set.ulSecondaryActionSet = 0; // ignored because of ulRestrictedToDevice is for all devices
	this->input.active_action_set.unPadding = 0; // ignored

	this->initialized = true;

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/		uninit()
/**
 * Uninitialize the VR module.
 */
int VR_Steam::uninit()
{
	if (!this->initialized) {
		return VR::Error_NotInitialized;
	}

	// save current context so that we can return
#ifdef _WIN32
	HDC   dc = wglGetCurrentDC();
	HGLRC rc = wglGetCurrentContext();
	// switch to the context we had when initializing
	if (rc != this->gl.context) {
		wglMakeCurrent(this->gl.device, this->gl.context);
	}

	// Destroy render buffers
	this->gl.release();

	this->releaseHMD();

	// return to previous context
	if (rc != this->gl.context) {
		wglMakeCurrent(dc, rc);
	}
#else
	Display* display = glXGetCurrentDisplay();
	GLXDrawable drawable = glXGetCurrentDrawable();
	GLXContext context = glXGetCurrentContext();
	// switch to the context we had when initializing
	if (context != this->gl.context) {
		glXMakeCurrent(this->gl.display, this->gl.drawable, this->gl.context);
	}

	// Destroy render buffers
	this->gl.release();

	this->releaseHMD();

	// return to previous context
	if (context != this->gl.context) {
		glXMakeCurrent(display, drawable, context);
	}
#endif

	this->initialized = false;
	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/   uninitRender()
/**
 * Helper function to convert vr matrices.
 * \param   in  Row-major transformation matrix, y-axis up in meter (vr matrix).
 * \param   out [OUT] Column-major matrix (openGL matrix), z-axis up, in mm.
 */
static void convertMatrix(const float in[3][4], float out[4][4])
{
	out[0][0] = in[0][0];      out[1][0] = in[0][1];       out[2][0] = in[0][2];      out[3][0] = in[0][3];
	out[0][1] = -in[2][0];     out[1][1] = -in[2][1];      out[2][1] = -in[2][2];     out[3][1] = -in[2][3];
	out[0][2] = in[1][0];      out[1][2] = in[1][1];       out[2][2] = in[1][2];      out[3][2] = in[1][3];
	out[0][3] = 0;             out[1][3] = 0;              out[2][3] = 0;             out[3][3] = 1;
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
 * \return      Zero on success, an error code on failure.
 */
int VR_Steam::updateTracking()
{
	if (!this->hmd) {
		return VR::Error_NotInitialized;
	}

	vr::IVRCompositor* compositor = vr::VRCompositor();
	if (!compositor) {
		return VR::Error_NotInitialized;
	}
	vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
	compositor->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, NULL, 0);

	// Assume tracking was lost
	this->tracking = false;
	// Just count up base stations - the order should be the same
	uint base_station_index = 0;
	// Process latest tracking data
	for (int i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i) {
		if (!this->hmd->IsTrackedDeviceConnected(i)) {
			continue;
		}
		if (!poses[i].bPoseIsValid) {
			continue;
		}
		const vr::HmdMatrix34_t& m = poses[i].mDeviceToAbsoluteTracking;
		switch (this->hmd->GetTrackedDeviceClass(i)) {
		case vr::TrackedDeviceClass_Controller:
			/* TODO_XR: Decouple controller type from HMD type. 
			 * For now, we assume the controller type based on the HMD. */
            /*
			char controller_type_string[vr::k_unMaxPropertyStringSize];
			if (uint32_t str_len = this->hmd->GetStringTrackedDeviceProperty(i, vr::ETrackedDeviceProperty::Prop_ControllerType_String, controller_type_string, vr::k_unMaxPropertyStringSize)) {
				if (strncmp(controller_type_string, "vive_controller", str_len) == 0) {
					this->hmd_type = HMDType_Vive;
				}
				else if (strncmp(controller_type_string, "holographic_controller", str_len) == 0) {
					this->hmd_type = HMDType_WindowsMR;
				}
				else if (strncmp(controller_type_string, "knuckles", str_len) == 0) {
					this->hmd_type = HMDType_Index;
				}
				else if (strncmp(controller_type_string, "logitech_stylus", str_len) == 0) {
					/ * TODO_XR * /
				} 
				else {
					// unsupported device / HMD
				}
			}
            */

			if (this->hmd->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand) == i) {
				vr::VRControllerState_t controller_state;
				this->hmd->GetControllerState(i, &controller_state, sizeof(controller_state));
				this->interpretControllerState(controller_state, m.m, this->t_controller[Side_Left], this->controller[Side_Left], &this->input.action_handles[Side_Left]);
			}
			else if (this->hmd->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand) == i) {
				vr::VRControllerState_t controller_state;
				this->hmd->GetControllerState(i, &controller_state, sizeof(controller_state));
				this->interpretControllerState(controller_state, m.m, this->t_controller[Side_Right], this->controller[Side_Right], &this->input.action_handles[Side_Right]);
			}
			else {
				vr::VRControllerState_t controller_state;
				this->hmd->GetControllerState(i, &controller_state, sizeof(controller_state));
				this->interpretControllerState(controller_state, m.m, this->t_controller[Side_AUX], this->controller[Side_AUX], &this->input.action_handles[Side_AUX]);
			}
			break;
		case vr::TrackedDeviceClass_HMD:
			this->tracking = true;
			convertMatrix(m.m, this->t_hmd);
			mat44_multiply(this->t_eye[Side_Left], this->t_hmd, this->t_hmd2eye[Side_Left]);
			mat44_multiply(this->t_eye[Side_Right], this->t_hmd, this->t_hmd2eye[Side_Right]);
			break;
		case vr::TrackedDeviceClass_GenericTracker:
			convertMatrix(m.m, this->t_controller[Side_AUX]);
			this->controller[Side_AUX].available = 1;
			break;
		case vr::TrackedDeviceClass_TrackingReference:
			if (base_station_index < VR_STEAM_NUMBASESTATIONS) {
				convertMatrix(m.m, this->t_basestation[base_station_index++]);
			}
			break;
		case vr::TrackedDeviceClass_Invalid:
		default:
			break;
		}
	}

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
int VR_Steam::getTrackerPosition(uint i, float t[4][4]) const
{
	if (i >= VR_STEAM_NUMBASESTATIONS) {
		return VR:: Error_InvalidParameter;
	}
	memcpy(t, this->t_basestation[i], sizeof(float) * 4 * 4);
	return VR_Steam::Error_None;
}

//                                                                      ____________________________
//_____________________________________________________________________/ interpretControllerState()
/**
 * Helper function to deal with Vive controller data.
 */
void VR_Steam::interpretControllerState(const vr::VRControllerState_t& s, const float m[3][4], float t_controller[4][4], Controller& c, const Input::ActionHandles *input_handles)
{
	c.available = 1;

	t_controller[0][0] = m[0][0];      t_controller[1][0] = -m[0][2];      t_controller[2][0] = m[0][1];      t_controller[3][0] = m[0][3];
	t_controller[0][1] = -m[2][0];     t_controller[1][1] = m[2][2];       t_controller[2][1] = -m[2][1];     t_controller[3][1] = -m[2][3];
	t_controller[0][2] = m[1][0];      t_controller[1][2] = -m[1][2];      t_controller[2][2] = m[1][1];      t_controller[3][2] = m[1][3];
	t_controller[0][3] = 0;            t_controller[1][3] = 0;             t_controller[2][3] = 0;            t_controller[3][3] = 1;
	
	// offset, so that the cursor is ahead of the controller
	// for HTC Vive contollers, the offset should be 60.0 mm
	// for WindowsMR controllers, the offset should be 30.0 mm
	float controller_offset;
	switch (this->hmd_type) {
	case HMDType_Vive:
	case HMDType_Pimax: {
		controller_offset = 0.06f;
		break;
	}
	case HMDType_WindowsMR: {
		controller_offset = 0.03f;
		break;
	}
	default: {
		controller_offset = 0.0f;
		break;
	}
    }
	t_controller[3][0] += t_controller[1][0] * controller_offset;
	t_controller[3][1] += t_controller[1][1] * controller_offset;
	t_controller[3][2] += t_controller[1][2] * controller_offset;

	uint64_t prior_touchpad_pressed = c.buttons & VR_STEAM_BTNBITS_DPADANY;
	uint64_t prior_thumbstick_pressed = c.buttons & VR_STEAM_BTNBITS_STICKANY;

	// Convert Valve button bits to Widget_Layout button bits.
	c.buttons = c.buttons_touched = 0;

#if 1 // New OpenVR Input System
	if (!input_handles) {
		return; // no button-bearing controller
	}

	vr::EVRInputError input_error;
	vr::IVRInput* vr_input = vr::VRInput();
	if (!vr_input) {
		return;
	}
	clock_t now = clock();

	input_error = vr_input->UpdateActionState(&this->input.active_action_set, sizeof(vr::VRActiveActionSet_t), 1);
	vr::InputAnalogActionData_t analog_action_data;
	vr::InputDigitalActionData_t digital_action_data;

	// Trigger:
	input_error = vr_input->GetAnalogActionData(input_handles->trigger, &analog_action_data, sizeof(vr::InputDigitalActionData_t), vr::k_ulInvalidInputValueHandle);
	if (input_error == vr::VRInputError_None && analog_action_data.bActive) {
		BITSET(c.buttons_touched, (c.side == Side_Left ? VR_STEAM_BTNBIT_LEFTTRIGGER : VR_STEAM_BTNBIT_RIGHTTRIGGER), (analog_action_data.x > 0.1) ? 1 : 0);
		if (analog_action_data.x >= VR_STEAM_TRIGGERPRESSURETHRESHOLD) {
			BITSET1(c.buttons, (c.side == Side_Left ? VR_STEAM_BTNBIT_LEFTTRIGGER : VR_STEAM_BTNBIT_RIGHTTRIGGER))
			// map pressure to 0~1 for everything above the threshold
			c.trigger_pressure = (analog_action_data.x - VR_STEAM_TRIGGERPRESSURETHRESHOLD) / (1.0f - VR_STEAM_TRIGGERPRESSURETHRESHOLD);  // trigger is at index 1 for some reason
		}
		else {
			BITSET0(c.buttons, (c.side == Side_Left ? VR_STEAM_BTNBIT_LEFTTRIGGER : VR_STEAM_BTNBIT_RIGHTTRIGGER))
			c.trigger_pressure = 0.0f;
		}
	}
	// grip
	if (this->hmd_type == HMDType_Index) { // knuckles "click" is too sensitive - use force instead
		input_error = vr_input->GetAnalogActionData(input_handles->grip_force, &analog_action_data, sizeof(vr::InputDigitalActionData_t), vr::k_ulInvalidInputValueHandle);
		if (input_error == vr::VRInputError_None && analog_action_data.bActive) {
			const ui64 btnbit = (c.side == Side_Left ? VR_STEAM_BTNBIT_LEFTGRIP : VR_STEAM_BTNBIT_RIGHTGRIP);
			if ((c.buttons & btnbit) == 0 && (analog_action_data.x > VR_STEAM_GRIPPRESSURETHRESHOLD)) {
				BITSET1(c.buttons, btnbit);
				// map pressure to 0~1 for everything above the threshold
				c.grip_pressure = (analog_action_data.x - VR_STEAM_GRIPPRESSURETHRESHOLD) / (1.0f - VR_STEAM_GRIPPRESSURETHRESHOLD);
			}
			else if ((c.buttons & btnbit) != 0 && (analog_action_data.x < 0.3)) {
				BITSET0(c.buttons, btnbit);
				c.grip_pressure = 0.0f;
			}
			if ((c.buttons_touched & btnbit) == 0 && (analog_action_data.x > 0.2)) {
				BITSET1(c.buttons_touched, btnbit);
			}
			else if ((c.buttons_touched & btnbit) != 0 && (analog_action_data.x < 0.1)) {
				BITSET0(c.buttons_touched, btnbit);
			}
		}
	}
	else {
		const ui64 btnbit = (c.side == Side_Left ? VR_STEAM_BTNBIT_LEFTGRIP : VR_STEAM_BTNBIT_RIGHTGRIP);
		input_error = vr_input->GetDigitalActionData(input_handles->grip, &digital_action_data, sizeof(vr::InputDigitalActionData_t), vr::k_ulInvalidInputValueHandle);
		if (input_error == vr::VRInputError_None && digital_action_data.bActive) {
			BITSET(c.buttons, btnbit, digital_action_data.bState ? 1 : 0);
		}
		input_error = vr_input->GetDigitalActionData(input_handles->grip_touch, &digital_action_data, sizeof(vr::InputDigitalActionData_t), vr::k_ulInvalidInputValueHandle);
		if (input_error == vr::VRInputError_None && digital_action_data.bActive) {
			BITSET(c.buttons_touched, btnbit, digital_action_data.bState ? 1 : 0);
		}
		c.grip_pressure = 0.0f;
	}
	// A button
	input_error = vr_input->GetDigitalActionData(input_handles->button_a, &digital_action_data, sizeof(vr::InputDigitalActionData_t), vr::k_ulInvalidInputValueHandle);
	if (input_error == vr::VRInputError_None && digital_action_data.bActive) {
		const ui64 btnbit = (c.side == Side_Left ? VR_STEAM_BTNBIT_LEFTA : VR_STEAM_BTNBIT_RIGHTA);
		BITSET(c.buttons_touched, btnbit, digital_action_data.bState ? 1 : 0);
		BITSET(c.buttons, btnbit, digital_action_data.bState ? 1 : 0);
	}
	// B button
	input_error = vr_input->GetDigitalActionData(input_handles->button_b, &digital_action_data, sizeof(vr::InputDigitalActionData_t), vr::k_ulInvalidInputValueHandle);
	if (input_error == vr::VRInputError_None && digital_action_data.bActive) {
		const ui64 btnbit = (c.side == Side_Left ? VR_STEAM_BTNBIT_LEFTB : VR_STEAM_BTNBIT_RIGHTB);
		BITSET(c.buttons_touched, btnbit, digital_action_data.bState ? 1 : 0);
		BITSET(c.buttons, btnbit, digital_action_data.bState ? 1 : 0);
	}
	// menu button
	input_error = vr_input->GetDigitalActionData(input_handles->button_menu, &digital_action_data, sizeof(vr::InputDigitalActionData_t), vr::k_ulInvalidInputValueHandle);
	if (input_error == vr::VRInputError_None && digital_action_data.bActive) {
		const ui64 btnbit = VR_STEAM_BTNBIT_MENU;
		BITSET(c.buttons_touched, btnbit, digital_action_data.bState ? 1 : 0);
		BITSET(c.buttons, btnbit, digital_action_data.bState ? 1 : 0);
	}
	// touchpad
	// Convert touchpad position to button
	vr_input->GetAnalogActionData(input_handles->touchpad, &analog_action_data, sizeof(vr::InputDigitalActionData_t), vr::k_ulInvalidInputValueHandle);
	if (input_error == vr::VRInputError_None && analog_action_data.bActive) {
		c.dpad[0] = analog_action_data.x;
		c.dpad[1] = analog_action_data.y;
		static ui64 touchpad_btn[2] = { 0,0 }; // static, so that it stays on the last button until I move on some other button
		if (abs(analog_action_data.x) > abs(analog_action_data.y)) { // LEFT or RIGHT
			if (analog_action_data.x > VR_STEAM_TRACKPADDIRECTIONTHRESHOLD) { // RIGHT
                touchpad_btn[c.side] = VR_STEAM_BTNBIT_DPADRIGHT;
			}
			else if (analog_action_data.x < -VR_STEAM_TRACKPADDIRECTIONTHRESHOLD) { // LEFT
                touchpad_btn[c.side] = VR_STEAM_BTNBIT_DPADLEFT;
			}
			else { // CENTER
				c.side == Side_Left ? touchpad_btn[c.side] = VR_STEAM_BTNBIT_LEFTDPAD :
									  touchpad_btn[c.side] = VR_STEAM_BTNBIT_RIGHTDPAD;
			}
		}
		else { // UP or DOWN
			if (analog_action_data.y > 0.05f) { // UP (reduced threshold, because it's hard to hit)
                touchpad_btn[c.side] = VR_STEAM_BTNBIT_DPADUP;
			}
			else if (analog_action_data.y < -VR_STEAM_TRACKPADDIRECTIONTHRESHOLD) { // DOWN
                touchpad_btn[c.side] = VR_STEAM_BTNBIT_DPADDOWN;
			}
			else { // CENTER
				c.side == Side_Left ? touchpad_btn[c.side] = VR_STEAM_BTNBIT_LEFTDPAD :
									  touchpad_btn[c.side] = VR_STEAM_BTNBIT_RIGHTDPAD;
			}
		}

		// Touchpad touch:
		input_error = vr_input->GetDigitalActionData(input_handles->touchpad_touch, &digital_action_data, sizeof(vr::InputDigitalActionData_t), vr::k_ulInvalidInputValueHandle);
		const bool touchpad_touched = digital_action_data.bActive && digital_action_data.bState;
		static clock_t prior_touch_touchpad[2] = { 0, 0 }; // for smoothing micro touch-losses
		if (touchpad_touched || (now - prior_touch_touchpad[c.side]) < VR_STEAM_DEBOUNCEPERIOD) {
			// if we're pressing a button, we stick with that until we let go
			if (prior_touchpad_pressed) {
				c.buttons_touched |= prior_touchpad_pressed;
			}
			else {
				c.buttons_touched |= touchpad_btn[c.side];
			}
			if (s.ulButtonTouched & (c.side == Side_Left ? VR_STEAM_BTNBIT_LEFTDPAD : VR_STEAM_BTNBIT_RIGHTDPAD)) {
				prior_touch_touchpad[c.side] = now;
			}
		}

		// Touchpad press:
		input_error = vr_input->GetDigitalActionData(input_handles->touchpad_press, &digital_action_data, sizeof(vr::InputDigitalActionData_t), vr::k_ulInvalidInputValueHandle);
		const bool touchpad_pressed = digital_action_data.bActive && digital_action_data.bState;
		// When the touchpad is pressed, find out in what quadrant (left, right, up, down)
		static clock_t prior_press_touchpad[2] = { 0, 0 }; // for smoothing micro touch-losses
		if (touchpad_pressed || (now - prior_press_touchpad[c.side]) < VR_STEAM_DEBOUNCEPERIOD) {
			// if we already are pressing one of the buttons, continue using it, 
			if (prior_touchpad_pressed) {
				c.buttons |= prior_touchpad_pressed;
			}
			else {
				c.buttons |= touchpad_btn[c.side];
			}
			if (s.ulButtonPressed & (c.side == Side_Left ? VR_STEAM_BTNBIT_LEFTDPAD : VR_STEAM_BTNBIT_RIGHTDPAD)) {
				prior_press_touchpad[c.side] = now;
			}
		}
	}

	// thumb-stick (if available): convert to buttons
	vr_input->GetAnalogActionData(input_handles->thumbstick, &analog_action_data, sizeof(vr::InputDigitalActionData_t), vr::k_ulInvalidInputValueHandle);
	if (input_error == vr::VRInputError_None && analog_action_data.bActive) {
		c.stick[0] = analog_action_data.x;
		c.stick[1] = analog_action_data.y;
		if (analog_action_data.x != 0 || analog_action_data.y != 0) {
			if (abs(analog_action_data.x) > abs(analog_action_data.y)) { // LEFT or RIGHT
				if (analog_action_data.x > VR_STEAM_TOUCHTHRESHOLD_STICKDIRECTION) { // RIGHT
					c.buttons_touched |= VR_STEAM_BTNBIT_STICKRIGHT;
					if (analog_action_data.x > VR_STEAM_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
						c.buttons |= VR_STEAM_BTNBIT_STICKRIGHT;
					}
				}
				else if (analog_action_data.x < -VR_STEAM_TOUCHTHRESHOLD_STICKDIRECTION) { // LEFT
					c.buttons_touched |= VR_STEAM_BTNBIT_STICKLEFT;
					if (analog_action_data.x < -VR_STEAM_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
						c.buttons |= VR_STEAM_BTNBIT_STICKLEFT;
					}
				} // else: center
			}
			else { // UP or DOWN
				if (analog_action_data.y > VR_STEAM_TOUCHTHRESHOLD_STICKDIRECTION * 0.7f) { // UP (reduced threshold, because it's hard to hit)
					c.buttons_touched |= VR_STEAM_BTNBIT_STICKUP;
					if (analog_action_data.y > VR_STEAM_PRESSTHRESHOLD_STICKDIRECTION * 0.7f) { // "PRESS"
						c.buttons |= VR_STEAM_BTNBIT_STICKUP;
					}
				}
				else if (analog_action_data.y < -VR_STEAM_TOUCHTHRESHOLD_STICKDIRECTION) { // DOWN
					c.buttons_touched |= VR_STEAM_BTNBIT_STICKDOWN;
					if (analog_action_data.y < -VR_STEAM_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
						c.buttons |= VR_STEAM_BTNBIT_STICKDOWN;
					}
				} // else: center
			}
		}
        input_error = vr_input->GetDigitalActionData(input_handles->thumbstick_press, &digital_action_data, sizeof(vr::InputDigitalActionData_t), vr::k_ulInvalidInputValueHandle);
		const bool thumbstick_pressed = digital_action_data.bActive && digital_action_data.bState;
		if (thumbstick_pressed) {
            c.buttons |= (c.side == Side_Left) ? VR_STEAM_BTNBIT_LEFTSTICK : VR_STEAM_BTNBIT_RIGHTSTICK;
		}
	}
#else
	clock_t now = clock();

	c.grip_pressure = 0.0f;
	if (s.ulButtonPressed & VR_STEAM_SVRGRIPBTN) {
		c.side == Side_Left ? c.buttons |= VR_STEAM_BTNBIT_LEFTGRIP :
							  c.buttons |= VR_STEAM_BTNBIT_RIGHTGRIP;
	}
	if (s.ulButtonPressed & VR_STEAM_SVRMENUBTN) {
		c.buttons |= VR_STEAM_BTNBIT_MENU;
	}
	if (s.ulButtonPressed & VR_STEAM_SVRSYSTEMBTN) {
		c.buttons |= VR_STEAM_BTNBIT_SYSTEM;
	}
	c.buttons_touched = c.buttons;

	// Trigger:
	// override the button with our own trigger pressure threshold
	c.trigger_pressure = 0.0f;
	if (s.rAxis[1].x > 0) {
		c.side == Side_Left ? c.buttons_touched |= VR_STEAM_BTNBIT_LEFTTRIGGER :
							  c.buttons_touched |= VR_STEAM_BTNBIT_RIGHTTRIGGER;
		if (s.rAxis[1].x >= VR_STEAM_TRIGGERPRESSURETHRESHOLD) {
			c.side == Side_Left ? c.buttons |= VR_STEAM_BTNBIT_LEFTTRIGGER :
								  c.buttons |= VR_STEAM_BTNBIT_RIGHTTRIGGER;
			// map pressure to 0~1 for everything above the threshold
			c.trigger_pressure = (s.rAxis[1].x - VR_STEAM_TRIGGERPRESSURETHRESHOLD) / (1.0f - VR_STEAM_TRIGGERPRESSURETHRESHOLD);  // trigger is at index 1 for some reason
		}
	}

	// Touchpad
	bool touchpad_touched, touchpad_pressed;
	if (s.ulButtonTouched & VR_STEAM_SVRDPADBTN) {
		touchpad_touched = true;
	}
	else {
		touchpad_touched = false;
	}
	if (s.ulButtonPressed & VR_STEAM_SVRDPADBTN) {
		touchpad_pressed = true;
	}
	else {
		touchpad_pressed = false;
	}

	if (touchpad_touched) {
		const vr::VRControllerAxis_t& trackpad_axis = s.rAxis[0]; // trackpad is at index 0 for some reason
		if (trackpad_axis.x != 0 || trackpad_axis.y != 0) {
			memcpy(c.dpad, &trackpad_axis, sizeof(vr::VRControllerAxis_t));
		}
	}

	// Convert touchpad position to button
	static ui64 touchpad_btn[2] = { 0,0 }; // static, so that it stays on the last button until I move on some other button
	if (c.dpad[0] != 0 || c.dpad[1] != 0) {
		if (std::abs(c.dpad[0]) > std::abs(c.dpad[1])) { // LEFT or RIGHT
			if (c.dpad[0] > VR_STEAM_TRACKPADDIRECTIONTHRESHOLD) { // RIGHT
				touchpad_btn[c.side] = VR_STEAM_BTNBIT_DPADRIGHT;
			}
			else if (c.dpad[0] < -VR_STEAM_TRACKPADDIRECTIONTHRESHOLD) { // LEFT
				touchpad_btn[c.side] = VR_STEAM_BTNBIT_DPADLEFT;
			}
			else { // CENTER
				c.side == Side_Left ? touchpad_btn[c.side] = VR_STEAM_BTNBIT_LEFTDPAD :
									  touchpad_btn[c.side] = VR_STEAM_BTNBIT_RIGHTDPAD;
			}
		}
		else { // UP or DOWN
			if (c.dpad[1] > 0.05f) { // UP (reduced threshold, because it's hard to hit)
				touchpad_btn[c.side] = VR_STEAM_BTNBIT_DPADUP;
			}
			else if (c.dpad[1] < -VR_STEAM_TRACKPADDIRECTIONTHRESHOLD) { // DOWN
				touchpad_btn[c.side] = VR_STEAM_BTNBIT_DPADDOWN;
			}
			else { // CENTER
				c.side == Side_Left ? touchpad_btn[c.side] = VR_STEAM_BTNBIT_LEFTDPAD :
									  touchpad_btn[c.side] = VR_STEAM_BTNBIT_RIGHTDPAD;
			}
		}
	}

	// Touchpad touch:
	static clock_t prior_touch_touchpad[2] = { 0, 0 }; // for smoothing micro touch-losses
	if (touchpad_touched || (now - prior_touch_touchpad[c.side]) < VR_STEAM_DEBOUNCEPERIOD) {
		// if we're pressing a button, we stick with that until we let go
		if (prior_touchpad_pressed) {
			c.buttons_touched |= prior_touchpad_pressed;
		}
		else {
			c.buttons_touched |= touchpad_btn[c.side];
		}
		if (touchpad_touched) {
			prior_touch_touchpad[c.side] = now;
		}
	}

	// Touchpad press:
	// When the touchpad is pressed, find out in what quadrant (left, right, up, down)
	static clock_t prior_press_touchpad[2] = { 0, 0 }; // for smoothing micro touch-losses
	if (touchpad_pressed || (now - prior_press_touchpad[c.side]) < VR_STEAM_DEBOUNCEPERIOD) {
		// if we already are pressing one of the buttons, continue using it,
		if (prior_touchpad_pressed) {
			c.buttons |= prior_touchpad_pressed;
		}
		else {
			c.buttons |= touchpad_btn[c.side];
		}
		if (touchpad_pressed) {
			prior_press_touchpad[c.side] = now;
		}
	}

	if ((this->hmd_type != HMDType_WindowsMR) && 
		(this->hmd_type != HMDType_Index)) {
		return;
	}

	ui64& btn_press = c.buttons;
	ui64& btn_touch = c.buttons_touched;
	// Special treatment for the stick
	const vr::VRControllerAxis_t& thumbstick_axis = s.rAxis[2]; // joystick is at index 2 for some reason
	if (thumbstick_axis.x != 0 || thumbstick_axis.y != 0) {
		memcpy(c.stick, &thumbstick_axis, sizeof(vr::VRControllerAxis_t));
		if (std::abs(thumbstick_axis.x) > std::abs(thumbstick_axis.y)) { // LEFT or RIGHT
			if (thumbstick_axis.x > VR_STEAM_TOUCHTHRESHOLD_STICKDIRECTION) { // RIGHT
				btn_touch |= VR_STEAM_BTNBIT_STICKRIGHT;
				if (thumbstick_axis.x > VR_STEAM_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
					btn_press |= VR_STEAM_BTNBIT_STICKRIGHT;
				}
			}
			else if (thumbstick_axis.x < -VR_STEAM_TOUCHTHRESHOLD_STICKDIRECTION) { // LEFT
				btn_touch |= VR_STEAM_BTNBIT_STICKLEFT;
				if (thumbstick_axis.x < -VR_STEAM_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
					btn_press |= VR_STEAM_BTNBIT_STICKLEFT;
				}
			} // else: center
		}
		else { // UP or DOWN
			if (thumbstick_axis.y > VR_STEAM_TOUCHTHRESHOLD_STICKDIRECTION*0.7f) { // UP (reduced threshold, because it's hard to hit)
				btn_touch |= VR_STEAM_BTNBIT_STICKUP;
				if (thumbstick_axis.y > VR_STEAM_PRESSTHRESHOLD_STICKDIRECTION*0.7f) { // "PRESS"
					btn_press |= VR_STEAM_BTNBIT_STICKUP;
				}
			}
			else if (thumbstick_axis.y < -VR_STEAM_TOUCHTHRESHOLD_STICKDIRECTION) { // DOWN
				btn_touch |= VR_STEAM_BTNBIT_STICKDOWN;
				if (thumbstick_axis.y < -VR_STEAM_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
					btn_press |= VR_STEAM_BTNBIT_STICKDOWN;
				}
			} // else: center
		}
	}
#endif
}

//                                                                          ________________________
//_________________________________________________________________________/     blitEye()
/**
 * Blit a rendered image into the internal eye texture.
  * TODO_XR: aperture_u and aperture_v currently don't do anything in the shader.
 */
int VR_Steam::blitEye(Side side, void* texture_resource, const float& aperture_u, const float& aperture_v)
{
	if (!this->initialized) {
		return VR::Error_NotInitialized;
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

	glUniform4f(this->gl.param_location, aperture_u, aperture_v, 1.0f / this->gamma, 0);

	// Render the provided texture into the hmd eye texture.
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

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/     blitEyes()
/**
 * Submit rendered images into the internal eye textures.
  * TODO_XR: aperture_u and aperture_v currently don't do anything in the shader.
 */
int VR_Steam::blitEyes(void* texture_resource_left, void* texture_resource_right, const float& aperture_u, const float& aperture_v)
{
	if (!this->initialized) {
		return VR::Error_NotInitialized;
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
	glUniform4f(this->gl.param_location, aperture_u, aperture_v, 1.0f / this->gamma, 0);
	glBindVertexArray(this->gl.vertex_array);
	glEnableVertexAttribArray(this->gl.position_location);
	glEnableVertexAttribArray(this->gl.uv_location);

	for (int i = 0; i < 2; ++i) {
		// Bind my render buffer as render target eye render target
		glBindFramebuffer(GL_FRAMEBUFFER, this->gl.framebuffer[i]);
		glViewport(0, 0, this->texture_width, this->texture_height);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		glUniform4f(this->gl.param_location, aperture_u, aperture_v, 1.0f / this->gamma, 0);

		// Render the provided texture into the HMD eye texture.
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

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/    submitFrame()
/**
 * Submit frame to device / screen.
 */
int VR_Steam::submitFrame()
{
	if (!this->initialized) {
		return VR::Error_NotInitialized;
	}

	vr::IVRCompositor* compositor = vr::VRCompositor();
	if (!compositor) {
		return VR::Error_NotInitialized;
	}

	vr::Texture_t leftEyeTexture = { (void*)this->gl.texture[Side_Left], vr::TextureType_OpenGL,vr::ColorSpace_Gamma };
	compositor->Submit(vr::Eye_Left, &leftEyeTexture, 0);
	vr::Texture_t rightEyeTexture = { (void*)this->gl.texture[Side_Right], vr::TextureType_OpenGL,vr::ColorSpace_Gamma };
	compositor->Submit(vr::Eye_Right, &rightEyeTexture, 0);
	compositor->PostPresentHandoff();

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/  getDefaultEyeTexSize
/**
 * Get the default eye texture size.
 */
int VR_Steam::getDefaultEyeTexSize(uint& w, uint& h, Side side)
{
	if (!this->hmd) {
		int e = this->acquireHMD();
		if (e || !this->hmd) {
			return VR::Error_NotInitialized;
		}
	}

	this->hmd->GetRecommendedRenderTargetSize(&w, &h);

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/  getDefaultEyeParams
/**
 * Get the HMD's default parameters.
 */
int VR_Steam::getDefaultEyeParams(Side side, float& fx, float& fy, float& cx, float& cy)
{
	if (!this->hmd) {
		int e = this->acquireHMD();
		if (e || !this->hmd) {
			return VR::Error_NotInitialized;
		}
	}

	float left, right, top, bottom;
	this->hmd->GetProjectionRaw((side == Side_Left) ? vr::Eye_Left : vr::Eye_Right, &left, &right, &top, &bottom);

	// OpenVR may consider the y-axis pointing down
	if (top < bottom) {
		top = -top;
		bottom = -bottom;
	}

	float width = right - left;
	float height = top - bottom;
	cx = -left / width;
	cy = -bottom / height;
	fx = 1.0f / width;
	fy = 1.0f / height;

	return VR_Steam::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/     setEyeParams()
/**
 * Set the HMD's projection parameters.
 * For correct distortion rendering and possibly other internal things,
 * the HMD might need to know these.
 * \param   side    Side of the eye which to set.
 * \param   fx      Horizontal focal length, in "image-width"-units (1=image width).
 * \param   fy      Vertical focal length, in "image-height"-units (1=image height).
 * \param   cx      Horizontal principal point, in "image-width"-units (0.5=image center).
 * \param   cy      Vertical principal point, in "image-height"-units (0.5=image center).
 */
int VR_Steam::setEyeParams(Side side, float fx, float fy, float cx, float cy)
{
	// Interestingly, openvr does not care about projection parameters
	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/    setEyeOffset
/**
 * Override the offset of the eyes (camera positions) relative to the HMD.
 */
int VR_Steam::setEyeOffset(Side side, float x, float y, float z)
{
	if (side != Side_Left && side != Side_Right) {
		return VR::Error_InvalidParameter;
	}

	// store the eye-to-hmd offset
	t_hmd2eye[side][0][0] = 1;   t_hmd2eye[side][0][1] = 0;   t_hmd2eye[side][0][2] = 0;   t_hmd2eye[side][0][3] = 0;
	t_hmd2eye[side][1][0] = 0;   t_hmd2eye[side][1][1] = 1;   t_hmd2eye[side][1][2] = 0;   t_hmd2eye[side][1][3] = 0;
	t_hmd2eye[side][2][0] = 0;   t_hmd2eye[side][2][1] = 0;   t_hmd2eye[side][2][2] = 1;   t_hmd2eye[side][2][3] = 0;
	t_hmd2eye[side][3][0] = x;   t_hmd2eye[side][3][1] = y;   t_hmd2eye[side][3][2] = z;   t_hmd2eye[side][3][3] = 1;
	this->eye_offset_override[side] = true; // don't use defaults anymore

	return VR_Steam::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/    action_manifest
/**
 * Action manifest file.
 */
const char* const VR_Steam::Input::action_manifest = R"ACTION_MANIFEST(
{
   "action_sets" : [
      {
         "name" : "/actions/main",
         "usage" : "leftright"
      }
   ],
   "actions" : [
      {
         "name" : "/actions/main/in/pos_left",
         "requirement" : "optional",
         "type" : "pose"
      },
      {
         "name" : "/actions/main/in/pos_right",
         "requirement" : "optional",
         "type" : "pose"
      },
      {
         "name" : "/actions/main/in/trigger_left",
         "requirement" : "optional",
         "type" : "vector1"
      },
      {
         "name" : "/actions/main/in/trigger_right",
         "requirement" : "optional",
         "type" : "vector1"
      },
      {
         "name" : "/actions/main/in/grip_left",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/grip_right",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/grip_touch_left",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/grip_touch_right",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/grip_force_left",
         "requirement" : "optional",
         "type" : "vector1"
      },
      {
         "name" : "/actions/main/in/grip_force_right",
         "requirement" : "optional",
         "type" : "vector1"
      },
      {
         "name" : "/actions/main/in/touchpad_left",
         "requirement" : "optional",
         "type" : "vector2"
      },
      {
         "name" : "/actions/main/in/touchpad_right",
         "requirement" : "optional",
         "type" : "vector2"
      },
      {
         "name" : "/actions/main/in/touchpad_press_left",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/touchpad_press_right",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/touchpad_touch_left",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/touchpad_touch_right",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/thumbstick_left",
         "requirement" : "optional",
         "type" : "vector2"
      },
      {
         "name" : "/actions/main/in/thumbstick_right",
         "requirement" : "optional",
         "type" : "vector2"
      },
      {
         "name" : "/actions/main/in/thumbstick_press_left",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/thumbstick_press_right",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/button_a_left",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/button_a_right",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/button_a_touch_left",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/button_a_touch_right",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/button_b_left",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/button_b_right",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/button_b_touch_left",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/button_b_touch_right",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/button_menu_left",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/button_menu_right",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/button_menu_touch_left",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/in/button_menu_touch_right",
         "requirement" : "optional",
         "type" : "boolean"
      },
      {
         "name" : "/actions/main/out/haptic_left",
         "requirement" : "optional",
         "type" : "vibration"
      },
      {
         "name" : "/actions/main/out/haptic_right",
         "requirement" : "optional",
         "type" : "vibration"
      }
   ],
   "default_bindings" : [
      {
         "controller_type": "vive_controller",
         "binding_url" : "binding_vive.json"
      },
      {
         "controller_type" : "holographic_controller",
         "binding_url" : "binding_windowsmr.json"
      },
      {
         "controller_type" : "knuckles",
         "binding_url" : "binding_index.json"
      },
      {
         "controller_type" : "logitech_stylus",
         "binding_url" : "binding_logitechink.json"
      },
      {
         "controller_type" : "vive_cosmos_controller",
         "binding_url" : "binding_cosmos.json"
      }
   ],
   "localization" : [
      {
         "/actions/main" : "BlenderXR controller bindings",
         "/actions/main/in/button_a_left" : "Left controller A button",
         "/actions/main/in/button_a_right" : "Right controller A button",
         "/actions/main/in/grip_left" : "Left controller grip (shoulder) button",
         "/actions/main/in/grip_right" : "Right controller grip (shoulder) button",
         "/actions/main/in/pos_left" : "Left controller position",
         "/actions/main/in/pos_right" : "Right controller position",
         "/actions/main/in/trigger_left" : "Left controller trigger button",
         "/actions/main/in/trigger_right" : "Right controller trigger button",
         "/actions/main/out/haptic_left" : "Left haptic feedback",
         "/actions/main/out/haptic_right" : "Right haptic feedback",
         "language_tag" : "en"
      }
   ]
}
)ACTION_MANIFEST";

//                                                                          ________________________
//_________________________________________________________________________/    binding_vive
/**
 * Binding for HTC Vive controllers.
 */
const char* const VR_Steam::Input::binding_vive = R"BINDING_VIVE({
   "alias_info" : {},
   "app_key" : "system.generated.blender.exe",
   "bindings" : {
      "/actions/main" : {
         "haptics" : [
            {
               "output" : "/actions/main/out/haptic_right",
               "path" : "/user/hand/right/output/haptic"
            },
            {
               "output" : "/actions/main/out/haptic_left",
               "path" : "/user/hand/left/output/haptic"
            }
         ],
         "poses" : [
            {
               "output" : "/actions/main/in/pos_left",
               "path" : "/user/hand/left/pose/raw"
            },
            {
               "output" : "/actions/main/in/pos_right",
               "path" : "/user/hand/right/pose/raw"
            }
         ],
         "sources" : [
            {
               "inputs" : {
                  "pull" : {
                     "output" : "/actions/main/in/trigger_left"
                  }
               },
               "mode" : "trigger",
               "path" : "/user/hand/left/input/trigger"
            },
            {
               "inputs" : {
                  "pull" : {
                     "output" : "/actions/main/in/trigger_right"
                  }
               },
               "mode" : "trigger",
               "path" : "/user/hand/right/input/trigger"
            },
            {
                "path" : "/user/hand/left/input/grip",
                "mode": "button",
				"inputs": {
					"click": {
						"output": "/actions/main/in/grip_left"
					},
					"touch": {
						"output": "/actions/main/in/grip_touch_left"
					}
				}
            },
            {
               "path" : "/user/hand/right/input/grip",
                "mode": "button",
				"inputs": {
					"click": {
						"output": "/actions/main/in/grip_right"
					},
					"touch": {
						"output": "/actions/main/in/grip_touch_right"
					}
				}
            },
            {
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/touchpad_press_left"
                  },
				  "touch": {
					 "output": "/actions/main/in/touchpad_touch_left"
				  },
                  "position" : {
                     "output" : "/actions/main/in/touchpad_left"
                  }
               },
               "mode" : "trackpad",
               "path" : "/user/hand/left/input/trackpad"
            },
            {
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/touchpad_press_right"
                  },
				  "touch": {
					 "output": "/actions/main/in/touchpad_touch_right"
				  },
                  "position" : {
                     "output" : "/actions/main/in/touchpad_right"
                  }
               },
               "mode" : "trackpad",
               "path" : "/user/hand/right/input/trackpad"
            },
            {
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/button_a_left"
                  }
               },
               "mode" : "button",
               "path" : "/user/hand/right/input/b"
            },
            {
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/button_a_right"
                  }
               },
               "mode" : "button",
               "path" : "/user/hand/right/input/b"
            },
            {
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/button_menu_left"
                  }
               },
               "mode" : "button",
               "path" : "/user/hand/left/input/application_menu"
            },
            {
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/button_menu_right"
                  }
               },
               "mode" : "button",
               "path" : "/user/hand/right/input/application_menu"
            }
         ]
      }
   },
   "controller_type" : "vive_controller",
   "description" : "Binding for BlenderXR for Vive controllers (v1)",
   "name" : "BlenderXR binding for Vive controllers (v1)",
   "options" : {},
   "simulated_actions" : []
}
)BINDING_VIVE";

//                                                                          ________________________
//_________________________________________________________________________/    binding_windowsmr
/**
 * Binding for Microsoft Windows MR type controllers.
 */
 const char* const VR_Steam::Input::binding_windowsmr = R"BINDING_WINMR({
   "alias_info" : {},
   "app_key" : "system.generated.blender.exe",
   "bindings" : {
      "/actions/main" : {
         "haptics" : [
            {
               "output" : "/actions/main/out/haptic_right",
               "path" : "/user/hand/right/output/haptic"
            },
            {
               "output" : "/actions/main/out/haptic_left",
               "path" : "/user/hand/left/output/haptic"
            }
         ],
         "poses" : [
            {
               "output" : "/actions/main/in/pos_left",
               "path" : "/user/hand/left/pose/raw"
            },
            {
               "output" : "/actions/main/in/pos_right",
               "path" : "/user/hand/right/pose/raw"
            }
         ],
         "sources" : [
            {
               "path" : "/user/hand/left/input/trigger",
               "mode" : "trigger",
               "inputs" : {
                  "pull" : {
                     "output" : "/actions/main/in/trigger_left"
                  }
               }
            },
            {
               "path" : "/user/hand/right/input/trigger",
               "mode" : "trigger",
               "inputs" : {
                  "pull" : {
                     "output" : "/actions/main/in/trigger_right"
                  }
               }
            },
            {
               "path" : "/user/hand/left/input/grip",
               "mode" : "button",
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/grip_left"
                  },
				  "touch": {
					 "output": "/actions/main/in/grip_touch_left"
				  }
               }
            },
            {
               "path" : "/user/hand/right/input/grip",
               "mode" : "button",
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/grip_right"
                  },
				  "touch": {
					 "output": "/actions/main/in/grip_touch_right"
				  }
               }
            },
            {
               "path" : "/user/hand/left/input/trackpad",
               "mode" : "trackpad",
               "inputs" : {
                  "position" : {
                     "output" : "/actions/main/in/touchpad_left"
                  },
                  "click" : {
                     "output" : "/actions/main/in/touchpad_press_left"
                  },
				  "touch": {
					 "output": "/actions/main/in/touchpad_touch_left"
			      }
               }
            },
            {
               "path" : "/user/hand/right/input/trackpad",
               "mode" : "trackpad",
               "inputs" : {
                  "position" : {
                     "output" : "/actions/main/in/touchpad_right"
                  },
                  "click" : {
                     "output" : "/actions/main/in/touchpad_press_right"
                  },
				  "touch": {
					 "output": "/actions/main/in/touchpad_touch_right"
			      }
               }
            },
            {
               "path" : "/user/hand/left/input/application_menu",
               "mode" : "button",
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/button_menu_left"
                  }
               }
            },
            {
               "path" : "/user/hand/right/input/application_menu",
               "mode" : "button",
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/button_menu_right"
                  }
               }
            },
            {
               "path" : "/user/hand/left/input/joystick",
               "mode" : "joystick",
               "inputs" : {
                  "position" : {
                     "output" : "/actions/main/in/thumbstick_left"
                  },
                  "click" : {
                     "output" : "/actions/main/in/thumbstick_press_left"
                  }
               }
            },
            {
               "path" : "/user/hand/right/input/joystick",
               "mode" : "joystick",
               "inputs" : {
                  "position" : {
                     "output" : "/actions/main/in/thumbstick_right"
                  },
                  "click" : {
                     "output" : "/actions/main/in/thumbstick_press_right"
                  }
               }
            }
         ]
      }
   },
   "controller_type" : "holographic_controller",
   "description" : "Binding for BlenderXR for Windows MR controllers (v1)",
   "name" : "BlenderXR binding for Windows MR controllers (v1)",
   "options" : {},
   "simulated_actions" : []
}
)BINDING_WINMR";

//                                                                          ________________________
//_________________________________________________________________________/    binding_index
/**
 * Binding for Valve Index (Knuckles) controllers.
 */
const char* const VR_Steam::Input::binding_index = R"BINDING_INDEX({
   "alias_info" : {},
   "app_key" : "system.generated.blender.exe",
   "bindings" : {
      "/actions/main": {
        "poses": [
            {
                "path": "/user/hand/left/pose/raw",
                "output": "/actions/main/in/pos_left"
            },
            {
                "path": "/user/hand/right/pose/raw",
                "output": "/actions/main/in/pos_right"
            }
        ],
        "haptics": [
            {
                "output": "/actions/main/out/haptic_left",
                "path": "/user/hand/left/output/haptic"
            },
            {
                "output": "/actions/main/out/haptic_right",
                "path": "/user/hand/right/output/haptic"
            }
        ],
        "sources": [
            {
                "path": "/user/hand/left/input/trigger",
                "mode": "trigger",
                "inputs": {
                    "pull": {
                        "output": "/actions/main/in/trigger_left"
                    }
                }
            },
            {
                "path": "/user/hand/right/input/trigger",
                "mode": "trigger",
                "inputs": {
                    "pull": {
                        "output": "/actions/main/in/trigger_right"
                    }
                }
            },
            {
                "path": "/user/hand/left/input/grip",
                "mode": "force_sensor",
				"inputs": {
					"force": {
						"output": "/actions/main/in/grip_force_left"
					}
				}
            },
            {
                "path": "/user/hand/right/input/grip",
                "mode": "force_sensor",
				"inputs": {
					"force": {
						"output": "/actions/main/in/grip_force_right"
					}
				}
            },
            {
                "path": "/user/hand/left/input/a",
                "mode": "button",
                "inputs": {
                    "click": {
                        "output": "/actions/main/in/button_a_left"
                    },
					"touch": {
						"output": "/actions/main/in/button_a_touch_left"
					}
                }
            },
            {
                "path": "/user/hand/right/input/a",
                "mode": "button",
                "inputs": {
                    "click": {
                        "output": "/actions/main/in/button_a_right"
                    },
					"touch": {
						"output": "/actions/main/in/button_a_touch_right"
					}
                }
            },
            {
                "path": "/user/hand/left/input/b",
                "mode": "button",
                "inputs": {
                    "click": {
                        "output": "/actions/main/in/button_b_left"
                    },
					"touch": {
						"output": "/actions/main/in/button_b_touch_left"
					}
                }
            },
            {
                "path": "/user/hand/right/input/b",
                "mode": "button",
                "inputs": {
                    "click": {
                        "output": "/actions/main/in/button_b_right"
                    },
					"touch": {
						"output": "/actions/main/in/button_b_touch_right"
					}
                }
            },
            {
                "path": "/user/hand/left/input/thumbstick",
                "mode": "joystick",
                "inputs": {
                    "position": {
                        "output": "/actions/main/in/thumbstick_left"
                    },
                    "click": {
                        "output": "/actions/main/in/thumbstick_press_left"
                    }
                }
            },
            {
                "path": "/user/hand/right/input/thumbstick",
                "mode": "joystick",
                "inputs": {
                    "position": {
                        "output": "/actions/main/in/thumbstick_right"
                    },
                    "click": {
                        "output": "/actions/main/in/thumbstick_press_right"
                    }
                }
            },
            {
                "path": "/user/hand/left/input/trackpad",
                "mode": "trackpad",
                "inputs": {
                    "position": {
                        "output": "/actions/main/in/touchpad_left"
                    },
                    "touch": {
                        "output": "/actions/main/in/touchpad_press_left"
                    }
                }
            },
            {
                "path": "/user/hand/right/input/trackpad",
                "mode": "trackpad",
                "inputs": {
                    "position": {
                        "output": "/actions/main/in/touchpad_right"
                    },
                    "touch": {
                        "output": "/actions/main/in/touchpad_press_right"
                    }
                }
            }
        ]
      }
   },
   "controller_type" : "knuckles",
   "description" : "BlenderXR default configuration for Index (Knuckles) controllers (v1)",
   "name" : "BlenderXR default configuration for Index (Knuckles) controllers (v1)",
   "options" : {},
   "simulated_actions" : []
}
)BINDING_INDEX";

//                                                                          ________________________
//_________________________________________________________________________/    binding_cosmos
/**
 * Binding for HTC Vive Cosmos controllers.
 */
const char* const VR_Steam::Input::binding_cosmos = R"BINDING_COSMOS({
   "alias_info" : {},
   "app_key" : "system.generated.maya.exe",
   "bindings" : {
      "/actions/main" : {
         "haptics" : [
            {
               "output" : "/actions/main/out/haptic_right",
               "path" : "/user/hand/right/output/haptic"
            },
            {
               "output" : "/actions/main/out/haptic_left",
               "path" : "/user/hand/left/output/haptic"
            }
         ],
         "poses" : [
            {
               "output" : "/actions/main/in/pos_left",
               "path" : "/user/hand/left/pose/raw"
            },
            {
               "output" : "/actions/main/in/pos_right",
               "path" : "/user/hand/right/pose/raw"
            }
         ],
         "sources" : [
            {
               "path" : "/user/hand/left/input/system",
               "mode" : "button",
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/button_menu_left"
                  }
               }
            },
            {
               "path" : "/user/hand/right/input/system",
               "mode" : "button",
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/button_menu_right"
                  }
               }
            },
            {
               "path" : "/user/hand/left/input/grip",
               "mode" : "button",
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/grip_left"
                  },
				  "touch": {
					 "output": "/actions/main/in/grip_touch_left"
				  }
               }
            },
            {
               "path" : "/user/hand/right/input/grip",
               "mode" : "button",
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/grip_right"
                  },
				  "touch": {
					 "output": "/actions/main/in/grip_touch_right"
				  }
               }
            },
			{
               "path" : "/user/hand/left/input/bumper",
               "mode" : "button",
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/left_bumper_press"
                  }
               }
            },
			{
               "path" : "/user/hand/right/input/bumper",
               "mode" : "button",
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/right_bumper_press"
                  }
               }
            },
            {
               "path" : "/user/hand/left/input/joystick",
               "mode" : "joystick",
               "inputs" : {
                  "position" : {
                     "output" : "/actions/main/in/thumbstick_left"
                  },
                  "click" : {
                     "output" : "/actions/main/in/thumbstick_press_left"
                  },
                  "touch" : {
                     "output" : "/actions/main/in/thumbstick_touch_left"
                  }
               }
            },
            {
               "path" : "/user/hand/right/input/joystick",
               "mode" : "joystick",
               "inputs" : {
                  "position" : {
                     "output" : "/actions/main/in/thumbstick_right"
                  },
                  "click" : {
                     "output" : "/actions/main/in/thumbstick_press_right"
                  },
                  "touch" : {
                     "output" : "/actions/main/in/thumbstick_touch_right"
                  }
               }
            },
            {
               "path" : "/user/hand/left/input/trigger",
               "mode" : "trigger",
               "inputs" : {
                  "pull" : {
                     "output" : "/actions/main/in/trigger_left"
                  },
                  "touch" : {
                     "output" : "/actions/main/in/trigger_touch_left"
                  }
               }
            },
            {
               "path" : "/user/hand/right/input/trigger",
               "mode" : "trigger",
               "inputs" : {
                  "pull" : {
                     "output" : "/actions/main/in/trigger_right"
                  },
                  "touch" : {
                     "output" : "/actions/main/in/trigger_touch_right"
                  }
               }
            },
			{
               "path" : "/user/hand/left/input/x",
               "mode" : "button",
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/button_a_left"
                  }
               }
            },          
			{
               "path" : "/user/hand/right/input/a",
               "mode" : "button",
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/button_a_right"
                  }
               }
            },
			{
               "path" : "/user/hand/left/input/y",
               "mode" : "button",
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/button_b_left"
                  }
               }
            },
			{
               "path" : "/user/hand/right/input/b",
               "mode" : "button",
               "inputs" : {
                  "click" : {
                     "output" : "/actions/main/in/button_b_right"
                  }
               }
            }
         ]
      }
   },
   "controller_type" : "vive_cosmos_controller",
   "description" : "Binding for MARUI for Vive Cosmos controllers (v1)",
   "name" : "MARUI binding for Vive Cosmos controllers (v1)",
   "options" : {},
   "simulated_actions" : []
}
)BINDING_COSMOS";


//                                                                          ________________________
//_________________________________________________________________________/    binding_logitechink
/**
 * Binding for Logitech VR Ink pen.
 */
 const char* const VR_Steam::Input::binding_logitechink = R"BINDING_LOGIINK({
   "alias_info" : {},
   "app_key" : "system.generated.blender.exe",
   "bindings" : {
      "/actions/main": {
        "poses": [
            {
                "path": "/user/hand/left/pose/tip",
                "output": "/actions/main/in/pos_left"
            },
            {
                "path": "/user/hand/right/pose/tip",
                "output": "/actions/main/in/pos_right"
            }
        ],
        "haptics": [
            {
                "output": "/actions/main/out/haptic_left",
                "path": "/user/hand/left/output/haptic"
            },
            {
                "output": "/actions/main/out/haptic_right",
                "path": "/user/hand/right/output/haptic"
            }
        ],
        "sources": [
            {
                "path": "/user/hand/left/input/primary",
                "mode": "force_sensor",
                "inputs": {
                    "force": {
                        "output": "/actions/main/in/trigger_left"
                    }
                }
            },
            {
                "path": "/user/hand/right/input/primary",
                "mode": "force_sensor",
                "inputs": {
                    "force": {
                        "output": "/actions/main/in/trigger_right"
                    }
                }
            },
            {
                "path" : "/user/hand/left/input/grip",
                "mode": "button",
				"inputs": {
					"click": {
						"output": "/actions/main/in/grip_left"
					},
					"touch": {
						"output": "/actions/main/in/grip_touch_left"
					}
				}
            },
            {
               "path" : "/user/hand/right/input/grip",
                "mode": "button",
				"inputs": {
					"click": {
						"output": "/actions/main/in/grip_right"
					},
					"touch": {
						"output": "/actions/main/in/grip_touch_right"
					}
				}
            },
            {
                "path": "/user/hand/left/input/menu",
                "mode": "button",
                "inputs": {
                    "click": {
                        "output": "/actions/main/in/button_menu_left"
                    }
                }
            },
            {
                "path": "/user/hand/right/input/menu",
                "mode": "button",
                "inputs": {
                    "click": {
                        "output": "/actions/main/in/button_menu_right"
                    }
                }
            },
            {
                "path": "/user/hand/left/input/touchstrip",
                "mode": "trackpad",
                "inputs": {
                    "position": {
                        "output": "/actions/main/in/touchpad_left"
                    },
                    "click" : {
                        "output" : "/actions/main/in/touchpad_press_left"
                    },
                    "touch": {
                        "output": "/actions/main/in/touchpad_touch_left"
                    }
                }
            },
            {
                "path": "/user/hand/right/input/touchstrip",
                "mode": "trackpad",
                "inputs": {
                    "position": {
                        "output": "/actions/main/in/touchpad_right"
                    },
                    "click" : {
                        "output" : "/actions/main/in/touchpad_press_right"
                    },
                    "touch": {
                        "output": "/actions/main/in/touchpad_touch_right"
                    }
                }
            }
        ]
      }
   },
   "controller_type" : "logitech_stylus",
   "description" : "BlenderXR default configuration for Logitech VR Ink stylus (v1)",
   "name" : "BlenderXR default configuration for Logitech VR Ink stylus (v1)",
   "options" : {},
   "simulated_actions" : []
}
)BINDING_LOGIINK";

/***********************************************************************************************//**
*								Exported shared library functions.								   *
***************************************************************************************************/
VR_Steam* c_obj(0);

/**
 * Create an object internally. Must be called before the functions below.
 */
int c_createVR()
{
	c_obj = new VR_Steam();
	return 0;
}

/**
 * Initialize the internal object (OpenGL).
 */
#ifdef _WIN32
int c_initVR(void* device, void* context)
#else
int c_initVR(void* display, void* drawable, void* context)
#endif
{
#ifdef _WIN32
	return c_obj->init(device, context);
#else
	return c_obj->init(display, drawable, context);
#endif
}

/**
 * Get the type of HMD used for VR.
 */
int c_getHMDType(int* type)
{
	/* TODO_XR: Decouple controller type from HMD type.
	 * For now, this will set the HMD type based on the controller type. 
	 * (This function should only be called once when starting a new VR session from blender). */
	c_obj->updateTracking();

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
	if (c_obj) {
		int error = c_obj->uninit();
		delete c_obj;
		c_obj = 0;
		return error;
	}
	return 0;
}
