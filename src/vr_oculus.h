/***********************************************************************************************//**
 * \file      vr_oculus.h
 *            Oculus Rift HMD module.
 *            This file contains code related to using Oculus Rift HMDs.
 *            Both tracking and rendering are implemented.
 ***************************************************************************************************
 * \brief     Oculus Rift HMD module.
 * \copyright MARUI-PlugIn (inc.)
 **************************************************************************************************/
#ifndef __VR_OCULUS_H__
#define __VR_OCULUS_H__

#include <OVR_CAPI_D3D.h>

#include <GL/CAPI_GLE.h>
#include <OVR_CAPI_GL.h>
#include <Extras/OVR_Math.h>

#include "vr.h"

#define VR_OCULUS_TOUCHTHRESHOLD_STICKDIRECTION		0.4f	//!< Threshold for thumb-stick interaction to be registered as an direction to somewhere to be a "touch".
#define VR_OCULUS_PRESSTHRESHOLD_STICKDIRECTION		0.9f	//!< Threshold for thumb-stick interaction to be registered as an direction to somewhere to be a "touch".
#define VR_OCULUS_PRESSTHRESHOLD_INDEXTRIGGER		0.35f   //!< Threshold for the index trigger to be considered a "button press".
#define VR_OCULUS_TOUCHTHRESHOLD_INDEXTRIGGER		0.05f   //!< Threshold for the index trigger to be considered a "button touch".
#define VR_OCULUS_PRESSTHRESHOLD_SHOULDERGRIP		0.85f   //!< Threshold for the shoulder/grip to be considered a "button press".
#define VR_OCULUS_TOUCHTHRESHOLD_SHOULDERGRIP		0.4f	//!< Threshold for the shoulder/grip to be considered a "button touch".

// Widget_Layout button bits.
#define VR_OCULUS_BTNBIT_LEFTTRIGGER	(uint64_t(1) << 0)		//!< Button bit for the left trigger.
#define VR_OCULUS_BTNBIT_RIGHTTRIGGER	(uint64_t(1) << 1)		//!< Button bit for the right trigger.
#define VR_OCULUS_BTNBIT_LEFTGRIP		(uint64_t(1) << 2)		//!< Button bit for the left grip.
#define VR_OCULUS_BTNBIT_RIGHTGRIP		(uint64_t(1) << 3)		//!< Button bit for the right grip.
#define VR_OCULUS_BTNBIT_STICKLEFT		(uint64_t(1) << 10)		//!< Button bit for pushing the stick left.
#define VR_OCULUS_BTNBIT_STICKRIGHT		(uint64_t(1) << 11)		//!< Button bit for pushing the stick right.
#define VR_OCULUS_BTNBIT_STICKUP		(uint64_t(1) << 12)		//!< Button bit for pushing the stick up.
#define VR_OCULUS_BTNBIT_STICKDOWN		(uint64_t(1) << 13)		//!< Button bit for pushing the stick down.
#define VR_OCULUS_BTNBIT_LEFTSTICK		(uint64_t(1) << 14)		//!< Button bit for pressing down on the left stick.
#define VR_OCULUS_BTNBIT_RIGHTSTICK		(uint64_t(1) << 15)		//!< Button bit for pressing down on the right stick.
#define VR_OCULUS_BTNBIT_LEFTTHUMBREST	(uint64_t(1) << 16)		//!< Button bit for pressing the left thumbrest.
#define VR_OCULUS_BTNBIT_RIGHTTHUMBREST	(uint64_t(1) << 17)		//!< Button bit for pressing the right thumbrest.
#define VR_OCULUS_BTNBIT_X				(uint64_t(1) << 18)		//!< Button bit for pressing the "X" button.
#define VR_OCULUS_BTNBIT_Y				(uint64_t(1) << 19)		//!< Button bit for pressing the "Y" button.
#define VR_OCULUS_BTNBIT_A				(uint64_t(1) << 20)		//!< Button bit for pressing the "A" button.
#define VR_OCULUS_BTNBIT_B				(uint64_t(1) << 21)		//!< Button bit for pressing the "B" button.

// Static-use wrapper for use in C projects that can't use the class definition.
extern "C" __declspec(dllexport) int c_createVR();	//!< Create n object internally. Must be called before the functions below.
extern "C" __declspec(dllexport) int c_initVR(void* device, void* context);	//!< Initialize the internal object (OpenGL).
extern "C" __declspec(dllexport) int c_getHMDType(int* type);	//!< Get the type of HMD used for VR.
extern "C" __declspec(dllexport) int c_setEyeParams(int side, float fx, float fy, float cx, float cy);	//!< Set rendering parameters.
extern "C" __declspec(dllexport) int c_getDefaultEyeParams(int side, float* fx, float* fy, float* cx, float* cy);	//!< Get the HMD's default parameters.
extern "C" __declspec(dllexport) int c_getDefaultEyeTexSize(int* w, int* h, int side);	//!< Get the default eye texture size.
extern "C" __declspec(dllexport) int c_updateTrackingVR();	//!< Update the t_eye positions based on latest tracking data.
extern "C" __declspec(dllexport) int c_getEyePositions(float t_eye[VR::Sides][4][4]);	//!< Last tracked position of the eyes.
extern "C" __declspec(dllexport) int c_getHMDPosition(float t_hmd[4][4]);	//!< Last tracked position of the HMD.
extern "C" __declspec(dllexport) int c_getControllerPositions(float t_controller[VR_MAX_CONTROLLERS][4][4]);	//!< Last tracked position of the controllers.
extern "C" __declspec(dllexport) int c_getControllerStates(void* controller_states[VR_MAX_CONTROLLERS]);	//!< Last tracked button states of the controller.
extern "C" __declspec(dllexport) int c_blitEye(int side, void* texture_resource, const float* aperture_u, const float* aperture_v);	//!< Blit a rendered image into the internal eye texture.
extern "C" __declspec(dllexport) int c_blitEyes(void* texture_resource_left, void* texture_resource_right, const float* aperture_u, const float* aperture_v);	//!< Blit rendered images into the internal eye textures.
extern "C" __declspec(dllexport) int c_submitFrame();	//!< Submit frame to the HMD.
extern "C" __declspec(dllexport) int c_uninitVR();	//!< Un-initialize the internal object.

