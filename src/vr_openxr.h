/***********************************************************************************************//**
 * \file      vr_openxr.h
 *            OpenXR VR module.
 *            This file contains code related to using the OpenXR API for HMDs and controllers.
 *            Both tracking and rendering are implemented.
 ***************************************************************************************************
 * \brief     OpenXR VR module.
 * \copyright 
 * \contributors Multiplexed Reality
 **************************************************************************************************/
#ifndef __VR_OPENXR_H__
#define __VR_OPENXR_H__

#ifdef _WIN32
#define XR_USE_PLATFORM_WIN32 1
#define XR_USE_GRAPHICS_API_D3D11 1
#include <d3d11.h>
#include <DirectXMath.h>
#if XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_OPENGL 0
#else
#define XR_USE_GRAPHICS_API_OPENGL 1
#endif
#else
#define XR_USE_PLATFORM_XLIB 1
#define XR_USE_GRAPHICS_API_OPENGL 1
#endif

#include "openxr.h"
#include "openxr_platform.h"

#include "vr.h"

#include <vector>
#include <map>
#include <array>
#include <list>

#define VR_OPENXR_DEBOUNCEPERIOD                 200	//!< Debounce period to avoid jumpy/slacky touchpad touches.
#define VR_OPENXR_BUTTONPRESSURETHRESHOLD        0.3f	//!< Threshold for general button pressure to be registered as "pressed".
#define VR_OPENXR_TRIGGERPRESSURETHRESHOLD       0.3f	//!< Threshold for trigger pressure to be registered as "pressed".
#define VR_OPENXR_GRIPPRESSURETHRESHOLD			 0.3f   //!< Threshold for grip pressure to be registered as "pressed".
#define VR_OPENXR_TRACKPADDIRECTIONTHRESHOLD     0.3f	//!< Threshold for trackpad interaction to be registered as an direction to somewhere.
#define VR_OPENXR_TOUCHTHRESHOLD_STICKDIRECTION  0.4f	//!< Threshold for thumb-stick interaction to be registered as an direction to somewhere to be a "touch".
#define VR_OPENXR_PRESSTHRESHOLD_STICKDIRECTION  0.9f	//!< Threshold for thumb-stick interaction to be registered as an direction to somewhere to be a "touch".

// Widget_Layout button bits.
#define VR_OPENXR_BTNBIT_LEFTTRIGGER	(uint64_t(1) << 0)		//!< Button bit for the left trigger.
#define VR_OPENXR_BTNBIT_RIGHTTRIGGER   (uint64_t(1) << 1)		//!< Button bit for the right trigger.
#define VR_OPENXR_BTNBIT_LEFTGRIP		(uint64_t(1) << 2)		//!< Button bit for the left grip.
#define VR_OPENXR_BTNBIT_RIGHTGRIP		(uint64_t(1) << 3)		//!< Button bit for the right grip.
#define VR_OPENXR_BTNBIT_DPADLEFT		(uint64_t(1) << 4)		//!< Button bit for pushing the trackpad left.
#define VR_OPENXR_BTNBIT_DPADRIGHT		(uint64_t(1) << 5)		//!< Button bit for pushing the trackpad right.
#define VR_OPENXR_BTNBIT_DPADUP			(uint64_t(1) << 6)		//!< Button bit for pushing the trackpad up.
#define VR_OPENXR_BTNBIT_DPADDOWN		(uint64_t(1) << 7)		//!< Button bit for pushing the trackpad down.
#define VR_OPENXR_BTNBITS_DPADANY		(VR_OPENXR_BTNBIT_DPADLEFT|VR_OPENXR_BTNBIT_DPADRIGHT|VR_OPENXR_BTNBIT_DPADUP|VR_OPENXR_BTNBIT_DPADDOWN)	//!< Button bits for pushing any of the trackpad "buttons".
#define VR_OPENXR_BTNBIT_LEFTDPAD		(uint64_t(1) << 8)		//!< Button bit for pressing down on the left stick.
#define VR_OPENXR_BTNBIT_RIGHTDPAD		(uint64_t(1) << 9)		//!< Button bit for pressing down on the right stick.
#define VR_OPENXR_BTNBIT_STICKLEFT		(uint64_t(1) << 10)		//!< Button bit for pushing the stick left.
#define VR_OPENXR_BTNBIT_STICKRIGHT		(uint64_t(1) << 11)		//!< Button bit for pushing the stick right.
#define VR_OPENXR_BTNBIT_STICKUP		(uint64_t(1) << 12)		//!< Button bit for pushing the stick up.
#define VR_OPENXR_BTNBIT_STICKDOWN		(uint64_t(1) << 13)		//!< Button bit for pushing the stick down.
#define VR_OPENXR_BTNBITS_STICKANY		(VR_OPENXR_BTNBIT_STICKLEFT|VR_OPENXR_BTNBIT_STICKRIGHT|VR_OPENXR_BTNBIT_STICKUP|VR_OPENXR_BTNBIT_STICKDOWN) //!< Button bits for pushing any of the stick "buttons".
#define VR_OPENXR_BTNBIT_LEFTSTICK		(uint64_t(1) << 14)		//!< Button bit for pressing down on the left stick.
#define VR_OPENXR_BTNBIT_RIGHTSTICK		(uint64_t(1) << 15)		//!< Button bit for pressing down on the right stick.
#define VR_OPENXR_BTNBIT_LEFTTHUMBREST	(uint64_t(1) << 16)		//!< Button bit for pressing the left thumbrest.
#define VR_OPENXR_BTNBIT_RIGHTTHUMBREST	(uint64_t(1) << 17)		//!< Button bit for pressing the right thumbrest.
#define VR_OPENXR_BTNBIT_X				(uint64_t(1) << 18)		//!< Button bit for pressing the "X" button.
#define VR_OPENXR_BTNBIT_Y				(uint64_t(1) << 19)		//!< Button bit for pressing the "Y" button.
#define VR_OPENXR_BTNBIT_A				(uint64_t(1) << 20)		//!< Button bit for pressing the "A" button.
#define VR_OPENXR_BTNBIT_B				(uint64_t(1) << 21)		//!< Button bit for pressing the "B" button.
#define VR_OPENXR_BTNBIT_MENU			(uint64_t(1) << 22)		//!< Button bit for pressing the "X" button.
#define VR_OPENXR_BTNBIT_SYSTEM			(uint64_t(1) << 23)		//!< Button bit for pressing the "Y" button.

