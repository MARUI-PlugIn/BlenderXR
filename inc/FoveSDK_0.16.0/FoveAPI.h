/////////////////////
// FOVE C/C++ API ///
/////////////////////

// This header defines both the C and C++ APIs.

#ifndef FOVE_API_H
#define FOVE_API_H

// Disable clang format on this file so that it isn't changed from its canonical form when included in projects using clang-format.
// clang-format off

/////////////////////////////////////////////////////////////////////////////////
// Macros -----------------------------------------------------------------------
/////////////////////////////////////////////////////////////////////////////////

//! Macro that controls whether the C++ API will be defined
/*!
	The C API is always defined after including this header, but the C++ API is optional.
	In C++ mode, the C API is extended (for example, with default values for struct members) automatically.

	User can set this macro to 0 or 1, either in code before including this, or via compile-flag to choose.

	Defaults to 0 if the compiler is a C compiler, 1 if the compiler is a C++ compiler.
*/
#if !defined(FOVE_DEFINE_CXX_API) || defined(FOVE_DOXYGEN)
	#ifdef __cplusplus
		#define FOVE_DEFINE_CXX_API 1
	#else
		#define FOVE_DEFINE_CXX_API 0
	#endif
#endif

#ifndef FOVE_EXTERN_C
	#ifdef __cplusplus
		#define FOVE_EXTERN_C extern "C"
	#else
		#define FOVE_EXTERN_C
	#endif
#endif

#ifndef FOVE_EXPORT
	#ifdef __GNUC__
		#define FOVE_EXPORT FOVE_EXTERN_C __attribute__((visibility("default")))
	#elif defined(_MSC_VER)
		#define FOVE_EXPORT FOVE_EXTERN_C __declspec(dllexport)
	#else
		#define FOVE_EXPORT FOVE_EXTERN_C
	#endif
#endif

#ifndef FOVE_ENUM
	#if FOVE_DEFINE_CXX_API
		#define FOVE_ENUM(enumName) enum class Fove_ ## enumName
	#else
		#define FOVE_ENUM(enumName) typedef enum
	#endif
#endif

#ifndef FOVE_ENUM_VAL
	#if FOVE_DEFINE_CXX_API
		#define FOVE_ENUM_VAL(enumName, valueName) valueName
	#else
		#define FOVE_ENUM_VAL(enumName, valueName) Fove_ ## enumName ## _ ## valueName
	#endif
#endif

#ifndef FOVE_ENUM_END
	#if FOVE_DEFINE_CXX_API
		#define FOVE_ENUM_END(enumName) ; namespace FOVE_CXX_NAMESPACE { using enumName = Fove_ ## enumName; }
	#else
		#define FOVE_ENUM_END(enumName) Fove_ ## enumName
	#endif
#endif

#ifndef FOVE_STRUCT
	#if FOVE_DEFINE_CXX_API
		#define FOVE_STRUCT(structName) struct Fove_ ## structName
	#else
		#define FOVE_STRUCT(structName) typedef struct
	#endif
#endif

#ifndef FOVE_STRUCT_VAL
	#if FOVE_DEFINE_CXX_API
		#define FOVE_STRUCT_VAL(memberName, defaultVal) memberName = defaultVal
	#else
		#define FOVE_STRUCT_VAL(memberName, defaultVal) memberName
	#endif
#endif

#ifndef FOVE_STRUCT_END
	#if FOVE_DEFINE_CXX_API
		#define FOVE_STRUCT_END(structName) ; namespace FOVE_CXX_NAMESPACE { using structName = Fove_ ## structName; }
	#else
		#define FOVE_STRUCT_END(structName) Fove_ ## structName
	#endif
#endif

#ifndef FOVE_STRUCT_END_NO_CXX_ALIAS
	#if FOVE_DEFINE_CXX_API
		#define FOVE_STRUCT_END_NO_CXX_ALIAS(structName) ;
	#else
		#define FOVE_STRUCT_END_NO_CXX_ALIAS(structName) Fove_ ## structName
	#endif
#endif

#ifndef FOVE_DEPRECATED
	#ifdef __GNUC__
		#define FOVE_DEPRECATED(func, rem) __attribute__ ((deprecated(rem))) func
	#elif defined(_MSC_VER)
		#define FOVE_DEPRECATED(func, rem) __declspec(deprecated(rem)) func
	#else
		#define FOVE_DEPRECATED(func, rem) func
	#endif
#endif

#if FOVE_DEFINE_CXX_API // C++ API specific macros are contained within here

// Namespace that C++ API will be put in
/*!
	Since the C++ API is header-only, the functions in it do not need to be in any particular namespace.

	By default, everything is put in the Fove namespace, but this can be customized if the user prefers.
*/
#ifndef FOVE_CXX_NAMESPACE
	#define FOVE_CXX_NAMESPACE Fove
#endif

// Determines if exceptions are enabled or not in the C++ API
/*!
	Exceptions are enabled by default in the C++ API, because they are a core C++ feature.
	However, they are only used in specific functions, not everywhere.
	If you are working in a code base that bans exceptions, define FOVE_EXCEPTIONS to zero.
	Exceptions are automatically disabled for the unreal engine.
*/
#ifndef FOVE_EXCEPTIONS
	#ifndef __UNREAL__
		#define FOVE_EXCEPTIONS 1
	#else
		#define FOVE_EXCEPTIONS 0
	#endif
#endif

#endif // FOVE_DEFINE_CXX_API

/////////////////////////////////////////////////////////////////////////////////
// Standard includes ------------------------------------------------------------
/////////////////////////////////////////////////////////////////////////////////

// The FOVE API has no dependencies other than the standard library

#if FOVE_DEFINE_CXX_API
#include <exception> // For std::exception
#include <string>    // For std::string
#include <utility>   // For std::move
#endif

#include <stdbool.h> // Pull in the bool type when using C
#include <stddef.h>  // Pull in size_t
#include <stdint.h>  // Pull in fixed length types

/////////////////////////////////////////////////////////////////////////////////
// Doxygen Main Page ------------------------------------------------------------
/////////////////////////////////////////////////////////////////////////////////

/*! @file FoveAPI.h
	@brief Complete self-contained FOVE API definition.
*/

#if FOVE_DEFINE_CXX_API
/*! \mainpage FOVE C++ API Documentation
	\section intro_sec Introduction

	This is the documentation for the FOVE C++ API.

	This API allows client applications to interface with the FOVE runtime system, including
	headsets, eye tracking, position tracking, and the compositor.

	Also included is a "Research API", which is intended specifically for researchers where the
	laboratory environment is fully controlled. The research features are not inteded for use by games.

	An example of using this API can be found at https://github.com/FoveHMD/FoveCppSample

	The C++ API wraps, and includes the FOVE C API.
	Items within the Fove namespace are part of the wrapper, though many are simply typedefs.
	Items outside the namespace are part of the C API, though in C++ mode, more features may be added (such as default values for struct members).

	The main place to get started is looking at the following classes:
	Fove::Headset
	Fove::Compositor

	\section install_sec Installation

	To use the API, simply drop FoveAPI.h into you project and include it.
	After that point you will be able to use all capabilities listed here.

	To link, simply add the FOVE shared library (or DLL on Windows) to the link libraries for your project.

	\section requirements_sec Requirements

	This API requires C++11 or later

	\section backcompat_sec Backwards Compatibility

	Except where noted (see fove_Headset_getResearchHeadset), the FOVE systen maintains backwards compatibility with old clients.

	For example, a v0.15.0 client can talk a a v0.16.0 server.

	Forwards compatibility is not provided: if you compile against v0.16.0, end users on older runtimes must update before using.

	ABI compatibility is not kept between different versions of this API.
	This is something we may consider in the future.
	A client built against v0.15.0 of this API must be recompiled to use v0.16.0 (instead of simply swapping out the new shared library).
*/
#else
/*! \mainpage FOVE C API Documentation
	\section intro_sec Introduction

	This is the documentation for the FOVE C API.

	This API allows client applications to interface with the FOVE runtime system, including
	headsets, eye tracking, position tracking, and the compositor.

	Also included is a "Research API", which is intended specifically for researchers where the
	laboratory environment is fully controlled. The research features are not inteded for use by games.

	Various higher-level bindings are provided by FOVE, such as C++ and C#.

	\section install_sec Installation

	To use the API, simply drop FoveAPI.h into you project and include it.
	After that point you will be able to use all capabilities listed here.

	To link, simply add the FOVE shared library (or DLL on Windows) to the link libraries for your project.

	\section requirements_sec Requirements

	This API requires C99 or later

	\section backcompat_sec Backwards Compatibility

	Except where noted (see fove_Headset_getResearchHeadset), the FOVE systen maintains backwards compatibility with old clients.

	For example, a v0.15.0 client can talk a a v0.16.0 server.

	Forwards compatibility is not provided: if you compile against v0.16.0, end users on older runtimes must update before using.

	ABI compatibility is not kept between different versions of this API.
	This is something we may consider in the future.
	A client built against v0.15.0 of this API must be recompiled to use v0.16.0 (instead of simply swapping out the new shared library).
*/
#endif

/////////////////////////////////////////////////////////////////////////////////
// Fove C/C++ Shared Types ------------------------------------------------------
/////////////////////////////////////////////////////////////////////////////////

//! List of capabilities usable by clients
/*!
	Most features require registering for the relevant capability.
	If a client queries data related to a capability it has not registered API_NotRegistered will be returned.

	This enum is designed to be used as a flag set, so items may be binary logic operators like |.

	The FOVE runtime will keep any given set of hardware/software running so long as one client is registering a capability.

	The registration of a capability does not necessarily mean that the capability is running.
	For example, if no position tracking camera is attached, no position tracking will occur regardless of how many clients registered for it.
*/
FOVE_ENUM(ClientCapabilities)
{
	FOVE_ENUM_VAL(ClientCapabilities, None) = 0x00,        //!< No capabilities requested
	FOVE_ENUM_VAL(ClientCapabilities, Gaze) = 0x01,        //!< Enables eye tracking
	FOVE_ENUM_VAL(ClientCapabilities, Orientation) = 0x02, //!< Enables headset orientation tracking
	FOVE_ENUM_VAL(ClientCapabilities, Position) = 0x04,    //!< Enables headset position tracking
} FOVE_ENUM_END(ClientCapabilities);

