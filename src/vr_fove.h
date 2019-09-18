/***********************************************************************************************//**
 * \file      vr_fove.h
 *            Fove HMD module.
 *            This file contains code related to using Fove HMDs.
 *            Both tracking and rendering are implemented.
 ***************************************************************************************************
 * \brief     Fove HMD module.
 * \copyright MARUI-PlugIn (inc.)
 **************************************************************************************************/
#ifndef __VR_FOVE_H__
#define __VR_FOVE_H__

#include "FoveAPI.h"

#include "vr.h"

#define VR_FOVE_WINK_THRESHOLD			300		//!< Minimum time threshold(ms) for eye to be closed to register as a "wink".
#define VR_FOVE_SQUINT_THRESHOLD		150		//!< Minimum time threshold(ms) for eye to be dilated to register as a "squint".
#define VR_FOVE_SQUINT_SIZE_THRESHOLD	0.85f	//!< Maximum pupil size for eye to be considered squinted.
#define VR_FOVE_DILATE_THRESHOLD		150		//!< Minimum time threshold(ms) for eye to be dilated to register as a "dilate".
#define VR_FOVE_DILATE_SIZE_THRESHOLD	1.15f	//!< Minimum pupil size for eye to be considered dilated.

#define VR_FOVE_USE_MOUSE	1	//!< Whether to use the mouse in addition to eye interactions.
#define VR_FOVE_USE_CONVERGENCE_DEPTH	0	//!< Whether to use the gaze convergence depth when setting the controller position.

 // Widget_Layout button bits.
#define VR_FOVE_BTNBITS_TRIGGERS	(uint64_t(1) << 0)|(uint64_t(1) << 1)	//!< Button bits for the triggers.
#define VR_FOVE_BTNBITS_GRIPS		(uint64_t(1) << 2)|(uint64_t(1) << 3)	//!< Button bits for the grips
#define VR_FOVE_BTNBIT_DPADLEFT		(uint64_t(1) << 4)						//!< Button bit for pushing the trackpad left.
#define VR_FOVE_BTNBIT_DPADRIGHT	(uint64_t(1) << 5)						//!< Button bit for pushing the trackpad right.
#define VR_FOVE_BTNBIT_DPADUP		(uint64_t(1) << 6)						//!< Button bit for pushing the trackpad up.
#define VR_FOVE_BTNBIT_DPADDOWN		(uint64_t(1) << 7)						//!< Button bit for pushing the trackpad down.
#define VR_FOVE_BTNBIT_STICKLEFT	(uint64_t(1) << 10)						//!< Button bit for pushing the stick left.
#define VR_FOVE_BTNBIT_STICKRIGHT	(uint64_t(1) << 11)						//!< Button bit for pushing the stick right.
#define VR_FOVE_BTNBIT_STICKUP		(uint64_t(1) << 12)						//!< Button bit for pushing the stick up.
#define VR_FOVE_BTNBIT_STICKDOWN	(uint64_t(1) << 13)						//!< Button bit for pushing the stick down.
#define VR_FOVE_BTNBITS_XA			(uint64_t(1) << 18)|(uint64_t(1) << 20) //!< Button bit for pressing the "X"/"A" button.
#define VR_FOVE_BTNBITS_YB			(uint64_t(1) << 19)|(uint64_t(1) << 21) //!< Button bit for pressing the "Y"/"B" button.

// Static-use wrapper for use in C projects that can't use the class definition.
extern "C" __declspec(dllexport) int c_createVR();	//!< Create an object internally. Must be called before the functions below.
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

// Fove HMD module.
class VR_Fove : public VR
{
public:
	Fove::Headset		hmd;		//!< HMD device.
	VR::HMDType			hmd_type;	//!< Type of the HMD attached

	uint texture_width;		//!< Width of the textures in pixels.
	uint texture_height;	//!< Height of the textures in pixels.

