#pragma once

#ifdef __GNUC__
#define FVR_DEPRECATED(func, rem) __attribute__ ((deprecated(rem))) func
#define FVR_EXPORT __attribute__((visibility("default")))
#elif defined(_MSC_VER)
#define FVR_DEPRECATED(func, rem) __declspec(deprecated(rem)) func
#define FVR_EXPORT __declspec(dllexport)
#else
#pragma message("WARNING: You need to implement DEPRECATED for this compiler")
#define FVR_DEPRECATED(func, rem) func
#define FVR_EXPORT
#endif

#include <cmath>
#include <cstdint>
#include <cstring>

namespace Fove
{
    //! Client capabilities to be requested
    /*! To be passed to the initialisation function of the client library to  */
    enum class EFVR_ClientCapabilities
    {
        None = 0x00,
        Gaze = 0x01,
        Orientation = 0x02,
        Position = 0x04
    };

    // hide functions for documentation
    /// @cond EFVR_ClientCapabilities_Functions
    inline EFVR_ClientCapabilities operator|(EFVR_ClientCapabilities a, EFVR_ClientCapabilities b)
    {
        return static_cast<EFVR_ClientCapabilities>(static_cast<int>(a) | static_cast<int>(b));
    }
    inline EFVR_ClientCapabilities operator&(EFVR_ClientCapabilities a, EFVR_ClientCapabilities b)
    {
        return static_cast<EFVR_ClientCapabilities>(static_cast<int>(a) & static_cast<int>(b));
    }
    inline EFVR_ClientCapabilities operator~(EFVR_ClientCapabilities a)
    {
        // bitwise negation
        return static_cast<EFVR_ClientCapabilities>(~static_cast<int>(a));
    }
    /// @endcond

    //! An enum of error codes that the system may return
	enum class EFVR_ErrorCode
	{
		None = 0,

		//! Connection Errors
		Connection_General = 1,
		Connect_NotConnected = 7,
		Connect_ServerUnreachable = 2,
		Connect_RegisterFailed = 3,
		Connect_DeregisterFailed = 8,
		Connect_RuntimeVersionTooOld = 4,
		Connect_HeartbeatNoReply = 5,
		Connect_ClientVersionTooOld = 6,

		//! API usage errors
		API_General = 100,                 //!< There was an error in the usage of the API other than one of the others in this section
		API_InitNotCalled = 101,           //!< A function that should only be called after Initialise() was invoked before/without Initialise()
		API_InitAlreadyCalled = 102,       //!< A function that should only be called before Initialise() was invoked, or Initialise() was invoked multiple times
		API_InvalidArgument = 103,         //!< An argument passed to an API function was invalid for a reason other than one of the below reasons
		API_NotRegistered = 104,           //!< Data was queried without first registering for that data
		API_NullInPointer = 110,           //!< An input argument passed to an API function was invalid for a reason other than the below reasons
		API_InvalidEnumValue = 111,        //!< An enum argument passed to an API function was invalid
		API_NullOutPointersOnly = 120,     //!< All output arguments were null on a function that requires at least one output (all getters that have no side effects)
		API_OverlappingOutPointers = 121,  //!< Two (or more) output parameters passed to an API function overlap in memory. Each output parameter should be a unique, separate object.
		API_CompositorNotSwapped = 122,    //!< This comes from submitting without calling WaitForRenderPose after a complete submit.

        //! Data Errors
        Data_General = 1000,
        Data_RegisteredWrongVersion = 1001,
        Data_UnreadableNotFound = 1002,
        Data_NoUpdate = 1003,
        Data_Uncalibrated = 1004,
        Data_MissingIPCData = 1005,

        //! Hardware Errors
        Hardware_General = 2000,
        Hardware_CoreFault = 2001,
        Hardware_CameraFault = 2002,
        Hardware_IMUFault = 2003,
        Hardware_ScreenFault = 2004,
        Hardware_SecurityFault = 2005,
        Hardware_Disconnected = 2006,
        Hardware_WrongFirmwareVersion = 2007,

        //! Server Response Errors
        Server_General = 3000,
        Server_HardwareInterfaceInvalid = 3001,
        Server_HeartbeatNotRegistered = 3002,
        Server_DataCreationError = 3003,
        Server_ModuleError_ET = 3004,

        //! Code and placeholders
        Code_NotImplementedYet = 4000,
        Code_FunctionDeprecated = 4001,