//! An enum of error codes that the system may return
FOVE_ENUM(ErrorCode)
{
	FOVE_ENUM_VAL(ErrorCode, None) = 0,

	// Connection Errors
	FOVE_ENUM_VAL(ErrorCode, Connection_General) = 1,
	FOVE_ENUM_VAL(ErrorCode, Connect_NotConnected) = 7,
	FOVE_ENUM_VAL(ErrorCode, Connect_ServerUnreachable) = 2,
	FOVE_ENUM_VAL(ErrorCode, Connect_RegisterFailed) = 3,
	FOVE_ENUM_VAL(ErrorCode, Connect_DeregisterFailed) = 8,
	FOVE_ENUM_VAL(ErrorCode, Connect_RuntimeVersionTooOld) = 4,
	FOVE_ENUM_VAL(ErrorCode, Connect_HeartbeatNoReply) = 5,
	FOVE_ENUM_VAL(ErrorCode, Connect_ClientVersionTooOld) = 6,

	// API usage errors
	FOVE_ENUM_VAL(ErrorCode, API_General) = 100,                //!< There was an error in the usage of the API other than one of the others in this section
	FOVE_ENUM_VAL(ErrorCode, API_InitNotCalled) = 101,          //!< A function that should only be called after initialis was invoked before/without initialise
	FOVE_ENUM_VAL(ErrorCode, API_InitAlreadyCalled) = 102,      //!< A function that should only be called before initialise() was invoked, or initialise was invoked multiple times
	FOVE_ENUM_VAL(ErrorCode, API_InvalidArgument) = 103,        //!< An argument passed to an API function was invalid for a reason other than one of the below reasons
	FOVE_ENUM_VAL(ErrorCode, API_NotRegistered) = 104,          //!< Data was queried without first registering for that data
	FOVE_ENUM_VAL(ErrorCode, API_NullInPointer) = 110,          //!< An input argument passed to an API function was invalid for a reason other than the below reasons
	FOVE_ENUM_VAL(ErrorCode, API_InvalidEnumValue) = 111,       //!< An enum argument passed to an API function was invalid
	FOVE_ENUM_VAL(ErrorCode, API_NullOutPointersOnly) = 120,    //!< All output arguments were null on a function that requires at least one output (all getters that have no side effects)
	FOVE_ENUM_VAL(ErrorCode, API_OverlappingOutPointers) = 121, //!< Two (or more) output parameters passed to an API function overlap in memory. Each output parameter should be a unique, separate object
	FOVE_ENUM_VAL(ErrorCode, API_CompositorNotSwapped) = 122,   //!< This comes from submitting without calling WaitForRenderPose after a complete submit
	FOVE_ENUM_VAL(ErrorCode, API_Timeout) = 130,                //!< A call to an API could not be completed within a timeout

	// Data Errors
	FOVE_ENUM_VAL(ErrorCode, Data_General) = 1000,
	FOVE_ENUM_VAL(ErrorCode, Data_RegisteredWrongVersion) = 1001,
	FOVE_ENUM_VAL(ErrorCode, Data_UnreadableNotFound) = 1002,
	FOVE_ENUM_VAL(ErrorCode, Data_NoUpdate) = 1003,
	FOVE_ENUM_VAL(ErrorCode, Data_Uncalibrated) = 1004,
	FOVE_ENUM_VAL(ErrorCode, Data_MissingIPCData) = 1005,

	// Hardware Errors
	FOVE_ENUM_VAL(ErrorCode, Hardware_General) = 2000,
	FOVE_ENUM_VAL(ErrorCode, Hardware_CoreFault) = 2001,
	FOVE_ENUM_VAL(ErrorCode, Hardware_CameraFault) = 2002,
	FOVE_ENUM_VAL(ErrorCode, Hardware_IMUFault) = 2003,
	FOVE_ENUM_VAL(ErrorCode, Hardware_ScreenFault) = 2004,
	FOVE_ENUM_VAL(ErrorCode, Hardware_SecurityFault) = 2005,
	FOVE_ENUM_VAL(ErrorCode, Hardware_Disconnected) = 2006,
	FOVE_ENUM_VAL(ErrorCode, Hardware_WrongFirmwareVersion) = 2007,

	// Server Response Errors
	FOVE_ENUM_VAL(ErrorCode, Server_General) = 3000,
	FOVE_ENUM_VAL(ErrorCode, Server_HardwareInterfaceInvalid) = 3001,
	FOVE_ENUM_VAL(ErrorCode, Server_HeartbeatNotRegistered) = 3002,
	FOVE_ENUM_VAL(ErrorCode, Server_DataCreationError) = 3003,
	FOVE_ENUM_VAL(ErrorCode, Server_ModuleError_ET) = 3004,

	// Code and placeholders
	FOVE_ENUM_VAL(ErrorCode, Code_NotImplementedYet) = 4000,
	FOVE_ENUM_VAL(ErrorCode, Code_FunctionDeprecated) = 4001,

	// Position Tracking
	FOVE_ENUM_VAL(ErrorCode, Position_NoObjectsInView) = 5000,
	FOVE_ENUM_VAL(ErrorCode, Position_NoDlibRegressor) = 5001,
	FOVE_ENUM_VAL(ErrorCode, Position_NoCascadeClassifier) = 5002,
	FOVE_ENUM_VAL(ErrorCode, Position_NoModel) = 5003,
	FOVE_ENUM_VAL(ErrorCode, Position_NoImages) = 5004,
	FOVE_ENUM_VAL(ErrorCode, Position_InvalidFile) = 5005,
	FOVE_ENUM_VAL(ErrorCode, Position_NoCamParaSet) = 5006,
	FOVE_ENUM_VAL(ErrorCode, Position_CantUpdateOptical) = 5007,
	FOVE_ENUM_VAL(ErrorCode, Position_ObjectNotTracked) = 5008,
	FOVE_ENUM_VAL(ErrorCode, Position_NoCameraFound) = 5009,

	// Eye Tracking
	FOVE_ENUM_VAL(ErrorCode, Eye_Left_NoDlibRegressor) = 6000,
	FOVE_ENUM_VAL(ErrorCode, Eye_Right_NoDlibRegressor) = 6001,
	FOVE_ENUM_VAL(ErrorCode, Eye_CalibrationFailed) = 6002,
	FOVE_ENUM_VAL(ErrorCode, Eye_LoadCalibrationFailed) = 6003,

	// User
	FOVE_ENUM_VAL(ErrorCode, User_General) = 7000,
	FOVE_ENUM_VAL(ErrorCode, User_ErrorLoadingProfile) = 7001,

	// Compositor
	FOVE_ENUM_VAL(ErrorCode, Compositor_UnableToCreateDeviceAndContext) = 8000, //!< Compositor was unable to initialize its backend component
	FOVE_ENUM_VAL(ErrorCode, Compositor_UnableToUseTexture) = 8001,             //!< Compositor was unable to use the given texture (likely due to mismatched client and data types or an incompatible format)
	FOVE_ENUM_VAL(ErrorCode, Compositor_DeviceMismatch) = 8002,                 //!< Compositor was unable to match its device to the texture's, either because of multiple GPUs or a failure to get the device from the texture
	FOVE_ENUM_VAL(ErrorCode, Compositor_IncompatibleCompositorVersion) = 8003,  //!< Compositor client is not compatible with the currently running compositor
	FOVE_ENUM_VAL(ErrorCode, Compositor_UnableToFindRuntime) = 8004,            //!< Compositor isn't running or isn't responding
	FOVE_ENUM_VAL(ErrorCode, Compositor_DisconnectedFromRuntime) = 8006,        //!< Compositor was running and is no longer responding
	FOVE_ENUM_VAL(ErrorCode, Compositor_ErrorCreatingTexturesOnDevice) = 8008,  //!< Failed to create shared textures for compositor
	FOVE_ENUM_VAL(ErrorCode, Compositor_NoEyeSpecifiedForSubmit) = 8009,        //!< The supplied Fove_Eye for submit is invalid (i.e. is Both or Neither)

	// Generic
	FOVE_ENUM_VAL(ErrorCode, UnknownError) = 9000, //!< Errors that are unknown or couldn't be classified. If possible, info will be logged about the nature of the issue
} FOVE_ENUM_END(ErrorCode);

//! Compositor layer type, which defines the order that clients are composited
FOVE_ENUM(CompositorLayerType)
{
	FOVE_ENUM_VAL(CompositorLayerType, Base) = 0,            //!< The first and main application layer
	FOVE_ENUM_VAL(CompositorLayerType, Overlay) = 0x10000,   //!< Layer over the base
	FOVE_ENUM_VAL(CompositorLayerType, Diagnostic) = 0x20000 //!< Layer over Overlay
} FOVE_ENUM_END(CompositorLayerType);

//! Struct to list various version info about the FOVE software
/*! Contains the version for the software (both runtime and client versions).
	A negative value in any int field represents unknown.
*/
FOVE_STRUCT(Versions)
{
	int FOVE_STRUCT_VAL(clientMajor, -1);
	int FOVE_STRUCT_VAL(clientMinor, -1);
	int FOVE_STRUCT_VAL(clientBuild, -1);
	int FOVE_STRUCT_VAL(clientProtocol, -1);
	int FOVE_STRUCT_VAL(runtimeMajor, -1);
	int FOVE_STRUCT_VAL(runtimeMinor, -1);
	int FOVE_STRUCT_VAL(runtimeBuild, -1);
	int FOVE_STRUCT_VAL(firmware, -1);
	int FOVE_STRUCT_VAL(maxFirmware, -1);
	int FOVE_STRUCT_VAL(minFirmware, -1);
	bool FOVE_STRUCT_VAL(tooOldHeadsetConnected, false);
} FOVE_STRUCT_END(Versions);

