/***********************************************************************************************//**
 * \file      vr.h
 *            Virtual Reality device/API abstraction layer module.
 *            Implements an abstract VR device that can be implemented using various APIs.
 ***************************************************************************************************
 * \brief     Virtual Reality device/API abstraction layer module.
 * \copyright MARUI-PlugIn (inc.)
 **************************************************************************************************/
#ifndef __VR_H
#define __VR_H

/**************************************************************************************************\
|*                                        BUILD DEFINITIONS                                       *|
\**************************************************************************************************/
#ifndef _WIN32
#define __declspec(dllexport) __attribute__((visibility("default")))
#endif

typedef unsigned char      uchar;  //!< unsigned 8bit integer (byte)
typedef unsigned short     ushort; //!< unsigned 16bit (short) integer
typedef unsigned int       uint;   //!< unsigned 32bit integer
typedef uint64_t		   ui64;   //!< unsigned 64bit integer

#define STRING(X)           #X //!< Multi-line string.
#define SET4X4IDENTITY(M)   {M[0][0]=M[1][1]=M[2][2]=M[3][3]=1;M[0][1]=M[0][2]=M[0][3]=M[1][0]=M[1][2]=M[1][3]=M[2][0]=M[2][1]=M[2][3]=M[3][0]=M[3][1]=M[3][2]=0;} //!< Set 4x4 matrix to identity.

#define VR_MAX_CONTROLLERS 3 //!< Maximum number of controllers that can be simultaneously supported.

/**************************************************************************************************/

// Virtual Reality device/API abstraction layer module.
struct VR
{
public:
	//                                                                  ________________
	//_________________________________________________________________/     Type
	/**
	 * API used to implement this device / module.
	 */
	typedef enum Type {
		Type_Null = 0 //!< Empty null-implementation.
		,
		Type_Oculus = 1 //!< Oculus OVR API was used for implementation.
		,
		Type_Steam = 2 //!< SteamVR (Valve OpenVR) was used for implementation.
		,
		Type_Fove = 3 //!< Fove API was used for implementation.
		,
		Types = 4 //!< Number of API types.
	} Type; //!< API used to implement this device / module.

	//                                                                  ________________
	//_________________________________________________________________/	HMDType
	/**
	 * HMD / device used for VR.
	 */
	typedef enum HMDType {
		HMDType_Null = 0 //!< Empty null-implementation.
		,
		HMDType_Oculus = 1 //!< Oculus Rift.
		,
		HMDType_Vive = 2 //!< HTC Vive.
		,
		HMDType_Microsoft = 3 //!< Windows MR headset.
		,
		HMDType_Fove = 4 //!< Fove0 headset.
		,
		HMDTypes = 5 //!< Number of API types.
	} HMDType; //!< HMD / device used for VR.

	//                                                              ____________________
	//_____________________________________________________________/   enum Error
	/**
	 * Enum defining error codes. Null indicates successful operation.
	 */
	typedef enum Error {
		Error_None = 0  //!< Operation performed successfully.
		,
		Error_NotInitialized = 1 //!< The module was not correctly initialized.
		,
		Error_InvalidParameter = 2 //!< One or more of the provided parameters were invalid.
		,
		Error_InternalFailure = 3 //!< A failure has occurred during execution.
		,
		Error_NotAvailable = 4 //!< The requested functionality is not available in this implementation.
	} Error; //!< Error codes.

	/**
	 * Enum for stereo, assigning numbers to mono, stereo left, stereo right,
	 * stereo both and the dominant eye.
	 */
	typedef enum Side {
		Side_Mono = 0 //!< The only available option in a mono rig.
		,
		Side_Left = 0 //!< The left side of the stereo rig.
		,
		Side_Right = 1 //!< The right side of the stereo rig.
		,
		Side_AUX = 2 //!< Auxilliary third "side" (where applicable).
		,
		Side_Both = -1 //!< Both sides (where applicable).
		,
		Side_Dominant = -2 //!< The side of the dominant eye (where applicable).
		,
		Sides = 2 //!< Number of (actual, non-symbolic) sides.
	} Side; //!< Global enum for stereo, assigning numbers to mono, stereo left, stereo right, stereo both and the dominant eye.

public:
	float t_hmd[4][4];		 //!< Last tracked position of the HMD.
	float t_eye[Sides][4][4];     //!< Last tracked position of the eyes.
	float t_hmd2eye[Sides][4][4]; //!< Transformation between the HMD and each eye (static).

	bool    tracking;    //!< Whether tracking is currently active / working (for the HMD).
	float	gamma;        //<! Gamma correction factor.