        //! Position Tracking
        Position_NoObjectsInView = 5000,
        Position_NoDlibRegressor = 5001,
        Position_NoCascadeClassifier = 5002,
        Position_NoModel = 5003,
        Position_NoImages = 5004,
        Position_InvalidFile = 5005,
        Position_NoCamParaSet = 5006,
        Position_CantUpdateOptical = 5007,
        Position_ObjectNotTracked = 5008,
        Position_NoCameraFound = 5009,

        //! Eye Tracking
        Eye_Left_NoDlibRegressor = 6000,
        Eye_Right_NoDlibRegressor = 6001,
        Eye_CalibrationFailed = 6002,
        Eye_LoadCalibrationFailed = 6003,

        //! User
        User_General = 7000,
        User_ErrorLoadingProfile = 7001,

        //! Compositor
        Compositor_UnableToCreateDeviceAndContext = 8000, //!< Compositor was unable to initialize its backend component.
        Compositor_UnableToUseTexture = 8001,             //!< Compositor was unable to use the given texture (likely due to mismatched client and data types or an incompatible format).
        Compositor_DeviceMismatch = 8002,                 //!< Compositor was unable to match its device to the texture's, either because of multiple GPUs or a failure to get the device from the texture.
        Compositor_IncompatibleCompositorVersion = 8003,  //!< Compositor client is not compatible with the currently running compositor.
        Compositor_UnableToFindRuntime = 8004,            //!< Compositor isn't running or isn't responding.
        Compositor_DisconnectedFromRuntime = 8006,        //!< Compositor was running and is no longer responding.
        Compositor_ErrorCreatingTexturesOnDevice = 8008,  //!< Failed to create shared textures for compositor.
        Compositor_NoEyeSpecifiedForSubmit = 8009,        //!< The supplied EFVR_Eye for submit is invalid (i.e. is Both or Neither).

        //! Generic
        UnknownError = 9000,  //!< Errors that are unknown or couldn't be classified. If possible, info will be logged about the nature of the issue.
    };

    //! Enum Corresponds to the order in which clients are composited
    /*! Corresponds to the order in which clients are composited (Base, then Overlay, then Diagnostic) */
    enum class EFVR_ClientType
    {
        Base = 0,             /*!< The first layer all the way in the background */
        Overlay = 0x10000,    /*!< Layer over the Base */
        Diagnostic = 0x20000  /*!< Layer over Overlay */
    };

    //! EFVR_Status
    /*! An enum used for the system status health check that tells you which parts of the hardware and software are functioning */
    enum class EFVR_HealthStatus
    {
        Unknown,
        Healthy,
        Uncalibrated,
        Sleeping,
        Disconnected,
        Error,
    };

    //! SFVR_SystemHealth
    /*! Contains the health status and error codes for the HMD, position camera, position LEDs, eye camera and eye LEDs */
    struct SFVR_SystemHealth
    {
        /*! The health status of the HMD */
        EFVR_HealthStatus HMD = EFVR_HealthStatus::Unknown;
        /*! Any error message from the HMD */
        EFVR_ErrorCode HMDError = EFVR_ErrorCode::None;
        /*! The health status of the position camera */
        EFVR_HealthStatus PositionCamera = EFVR_HealthStatus::Unknown;
        /*! Any error message from the position camera */
        EFVR_ErrorCode PositionCameraError = EFVR_ErrorCode::None;
        /*! The health status of the eye cameras */
        EFVR_HealthStatus EyeCamera = EFVR_HealthStatus::Unknown;
        /*! Any error message from the eye cameras */
        EFVR_ErrorCode EyeCameraError = EFVR_ErrorCode::None;
        /*! The health status of the position LEDs */
        EFVR_HealthStatus PositionLEDs = EFVR_HealthStatus::Unknown;
        /*! The health status of the eye LEDs */
        EFVR_HealthStatus EyeLEDs = EFVR_HealthStatus::Unknown;
    };

    //! Struct Contains the version for the software
    /*! Contains the version for the software (both runtime and client versions).
        A negative value in any int field represents unknown.
    */
    struct SFVR_Versions
    {
        int clientMajor = -1;
        int clientMinor = -1;
        int clientBuild = -1;
        int clientProtocol = -1;
        int runtimeMajor = -1;
        int runtimeMinor = -1;
        int runtimeBuild = -1;
        int firmware = -1;
        int maxFirmware = -1;
        int minFirmware = -1;
        bool tooOldHeadsetConnected = false;
    };