//! Struct Contains hardware information for the headset
/*! Contains the serial number, manufacturer and model name for the headset.
	Values of the member fields originates from their UTF-8 string representations
	defined by headset manufacturers, and passed to us (FoveClient) by FoveService
	server through an IPC message.
	The server may be sending very long strings, but the FoveClient library will
	be truncating them in an unspecified manner to 0-terminated strings of length
	at most 256.
*/
FOVE_STRUCT(HeadsetHardwareInfo)
{
	char FOVE_STRUCT_VAL(serialNumber[256], {}); //!< Serial number, as a null-terminated UTF8 string
	char FOVE_STRUCT_VAL(manufacturer[256], {}); //!< Manufacturer info, as a null-terminated UTF8 string
	char FOVE_STRUCT_VAL(modelName[256], {});    //!< Model name, as a null-terminated UTF8 string
} FOVE_STRUCT_END_NO_CXX_ALIAS(HeadsetHardwareInfo);

//! Struct representation on a quaternion
/*! A quaternion represents an orientation in 3D space.*/
FOVE_STRUCT(Quaternion)
{
	float FOVE_STRUCT_VAL(x, 0);
	float FOVE_STRUCT_VAL(y, 0);
	float FOVE_STRUCT_VAL(z, 0);
	float FOVE_STRUCT_VAL(w, 1);

#if FOVE_DEFINE_CXX_API // Mostly for MSVC 2015 which doesn't properly implement brace-intialization of structs
	Fove_Quaternion(float xx = 0, float yy = 0, float zz = 0, float ww = 0) : x(xx), y(yy), z(zz), w(ww) {}
#endif
} FOVE_STRUCT_END(Quaternion);

//! Struct to represent a 3D-vector
/*! A vector that represents an position in 3D space. */
FOVE_STRUCT(Vec3)
{
	float FOVE_STRUCT_VAL(x, 0);
	float FOVE_STRUCT_VAL(y, 0);
	float FOVE_STRUCT_VAL(z, 0);

#if FOVE_DEFINE_CXX_API // Mostly for MSVC 2015 which doesn't properly implement brace-intialization of structs
	Fove_Vec3(float xx = 0, float yy = 0, float zz = 0) : x(xx), y(yy), z(zz) {}
#endif
} FOVE_STRUCT_END(Vec3);

//! Struct to represent a 2D-vector
/*! A vector that represents a position or orientation in 2D space, such as screen or image coordinates. */
FOVE_STRUCT(Vec2)
{
	float FOVE_STRUCT_VAL(x, 0);
	float FOVE_STRUCT_VAL(y, 0);

#if FOVE_DEFINE_CXX_API // Mostly for MSVC 2015 which doesn't properly implement brace-intialization of structs
	Fove_Vec2(float xx = 0, float yy = 0): x(xx), y(yy) {}
#endif
} FOVE_STRUCT_END(Vec2);

//! Struct to represent a 2D-vector of integers
FOVE_STRUCT(Vec2i)
{
	int FOVE_STRUCT_VAL(x, 0);
	int FOVE_STRUCT_VAL(y, 0);

#if FOVE_DEFINE_CXX_API // Mostly for MSVC 2015 which doesn't properly implement brace-intialization of structs
	Fove_Vec2i(int xx = 0, int yy = 0) : x(xx), y(yy) {}
#endif
} FOVE_STRUCT_END(Vec2i);

//! Struct to represent a Ray
/*! Stores the start point and direction of a Ray */
FOVE_STRUCT(Ray)
{
	//! The start point of the Ray
	Fove_Vec3 FOVE_STRUCT_VAL(origin, (Fove_Vec3{0, 0, 0}));
	//! The direction of the Ray
	Fove_Vec3 FOVE_STRUCT_VAL(direction, (Fove_Vec3{0, 0, 1}));
} FOVE_STRUCT_END(Ray);

//! Struct to represent a combination of position and orientation of Fove Headset
/*! This structure is a combination of the Fove headset position and orientation in 3D space, collectively known as the "pose".
	In the future this may also contain accelleration information for the headset, and may also be used for controllers.
*/
FOVE_STRUCT(Pose)
{
	//! Incremental counter which tells if the coord captured is a fresh value at a given frame
	uint64_t FOVE_STRUCT_VAL(id, 0);
	//! The time at which the pose was captured, in microseconds since an unspecified epoch
	uint64_t FOVE_STRUCT_VAL(timestamp, 0);
	//! The Quaternion which represents the orientation of the head
	Fove_Quaternion FOVE_STRUCT_VAL(orientation, {});
	//! The angular velocity of the head
	Fove_Vec3 FOVE_STRUCT_VAL(angularVelocity, {});
	//! The angular acceleration of the head
	Fove_Vec3 FOVE_STRUCT_VAL(angularAcceleration, {});
	//! The position of headset in 3D space. Tares to (0, 0, 0). Use for sitting applications
	Fove_Vec3 FOVE_STRUCT_VAL(position, {});
	//! The position of headset including offset for camera location. Will not tare to zero. Use for standing applications
	Fove_Vec3 FOVE_STRUCT_VAL(standingPosition, {});
	//! The velocity of headset in 3D space
	Fove_Vec3 FOVE_STRUCT_VAL(velocity, {});
	//! The acceleration of headset in 3D space
	Fove_Vec3 FOVE_STRUCT_VAL(acceleration, {});
} FOVE_STRUCT_END(Pose);

//! Struct to represent a unit vector out from the eye center along which that eye is looking
/*!
	The vector value is in eye-relative coordinates, meaning that it is not affected by the position
	or orientation of the HMD, but rather represents the absolute orientation of the eye's gaze.
*/
FOVE_STRUCT(GazeVector)
{
	//! Incremental counter which tells if the convergence data is a fresh value at a given frame
	uint64_t FOVE_STRUCT_VAL(id, 0);
	//! The time at which the gaze data was captured, in microseconds since an unspecified epoch
	uint64_t FOVE_STRUCT_VAL(timestamp, 0);
	//! Directional veector of the gaze
	Fove_Vec3 FOVE_STRUCT_VAL(vector, (Fove_Vec3{0, 0, 1}));
} FOVE_STRUCT_END(GazeVector);

//! Struct to represent the vector pointing where the user is looking at
/*! The vector (from the center of the player's head in world space) that can be used to approximate the point that the user is looking at. */
FOVE_STRUCT(GazeConvergenceData)
{
	//! Incremental counter which tells if the convergence data is a fresh value at a given frame
	uint64_t FOVE_STRUCT_VAL(id, 0);
	//! The time at which the convergence data was captured, in microseconds since an unspecified epoch
	uint64_t FOVE_STRUCT_VAL(timestamp, 0);
	//! The ray pointing towards the expected convergence point
	Fove_Ray FOVE_STRUCT_VAL(ray, {});
	//! The expected distance to the convergence point, Range: 0 to Infinity
	float FOVE_STRUCT_VAL(distance, 0.0f);
	//! Pupil dilation is given as a ratio relative to a baseline. 1 means average. Range: 0 to Infinity
	float FOVE_STRUCT_VAL(pupilDilation, 0.0f);
	//! True if the user is looking at something (fixation or pursuit), rather than saccading between objects. This could be used to suppress eye input during large eye motions
	bool FOVE_STRUCT_VAL(attention, false);
} FOVE_STRUCT_END(GazeConvergenceData);

//! Severity level of log messages
FOVE_ENUM(LogLevel)
{
	FOVE_ENUM_VAL(LogLevel, Debug) = 0,
	FOVE_ENUM_VAL(LogLevel, Warning) = 1,
	FOVE_ENUM_VAL(LogLevel, Error) = 2,
} FOVE_ENUM_END(LogLevel);

//! Enum to identify which eye is being used
/*! This is usually returned with any eye tracking information and tells the client which eye(s) the information is based on. */
FOVE_ENUM(Eye)
{
	FOVE_ENUM_VAL(Eye, Neither) = 0, //!< Neither eye
	FOVE_ENUM_VAL(Eye, Left) = 1,    //!< Left eye only
	FOVE_ENUM_VAL(Eye, Right) = 2,   //!< Right eye only
	FOVE_ENUM_VAL(Eye, Both) = 3     //!< Both eyes
} FOVE_ENUM_END(Eye);

//! Struct to hold a rectangular array
FOVE_STRUCT(Matrix44)
{
	float FOVE_STRUCT_VAL(mat[4][4], {}); //!< Matrix data
} FOVE_STRUCT_END(Matrix44);

//! Struct holding information about projection fustum planes
/*! Values are given for a depth of 1 so that it's asy to multiply them by your near clipping plan, for example, to get the correct values for your use. */
FOVE_STRUCT(ProjectionParams)
{
	float FOVE_STRUCT_VAL(left, -1);   //!< Left side (low-X)
	float FOVE_STRUCT_VAL(right, 1);   //!< Right side (high-X)
	float FOVE_STRUCT_VAL(top, 1);     //!< Top (high-Y)
	float FOVE_STRUCT_VAL(bottom, -1); //!< Bottom (low-Y)
} FOVE_STRUCT_END(ProjectionParams);

//! enum for type of Graphics API
/*! Type of Graphics API
	Note: We currently only support DirectX
*/
FOVE_ENUM(GraphicsAPI)
{
	FOVE_ENUM_VAL(GraphicsAPI, DirectX) = 0, //!< DirectX (Windows only)
	FOVE_ENUM_VAL(GraphicsAPI, OpenGL) = 1,  //!< OpenGL (All platforms, currently in BETA)
	FOVE_ENUM_VAL(GraphicsAPI, Metal) = 2,   //!< Metal (Mac only)
} FOVE_ENUM_END(GraphicsAPI);

//! Enum to help interpret the alpha of texture
/*! Determines how to interpret the alpha of a compositor client texture */
FOVE_ENUM(AlphaMode)
{
	FOVE_ENUM_VAL(AlphaMode, Auto) = 0,   //!< Base layers will use One, overlay layers will use Sample
	FOVE_ENUM_VAL(AlphaMode, One) = 1,    //!< Alpha will always be one (fully opaque)
	FOVE_ENUM_VAL(AlphaMode, Sample) = 2, //!< Alpha fill be sampled from the alpha channel of the buffer
} FOVE_ENUM_END(AlphaMode);