	float t_controller[VR_MAX_CONTROLLERS][4][4];	//!< Last tracked position of the controllers.

	/// Simple struct for 3D input device information
	typedef struct Controller {
		Side    side;       //!< Side of the controller.
		bool    available;  //!< Whether the controller are (currently) available.
		ui64    buttons;    //!< Buttons currently pressed on the controller.
		ui64    buttons_touched; //!< Buttons currently touched on the controller (if available).
		float  dpad[2]; //!< Dpad / touchpad position (u,v).
		float  stick[2];   //!< Joystick / thumbstick position (u,v).
		float trigger_pressure; //!< Analog trigger pressure (0~1) (if available).
		float grip_pressure; //!< Analog grip pressure (0~1) (if available).
		Controller();       //!< Constructor (null-init)
	} Controller; //!< Controller

	Controller controller[VR_MAX_CONTROLLERS];   //!< Left and right controllers (if available), and additional controllers (if available).

public:
	VR();           //!< Class constructor.
	virtual ~VR();  //!< Class destructor.

	virtual Type type(); //!< Get which API type was used for VR.
	virtual HMDType hmdType(); //!< Get which HMD type was used for VR. 

#ifdef _WIN32
	virtual int init(void* device, void* context); //!< Initialize the VR device.
#else
	virtual int init(void* display, void* drawable, void* context); //!< Initialize the VR device.
#endif
	virtual int getDefaultEyeTexSize(uint& w, uint& h, Side side = Side_Both); //!< Get the default eye texture size.
	virtual int getDefaultEyeParams(Side side, float& fx, float& fy, float& cx, float& cy); //!< Get the HMD's default parameters.

	virtual int setEyeParams(Side side, float fx, float fy, float cx = 0.5f, float cy = 0.5f);  //!< Set rendering parameters.
	virtual int setEyeOffset(Side side, float x, float y, float z); //!< Override the offset of the eyes (camera positions) relative to the HMD.

	virtual int updateTracking();      //!< Update the HMD/Eye/Controller positions based on latest tracking data.

	virtual int blitEye(Side side, void* texture_resource, const float& aperture_u, const float& aperture_v); //!< Blit a rendered image into the internal eye texture.
	virtual int blitEyes(void* texture_resource_left, void* texture_resource_right, const float& aperture_u, const float& aperture_v); //!< Blit rendered images into the internal eye textures.
	virtual int submitFrame();         //!< Submit frame to the HMD.

	virtual int getTrackerPosition(uint i, float t[4][4]) const; //!< Get the position of a tracking camera / device (if available).
};

//                                                                          ________________________
//_________________________________________________________________________/         VR()
/**
 * Class constructor.
 */
inline VR::VR()
	: tracking(false)
	, gamma(1.0f)
{
	SET4X4IDENTITY(t_hmd);
	SET4X4IDENTITY(t_eye[Side_Left]);
	SET4X4IDENTITY(t_eye[Side_Right]);
	SET4X4IDENTITY(t_hmd2eye[Side_Left]);
	SET4X4IDENTITY(t_hmd2eye[Side_Right]);

	controller[Side_Left] = VR::Controller();
	controller[Side_Left].side = Side_Left;
	SET4X4IDENTITY(t_controller[Side_Left]);
	controller[Side_Right] = VR::Controller();
	controller[Side_Right].side = Side_Right;
	SET4X4IDENTITY(t_controller[Side_Right]);

	for (int i = 2; i < VR_MAX_CONTROLLERS; ++i) {
		controller[i] = VR::Controller();
		controller[i].side = Side_AUX;
		SET4X4IDENTITY(t_controller[i]);
	}
}

//                                                                          ________________________
//_________________________________________________________________________/         ~VR()
/**
 * Class destructor.
 */
inline VR::~VR()
{
	//
}

//                                                                          ________________________
//_________________________________________________________________________/         type()
/**
 * Get which API was used for VR.
 */
inline VR::Type VR::type()
{
	return Type_Null;
}

//                                                                          ________________________
//_________________________________________________________________________/        hmdType()
/**
 * Get which HMD type was used for VR.
 */
inline VR::HMDType VR::hmdType()
{
	return HMDType_Null;
}

#ifdef _WIN32
//                                                                          ________________________
//_________________________________________________________________________/        init()
/**
 * Initialize the VR device.
 * \param   device          The graphics device used by Blender (HDC)
 * \param   context         The rendering context used by blender (HGLRC)
 * \return  Zero on success, an error code on failure.
 */