#define VR_OPENXR_NUMINPUTBINDINGS_OCULUS	29	//!< Total number of input action bindings for the Oculus Touch controllers.
#define VR_OPENXR_NUMINPUTBINDINGS_VIVE		21	//!< Total number of input action bindings for the HTC Vive controllers.
#define VR_OPENXR_NUMINPUTBINDINGS_WMR		23	//!< Total number of input action bindings for the Windows MR controllers.
#define VR_OPENXR_NUMINPUTBINDINGS_FOVE		7	//!< Total number of input action bindings for the Fove (Khronos simple controller).
#define VR_OPENXR_NUMINPUTBINDINGS_INDEX	41	//!< Total number of input action bindings for the Valve Index controllers.

#define VR_OPENXR_NUMBASESTATIONS	2   //!< Number of base stations supported.

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

// OpenXR VR module.
class VR_OpenXR : public VR
{
protected:
	// OpenXR objects
	XrInstance m_instance;
	XrSession m_session;
	XrSpace m_appSpace;
	XrFormFactor m_formFactor;
	XrViewConfigurationType m_viewConfigType;
	XrEnvironmentBlendMode m_environmentBlendMode;
	XrSystemId m_systemId;

	struct Swapchain {
		XrSwapchain handle;
		int32_t width;
		int32_t height;
	};
	XrViewConfigurationProperties m_viewConfig{};
	std::vector<XrViewConfigurationView> m_configViews;
	std::vector<Swapchain> m_swapchains;
	std::map<XrSwapchain, std::vector<XrSwapchainImageBaseHeader*>> m_swapchainImages;
	std::vector<XrView> m_views;
	int64_t m_colorSwapchainFormat;
#ifdef _WIN32
#if XR_USE_GRAPHICS_API_D3D11
	std::list<std::vector<XrSwapchainImageD3D11KHR>> m_swapchainImageBuffers;
#else
	std::list<std::vector<XrSwapchainImageOpenGLKHR>> m_swapchainImageBuffers;
#endif
#else
	std::list<std::vector<XrSwapchainImageOpenGLKHR>> m_swapchainImageBuffers;
#endif

	XrFrameState m_frameState;
	XrSessionState m_sessionState;