//! Struct used to define the settings for a compositor client
/*! Structure used to define the settings for a compositor client.*/
FOVE_STRUCT(CompositorLayerCreateInfo)
{
	//! The type (layer) upon which the client will draw
	Fove_CompositorLayerType FOVE_STRUCT_VAL(type, Fove_CompositorLayerType::Base);
	//! Setting to disable timewarp, e.g. if an overlay client is operating in screen space
	bool FOVE_STRUCT_VAL(disableTimeWarp, false);
	//! Setting about whether to use alpha sampling or not, e.g. for a base client
	Fove_AlphaMode FOVE_STRUCT_VAL(alphaMode, Fove_AlphaMode::Auto);
	//! Setting to disable fading when the base layer is misbehaving, e.g. for a diagnostic client
	bool FOVE_STRUCT_VAL(disableFading, false);
	//! Setting to disable a distortion pass, e.g. for a diagnostic client, or a client intending to do its own distortion
	bool FOVE_STRUCT_VAL(disableDistortion, false);
} FOVE_STRUCT_END(CompositorLayerCreateInfo);

//! Struct used to store information about an existing compositor layer (after it is created)
/*! This exists primarily for future expandability. */
FOVE_STRUCT(CompositorLayer)
{
	//! Uniquely identifies a compositor layer
	int FOVE_STRUCT_VAL(layerId, 0);

	/*! The optimal resolution for a submitted buffer on this layer (for a single eye).
		Clients are allowed to submit buffers of other resolutions.
		In particular, clients can use a lower resolution buffer to reduce their rendering overhead.
	*/
	Fove_Vec2i FOVE_STRUCT_VAL(idealResolutionPerEye, {});
} FOVE_STRUCT_END(CompositorLayer);

//! Base class of API-specific texture classes
FOVE_STRUCT(CompositorTexture)
{
	//! Rendering API of this texture
	/*!
		If this is DirectX, this object must be a Fove_DX11Texture
		If this is OpenGL, this object must be a Fove_GLTexture
		In C++ this field is initialized automatically by the subclass
	*/
	Fove_GraphicsAPI graphicsAPI;

#if FOVE_DEFINE_CXX_API
protected:
	// Create and destroy objects via one of the derived classes, based on which graphics API you are submitting with.
	Fove_CompositorTexture(const Fove_GraphicsAPI api) : graphicsAPI{api} {}
	~Fove_CompositorTexture() = default;
#endif // FOVE_DEFINE_CXX_API

} FOVE_STRUCT_END(CompositorTexture);

//! Struct used to submit a DirectX 11 texture
FOVE_STRUCT(DX11Texture)
#if FOVE_DEFINE_CXX_API
	: public Fove_CompositorTexture
#endif
{
#if !FOVE_DEFINE_CXX_API
	//! Parent object
	Fove_CompositorTexture parent;
#endif

	//! This must point to a ID3D11Texture2D
	void* FOVE_STRUCT_VAL(texture, nullptr);

#if FOVE_DEFINE_CXX_API
	Fove_DX11Texture(void* const t = nullptr) : Fove_CompositorTexture{Fove_GraphicsAPI::DirectX}, texture{t} {}
#endif // FOVE_DEFINE_CXX_API

} FOVE_STRUCT_END(DX11Texture);

//! Struct used to submit an OpenGL texture
/*! The GL context must be active on the thread that submits this. */
FOVE_STRUCT(GLTexture)
#if FOVE_DEFINE_CXX_API
	: public Fove_CompositorTexture
#endif
{
#if !FOVE_DEFINE_CXX_API
	//! Parent object
	Fove_CompositorTexture parent;
#endif

	//! The opengl id of the texture, as returned by glGenTextures
	uint32_t FOVE_STRUCT_VAL(textureId, 0);
	//! On mac, this is a CGLContextObj, otherwise this field is reserved and you must pass null
	void* FOVE_STRUCT_VAL(context, nullptr);

#if FOVE_DEFINE_CXX_API
	Fove_GLTexture(const uint32_t tid = 0, void* const c = nullptr) : Fove_CompositorTexture{Fove_GraphicsAPI::OpenGL}, textureId{tid}, context{c} {}
#endif // FOVE_DEFINE_CXX_API

} FOVE_STRUCT_END(GLTexture);

//! Struct used to submit a texture using the Apple Metal API
FOVE_STRUCT(MetalTexture)
#if FOVE_DEFINE_CXX_API
	: public Fove_CompositorTexture
#endif
{
#if !FOVE_DEFINE_CXX_API
	//! Parent object
	Fove_CompositorTexture parent;
#endif

	//! Pointer to an MTLTexture (which must have MTLTextureUsageShaderRead specified).
	void* FOVE_STRUCT_VAL(texture, nullptr);

#if FOVE_DEFINE_CXX_API
	Fove_MetalTexture(void* const t = nullptr) : Fove_CompositorTexture{Fove_GraphicsAPI::Metal}, texture{t} {}
#endif // FOVE_DEFINE_CXX_API

} FOVE_STRUCT_END(MetalTexture);

//! Struct to represent coordinates in normalized space
/*! Coordinates in normalized space where 0 is left/top and 1 is bottom/right */
FOVE_STRUCT(TextureBounds)
{
	float FOVE_STRUCT_VAL(left, 0.0f);
	float FOVE_STRUCT_VAL(top, 0.0f);
	float FOVE_STRUCT_VAL(right, 0.0f);
	float FOVE_STRUCT_VAL(bottom, 0.0f);
} FOVE_STRUCT_END(TextureBounds);

//! Struct used to conglomerate the texture settings for a single eye, when submitting a given layer
FOVE_STRUCT(CompositorLayerEyeSubmitInfo)
{
	//! Texture to submit for this eye
	/*! This may be null as long as the other submitted eye's texture isn't (thus allowing each eye to be submitted separately) */
	const Fove_CompositorTexture* FOVE_STRUCT_VAL(texInfo, nullptr);

	//! The portion of the texture that is used to represent the eye (Eg. half of it if the texture contains both eyes)
	Fove_TextureBounds FOVE_STRUCT_VAL(bounds, {});
} FOVE_STRUCT_END(CompositorLayerEyeSubmitInfo);

//! Struct used to conglomerate the texture settings when submitting a given layer
FOVE_STRUCT(CompositorLayerSubmitInfo)
{
	int FOVE_STRUCT_VAL(layerId, 0);                              //!< The layer ID as fetched from Fove_CompositorLayer
	Fove_Pose FOVE_STRUCT_VAL(pose, {});                          //!< The pose used to draw this layer, usually coming from fove_Compositor_waitForRenderPose
	Fove_CompositorLayerEyeSubmitInfo FOVE_STRUCT_VAL(left, {});  //!< Information about the left eye
	Fove_CompositorLayerEyeSubmitInfo FOVE_STRUCT_VAL(right, {}); //!< Information about the left eye
} FOVE_STRUCT_END(CompositorLayerSubmitInfo);

//! Struct used to identify a GPU adapter (Windows only)
FOVE_STRUCT(AdapterId)
{
#ifdef _WIN32
	// On windows, this forms a LUID structure
	uint32_t FOVE_STRUCT_VAL(lowPart, 0);
	int32_t FOVE_STRUCT_VAL(highPart, 0);
#endif
} FOVE_STRUCT_END(AdapterId);

//! A generic memory buffer
/*! No ownership or lifetime semantics are specified. Please see the comments on the functions that use this. */
FOVE_STRUCT(Buffer)
{
	//! Pointer to the start of the memory buffer
	const void* FOVE_STRUCT_VAL(data, nullptr);
	//! Length, in bytes, of the buffer
	size_t FOVE_STRUCT_VAL(length, 0);
} FOVE_STRUCT_END(Buffer);

//! Research-API-specific capabilities
FOVE_ENUM(ResearchCapabilities)
{
	FOVE_ENUM_VAL(ResearchCapabilities, None) = 0x00,
	FOVE_ENUM_VAL(ResearchCapabilities, EyeImage) = 0x01,
	FOVE_ENUM_VAL(ResearchCapabilities, PositionImage) = 0x02,
} FOVE_ENUM_END(ResearchCapabilities);

//! Struct for returning gaze data from the research API
FOVE_STRUCT(ResearchGaze)
{
	uint64_t FOVE_STRUCT_VAL(id, 0);        //!< Incremental counter which tells if the data is a fresh value at a given frame
	uint64_t FOVE_STRUCT_VAL(timestamp, 0); //!< The time at which the gaze data was captured, in microseconds since an unspecified epoch
	float FOVE_STRUCT_VAL(pupilRadiusL, 0); //!< Radius in meters of the left pupil
	float FOVE_STRUCT_VAL(pupilRadiusR, 0); //!< Radius in meters of the right pupil
	float FOVE_STRUCT_VAL(iod, 0);          //!< Distance in meters between the center of the eyes
	float FOVE_STRUCT_VAL(ipd, 0);          //!< Distance in meters between the pupil centers (continually updated as the eyes move)
} FOVE_STRUCT_END(ResearchGaze);

//! Indicates the source of an image
FOVE_ENUM(ImageType)
{
	FOVE_ENUM_VAL(ImageType, StereoEye) = 0x00, //!< Image comes from an eye camera, with the left/right eyes stiched into one image
	FOVE_ENUM_VAL(ImageType, Position) = 0x01   //!< Image comes from a position tracking camera
} FOVE_ENUM_END(ImageType);

//! A 2D bitmap image
FOVE_STRUCT(BitmapImage)
{
	//! Timestamp of the image, in microseconds since an unspecified epoch
	uint64_t FOVE_STRUCT_VAL(timestamp, 0);
	//! Type of the bitmap for disambiguation
	Fove_ImageType FOVE_STRUCT_VAL(type, {});
	//! BMP data (including full header that contains size, format, etc)
	/*! The height may be negative to specify a top-down bitmap. */
	Fove_Buffer FOVE_STRUCT_VAL(image, {});
} FOVE_STRUCT_END(BitmapImage);