inline int VR::init(void* device, void* context)
{
	return VR::Error_NotAvailable; // Dummy implementation.
}
#else
//                                                                          ________________________
//_________________________________________________________________________/        init()
/**
 * Initialize the VR device.
 * \param   display     The connection to the X server (Display*).
 * \param   drawable    The GLX drawable (GLXDrawable*). Either an X window ID or a GLX pixmap ID.
 * \param   context     The GLX rendering context (GLXContext*) to be attached to drawable.
 * \return  Zero on success, an error code on failure.
 */
inline int VR::init(void* display, void* drawable, void* context)
{
	return VR::Error_NotAvailable; // Dummy implementation.
}
#endif
//                                                                          ________________________
//_________________________________________________________________________/ getDefaultEyeTexSize()
/**
 * Get the default eye texture size.
 */
inline int VR::getDefaultEyeTexSize(uint& w, uint& h, Side side)
{
	return VR::Error_NotAvailable; // Dummy implementation.
}

//                                                                          ________________________
//_________________________________________________________________________/ getDefaultEyeParams()
/**
 * Get the HMD's default parameters.
 */
inline int VR::getDefaultEyeParams(Side side, float& fx, float& fy, float& cx, float& cy)
{
	return VR::Error_NotAvailable; // Dummy implementation.
}

//                                                                          ________________________
//_________________________________________________________________________/    setEyeParams()
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
inline int VR::setEyeParams(Side side, float fx, float fy, float cx, float cy)
{
	return VR::Error_NotAvailable; // Dummy implementation.
}

//                                                                          ________________________
//_________________________________________________________________________/    setEyeOffset()
/**
 * Override the offset of the eyes (camera positions) relative to the HMD.
 */
inline int VR::setEyeOffset(Side side, float x, float y, float z)
{
	return VR::Error_NotAvailable; // Dummy implementation.
}

//                                                                          ________________________
//_________________________________________________________________________/   updateTracking()
/**
 * Update the t_eye positions based on latest tracking data.
 */
inline int VR::updateTracking()
{
	return VR::Error_NotAvailable; // Dummy implementation.
}

//                                                                          ________________________
//_________________________________________________________________________/    submitFrame()
/**
 * Submit frame to the HMD.
 */
inline int VR::submitFrame()
{
	return VR::Error_NotAvailable; // Dummy implementation.
}

//                                                                          ________________________
//_________________________________________________________________________/       blitEye()
/**
 * Blit a rendered image into the internal eye texture.
 * \param   side                Which eye texture to blit to.
 * \param   texture_resource    Texture cotaining the image (OpenGL texture ID).
 * \param   aperture_u          The aperture of the texture (0~u) that contains the rendering.
 * \param   aperture_v          The aperture of the texture (0~v) that contains the rendering.
 */
inline int VR::blitEye(Side side, void* texture_resource, const float& aperture_u, const float& aperture_v)
{
	return VR::Error_NotAvailable; // Dummy implementation.
}

//                                                                          ________________________
//_________________________________________________________________________/       blitEye()
/**
 * Blit a rendered image into the internal eye texture.
 * \param   side                   Which eye texture to blit to.
 * \param   texture_resource_left  Texture cotaining the image for the left eye (OpenGL texture ID).
 * \param   texture_resource_right Texture cotaining the image for the left eye (OpenGL texture ID).
 * \param   aperture_u             The aperture of the texture (0~u) that contains the rendering.
 * \param   aperture_v             The aperture of the texture (0~v) that contains the rendering.
 */
inline int VR::blitEyes(void* texture_resource_left, void* texture_resource_right, const float& aperture_u, const float& aperture_v)
{
	return VR::Error_NotAvailable; // Dummy implementation.
}

//                                                                          ________________________
//_________________________________________________________________________/  getTrackerPosition()
/**
 * Get the position of a tracking camera / device (if available).
 * \i           Index of the camera tracking device to query.
 * \t           [OUT] Matrix to receive the transformation.
 * \return      Zero on success, an error code on failure.
 */
inline int VR::getTrackerPosition(uint i, float t[4][4]) const
{
	return VR::Error_NotAvailable;
}

//                                                                          ________________________
//_________________________________________________________________________/    Controller()
/**
 * Constructor (null-init).
 */
inline VR::Controller::Controller()
	: available(false)
	, buttons(0)
	, buttons_touched(0)
	, trigger_pressure(0.0f)
{
	dpad[0] = dpad[1] = 0;
	stick[0] = stick[1] = 0;
}

#endif //__VR_H