    //! Struct representation on a quaternion
    /*! A quaternion represents an orientation in 3d space.*/
    struct SFVR_Quaternion
    {
        float x = 0;
        float y = 0;
        float z = 0;
        float w = 1;

        //! default quaternion constructor by c++
        SFVR_Quaternion() = default;

        /*! Initialize the Quaternion
            \param ix x-component of Quaternion
            \param iy y-component of Quaternion
            \param iz z-component of Quaternion
            \param iw w-component of Quaternion
        */
        SFVR_Quaternion(float ix, float iy, float iz, float iw) : x(ix), y(iy), z(iz), w(iw) {}

        //! Generate and return a conjugate of this quaternion
        SFVR_Quaternion Conjugate() const
        {
            return SFVR_Quaternion(-x, -y, -z, w);
        }

        //! Normalize the Quaternion
        SFVR_Quaternion Normalize() const
        {
            float d = std::sqrt(w*w + x*x + y*y + z*z);
            SFVR_Quaternion result(x / d, y / d, z / d, w / d);
            return result;
        }

        //! Return the result of multiplying this quaternion Q1 by another Q2 such that OUT = Q1 * Q2
        SFVR_Quaternion MultiplyBefore(const SFVR_Quaternion &second) const
        {
            auto nx =  x * second.w + y * second.z - z * second.y + w * second.x;
            auto ny = -x * second.z + y * second.w + z * second.x + w * second.y;
            auto nz =  x * second.y - y * second.x + z * second.w + w * second.z;
            auto nw = -x * second.x - y * second.y - z * second.z + w * second.w;
            return SFVR_Quaternion(nx, ny, nz, nw);
        }

        //! Return the result of multiplying this quaternion Q2 by another Q1 such that OUT = Q1 * Q2
        SFVR_Quaternion MultiplyAfter(const SFVR_Quaternion &first) const
        {
            auto nx =  first.x * w + first.y * z - first.z * y + first.w * x;
            auto ny = -first.x * z + first.y * w + first.z * x + first.w * y;
            auto nz =  first.x * y - first.y * x + first.z * w + first.w * z;
            auto nw = -first.x * x - first.y * y - first.z * z + first.w * w;
            return SFVR_Quaternion(nx, ny, nz, nw);
        }
    };

    //! Struct to represent a 3D-vector
    /*! A vector that represents an position in 3d space. */
    struct SFVR_Vec3
    {
        float x = 0;
        float y = 0;
        float z = 0;

        //! default vector constructor by c++
        SFVR_Vec3() = default;

        /*! Initialize the Vector
            \param ix x-component of Vector
            \param iy y-component of Vector
            \param iz z-component of Vector
        */
        SFVR_Vec3(float ix, float iy, float iz) : x(ix), y(iy), z(iz) {}
    };

    //! Struct to represent a 2D-vector
    /*! A vector that represents an position in 2d space. Usually used when refering to screen or image coordinates. */
    struct SFVR_Vec2
    {
        float x = 0;
        float y = 0;

        //! default vector constructor by c++
        SFVR_Vec2() = default;

        /*! Initialize the 2-component float vector
            \param ix The x component of the vector
            \param iy The y component of the vector
        */
        SFVR_Vec2(float ix, float iy) : x(ix), y(iy) {}
    };

    //! Struct to represent a 2D-vector
    /*! A 2-component integer vector. */
    struct SFVR_Vec2i
    {
        int x = 0;
        int y = 0;

        //! default vector constructor by c++
        SFVR_Vec2i() = default;

        /*! Initialize the 2-component integer vector
            \param ix The x component of the vector
            \param iy The y component of the vector
        */
        SFVR_Vec2i(int ix, int iy) : x(ix), y(iy) {}
    };

    //! Struct to represent a Ray
    /*! Stores the start point and direction of a Ray */
    struct SFVR_Ray
    {
        //! The start point of the Ray
        SFVR_Vec3 origin;
        //! The direction of the Ray
        SFVR_Vec3 direction = { 0, 0, 1 };

        //! default Ray constructor by c++
        SFVR_Ray() = default;

        /*! Initialize the Ray
            \param _origin    The start point of the ray
            \param _direction The direction of the ray
        */
        SFVR_Ray(const SFVR_Vec3& _origin, const SFVR_Vec3& _direction) : origin(_origin), direction(_direction) {}
    };