/////////////////////////////////////////////////////////////////////////////////
// Fove C API -------------------------------------------------------------------
/////////////////////////////////////////////////////////////////////////////////

// This API requires C99 or later

// All functions in the C API return Fove_ErrorCode
// Other return parameters are written via out pointers

typedef struct Fove_Headset_* Fove_Headset;                 //!< Opaque type representing a headset object
typedef struct Fove_Compositor_* Fove_Compositor;           //!< Opaque type representing a compositor connection
typedef struct Fove_ResearchHeadset_* Fove_ResearchHeadset; //!< Opaque type representing a headset with research-specific capabilities

//! Writes some text to the FOVE log something to the FOVE log
/*!
	\param level What severity level the log will use
	\param utf8Text Null-terminated text string in UTF8
 */
FOVE_EXPORT Fove_ErrorCode fove_logText(Fove_LogLevel level, const char* utf8Text);

//! Creates and returns an Fove_Headset object, which is the entry point to the entire API
/*!
	The result headset should be destroyed using fove_Headset_destroy when no longer needed.
	\param capabilities The desired capabilities (Gaze, Orientation, Position), for multiple capabilities, use bitwise-or input: Fove_ClientCapabilities::Gaze | Fove_ClientCapabilities::Position
	\param outHeadset A pointer where the address of the newly created headset will be written upon success
	\see fove_Headset_destroy
*/
FOVE_EXPORT Fove_ErrorCode fove_createHeadset(Fove_ClientCapabilities capabilities, Fove_Headset** outHeadset);

//! Frees resources used by a headset object, including memory and sockets
/*!
	Upon return, this headset pointer, and any research headsets from it, should no longer be used.
	\see fove_createHeadset
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_destroy(Fove_Headset*);

//! Writes out whether an HMD is know to be connected or not
/*!
	\param outHardwareConnected A pointer to the value to be written
	\return Any error detected that might make the out data unreliable
	\see fove_createHeadset
 */
FOVE_EXPORT Fove_ErrorCode fove_Headset_isHardwareConnected(Fove_Headset*, bool* outHardwareConnected);

//! Writes out whether the hardware for the requested capabilities has started
/*!
	\return Any error detected that might make the out data unreliable
 */
FOVE_EXPORT Fove_ErrorCode fove_Headset_isHardwareReady(Fove_Headset*, bool* outIsReady);

//! Checks whether the client can run against the installed version of the FOVE SDK
/*!
	\return None if this client is compatible with the currently running service
			Connect_RuntimeVersionTooOld if not compatible with the currently running service
			Otherwise returns an error representing why this can't be determined
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_checkSoftwareVersions(Fove_Headset*);

//! Writes out information about the current software versions
/*!
	Allows you to get detailed information about the client and runtime versions.
	Instead of comparing software versions directly, you should simply call
	`CheckSoftwareVersions` to ensure that the client and runtime are compatible.
 */
FOVE_EXPORT Fove_ErrorCode fove_Headset_getSoftwareVersions(Fove_Headset*, Fove_Versions* outSoftwareVersions);

//! Writes out information about the hardware information
/*!
	Allows you to get serial number, manufacturer, and model name of the headset.
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_getHardwareInfo(Fove_Headset*, Fove_HeadsetHardwareInfo* outHardwareInfo);

//! Waits for next camera frame and associated eye tracking info becomes available
/*!
	Allows you to sync your eye tracking loop to the actual eye-camera loop.
	On each loop, you would first call this blocking function to wait for a new frame
	and then proceed to consume eye tracking info accociated with the frame.
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_waitForNextEyeFrame(Fove_Headset*);

//! Writes out each eye's current gaze vector
/*!
	If either argument is `nullptr`, only the other value will be written. It is an error for both arguments to
	be `nullptr`.
	\param outLeft  A pointer to the left eye gaze vector which will be written to
	\param outRight A pointer to the right eye gaze vector which will be written to
	\return         Any error detected while fetching and writing the gaze vectors
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_getGazeVectors(Fove_Headset*, Fove_GazeVector* outLeft, Fove_GazeVector* outRight);

//! Writes out the user's 2D gaze position on the screens seen through the HMD's lenses
/*!
	The use of lenses and distortion correction creates a screen in front of each eye.
	This function returns 2D vectors representing where on each eye's screen the user
	is looking.
	The vectors are normalized in the range [-1, 1] along both X and Y axes such that the
	following points are true:

	Center: (0, 0)
	Bottom-Left: (-1, -1)
	Top-Right: (1, 1)

	\param outLeft  A pointer to the left eye gaze point in the HMD's virtual screen space
	\param outRight A pointer to the right eye gaze point in the HMD's virtual screen space
	\return         Any error detected while fetching and writing the data
 */
FOVE_EXPORT Fove_ErrorCode fove_Headset_getGazeVectors2D(Fove_Headset*, Fove_Vec2* outLeft, Fove_Vec2* outRight);

//! Writes out eye convergence data
/*!
	\param  outConvergenceData  A pointer to the convergence data struct to be written
	\return                     Any error detected while fetching and writing the data
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_getGazeConvergence(Fove_Headset*, Fove_GazeConvergenceData* outConvergenceData);

//! Writes out which eyes are closed
/*!
	\param outEye   A pointer to the variable to be written
	\return         Any error detected while fetching and writing the data
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_checkEyesClosed(Fove_Headset*, Fove_Eye* outEye);

//! Writes out which eyes are being tracked
/*!
	\param outEye   A pointer to the variable to be written
	\return         Any error detected while fetching and writing the data
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_checkEyesTracked(Fove_Headset*, Fove_Eye* outEye);

//! Writes out whether the eye tracking hardware has started
/*!
	\param outEyeTrackingEnabled    A pointer to the variable to be written
	\return                         Any error detected while fetching and writing the data
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_isEyeTrackingEnabled(Fove_Headset*, bool* outEyeTrackingEnabled);

//! Writes out whether eye tracking has been calibrated
/*!
	\param outEyeTrackingCalibrated A pointer to the variable to be written
	\return                         Any error detected while fetching and writing the data
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_isEyeTrackingCalibrated(Fove_Headset*, bool* outEyeTrackingCalibrated);

//! Writes out whether eye tracking is in the process of performing a calibration
/*!
	\param outEyeTrackingCalibrating    A pointer to the variable to be written
	\return                             Any error detected while fetching and writing the data
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_isEyeTrackingCalibrating(Fove_Headset*, bool* outEyeTrackingCalibrating);

//! Writes out whether eye tracking is actively tracking an eye - or eyes
/*!
	This means that hardware is enabled and eye tracking is calibrated when the variable is set to `true`.
	\param outEyeTrackingReady  A pointer to the variable to be written
	\return                     Any error detected while fetching and writing the data
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_isEyeTrackingReady(Fove_Headset*, bool* outEyeTrackingReady);

//! Writes out whether motion tracking hardware has started
/*!
	\param outMotionReady   A pointer to the variable to be written
	\return                 Any error detected while fetching and writing the data
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_isMotionReady(Fove_Headset*, bool* outMotionReady);

//! Tares the orientation of the headset
/*!
	\return Any error detected while fetching and writing the data
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_tareOrientationSensor(Fove_Headset*);

//! Writes out whether position tracking hardware has started and returns whether it was successful
/*!
	\param outPositionReady A pointer to the variable to be written
	\return                 Any error detected while fetching and writing the data
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_isPositionReady(Fove_Headset*, bool* outPositionReady);

//! Tares the position of the headset
/*!
	\return Any error detected while fetching and writing the data
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_tarePositionSensors(Fove_Headset*);

//! Writes out the pose of the head-mounted display
/*!
	\param outPose  A pointer to the variable to be written
	\return         Any error detected while fetching and writing the data
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_getLatestPose(Fove_Headset*, Fove_Pose* outPose);

//! Writes out the values of passed-in left-handed 4x4 projection matrices
/*!
	Writes 4x4 projection matrices for both eyes using near and far planes in a left-handed coordinate
	system. Either outLeftMat or outRightMat may be `nullptr` to only write the other matrix, however setting
	both to `nullptr` is considered invalid and will return `Fove_ErrorCode::API_NullOutPointersOnly`.
	\param zNear        The near plane in float, Range: from 0 to zFar
	\param zFar         The far plane in float, Range: from zNear to infinity
	\param outLeftMat   A pointer to the matrix you want written
	\param outRightMat  A pointer to the matrix you want written
	\return             Any error detected while fetching and writing the data
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_getProjectionMatricesLH(Fove_Headset*, float zNear, float zFar, Fove_Matrix44* outLeftMat, Fove_Matrix44* outRightMat);

//! Writes out the values of passed-in right-handed 4x4 projection matrices
/*!
	Writes 4x4 projection matrices for both eyes using near and far planes in a right-handed coordinate
	system. Either outLeftMat or outRightMat may be `nullptr` to only write the other matrix, however setting
	both to `nullptr` is considered invalid and will return `Fove_ErrorCode::API_NullOutPointersOnly`.
	\param zNear        The near plane in float, Range: from 0 to zFar
	\param zFar         The far plane in float, Range: from zNear to infinity
	\param outLeftMat   A pointer to the matrix you want written
	\param outRightMat  A pointer to the matrix you want written
	\return             Any error detected while fetching and writing the data
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_getProjectionMatricesRH(Fove_Headset*, float zNear, float zFar, Fove_Matrix44* outLeftMat, Fove_Matrix44* outRightMat);

//! Writes out values for the view frustum of the specified eye at 1 unit away
/*!
	Writes out values for the view frustum of the specified eye at 1 unit away. Please multiply them by zNear to
	convert to your correct frustum near-plane. Either outLeft or outRight may be `nullptr` to only write the
	other struct, however setting both to `nullptr` is considered and error and the function will return
	`Fove_ErrorCode::API_NullOutPointersOnly`.
	\param outLeft  A pointer to the struct describing the left camera projection parameters
	\param outRight A pointer to the struct describing the right camera projection parameters
	\return         Any error detected while fetching and writing data
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_getRawProjectionValues(Fove_Headset*, Fove_ProjectionParams* outLeft, Fove_ProjectionParams* outRight);

//! Writes out the matrices to convert from eye- to head-space coordinates
/*!
	This is simply a translation matrix that returns +/- IOD/2
	\param outLeft   A pointer to the matrix where left-eye transform data will be written
	\param outRight  A pointer to the matrix where right-eye transform data will be written
	\return          Any error detected while fetching and writing data
 */
