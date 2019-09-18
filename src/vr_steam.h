/***********************************************************************************************//**
 * \file      vr_steam.h
 *            Valve openvr HMD / VR module for use with SteamVR.
 *            This file contains code related to using Valve's openvr / SteamVR API for HMDs and controllers.
 *            Both tracking and rendering are implemented.
 ***************************************************************************************************
 * \brief     Valve openvr API module for use with SteamVR.
 * \copyright MARUI-PlugIn (inc.)
 * \contributors Multiplexed Reality
 **************************************************************************************************/
#ifndef __VR_STEAM_H__
#define __VR_STEAM_H__

#include "vr.h"

#define VR_STEAM_DEBOUNCEPERIOD                 200		//!< Debounce period to avoid jumpy/slacky touchpad touches.
#define VR_STEAM_TRIGGERPRESSURETHRESHOLD       0.3f	//!< Threshold for trigger pressure to be registered as "pressed".
#define VR_STEAM_GRIPPRESSURETHRESHOLD			0.4f	//!< Threshold for grip pressure to be registered as "pressed".
#define VR_STEAM_TRACKPADDIRECTIONTHRESHOLD     0.3f	//!< Threshold for trackpad interaction to be registered as an direction to somewhere.
#define VR_STEAM_TOUCHTHRESHOLD_STICKDIRECTION  0.4f	//!< Threshold for thumb-stick interaction to be registered as an direction to somewhere to be a "touch".
#define VR_STEAM_PRESSTHRESHOLD_STICKDIRECTION  0.9f	//!< Threshold for thumb-stick interaction to be registered as an direction to somewhere to be a "touch".

// SteamVR button bits.
#define VR_STEAM_SVRTRIGGERBTN		(uint64_t(1)<<vr::k_EButton_SteamVR_Trigger)	//!< SteamVR bit of the controller trigger as a button.
#define VR_STEAM_SVRGRIPBTN			(uint64_t(1)<<vr::k_EButton_Grip)				//!< SteamVR bit for pressing the grip.
#define VR_STEAM_SVRDPADBTN			(uint64_t(1)<<vr::k_EButton_SteamVR_Touchpad)	//!< SteamVR bit for pressing the trackpad in general.
#define VR_STEAM_SVRMENUBTN			(uint64_t(1)<<vr::k_EButton_ApplicationMenu)	//!< SteamVR bit for pressing the menu button.
#define VR_STEAM_SVRSYSTEMBTN		(uint64_t(1)<<vr::k_EButton_System)				//!< SteamVR bit for pressing the system button.

// Widget_Layout button bits.
#define VR_STEAM_BTNBIT_LEFTTRIGGER		(uint64_t(1) << 0)		//!< Button bit for the left trigger.
#define VR_STEAM_BTNBIT_RIGHTTRIGGER	(uint64_t(1) << 1)		//!< Button bit for the right trigger.
#define VR_STEAM_BTNBIT_LEFTGRIP		(uint64_t(1) << 2)		//!< Button bit for the left grip.
#define VR_STEAM_BTNBIT_RIGHTGRIP		(uint64_t(1) << 3)		//!< Button bit for the right grip.
#define VR_STEAM_BTNBIT_DPADLEFT		(uint64_t(1) << 4)		//!< Button bit for pushing the trackpad left.
#define VR_STEAM_BTNBIT_DPADRIGHT		(uint64_t(1) << 5)		//!< Button bit for pushing the trackpad right.
#define VR_STEAM_BTNBIT_DPADUP			(uint64_t(1) << 6)		//!< Button bit for pushing the trackpad up.
#define VR_STEAM_BTNBIT_DPADDOWN		(uint64_t(1) << 7)		//!< Button bit for pushing the trackpad down.
#define VR_STEAM_BTNBITS_DPADANY		(VR_STEAM_BTNBIT_DPADLEFT|VR_STEAM_BTNBIT_DPADRIGHT|VR_STEAM_BTNBIT_DPADUP|VR_STEAM_BTNBIT_DPADDOWN)	//!< Button bits for pushing any of the trackpad "buttons".
#define VR_STEAM_BTNBIT_LEFTDPAD		(uint64_t(1) << 8)		//!< Button bit for pressing down on the left stick.
#define VR_STEAM_BTNBIT_RIGHTDPAD		(uint64_t(1) << 9)		//!< Button bit for pressing down on the right stick.
#define VR_STEAM_BTNBIT_STICKLEFT		(uint64_t(1) << 10)		//!< Button bit for pushing the stick left.
#define VR_STEAM_BTNBIT_STICKRIGHT		(uint64_t(1) << 11)		//!< Button bit for pushing the stick right.
#define VR_STEAM_BTNBIT_STICKUP			(uint64_t(1) << 12)		//!< Button bit for pushing the stick up.
#define VR_STEAM_BTNBIT_STICKDOWN		(uint64_t(1) << 13)		//!< Button bit for pushing the stick down.
#define VR_STEAM_BTNBITS_STICKANY		(VR_STEAM_BTNBIT_STICKLEFT|VR_STEAM_BTNBIT_STICKRIGHT|VR_STEAM_BTNBIT_STICKUP|VR_STEAM_BTNBIT_STICKDOWN) //!< Button bits for pushing any of the stick "buttons".
#define VR_STEAM_BTNBIT_LEFTSTICK		(uint64_t(1) << 14)		//!< Button bit for pressing down on the left stick.
#define VR_STEAM_BTNBIT_RIGHTSTICK		(uint64_t(1) << 15)		//!< Button bit for pressing down on the right stick.
#define VR_STEAM_BTNBIT_LEFTA				(uint64_t(1) << 18)		//!< Button bit for pressing the "X" button on the left controller.
#define VR_STEAM_BTNBIT_RIGHTA				(uint64_t(1) << 20)		//!< Button bit for pressing the "A" button on the right controller.
#define VR_STEAM_BTNBIT_LEFTB				(uint64_t(1) << 19)		//!< Button bit for pressing the "Y" button on the left controller.
#define VR_STEAM_BTNBIT_RIGHTB				(uint64_t(1) << 21)		//!< Button bit for pressing the "B" button on the right controller.
#define VR_STEAM_BTNBIT_MENU			(uint64_t(1) << 22)		//!< Button bit for pressing the "X" button.
#define VR_STEAM_BTNBIT_SYSTEM			(uint64_t(1) << 23)		//!< Button bit for pressing the "Y" button.