	struct InputState {
		XrActionSet actionSet;
		XrAction headPoseAction;
		XrAction handPoseAction;
		XrAction triggerTouchAction;
		XrAction triggerClickAction;
		XrAction triggerValueAction;
		XrAction gripClickAction;
		XrAction gripValueAction;
		XrAction gripForceAction;
		XrAction trackpadXAction;
		XrAction trackpadYAction;
		XrAction trackpadTouchAction;
		XrAction trackpadClickAction;
		XrAction trackpadForceAction;
		XrAction thumbstickXAction;
		XrAction thumbstickYAction;
		XrAction thumbstickTouchAction;
		XrAction thumbstickClickAction;
		XrAction thumbrestTouchAction;
		XrAction XTouchAction;
		XrAction XClickAction;
		XrAction YTouchAction;
		XrAction YClickAction;
		XrAction ATouchAction;
		XrAction AClickAction;
		XrAction BTouchAction;
		XrAction BClickAction;
		XrAction menuClickAction;
		XrAction systemTouchAction;
		XrAction systemClickAction;
		XrPath headSubactionPath;
		XrSpace headSpace;
		std::array<XrPath, Sides> handSubactionPath;
		std::array<XrSpace, Sides> handSpace;
	};
	InputState m_inputState;

	HMDType	hmd_type;	//!< Type of hmd used for VR.

	uint32_t texture_width;  //!< Width of the textures in pixels.
	uint32_t texture_height; //!< Height of the textures in pixels.

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
	} GL; //!< OpenGL objects.
	GL gl; //!< OpenGL objects.

#ifdef _WIN32
	// Direct3D11 structs.
	typedef struct D3D {
		ID3D11Device*        device;  //!< Windows device context.
		ID3D11DeviceContext* context; //!< Direct3D rendering context.

		/// Definition of a 3D vertex with texture coordinates.
		typedef struct Vertex {
			DirectX::XMFLOAT3 pos; //!< Position.
			DirectX::XMFLOAT2 tex; //!< Texture uv-coordinates.
		} Vertex; //!< Definition of a 3D vertex with texture coordinates.
		ID3D11Texture2D*        texture[2];   //!< D3D11 texture render target.
		ID3D11RenderTargetView* view[2];      //!< D3D11 render target.

		ID3D11SamplerState*     sampler_state;   //!< Texture sampler state.
		ID3D11RasterizerState*  rasterizer_state;//!< Rasterizer state.
		ID3D11VertexShader*     vertex_shader;  //!< Primitive pass-through vertex shader.
		ID3D11PixelShader*      pixel_shader;   //!< Primitive texture look-up pixel shader.
		ID3D11InputLayout*      input_layout;   //!< Vertex shader input layout.
		ID3D11Buffer*           vertex_buffer;  //!< Vertex buffer for primitive quad.
		ID3D11Buffer*           index_buffer;   //!< Index buffer for primitive quad.
		ID3D11Buffer*           param_buffer;   //!< Shader parameter value buffer.

		static const char* const vshader_source; //!< Source code of the blitting vertex shader.
		static const char* const pshader_source; //!< Source code of the blitting pixel shader.

		bool create(uint width, uint height); //!< Create required openGL/D3D objects.
		void release(); //!< Delete / free OpenGL objects.
	} D3D; //!< Direct3D11 objects.
	D3D d3d; //!< Direct3D11 objects.

	// Shared OpenGL / Direct3D objects.
	HANDLE shared_device;
	HANDLE shared_texture[2];
#endif

	bool   eye_offset_override[2]; //!< Whether the user defined the offset manually.

public:
	VR_OpenXR();           //!< Class constructor
	virtual ~VR_OpenXR();  //!< Class destructor

protected:
	bool initialized; //!< Whether the module is initialized.

	int acquireHMD(); //!< Initialize basic VR operation and acquire the HMD object.
	int releaseHMD(); //!< Delete the HMD object and uninitialize basic VR operation.

	void interpretControllerState(float t_controller[4][4], Controller& c); //!< Helper function to deal with VR controller data.
	bool renderLayer(XrTime predictedDisplayTime, std::vector<XrCompositionLayerProjectionView>& projectionLayerViews,
					 XrCompositionLayerProjection& layer); //!< Helper function to render compositor layer.
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

	float t_basestation[VR_OPENXR_NUMBASESTATIONS][4][4]; //!< Transformation matrix for basestation position.
};

#endif //__VR_OPENXR_H__