	//                                                                  ________________
	//_________________________________________________________________/     Eye
	/**
	* Collection of data per eye
	*/
	typedef struct Eye {
		Fove::CompositorLayerEyeSubmitInfo tex_info;	//!< Texture info for eye.
		Fove::Pose	pose;								//!< Pose of each eye.
		Fove::Matrix44	offset;							//!< Offset between eye and HMD. Used to calculate eye positions.
		float fx;										//!< Horizontal focal length, in "image-width"-units (1=image width).
		float fy;										//!< Vertical focal length, in "image-height"-units (1=image height).
		float cx;										//!< Horizontal principal point, in "image-width"-units (0.5=image center).
		float cy;										//!< Vertical principal point, in "image-height"-units (0.5=image center).

		Fove::GazeVector gaze;							//!< Gaze vector for eye.
		float pupil_dilation;							//!< Pupil dilation.
		bool  attention;								//!< True if the user is looking at something, rather than saccading.

		/// Default constructor (zero-init).
		Eye() : fx(0)
			, fy(0)
			, cx(0.5)
			, cy(0.5)
			, pupil_dilation(0.0f)
			, attention(false) {};
	} Eye; //!< Collection of data per eye.

	typedef struct GL {
		HDC     device; //!< Windows device context (HDC) for the Blender viewport window.
		HGLRC   context;//!< OpenGL rendering context (HGLRC) for the Blender viewport window.

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
	} GL; //!< OpenGL object/instance collection.

protected:
	bool initialized; //!< Whether the module is currently initialized.

	int acquireHMD(); //!< Initialize basic OVR operation and acquire the HMD object.
	int releaseHMD(); //!< Delete the HMD object and uninitialize basic OVR operation.

public:
	VR_Fove();           //!< Class constructor
	virtual ~VR_Fove();  //!< Class destructor

protected:
	Fove::Compositor compositor;	//!< Compositor.
	Fove::CompositorLayer compositor_layer;//!< Compositor layer.
	Fove::CompositorLayerCreateInfo compositor_create_info;	//!< Compositor creation info;
	Fove::CompositorLayerSubmitInfo compositor_submit_info;	//!< Compositor texture submission info.

	Fove::Pose	hmd_pose;	//!< HMD pose.
	Fove::Matrix44 camera_matrix;	//!< Stores the camera translation used each frame.
	Fove::GazeConvergenceData convergence;	//!< Gaze convergence data from each frame.

	Eye eye[2]; //!< Eye-related data.
	GL  gl;  //!< OpenGL related objects/instances.

public:
	bool eye_tracking_enabled; //!< Whether to enable eye tracking at startup.

	virtual Type type(); //<! Get which API was used in this implementation.
	virtual HMDType hmdType(); //<! Get which HMD was used in this implementation.

	virtual int init(void* device, void* context); //!< Initialize the VR module.
	virtual int uninit(); //!< Un-initialize the VR module.

	virtual int setEyeOffset(Side side, float x, float y, float z); //!< Override the offset of the eyes (camera positions) relative to the HMD.

	virtual int setEyeParams(Side side, float fx, float fy, float cx = 0.5f, float cy = 0.5f);  //!< Set rendering parameters.
	virtual int getDefaultEyeParams(Side side, float& fx, float& fy, float& cx, float& cy); //!< Get the HMD's default parameters.
	virtual int getDefaultEyeTexSize(uint& w, uint& h, Side side = Side_Both); //!< Get the default eye texture size.

	virtual int updateTracking();      //!< Update the t_eye positions based on latest tracking data.

	virtual int blitEye(Side side, void* texture_resource, const float& aperture_u, const float& aperture_v); //!< Blit a rendered image into the internal eye texture.
	virtual int blitEyes(void* texture_resource_left, void* texture_resource_right, const float& aperture_u, const float& aperture_v); //!< Blit rendered images into the internal eye textures.
	virtual int submitFrame();         //!< Submit frame to the HMD.
};

#endif //__VR_FOVE_H__