FOVE_EXPORT Fove_ErrorCode fove_Headset_getEyeToHeadMatrices(Fove_Headset*, Fove_Matrix44* outLeft, Fove_Matrix44* outRight);

//! Interocular distance, returned in meters
/*!
	This is an estimation of the distance between centers of the left and right eyeballs.
	Half of the IOD can be used to displace the left and right cameras for stereoscopic rendering.
	We recommend calling this each frame when doing stereoscoping rendering.
	Future versions of the FOVE service may update the IOD during runtime as needed.
	\param outIOD A floating point value where the IOD will be written upon exit if there is no error
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_getIOD(Fove_Headset*, float* outIOD);

//! Starts calibration if not already calibrated
/*!
	Does nothing if the user is already calibrated.
	Does nothing if the calibration is currently running.

	All eye tracking content should call this before using the gaze to ensure that there's a valid calibration.
	After calling this, content should periodically poll for IsEyeTrackingCalibration() to become false,
	so as to ensure that the content is not updating while obscured by the calibrator
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_ensureEyeTrackingCalibration(Fove_Headset*);

//! Starts eye tracking calibration
/*!
	\param restartIfRunning If true, this will cause the calibration to restart if it's already running
	Otherwise this will do nothing if eye tracking calibration is currently running.
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_startEyeTrackingCalibration(Fove_Headset*, bool restartIfRunning);

//! Stops eye tracking calibration if it's running, does nothing if it's not running
FOVE_EXPORT Fove_ErrorCode fove_Headset_stopEyeTrackingCalibration(Fove_Headset*);

//! Returns a compositor interface from the given headset
/*!
	Each call to this function creates a new object. The object should be destroyed with fove_Compositor_destroy
	It is fine to call this function multiple times with the same headset, the same pointer will be returned.
	It is ok for the compositor to outlive the headset passed in.
	\see fove_Compositor_destroy
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_createCompositor(Fove_Headset*, Fove_Compositor** outCompositor);

//! Frees resources used by ta compositor object, including memory and sockets
/*!
	Upon return, this compositor pointer should no longer be used.
	\see fove_Headset_createCompositor
*/
FOVE_EXPORT Fove_ErrorCode fove_Compositor_destroy(Fove_Compositor*);

//! Creates a new layer within the compositor
/*!
	This function create a layer upon which frames may be submitted to the compositor by this client.

	A connection to the compositor must exists for this to pass.
	This means you need to wait for fove_Compositor_isReady before calling this function.
	However, if connection to the compositor is lost and regained, this layer will persist.
	For this reason, you should not recreate your layers upon reconnection, simply create them once.

	There is no way to delete a layer once created, other than to destroy the Fove_Compositor object.
	This is a feature we would like to add in the future.

	\param layerInfo The settings for the layer to be created
	\param outLayer A struct where the the defaults of the newly created layer will be written
	\see fove_Compositor_submit
*/
FOVE_EXPORT Fove_ErrorCode fove_Compositor_createLayer(Fove_Compositor*, const Fove_CompositorLayerCreateInfo* layerInfo, Fove_CompositorLayer* outLayer);

//! Submit a frame to the compositor
/*! This function takes the feed from your game engine to the compositor for output.
	\param submitInfo   An array of layerCount Fove_LayerSubmitInfo structs, each of which provides texture data for a unique layer
	\param layerCount   The number of layers you are submitting
	\see fove_Compositor_submit
*/
FOVE_EXPORT Fove_ErrorCode fove_Compositor_submit(Fove_Compositor*, const Fove_CompositorLayerSubmitInfo* submitInfo, size_t layerCount);

//! Wait for the most recent pose for rendering purposes
/*! All compositor clients should use this function as the sole means of limiting their frame rate.
	This allows the client to render at the correct frame rate for the HMD display.
	Upon this function returning, the client should proceed directly to rendering, to reduce the chance of missing the frame.
	If outPose is not null, this function will return the latest pose as a conveience to the caller.
	In general, a client's main loop should look like:
	{
		Update();                            // Run AI, physics, etc, for the next frame
		compositor.WaitForRenderPose(&pose); // Wait for the next frame, and get the pose
		Draw(pose);                          // Render the scene using the new pose
	}
*/
FOVE_EXPORT Fove_ErrorCode fove_Compositor_waitForRenderPose(Fove_Compositor*, Fove_Pose* outPose);

//! Get the last cached pose for rendering purposes
FOVE_EXPORT Fove_ErrorCode fove_Compositor_getLastRenderPose(Fove_Compositor*, Fove_Pose* outPose);

//! Returns true if we are connected to a running compositor and ready to submit frames for compositing
FOVE_EXPORT Fove_ErrorCode fove_Compositor_isReady(Fove_Compositor*, bool* outIsReady);

//! Returns the ID of the GPU currently attached to the headset
/*! For systems with multiple GPUs, submitted textures to the compositor must from the same GPU that the compositor is using */
FOVE_EXPORT Fove_ErrorCode fove_Compositor_getAdapterId(Fove_Compositor*, Fove_AdapterId* outAdapterId);

//! Converts an existing headset object into a research headset
/*!
	It is fine to call this function multiple times with the same headset, the same pointer will be returned.
	The research API does not provide backwards or forwards compatibility with different FOVE runtimes.
	Do not release general purpose software using this API, this is meant for researcher user in a controlled environment (lab).
	The result Fove_ResearchHeadset is destroyed when the input headset object is destroyed. There is no destroy/free function for the research headset specifically.
	\param caps These capabilities are automatically passed to fove_ResearchHeadset_registerCapabilities so as to avoid an extra call
	\param outHeadset A pointer where the address of the newly created research headset object will be written upon success
	\see fove_Headset_destroy
*/
FOVE_EXPORT Fove_ErrorCode fove_Headset_getResearchHeadset(Fove_Headset*, Fove_ResearchCapabilities caps, Fove_ResearchHeadset** outHeadset);

//! Registers a research capability, enabling the required hardware as needed
/*!
	Normally this is invoked directly via fove_Headset_getResearchHeadset.
	You can add and remove capabilities while the object is alive.
	\param caps A set of capabitilties to register. Reregistering an existing capability is a no-op
*/
FOVE_EXPORT Fove_ErrorCode fove_ResearchHeadset_registerCapabilities(Fove_ResearchHeadset*, Fove_ResearchCapabilities caps);

//! Deregisters a research capability previously registed with Fove_RegisterResearchCapabilities
FOVE_EXPORT Fove_ErrorCode fove_ResearchHeadset_unregisterCapabilities(Fove_ResearchHeadset*, Fove_ResearchCapabilities caps);

//! Returns the latest image of the given type
/*! The image data buffer is invalidated upon the next call to this function with the same image type */
FOVE_EXPORT Fove_ErrorCode fove_ResearchHeadset_getImage(Fove_ResearchHeadset*, Fove_ImageType type, Fove_BitmapImage* outImage);

//! Returns research-related information from eye tracking
FOVE_EXPORT Fove_ErrorCode fove_ResearchHeadset_getGaze(Fove_ResearchHeadset*, Fove_ResearchGaze* outGaze);

/////////////////////////////////////////////////////////////////////////////////
// Fove C++ API -----------------------------------------------------------------
/////////////////////////////////////////////////////////////////////////////////

// This API is header only so C++ ABI compatibility is not an issue
// The only external references used are via the C API

#if FOVE_DEFINE_CXX_API

// Helpers for bit-joining capabilities
/// @cond Capabilities_Functions
inline constexpr Fove_ClientCapabilities operator|(Fove_ClientCapabilities a, Fove_ClientCapabilities b) { return static_cast<Fove_ClientCapabilities>(static_cast<int>(a) | static_cast<int>(b)); }
inline constexpr Fove_ClientCapabilities operator&(Fove_ClientCapabilities a, Fove_ClientCapabilities b) { return static_cast<Fove_ClientCapabilities>(static_cast<int>(a) & static_cast<int>(b)); }
inline constexpr Fove_ClientCapabilities operator~(Fove_ClientCapabilities a) { return static_cast<Fove_ClientCapabilities>(~static_cast<int>(a)); }
inline constexpr Fove_ResearchCapabilities operator|(Fove_ResearchCapabilities a, Fove_ResearchCapabilities b) { return static_cast<Fove_ResearchCapabilities>(static_cast<int>(a) | static_cast<int>(b)); }
inline constexpr Fove_ResearchCapabilities operator&(Fove_ResearchCapabilities a, Fove_ResearchCapabilities b) { return static_cast<Fove_ResearchCapabilities>(static_cast<int>(a) & static_cast<int>(b)); }
inline constexpr Fove_ResearchCapabilities operator~(Fove_ResearchCapabilities a) { return static_cast<Fove_ResearchCapabilities>(~static_cast<int>(a)); }
/// @endcond

namespace FOVE_CXX_NAMESPACE
{

//! Class to hold two copies of something, one for each left and right respectively
/*!
	This is similar to std::pair but more explicitly meant for stereo items.
*/
template <typename Type>
struct Stereo
{
	Type l{}; //!< Left side
	Type r{}; //!< Right side
};

//! Struct Contains hardware information for the headset
/*!
	C++ version of the C API's Fove_HeadsetHardwareInfo
*/
struct HeadsetHardwareInfo
{
	std::string serialNumber; //!< Serial number in UTF8
	std::string manufacturer; //!< Manufacturer info in UTF8
	std::string modelName;    //!< Model name in UTF8
};

//! Exception type that is thrown when an error is ignored in the FOVE API
struct Exception : public std::exception
{
public:
	ErrorCode error = ErrorCode::None;