    //! Struct to represent a combination of position and orientation of Fove Headset
    /*! This structure is a combination of the Fove headset position and orientation in 3d space, collectively known as the "pose".
        In the future this may also contain accelleration information for the headset, and may also be used for controllers.
    */
    struct SFVR_Pose
    {
        /*! Incremental counter which tells if the coord captured is a fresh value at a given frame */
        std::uint64_t id = 0;
        /*! The time at which the pose was captured, in milliseconds since an unspecified epoch */
        std::uint64_t timestamp = 0;
        /*! The Quaternion which represents the orientation of the head. */
        SFVR_Quaternion orientation;
        /*! The angular velocity of the head. */
        SFVR_Vec3 angularVelocity;
        /*! The angular acceleration of the head. */
        SFVR_Vec3 angularAcceleration;
        /*! The position of headset in 3D space. Tares to (0, 0, 0). Use for sitting applications. */
        SFVR_Vec3 position;
        /*! The position of headset including offset for camera location. Will not tare to zero. Use for standing applications. */
        SFVR_Vec3 standingPosition;
        /*! The velocity of headset in 3D space */
        SFVR_Vec3 velocity;
        /*! The acceleration of headset in 3D space */
        SFVR_Vec3 acceleration;
    };

    //! Struct to represent a unit vector out from the eye center along which that eye is looking
    /*! The vector value is in eye-relative coordinates, meaning that it is not affected by the position
     * or orientation of the HMD, but rather represents the absolute orientation of the eye's gaze.
    */
    struct SFVR_GazeVector
    {
        /*! Incremental counter which tells if the convergence data is a fresh value at a given frame */
        std::uint64_t id = 0;
        /*! The time at which the gaze data was captured, in milliseconds since an unspecified epoch */
        std::uint64_t timestamp = 0;
        SFVR_Vec3 vector = { 0, 0, 1 };
    };

    //! Struct to represent the vector pointing where the user is looking at.
    /*! The vector (from the center of the player's head in world space) that can be used to approximate the point that the user is looking at. */
    struct SFVR_GazeConvergenceData
    {
        /*! Incremental counter which tells if the convergence data is a fresh value at a given frame */
        std::uint64_t id = 0;
        /*! The time at which the convergence data was captured, in milliseconds since an unspecified epoch */
        std::uint64_t timestamp = 0;
        /*! The ray pointing towards the expected convergence point */
        SFVR_Ray ray;
        /*! The expected distance to the convergence point, Range: 0 to Infinity*/
        float distance = 0.f;
        /*! Pupil dilation is given as a ratio relative to a baseline. 1 means average. Range: 0 to Infinity */
        float pupilDilation = 0.f;
        /*! True if the user is looking at something (fixation or pursuit), rather than saccading between objects. This could be used to suppress eye input during large eye motions. */
        bool attention = false;
    };

    //! Enum to identify which eye is being used.
    /*! This is usually returned with any eye tracking information and tells the client which eye(s) the information is based on. */
    enum class EFVR_Eye
    {
        Neither = 0, /*!< Neither eye */
        Left = 1,    /*!< Left eye only */
        Right = 2,   /*!< Right eye only */
        Both = 3     /*!< Both eyes */
    };

    //! Struct to hold a rectangular array
    /*! A rectangular array of numbers, symbols, or expressions, arranged in rows and columns.  */
    struct SFVR_Matrix44
    {
        float mat[4][4] = {};
    };

    //! Struct to hold a rectangular array
    /*! A rectangular array of numbers, symbols, or expressions, arranged in rows and columns.  */
    struct SFVR_Matrix34
    {
        float mat[3][4] = {};
    };

    //! Structure holding information about projection fustum planes. Values are given for a depth of 1 so that it's
    //! easy to multiply them by your near clipping plan, for example, to get the correct values for your use.
    struct SFVR_ProjectionParams
    {
        float left   = -1;
        float right  =  1;
        float top    =  1;
        float bottom = -1;
    };

    //! enum for type of Graphics API
    /*! Type of Graphics API
        Note: We currently only support DirectX
    */
    enum class EFVR_GraphicsAPI
    {
        DirectX = 0, /*!< DirectX (Windows only)   */
        OpenGL = 1,  /*!< OpenGL (All platforms, currently in BETA) */
	};

    //! Enum to help interpret the alpha of texture
    /*! Determines how to interpret the alpha of a compositor client texture */
    enum class EFVR_AlphaMode
    {
        Auto = 0,   /*!< Base layers will use One, overlay layers will use Sample */
        One = 1,    /*!< Alpha will always be one (fully opaque) */
        Sample = 2, /*!< Alpha fill be sampled from the alpha channel of the buffer */
    };