#define VR_STEAM_NUMBASESTATIONS	2   //!< Number of base stations supported.

// Static-use wrapper for use in C projects that can't use the class definition.
extern "C" __declspec(dllexport) int c_createVR(); //!< Create an object internally. Must be called before the functions below.
#ifdef _WIN32
extern "C" __declspec(dllexport) int c_initVR(void* device, void* context); //!< Initialize the internal object (OpenGL).
#else
extern "C" __declspec(dllexport) int c_initVR(void* display, void* drawable, void* context); //!< Initialize the internal object (OpenGL).
#endif
extern "C" __declspec(dllexport) int c_getHMDType(int* type); //!< Get the type of HMD used for VR.
extern "C" __declspec(dllexport) int c_setEyeParams(int side, float fx, float fy, float cx, float cy);  //!< Set rendering parameters.
extern "C" __declspec(dllexport) int c_getDefaultEyeParams(int side, float* fx, float* fy, float* cx, float* cy); //!< Get the HMD's default parameters.
extern "C" __declspec(dllexport) int c_getDefaultEyeTexSize(int* w, int* h, int side); //!< Get the default eye texture size.
extern "C" __declspec(dllexport) int c_updateTrackingVR(); //!< Update the t_eye positions based on latest tracking data.
extern "C" __declspec(dllexport) int c_getEyePositions(float t_eye[VR::Sides][4][4]); //!< Last tracked position of the eyes.
extern "C" __declspec(dllexport) int c_getHMDPosition(float t_hmd[4][4]);      //!< Last tracked position of the HMD.
extern "C" __declspec(dllexport) int c_getControllerPositions(float t_controller[VR_MAX_CONTROLLERS][4][4]); //!< Last tracked position of the controller.
extern "C" __declspec(dllexport) int c_getControllerStates(void* controller_states[VR_MAX_CONTROLLERS]); //!< Last tracked button states of the controllers.
extern "C" __declspec(dllexport) int c_blitEye(int side, void* texture_resource, const float* aperture_u, const float* aperture_v); //!< Blit a rendered image into the internal eye texture.
extern "C" __declspec(dllexport) int c_blitEyes(void* texture_resource_left, void* texture_resource_right, const float* aperture_u, const float* aperture_v); //!< Blit rendered images into the internal eye textures.
extern "C" __declspec(dllexport) int c_submitFrame(); //!< Submit frame to the HMD.
extern "C" __declspec(dllexport) int c_uninitVR(); //!< Un-initialize the internal object.