	Exception(const ErrorCode e) : error{e} {}

	const char* what() const noexcept override
	{
		return "Fove API Exception"; // Todo: base this on the error
	}
};

//! Class for return values from the C++ API
/*!
	This class represents either an error or a value, similar to std::variant<ErrorCode, Value> but without including C++17
	The there is an error and the value is requested, an exception will be thrown.

	nullptr_t is a special type that indicates "no value", just an error field.
*/
template <typename Value = std::nullptr_t>
class Result
{
	ErrorCode m_err = ErrorCode::None;
	Value m_value{};

public:
	Result() = default;                                //!< Constructs a result with a default-initialized value
	Result(const ErrorCode err) : m_err{err} {}        //!< Constructs a result with an error
	Result(const Value& data) : m_value{data} {}       //!< Constructs a result with a value
	Result(Value&& data) : m_value{std::move(data)} {} //!< Move constructor

	//! Returns the error code
	ErrorCode getError() const { return m_err; }

	//! Returns true if there is no error
	bool isValid() const { return m_err == ErrorCode::None; }

	//! Returns the value if isValid() is true, otherwise throws
	Value& getValue() &
	{
		throwIfInvalid();
		return m_value;
	}

	//! Returns the value if isValid() is true, otherwise throws
	Value&& getValue() &&
	{
		throwIfInvalid();
		return std::move(m_value);
	}

	//! Returns the value if isValid() is true, otherwise throws
	const Value& getValue() const &
	{
		throwIfInvalid();
		return m_value;
	}

	//! Returns the value without checking the error field
	/*!
		It is expected that the error was checked manually before calling this field, for example via throwIfInvalid().
		The return value is undefined if isValid() is false.
	*/
	const Value& getValueUnchecked() const { return m_value; }

	//! Returns the value if available, otherwise returns the provided default
	/*!
		\param defValue A value to return if this object is not valid
	*/
	Value valueOr(Value defValue) const { return isValid() ? m_value : std::move(defValue); }

	//! Throws if there is an error, otherwise is a no-op
	/*!
		If exceptions are disabled, this will terminate the program instead of throwing.
	*/
	void throwIfInvalid() const
	{
		if (!isValid())
		{
#if FOVE_EXCEPTIONS
			throw Exception{m_err};
#else
			std::terminate();
#endif
		}
	}

	//! Explicit conversion to bool, for use in if statements
	explicit operator bool () const { return isValid(); }

	Value      & operator* ()       { return  getValue(); } //!< Pointer-like semantics to fetch value, throws if invalid
	Value const& operator* () const { return  getValue(); } //!< Pointer-like semantics to fetch value, throws if invalid
	Value      * operator->()       { return &getValue(); } //!< Pointer-like semantics to fetch value, throws if invalid
	Value const* operator->() const { return &getValue(); } //!< Pointer-like semantics to fetch value, throws if invalid

	//! Helper function to create an error by calling a C API function
	template <typename Call, typename... Args>
	static Result invoke(Call* call, const Args... args)
	{
		Value ret{};
		const ErrorCode err = (*call)(args..., &ret);
		if (err != ErrorCode::None)
			return err;
		return ret;
	}

	//! Variant of invoke for functions that have a left and a right output
	template <typename Call, typename... Args>
	static Result invokeStereo(Call* call, const Args... args)
	{
		Value ret{};
		const ErrorCode err = (*call)(args..., &ret.l, &ret.r);
		if (err != ErrorCode::None)
			return err;
		return ret;
	}
};

//! Base class for classes in the FOVE C++ API
template <typename CType>
class Object
{
public:
	//! Returns the underlying C type which the caller can use to invoke the C API directly, or null if not valid
	CType* getCObject() const { return m_object; }

	//! Returns true if this object is non-empty
	/*! An object may be empty if it's contained data was moved to another variable. */
	bool isValid() const { return m_object != nullptr; }

protected:
	CType* m_object = nullptr;

	Object() = default;
	Object(CType& obj) : m_object{&obj} {}
	Object(Object&& o) : m_object{o.m_object} { o.m_object = nullptr; }

	//! Destructor is protected to avoid needing to be virtual
	~Object() = default;
};

//! Compositor API
/*!
	This class is a wrapper around the C API's Fove_Compositor.

	It is the main way to draw content to a headset.
*/
class Compositor : public Object<Fove_Compositor>
{
public:
	//! Creates an empty compositor
	/*!
		Please use Headset::createCompositor() to get a valid compositor
		\see Headset::createCompositor()
	*/
	Compositor() = default;

	//! Creates a compositor from an existing C API object
	/*!
		This is not normally invoked directly, rather Headset::createCompositor(), which wraps this, is typically used.
		\see Headset::createCompositor()
	*/
	Compositor(Fove_Compositor& compositor) : Object{compositor} {}

	//! Move constructs a compositor
	/*!
		\param other May be empty or non-empty. By return, it will be empty.
	*/
	Compositor(Compositor&& other) : Object{std::move(other)} {}

	//! Destroys the existing compositor if any, then moves the one referenced by \p other, if any, into this object
	/*!
		\param other May be empty or non-empty. By return, it will be empty.
	*/
	Compositor& operator=(Compositor&& other)
	{
		const Result<> err = destroy();
		if (!err)
			fove_logText(LogLevel::Error, "fove_Compositor_destroy failed");

		m_object = other.m_object;
		other.m_object = nullptr;
		return *this;
	}

	//! Destroys the compositor, freeing any resources used (including all layers)
	/*! Since an error cannot be returned, any error from fove_Compositor_destroy will be logged. */
	~Compositor()
	{
		if (!destroy())
			fove_logText(LogLevel::Error, "fove_Compositor_destroy failed");
	}

	//! Destroys the compositor object, releasing resources
	/*!
		Afer this call, this object will be in an empty state and future calls will fail.
		This is handled by the destructor, usually the user doesn't need to call this.
	*/
	Result<> destroy()
	{
		Fove_Compositor* const object = m_object;
		m_object = nullptr;
		return object ? fove_Compositor_destroy(object) : ErrorCode::None;
	}

	//! Wraps fove_Compositor_createLayer()
	Result<CompositorLayer> createLayer(const CompositorLayerCreateInfo& layerInfo)
	{
		return Result<CompositorLayer>::invoke(&fove_Compositor_createLayer, m_object, &layerInfo);
	}

	//! Wraps fove_Compositor_submit()
	Result<> submit(const CompositorLayerSubmitInfo* submitInfo, const size_t layerCount)
	{
		return fove_Compositor_submit(m_object, submitInfo, layerCount);
	}

	//! Alternate version of submit() that simply takes one layer
	Result<> submit(const CompositorLayerSubmitInfo& submitInfo)
	{
		return submit(&submitInfo, 1);
	}

	//! Wraps fove_Compositor_waitForRenderPose()
	Result<Pose> waitForRenderPose()
	{
		return Result<Pose>::invoke(&fove_Compositor_waitForRenderPose, m_object);
	}

	//! Wraps fove_Compositor_getLastRenderPose()
	Result<Pose> getLastRenderPose()
	{
		return Result<Pose>::invoke(&fove_Compositor_getLastRenderPose, m_object);
	}

	//! Wraps fove_Compositor_isReady()
	Result<bool> isReady()
	{
		return Result<bool>::invoke(&fove_Compositor_isReady, m_object);
	}

	//! Wraps fove_Compositor_getAdapterId()
	/*!
		\param ignore This is not used and will be removed in a future version
	*/
	Result<AdapterId> getAdapterId(AdapterId* ignore = nullptr)
	{
		return Result<AdapterId>::invoke(&fove_Compositor_getAdapterId, m_object);
	}
};

//! Research API
/*!
	This class is a wrapper around the C API's Fove_ResearchHeadset.

	It is not intended for use in general-purpose software, eg. games, but rather for a labratory environment.

	Using this class will limit the backwards compatibility of your program.
*/
class ResearchHeadset : public Object<Fove_ResearchHeadset>
{
public:
	//! Creates an empty research headset
	/*!
		Please use Headset::getResearchHeadset() to get a valid research headset.
		\see Headset::getResearchHeadset()
	*/
	ResearchHeadset() = default;

	//! Creates a headset from an existing C API object
	/*!
		This is not normally invoked directly, rather Headset::getResearchHeadset(), which wraps this, is typically used.
		\see Headset::getResearchHeadset()
	*/
	ResearchHeadset(Fove_ResearchHeadset& headset) : Object{headset} {}

	//! Move constructs a research headset
	/*!
		\param other May be empty or non-empty. By return, it will be empty.
	*/
	ResearchHeadset(ResearchHeadset&& other) : Object{std::move(other)} {}

	//! Destroys the existing research headset if any, then moves the one referenced by \p other, if any, into this object
	/*!
		\param other May be empty or non-empty. By return, it will be empty.
	*/
	ResearchHeadset& operator=(ResearchHeadset&& other)
	{
		m_object = other.m_object;
		other.m_object = nullptr;
		return *this;
	}

	//! Does nothing, the underlying C API object's lifecycle is tied to the headset is was created from
	~ResearchHeadset() = default;

	//! Wraps fove_ResearchHeadset_registerCapabilities()
	Result<> registerCapabilities(const ResearchCapabilities caps)
	{
		return fove_ResearchHeadset_registerCapabilities(m_object, caps);
	}

	//! Wraps fove_ResearchHeadset_unregisterCapabilities()
	Result<> unregisterCapabilities(const ResearchCapabilities caps)
	{
		return fove_ResearchHeadset_unregisterCapabilities(m_object, caps);
	}

	//! Wraps fove_ResearchHeadset_getImage()
	Result<BitmapImage> getImage(const ImageType type)
	{
		return Result<BitmapImage>::invoke(&fove_ResearchHeadset_getImage, m_object, type);
	}

