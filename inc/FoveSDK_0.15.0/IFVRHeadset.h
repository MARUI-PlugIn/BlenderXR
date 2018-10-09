#pragma once

#include "FoveTypes.h"

// This is purely needed for doxygen documentation
/*! \mainpage Fove-SDK C++ Documentation
*
* \section intro_sec Introduction
*
* This is the Fove-SDK documentation (C++).
*
* \section install_sec Example
*
* We have a sample hosted on github: https://github.com/FoveHMD/FoveCppSample
*
*/

namespace Fove
{
    //! Class to handle all Fove headset related inquiries
    class IFVRHeadset
    {
    public:
        // members
        // General
        /*! Initialise the client with desired capabilities
            \param capabilities The desired capabilities (Gaze, Orientation, Position), for multiple capabilities, use piped input  e.g.: EFVR_ClientCapabilities::Gaze | EFVR_ClientCapabilities::Position
            \return Any error detected that might make the out data unreliable
        */
        virtual EFVR_ErrorCode Initialise(EFVR_ClientCapabilities capabilities) = 0;

        //! Writes out whether an HMD is know to be connected or not
        /*!
            \param outHardwareConnected A pointer to the value to be written
            \return Any error detected that might make the out data unreliable
         */
        virtual EFVR_ErrorCode IsHardwareConnected(bool* outHardwareConnected) = 0;

        //! Writes out whether the hardware for the requested capabilities has started
        /*!
            \return Any error detected that might make the out data unreliable
         */
        virtual EFVR_ErrorCode IsHardwareReady(bool* outIsReady) = 0;

        //! Checks whether the client can run against the installed version of the FOVE SDK
        /*!
            \return None if this client is compatible with the currently running service
                    Connect_RuntimeVersionTooOld if not compatible with the currently running service
                    Otherwise returns an error representing why this can't be determined
        */
        virtual EFVR_ErrorCode CheckSoftwareVersions() = 0;

        //! Writes out information about the current software versions
        /*!
            Allows you to get detailed information about the client and runtime versions.
            Instead of comparing software versions directly, you should simply call
            `CheckSoftwareVersions` to ensure that the client and runtime are compatible.
            \param outSoftwareVersions  A pointer to the left eye gaze vector which will be written to
            \return If this is None, then all fields of outSoftwareVersions are accurate.
                    If this is API_NullOutPointersOnly, nothing will be written
                    In the event of any other error, as many fields are written as possible, and the rest are set to default (negative) values.
         */
        virtual EFVR_ErrorCode GetSoftwareVersions(SFVR_Versions* outSoftwareVersions) = 0;

        //! Writes out the specified eye's current gaze vector.
        /*!
            \param eye              The eye you want data for, only `EFVR_Eye::Left` and `EFVR_Eye::Right` are valid
            \param outGazeVector    A pointer to the gaze vector struct that will contain the requested data
            \return                 Any error detected while fetching and writing the gaze vector
        */
        virtual EFVR_ErrorCode GetGazeVector(EFVR_Eye eye, SFVR_GazeVector* outGazeVector) = 0;

        //! Writes out each eye's current gaze vector
        /*!
            If either argument is `nullptr`, only the other value will be written. It is an error for both arguments to
            be `nullptr`.
            \param outLeft  A pointer to the left eye gaze vector which will be written to
            \param outRight A pointer to the right eye gaze vector which will be written to
            \return         Any error detected while fetching and writing the gaze vectors
        */
        virtual EFVR_ErrorCode GetGazeVectors(SFVR_GazeVector* outLeft, SFVR_GazeVector* outRight) = 0;

        //! Writes out eye convergence data
        /*!
            \param  outConvergenceData  A pointer to the convergence data struct to be written
            \return                     Any error detected while fetching and writing the data
        */
        virtual EFVR_ErrorCode GetGazeConvergence(SFVR_GazeConvergenceData* outConvergenceData) = 0;

		//! Writes out the user's 2D gaze position on the virtual screens seen through the HMD's lenses
		/*!
			The use of lenses and distortion correction creates a virtual screen in front of each eye.
			This function returns 2D vectors representing where on each eye's virtual screen the user
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
		virtual EFVR_ErrorCode GetGazeVectors2D(SFVR_Vec2* outLeft, SFVR_Vec2* outRight) = 0;

        //! Writes out which eyes are closed
        /*!
            \param outEye   A pointer to the variable to be written
            \return         Any error detected while fetching and writing the data
        */
        virtual EFVR_ErrorCode CheckEyesClosed(EFVR_Eye* outEye) = 0;