// SteamVR (Valve OpenVR) API module.
class VR_Steam : public VR
{
protected:
	vr::IVRSystem*	hmd;		//!< HMD device.
	HMDType			hmd_type;	//!< Type of hmd used for VR.

private:
private:
	// New OpenVR Input System
	struct Input {
		vr::VRActionSetHandle_t action_set_handle;
		vr::VRActiveActionSet_t active_action_set;
		struct ActionHandles {
			vr::VRActionHandle_t pos;
			vr::VRActionHandle_t trigger;
			vr::VRActionHandle_t grip;
			vr::VRActionHandle_t grip_touch;
			vr::VRActionHandle_t grip_force;
			vr::VRActionHandle_t touchpad;
			vr::VRActionHandle_t touchpad_press;
			vr::VRActionHandle_t touchpad_touch;
			vr::VRActionHandle_t thumbstick;
			vr::VRActionHandle_t thumbstick_press;
			vr::VRActionHandle_t button_a;
			vr::VRActionHandle_t button_a_touch;
			vr::VRActionHandle_t button_b;
			vr::VRActionHandle_t button_b_touch;
			vr::VRActionHandle_t button_menu;
			vr::VRActionHandle_t button_menu_touch;
			//vr::VRActionHandle_t button_system;
			//vr::VRActionHandle_t button_system_touch;
		};
		ActionHandles action_handles[VR_MAX_CONTROLLERS];
		static const char* const action_manifest;
		static const char* const binding_vive;
		static const char* const binding_windowsmr;
		static const char* const binding_index;
		static const char* const binding_logitechink;
	};
	Input input;

protected:
	uint texture_width;  //!< Width of the textures in pixels.
	uint texture_height; //!< Height of the textures in pixels.

	// OpenGL objects
	typedef struct GL {
#ifdef _WIN32
		HDC     device;   //!< Windows device context (HDC) for the Blender viewport window.
		HGLRC   context;  //!< OpenGL rendering context (HGLRC) for the Blender viewport window.
#else
		Display* display;      //!< The connection to the X server.
		GLXDrawable drawable;  //!< The GLX drawable. Either an X window ID or a GLX pixmap ID.
		GLXContext context;    //!< The GLX rendering context attached to the GLX drawable.
#endif
		GLuint  framebuffer[2];   //!< Framebuffer objects for storing completed renderings.
		GLuint  texture[2];       //!< Color textures for storing completed renderings.
		GLuint  verts;			  //!< Vertex buffer.
		GLuint  uvs;			  //!< UV buffer.
		GLuint  vertex_array;	  //!< Vertex array.

		GLuint  program;    //!< Shader program handle.
		GLuint  vshader;    //!< Vertex shader handle.
		GLuint  fshader;    //!< Fragment shader handle.

		GLint   position_location; //!< Location of the shader position vector.
		GLint   uv_location;	   //!< Location of the shader position vector.
		GLint   sampler_location; //!< Location of the shader sampler.
		GLint   param_location; //!< Location of the shader parameter vector.

		static const char* const vshader_source; //!< Source code of the vertex shader.
		static const char* const fshader_source; //!< Source code of the pixel shader.

		bool create(uint width, uint height); //!< Create required openGL/D3D objects.
		void release(); //!< Delete / free OpenGL objects.
	} GL; //< OpenGL objects.
	GL gl; //< OpenGL objects.

	bool   eye_offset_override[2]; //!< Whether the user defined the offset manually.

public:
	VR_Steam();           //!< Class constructor
	virtual ~VR_Steam();  //!< Class destructor

protected:
	bool initialized; //!< Whether the module is initialized.

	int acquireHMD(); //!< Initialize basic VR operation and acquire the HMD object.
	int releaseHMD(); //!< Delete the HMD object and uninitialize basic VR operation.

	void interpretControllerState(const vr::VRControllerState_t& s, const float m[3][4], float t_controller[4][4], Controller& c, const Input::ActionHandles *input_handles); //!< Helper function to deal with Vive controller data.

public:
	virtual Type type(); //<! Get which API was used in this implementation.
	virtual HMDType hmdType(); //<! Get which HMD was used in this implementation.

#ifdef _WIN32
	virtual int init(void* device, void* context); //!< Initialize the VR module.
#else
	virtual int init(void* display, void* drawable, void* context); //!< Initialize the VR module
#endif
	virtual int uninit(); //!< Un-initialize the VR module.

	virtual int getDefaultEyeTexSize(uint& w, uint& h, Side side = Side_Both); //!< Get the default eye texture size.
	virtual int getDefaultEyeParams(Side side, float& fx, float& fy, float& cx, float& cy); //!< Get the HMD's default parameters
	virtual int setEyeParams(Side side, float fx, float fy, float cx = 0.5f, float cy = 0.5f);  //!< Set rendering parameters.
	virtual int setEyeOffset(Side side, float x, float y, float z); //!< Override the offset of the eyes (camera positions) relative to the HMD.

	virtual int updateTracking();      //!< Update the t_eye positions based on latest tracking data.

	virtual int blitEye(Side side, void* texture_resource, const float& aperture_u, const float& aperture_v); //!< Blit a rendered image into the internal eye texture.
	virtual int blitEyes(void* texture_resource_left, void* texture_resource_right, const float& aperture_u, const float& aperture_v); //!< Blit rendered images into the internal eye textures.
	virtual int submitFrame();         //!< Submit frame to the HMD.

	virtual int getTrackerPosition(uint i, float t[4][4]) const override; //!< Get the position of a tracking camera / device (if available).

	float t_basestation[VR_STEAM_NUMBASESTATIONS][4][4]; //!< Transformation matrix for basestation position.
};

#endif //__VR_STEAM_H__