	//! Wraps fove_ResearchHeadset_getGaze()
	Result<ResearchGaze> getGaze()
	{
		return Result<ResearchGaze>::invoke(&fove_ResearchHeadset_getGaze, m_object);
	}
};

//! Main API for using headsets
/*!
	This class is a wrapper around the C API's Fove_Headset, and is the main class of the FOVE API.
*/
class Headset : public Object<Fove_Headset>
{
public:
	//! Creates an empty headset
	/*!
		Please use Headset::create() to create a valid headset.
		\see Headset::create
	*/
	Headset() = default;

	//! Creates a headset from an existing C API object
	/*!
		This is not normally invoked directly, rather Headset::create(), which wraps this, is typically used.
		\see Headset::create
	*/
	Headset(Fove_Headset& headset) : Object{headset} {}

	//! Move constructs a headset
	/*!
		\param other May be empty or non-empty. By return, it will be empty.
	*/
	Headset(Headset&& other) : Object{std::move(other)} {}

	//! Destroys the existing headset if any, then moves the one referenced by \p other, if any, into this object
	/*!
		\param other May be empty or non-empty. By return, it will be empty.
	*/
	Headset& operator=(Headset&& other)
	{
		const Result<> err = destroy();
		if (!err)
			fove_logText(LogLevel::Error, "fove_Headset_destroy failed");

		m_object = other.m_object;
		other.m_object = nullptr;
		return *this;
	}

	//! Creates a new headset object with the given capabitilies
	static Result<Headset> create(const ClientCapabilities capabilities)
	{
		const Result<Fove_Headset*> ret = Result<Fove_Headset*>::invoke(fove_createHeadset, capabilities);
		if (!ret)
			return {ret.getError()};

		return {Headset{*ret.getValueUnchecked()}};
	}

	//! Destroys the headset, releasing any resources
	/*! Since an error cannot be returned, any error from fove_Headset_destroy will be logged. */
	~Headset()
	{
		if (!destroy())
			fove_logText(LogLevel::Error, "fove_Headset_destroy failed");
	}

	//! Destroys the headset, releasing resources
	/*!
		Afer this call, this object will be in an empty state and future calls will fail.
		This is handled by the destructor, usually the user doesn't need to call this.
	*/
	Result<> destroy()
	{
		Fove_Headset* const object = m_object;
		m_object = nullptr;
		return object ? fove_Headset_destroy(object) : ErrorCode::None;
	}

	//! Creates a new compositor object
	Result<Compositor> createCompositor()
	{
		const Result<Fove_Compositor*> ret = Result<Fove_Compositor*>::invoke(fove_Headset_createCompositor, m_object);
		if (!ret)
			return {ret.getError()};

		return {Compositor{*ret.getValueUnchecked()}};
	}

	//! Creates a new research headet
	/*!
		Keep in mind the research API is meant for researcher use and not for general purpose software.
		Using this function will limit backwards compatibility.
	*/
	Result<ResearchHeadset> getResearchHeadset(const ResearchCapabilities caps)
	{
		const Result<Fove_ResearchHeadset*> ret = Result<Fove_ResearchHeadset*>::invoke(fove_Headset_getResearchHeadset, m_object, caps);
		if (!ret)
			return {ret.getError()};

		return {ResearchHeadset{*ret.getValueUnchecked()}};
	}

	//! Wraps fove_Headset_isHardwareConnected()
	Result<bool> isHardwareConnected()
	{
		return Result<bool>::invoke(fove_Headset_isHardwareConnected, m_object);
	}

	//! Wraps fove_Headset_isHardwareReady()
	Result<bool> isHardwareReady()
	{
		return Result<bool>::invoke(fove_Headset_isHardwareReady, m_object);
	}

	//! Wraps fove_Headset_getHardwareInfo()
	Result<HeadsetHardwareInfo> getHeadsetHardwareInfo()
	{
		const Result<Fove_HeadsetHardwareInfo> cRet = Result<Fove_HeadsetHardwareInfo>::invoke(fove_Headset_getHardwareInfo, m_object);
		if (!cRet.isValid())
			return cRet.getError();

		// Convert to CXX version of struct (std::string instead of C string)
		HeadsetHardwareInfo cxxRet;
		cxxRet.manufacturer = cRet.getValueUnchecked().manufacturer;
		cxxRet.modelName = cRet.getValueUnchecked().modelName;
		cxxRet.serialNumber = cRet.getValueUnchecked().serialNumber;
		return cxxRet;
	}

	//! Wraps fove_Headset_checkSoftwareVersions()
	Result<> checkSoftwareVersions()
	{
		return fove_Headset_checkSoftwareVersions(m_object);
	}

	//! Wraps fove_Headset_getSoftwareVersions()
	Result<Versions> getSoftwareVersions()
	{
		return Result<Versions>::invoke(fove_Headset_getSoftwareVersions, m_object);
	}

	//! Wraps fove_Headset_waitForNextEyeFrame()
	Result<> waitForNextEyeFrame()
	{
		return fove_Headset_waitForNextEyeFrame(m_object);
	}

	//! Wraps fove_Headset_getGazeVectors()
	Result<Stereo<GazeVector>> getGazeVectors()
	{
		return Result<Stereo<GazeVector>>::invokeStereo(fove_Headset_getGazeVectors, m_object);
	}

	//! Wraps fove_Headset_getGazeVectors2D()
	Result<Stereo<Vec2>> getGazeVectors2D()
	{
		return Result<Stereo<Vec2>>::invokeStereo(fove_Headset_getGazeVectors2D, m_object);
	}

	//! Wraps fove_Headset_getGazeConvergence()
	Result<GazeConvergenceData> getGazeConvergence()
	{
		return Result<GazeConvergenceData>::invoke(fove_Headset_getGazeConvergence, m_object);
	}

	//! Wraps fove_Headset_checkEyesClosed()
	Result<Eye> checkEyesClosed()
	{
		return Result<Eye>::invoke(fove_Headset_checkEyesClosed, m_object);
	}

	//! Wraps fove_Headset_checkEyesTracked()
	Result<Eye> checkEyesTracked()
	{
		return Result<Eye>::invoke(fove_Headset_checkEyesTracked, m_object);
	}

	//! Wraps fove_Headset_isEyeTrackingEnabled()
	Result<bool> isEyeTrackingEnabled()
	{
		return Result<bool>::invoke(fove_Headset_isEyeTrackingEnabled, m_object);
	}

	//! Wraps fove_Headset_isEyeTrackingCalibrated()
	Result<bool> isEyeTrackingCalibrated()
	{
		return Result<bool>::invoke(fove_Headset_isEyeTrackingCalibrated, m_object);
	}

	//! Wraps fove_Headset_isEyeTrackingCalibrating()
	Result<bool> isEyeTrackingCalibrating()
	{
		return Result<bool>::invoke(fove_Headset_isEyeTrackingCalibrating, m_object);
	}

	//! Wraps fove_Headset_isEyeTrackingReady()
	Result<bool> isEyeTrackingReady()
	{
		return Result<bool>::invoke(fove_Headset_isEyeTrackingReady, m_object);
	}

	//! Wraps fove_Headset_isMotionReady()
	Result<bool> isMotionReady()
	{
		return Result<bool>::invoke(fove_Headset_isMotionReady, m_object);
	}

	//! Wraps fove_Headset_tareOrientationSensor()
	Result<> tareOrientationSensor()
	{
		return fove_Headset_tareOrientationSensor(m_object);
	}

	//! Wraps fove_Headset_isPositionReady()
	Result<bool> isPositionReady()
	{
		return Result<bool>::invoke(fove_Headset_isPositionReady, m_object);
	}

	//! Wraps fove_Headset_tarePositionSensors()
	Result<> tarePositionSensors()
	{
		return fove_Headset_tarePositionSensors(m_object);
	}

	//! Wraps fove_Headset_getLatestPose()
	Result<Pose> getLatestPose()
	{
		return Result<Pose>::invoke(fove_Headset_getLatestPose, m_object);
	}

	//! Wraps fove_Headset_getProjectionMatricesLH()
	Result<Stereo<Matrix44>> getProjectionMatricesLH(const float zNear, const float zFar)
	{
		return Result<Stereo<Matrix44>>::invokeStereo(fove_Headset_getProjectionMatricesLH, m_object, zNear, zFar);
	}

	//! Wraps fove_Headset_getProjectionMatricesRH()
	Result<Stereo<Matrix44>> getProjectionMatricesRH(const float zNear, const float zFar)
	{
		return Result<Stereo<Matrix44>>::invokeStereo(fove_Headset_getProjectionMatricesRH, m_object, zNear, zFar);
	}

	//! Wraps fove_Headset_getRawProjectionValues()
	Result<Stereo<ProjectionParams>> getRawProjectionValues()
	{
		return Result<Stereo<ProjectionParams>>::invokeStereo(fove_Headset_getRawProjectionValues, m_object);
	}

	//! Wraps fove_Headset_getEyeToHeadMatrices()
	Result<Stereo<Matrix44>> getEyeToHeadMatrices()
	{
		return Result<Stereo<Matrix44>>::invokeStereo(&fove_Headset_getEyeToHeadMatrices, m_object);
	}

	//! Wraps fove_Headset_getIOD()
	Result<float> getIOD()
	{
		return Result<float>::invoke(&fove_Headset_getIOD, m_object);
	}

	//! Wraps fove_Headset_ensureEyeTrackingCalibration()
	Result<> ensureEyeTrackingCalibration()
	{
		return fove_Headset_ensureEyeTrackingCalibration(m_object);
	}

	//! Wraps fove_Headset_startEyeTrackingCalibration()
	Result<> startEyeTrackingCalibration(const bool restartIfRunning)
	{
		return fove_Headset_startEyeTrackingCalibration(m_object, restartIfRunning);
	}

	//! Wraps fove_Headset_stopEyeTrackingCalibration()
	Result<> stopEyeTrackingCalibration()
	{
		return fove_Headset_stopEyeTrackingCalibration(m_object);
	}
};

//! Wraps fove_logText()
inline Result<> logText(const LogLevel level, const std::string& utf8Text)
{
	return fove_logText(level, utf8Text.c_str());
}

} // namespace FOVE_CXX_NAMESPACE

#endif // FOVE_DEFINE_CXX_API

// clang-format on

#endif // FOVE_API_H