// Oculus Rift HMD.
class VR_Oculus : public VR
{
public:
	//                                                                  ________________
	//_________________________________________________________________/     Eye
	/**
	 * Collection of data per eye.
	 */
	typedef struct Eye {
		ovrTextureSwapChain swap_texture_set;   //!< Oculus round-robin texture buffer.
		OVR::Sizei          texsize;            //!< Texture size.
		GLuint              framebuffer;        //!< Eye framebuffer target.
		ovrEyeRenderDesc    render_desc;        //!< Rendering details per eye.
		ovrPosef            pose;               //!< Pose of each eye
		ovrPosef            offset;             //!< Offset between eye and HMD. Used to calculate eye positions.
		ovrFovPort          fov;                //!< Field-of-view per eye.
		float				fx;                 //!< Horizontal focal length, in "image-width"-units (1=image width).
		float				fy;                 //!< Vertical focal length, in "image-height"-units (1=image height).
		float				cx;                 //!< Horizontal principal point, in "image-width"-units (0.5=image center).
		float				cy;                 //!< Vertical principal point, in "image-height"-units (0.5=image center).
		// Default constructor (zero-init).
		Eye() : swap_texture_set(0)
			, texsize(0, 0)
			, framebuffer(0)
			, offset()
			, fx(0)
			, fy(0)
			, cx(0.5)
			, cy(0.5) {};
	} Eye; //!< Collection of data per eye.

	typedef struct GL {
		HDC     device;		//!< Windows device context (HDC) for the Blender viewport window.
		HGLRC   context;	//!< OpenGL rendering context (HGLRC) for the Blender viewport window.

		GLuint  verts;		//!< Vertex buffer.
		GLuint  uvs;		//!< UV buffer.
		GLuint  vertex_array;//!< Vertex array.

		GLuint  program;    //!< Shader program handle.
		GLuint  vshader;    //!< Vertex shader handle.
		GLuint  fshader;    //!< Fragment shader handle.

		GLint   position_location;	//!< Location of the shader position vector.
		GLint   uv_location;		//!< Location of the shader position vector.
		GLint   sampler_location;	//!< Location of the shader sampler.
		GLint   param_location;		//!< Location of the shader parameter vector.

		static const char* const vshader_source;	//!< Source code of the vertex shader.
		static const char* const fshader_source;	//!< Source code of the pixel shader.
	} GL; //!< OpenGL object/instance collection.

protected:
	bool initialized;	//!< Whether the module is currently initialized.
	static OVR::GLEContext gle_context;	//!< OpenGL extension management for Oculus. -> made static to avoid crashes

	int acquireHMD();	//!< Initialize basic OVR operation and acquire the HMD object.
	int releaseHMD();	//!< Delete the HMD object and uninitialize basic OVR operation.

public:
	VR_Oculus();           //!< Class constructor
	virtual ~VR_Oculus();  //!< Class destructor

protected:
	long long           frame_index;        //!< Frame index counter.
	double				sensor_sample_time; //!< Time of last sensor sampling.
	ovrGraphicsLuid		luid;				//!< Oculus luid.
	ovrSession          hmd;                //!< Rift HMD identifier.
	ovrHmdDesc          hmd_desc;           //!< Descriptor of the HMD.
	VR::HMDType			hmd_type;			//!< Type of the HMD attached.
	Eye                 eye[2];             //!< Eye-related data.
	GL  gl;	//!< OpenGL related objects/instances.

public:
	virtual Type type();	//<! Get which API was used in this implementation.
	virtual HMDType hmdType();	//<! Get which HMD was used in this implementation.

	virtual int init(void* device, void* context);	//!< Initialize the VR device.
	virtual int uninit();	//!< Un-initialize the module.

	virtual int setEyeOffset(Side side, float x, float y, float z);//!< Override the offset of the eyes (camera positions) relative to the HMD.
	virtual int setEyeParams(Side side, float fx, float fy, float cx = 0.5f, float cy = 0.5f);	//!< Set rendering parameters.
	virtual int getDefaultEyeParams(Side side, float& fx, float& fy, float& cx, float& cy);	//!< Get the HMD's default parameters.
	virtual int getDefaultEyeTexSize(uint& w, uint& h, Side side = Side_Both);	//!< Get the default eye texture size.

	virtual int updateTracking();	//!< Update the t_eye positions based on latest tracking data.

	virtual int blitEye(Side side, void* texture_resource, const float& aperture_u, const float& aperture_v);	//!< Blit a rendered image into the internal eye texture.
	virtual int blitEyes(void* texture_resource_left, void* texture_resource_right, const float& aperture_u, const float& aperture_v);	//!< Blit rendered images into the internal eye textures.
	virtual int submitFrame();	//!< Submit frame to the HMD.

	virtual int getTrackerPosition(uint i, float t[4][4]) const override;	//!< Get the position of a tracking camera / device (if available).
};

#endif //__VR_OCULUS_H__