        //! Writes out which eyes are being tracked
        /*!
            \param outEye   A pointer to the variable to be written
            \return         Any error detected while fetching and writing the data
        */
        virtual EFVR_ErrorCode CheckEyesTracked(EFVR_Eye* outEye) = 0;

        //! Writes out whether the eye tracking hardware has started
        /*!
            \param outEyeTrackingEnabled    A pointer to the variable to be written
            \return                         Any error detected while fetching and writing the data
        */
        virtual EFVR_ErrorCode IsEyeTrackingEnabled(bool* outEyeTrackingEnabled) = 0;

        //! Writes out whether eye tracking has been calibrated
        /*!
            \param outEyeTrackingCalibrated A pointer to the variable to be written
            \return                         Any error detected while fetching and writing the data
        */
        virtual EFVR_ErrorCode IsEyeTrackingCalibrated(bool* outEyeTrackingCalibrated) = 0;

        //! Writes out whether eye tracking is in the process of performing a calibration
        /*!
            \param outEyeTrackingCalibrating    A pointer to the variable to be written
            \return                             Any error detected while fetching and writing the data
        */
        virtual EFVR_ErrorCode IsEyeTrackingCalibrating(bool* outEyeTrackingCalibrating) = 0;

        //! Writes out whether eye tracking is actively tracking an eye - or eyes
        /*!
            This means that hardware is enabled and eye tracking is calibrated when the variable is set to `true`.
            \param outEyeTrackingReady  A pointer to the variable to be written
            \return                     Any error detected while fetching and writing the data
        */
        virtual EFVR_ErrorCode IsEyeTrackingReady(bool* outEyeTrackingReady) = 0;

        //! Writes out whether motion tracking hardware has started
        /*!
            \param outMotionReady   A pointer to the variable to be written
            \return                 Any error detected while fetching and writing the data
        */
        virtual EFVR_ErrorCode IsMotionReady(bool* outMotionReady) = 0;

        //! Tares the orientation of the headset
        /*!
            \return Any error detected while fetching and writing the data
        */
        virtual EFVR_ErrorCode TareOrientationSensor() = 0;

        //! Writes out whether position tracking hardware has started and returns whether it was successful
        /*!
            \param outPositionReady A pointer to the variable to be written
            \return                 Any error detected while fetching and writing the data
        */
        virtual EFVR_ErrorCode IsPositionReady(bool* outPositionReady) = 0;

        //! Tares the position of the headset
        /*!
            \return Any error detected while fetching and writing the data
        */
        virtual EFVR_ErrorCode TarePositionSensors() = 0;

        //! Writes out the pose of the head-mounted display
        /*!
            \param outPose  A pointer to the variable to be written
            \return         Any error detected while fetching and writing the data
        */
        virtual EFVR_ErrorCode GetHMDPose(SFVR_Pose* outPose) = 0;

        //! Writes out the pose of the specified device
        /*!
            \param id       The identifier index of the desired device
            \param outPose  A pointer to the variable to be written
            \return         Any error detected while fetching and writing the data
        */
        virtual EFVR_ErrorCode GetPoseByIndex(int id, SFVR_Pose* outPose) = 0;

        //! Writes out the values of passed-in left-handed 4x4 projection matrices
        /*!
            Writes 4x4 projection matrices for both eyes using near and far planes in a left-handed coordinate
            system. Either outLeftMat or outRightMat may be `nullptr` to only write the other matrix, however setting
            both to `nullptr` is considered invalid and will return `EFVR_ErrorCode::API_NullOutPointersOnly`.
            \param zNear        The near plane in float, Range: from 0 to zFar
            \param zFar         The far plane in float, Range: from zNear to infinity
            \param outLeftMat   A pointer to the matrix you want written
            \param outRightMat  A pointer to the matrix you want written
            \return             Any error detected while fetching and writing the data
        */
        virtual EFVR_ErrorCode GetProjectionMatricesLH(float zNear, float zFar, SFVR_Matrix44* outLeftMat, SFVR_Matrix44* outRightMat) = 0;