    //! Struct used to define the settings for a compositor client.
    /*! Structure used to define the settings for a compositor client.*/
    struct SFVR_CompositorLayerCreateInfo
    {
        //! The type (layer) upon which the client will draw.
        EFVR_ClientType type = EFVR_ClientType::Base;

        //! Setting to disable timewarp, e.g. if an overlay client is operating in screen space.
        bool disableTimeWarp = false;
        //! Setting about whether to use alpha sampling or not, e.g. for a base client.
        EFVR_AlphaMode alphaMode = EFVR_AlphaMode::Auto;
        //! Setting to disable fading when the base layer is misbehaving, e.g. for a diagnostic client.
        bool disableFading = false;
        //! Setting to disable a distortion pass, e.g. for a diagnostic client, or a client intending to do its own distortion.
        bool disableDistortion = false;
    };

    //! Struct used to store information about an existing compositor layer (after it is created)
    /*! This exists primarily for future expandability. */
    struct SFVR_CompositorLayer
    {
        //! Uniquely identifies a layer created within an IFVRCompositor object.
        int layerId = 0;

        /*! The optimal resolution for a submitted buffer on this layer (for a single eye).
            Clients are allowed to submit buffers of other resolutions.
            In particular, clients can use a lower resolution buffer to reduce their rendering overhead.
        */
        SFVR_Vec2i idealResolutionPerEye;
    };

	//! Base class of API-specific texture classes.
	struct SFVR_CompositorTexture
	{
		//! Rendering API of this texture
		//! If this is DirectX, this object must be a SFVR_DX11Texture
		//! If this is OpenGL, this object must be a SFVR_GLTexture
		const EFVR_GraphicsAPI graphicsAPI;

	protected:
		// Create and destroy objects via one of the derived classes, based on which graphics API you are submitting with.
		SFVR_CompositorTexture(EFVR_GraphicsAPI api) : graphicsAPI{api} {}
		~SFVR_CompositorTexture() = default;
	};

	//! Struct used to submit a DirectX 11 texture.
	struct SFVR_DX11Texture : public SFVR_CompositorTexture
	{
		//! This must point to a ID3D11Texture2D
		void* texture = nullptr;

		SFVR_DX11Texture(void* t = nullptr) : SFVR_CompositorTexture{EFVR_GraphicsAPI::DirectX}, texture{t} {}
	};

    //! Struct used to submit an OpenGL texture.
    //! The GL context must be active on the thread that submits this.
    struct SFVR_GLTexture : public SFVR_CompositorTexture
    {
        //! The opengl id of the texture, as returned by glGenTextures
        std::uint32_t textureId = 0;
        //! On mac, this is a CGLContextObj, otherwise this field is reserved and you must pass null
        void* context = nullptr;

		SFVR_GLTexture(std::uint32_t tid = 0, void* c = nullptr) : SFVR_CompositorTexture{EFVR_GraphicsAPI::OpenGL}, textureId{tid}, context{c} {}
	};

    //! Struct to represent coordinates in normalized space
    /*! Coordinates in normalized space where 0 is left/top and 1 is bottom/right */
    struct SFVR_TextureBounds
    {
        ///@{
        float left = 0;
        float top = 0;
        float right = 0;
        float bottom = 0;
        ///@}
    };

    //! Struct used to conglomerate the texture settings for a single eye, when submitting a given layer.
    struct SFVR_CompositorLayerEyeSubmitInfo
    {
        //! Texture to submit for this eye
        //! This may be null as long as the other submitted eye's texture isn't (thus allowing each eye to be submitted separately)
        const SFVR_CompositorTexture* texInfo;

        //! The portion of the texture that is used to represent the eye (Eg. half of it if the texture contains both eyes)
        SFVR_TextureBounds bounds;
    };

    //! Struct used to conglomerate the texture settings when submitting a given layer.
    struct SFVR_CompositorLayerSubmitInfo
    {
        int layerId = 0;
        SFVR_Pose pose;
        SFVR_CompositorLayerEyeSubmitInfo left;
        SFVR_CompositorLayerEyeSubmitInfo right;
    };

    //! Struct used to identify a GPU adapter.
    struct SFVR_AdapterId
    {
#ifdef _WIN32
        // On windows, this forms a LUID structure
        uint32_t lowPart = 0;
        int32_t  highPart = 0;
#endif
    };
}