        //! Writes out the values of passed-in right-handed 4x4 projection matrices
        /*!
            Writes 4x4 projection matrices for both eyes using near and far planes in a right-handed coordinate
            system. Either outLeftMat or outRightMat may be `nullptr` to only write the other matrix, however setting
            both to `nullptr` is considered invalid and will return `EFVR_ErrorCode::API_NullOutPointersOnly`.
            \param zNear        The near plane in float, Range: from 0 to zFar
            \param zFar         The far plane in float, Range: from zNear to infinity
            \param outLeftMat   A pointer to the matrix you want written
            \param outRightMat  A pointer to the matrix you want written
            \return             Any error detected while fetching and writing the data
        */
        virtual EFVR_ErrorCode GetProjectionMatricesRH(float zNear, float zFar, SFVR_Matrix44* outLeftMat, SFVR_Matrix44* outRightMat) = 0;

        //! Writes out values for the view frustum of the specified eye at 1 unit away.
        /*!
            Writes out values for the view frustum of the specified eye at 1 unit away. Please multiply them by zNear to
            convert to your correct frustum near-plane. Either outLeft or outRight may be `nullptr` to only write the
            other struct, however setting both to `nullptr` is considered and error and the function will return
            `EFVR_ErrorCode::API_NullOutPointersOnly`.
            \param outLeft  A pointer to the struct describing the left camera projection parameters
            \param outRight A pointer to the struct describing the right camera projection parameters
            \return         Any error detected while fetching and writing data
        */
        virtual EFVR_ErrorCode GetRawProjectionValues(SFVR_ProjectionParams* outLeft, SFVR_ProjectionParams* outRight) = 0;

        //! Writes out the matrices to convert from eye- to head-space coordinates
        /*!
            This is simply a translation matrix that returns +/- IOD/2
            \param outLeft   A pointer to the matrix where left-eye transform data will be written
            \param outRight  A pointer to the matrix where right-eye transform data will be written
            \return          Any error detected while fetching and writing data
         */
        virtual EFVR_ErrorCode GetEyeToHeadMatrices(SFVR_Matrix44 *outLeft, SFVR_Matrix44 *outRight) = 0;

        //! Closes any exists connections to the service, and cleans up all resources used by this class
        virtual ~IFVRHeadset() {}

        //! Override delete to ensure that deallocation happens within the same dll as GetFVRHeadset's allocation
        FVR_EXPORT void operator delete(void* ptr);

        //! Drift correction - Not implemented yet
        virtual EFVR_ErrorCode TriggerOnePointCalibration() = 0;

        //! Manual Drift correction - Not implemented yet
        virtual EFVR_ErrorCode ManualDriftCorrection3D(SFVR_Vec3 position) = 0;

        //! Interocular distance, returned in meters
        /*!
            This is an estimation of the distance between centers of the left and right eyeballs.
            Half of the IOD can be used to displace the left and right cameras for stereoscopic rendering.
            We recommend calling this each frame when doing stereoscoping rendering.
            Future versions of the FOVE service may update the IOD during runtime as needed.
            \param outIOD A floating point value where the IOD will be written upon exit if there is no error
        */
        virtual EFVR_ErrorCode GetIOD(float* outIOD) = 0;

        //! Writes out the health status of the FOVE hardware
        /*!
            \param outStatus    A pointer to a SFVR_SystemHealth struct that will be used to output all system components' health states
            \param runTest      If true, the eye tracking and position tracking systems are activated (if necessary) to make sure they work
            \return             Any error detected while fetching and writing data
        */
        virtual EFVR_ErrorCode GetSystemHealth(SFVR_SystemHealth* outStatus, bool runTest) = 0;

        /*! Starts calibration if needed
            All eye tracking content should call this before using the gaze to ensure that the calibration is good
            After calling this, content should periodically poll for IsEyeTrackingCalibration() to become false,
            so as to ensure that the content is not updating while obscured by the calibrator
        */
        virtual EFVR_ErrorCode EnsureEyeTrackingCalibration() = 0;
    };

    //! Creates and returns an IFVRHeadset object, which allows access to the full API
    /*! Returns null in the event of an error
        Upon calling this, typically the next step is to call Initialise() with the needed capabilities
        The caller is reponsible for deleting the returned pointer when finished, preferably via RAII, such as std::unique_ptr<IFVRHeadset>
    */
    FVR_EXPORT IFVRHeadset* GetFVRHeadset();
}
