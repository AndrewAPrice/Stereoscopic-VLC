/*****************************************************************************
 * nvapi.h: nVidia stereoscopic API
 *****************************************************************************
 * Copyright (C) 2011 the VideoLAN team
 * $Id$
 *
 * Authors: Andrew Price <andrewprice@andrewalexanderprice.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

typedef void* StereoHandle;

typedef enum _NvAPI_Status
{
    NVAPI_OK                                    =  0,      // Success
    NVAPI_ERROR                                 = -1,      // Generic error
    NVAPI_LIBRARY_NOT_FOUND                     = -2,      // nvapi.dll can not be loaded
    NVAPI_NO_IMPLEMENTATION                     = -3,      // not implemented in current driver installation
    NVAPI_API_NOT_INITIALIZED                   = -4,      // NvAPI_Initialize has not been called (successfully)
    NVAPI_INVALID_ARGUMENT                      = -5,      // invalid argument
    NVAPI_NVIDIA_DEVICE_NOT_FOUND               = -6,      // no NVIDIA display driver was found
    NVAPI_END_ENUMERATION                       = -7,      // no more to enum
    NVAPI_INVALID_HANDLE                        = -8,      // invalid handle
    NVAPI_INCOMPATIBLE_STRUCT_VERSION           = -9,      // an argument's structure version is not supported
    NVAPI_HANDLE_INVALIDATED                    = -10,     // handle is no longer valid (likely due to GPU or display re-configuration)
    NVAPI_OPENGL_CONTEXT_NOT_CURRENT            = -11,     // no NVIDIA OpenGL context is current (but needs to be)
    NVAPI_INVALID_POINTER                       = -14,     // An invalid pointer, usually NULL, was passed as a parameter
    NVAPI_NO_GL_EXPERT                          = -12,     // OpenGL Expert is not supported by the current drivers
    NVAPI_INSTRUMENTATION_DISABLED              = -13,     // OpenGL Expert is supported, but driver instrumentation is currently disabled
    NVAPI_NO_GL_NSIGHT                          = -15,     // OpenGL does not support Nsight
    NVAPI_EXPECTED_LOGICAL_GPU_HANDLE           = -100,    // expected a logical GPU handle for one or more parameters
    NVAPI_EXPECTED_PHYSICAL_GPU_HANDLE          = -101,    // expected a physical GPU handle for one or more parameters
    NVAPI_EXPECTED_DISPLAY_HANDLE               = -102,    // expected an NV display handle for one or more parameters
    NVAPI_INVALID_COMBINATION                   = -103,    // used in some commands to indicate that the combination of parameters is not valid
    NVAPI_NOT_SUPPORTED                         = -104,    // Requested feature not supported in the selected GPU
    NVAPI_PORTID_NOT_FOUND                      = -105,    // NO port ID found for I2C transaction
    NVAPI_EXPECTED_UNATTACHED_DISPLAY_HANDLE    = -106,    // expected an unattached display handle as one of the input param
    NVAPI_INVALID_PERF_LEVEL                    = -107,    // invalid perf level
    NVAPI_DEVICE_BUSY                           = -108,    // device is busy, request not fulfilled
    NVAPI_NV_PERSIST_FILE_NOT_FOUND             = -109,    // NV persist file is not found
    NVAPI_PERSIST_DATA_NOT_FOUND                = -110,    // NV persist data is not found
    NVAPI_EXPECTED_TV_DISPLAY                   = -111,    // expected TV output display
    NVAPI_EXPECTED_TV_DISPLAY_ON_DCONNECTOR     = -112,    // expected TV output on D Connector - HDTV_EIAJ4120.
    NVAPI_NO_ACTIVE_SLI_TOPOLOGY                = -113,    // SLI is not active on this device
    NVAPI_SLI_RENDERING_MODE_NOTALLOWED         = -114,    // setup of SLI rendering mode is not possible right now
    NVAPI_EXPECTED_DIGITAL_FLAT_PANEL           = -115,    // expected digital flat panel
    NVAPI_ARGUMENT_EXCEED_MAX_SIZE              = -116,    // argument exceeds expected size
    NVAPI_DEVICE_SWITCHING_NOT_ALLOWED          = -117,    // inhibit ON due to one of the flags in NV_GPU_DISPLAY_CHANGE_INHIBIT or SLI Active
    NVAPI_TESTING_CLOCKS_NOT_SUPPORTED          = -118,    // testing clocks not supported
    NVAPI_UNKNOWN_UNDERSCAN_CONFIG              = -119,    // the specified underscan config is from an unknown source (e.g. INF)
    NVAPI_TIMEOUT_RECONFIGURING_GPU_TOPO        = -120,    // timeout while reconfiguring GPUs
    NVAPI_DATA_NOT_FOUND                        = -121,    // Requested data was not found
    NVAPI_EXPECTED_ANALOG_DISPLAY               = -122,    // expected analog display
    NVAPI_NO_VIDLINK                            = -123,    // No SLI video bridge present
    NVAPI_REQUIRES_REBOOT                       = -124,    // NVAPI requires reboot for its settings to take effect
    NVAPI_INVALID_HYBRID_MODE                   = -125,    // the function is not supported with the current hybrid mode.
    NVAPI_MIXED_TARGET_TYPES                    = -126,    // The target types are not all the same
    NVAPI_SYSWOW64_NOT_SUPPORTED                = -127,    // the function is not supported from 32-bit on a 64-bit system
    NVAPI_IMPLICIT_SET_GPU_TOPOLOGY_CHANGE_NOT_ALLOWED = -128,    //there is any implicit GPU topo active. Use NVAPI_SetHybridMode to change topology.
    NVAPI_REQUEST_USER_TO_CLOSE_NON_MIGRATABLE_APPS = -129,      //Prompt the user to close all non-migratable apps.
    NVAPI_OUT_OF_MEMORY                         = -130,    // Could not allocate sufficient memory to complete the call
    NVAPI_WAS_STILL_DRAWING                     = -131,    // The previous operation that is transferring information to or from this surface is incomplete
    NVAPI_FILE_NOT_FOUND                        = -132,    // The file was not found
    NVAPI_TOO_MANY_UNIQUE_STATE_OBJECTS         = -133,    // There are too many unique instances of a particular type of state object
    NVAPI_INVALID_CALL                          = -134,    // The method call is invalid. For example, a method's parameter may not be a valid pointer
    NVAPI_D3D10_1_LIBRARY_NOT_FOUND             = -135,    // d3d10_1.dll can not be loaded
    NVAPI_FUNCTION_NOT_FOUND                    = -136,    // Couldn't find the function in loaded dll library
    NVAPI_INVALID_USER_PRIVILEGE                = -137,    // Current User is not Admin
    NVAPI_EXPECTED_NON_PRIMARY_DISPLAY_HANDLE   = -138,    // The handle corresponds to GDIPrimary
    NVAPI_EXPECTED_COMPUTE_GPU_HANDLE           = -139,    // Setting Physx GPU requires that the GPU is compute capable
    NVAPI_STEREO_NOT_INITIALIZED                = -140,    // Stereo part of NVAPI failed to initialize completely. Check if stereo driver is installed.
    NVAPI_STEREO_REGISTRY_ACCESS_FAILED         = -141,    // Access to stereo related registry keys or values failed.
    NVAPI_STEREO_REGISTRY_PROFILE_TYPE_NOT_SUPPORTED = -142, // Given registry profile type is not supported.
    NVAPI_STEREO_REGISTRY_VALUE_NOT_SUPPORTED   = -143,    // Given registry value is not supported.
    NVAPI_STEREO_NOT_ENABLED                    = -144,    // Stereo is not enabled and function needed it to execute completely.
    NVAPI_STEREO_NOT_TURNED_ON                  = -145,    // Stereo is not turned on and function needed it to execute completely.
    NVAPI_STEREO_INVALID_DEVICE_INTERFACE       = -146,    // Invalid device interface.
    NVAPI_STEREO_PARAMETER_OUT_OF_RANGE         = -147,    // Separation percentage or JPEG image capture quality out of [0-100] range.
    NVAPI_STEREO_FRUSTUM_ADJUST_MODE_NOT_SUPPORTED = -148, // Given frustum adjust mode is not supported.
    NVAPI_TOPO_NOT_POSSIBLE                     = -149,    // The mosaic topo is not possible given the current state of HW
    NVAPI_MODE_CHANGE_FAILED                    = -150,    // An attempt to do a display resolution mode change has failed
    NVAPI_D3D11_LIBRARY_NOT_FOUND               = -151,    // d3d11.dll/d3d11_beta.dll cannot be loaded.
    NVAPI_INVALID_ADDRESS                       = -152,    // Address outside of valid range.
    NVAPI_STRING_TOO_SMALL                      = -153,    // The pre-allocated string is too small to hold the result.
    NVAPI_MATCHING_DEVICE_NOT_FOUND             = -154,    // The input does not match any of the available devices.
    NVAPI_DRIVER_RUNNING                        = -155,    // Driver is running
    NVAPI_DRIVER_NOTRUNNING                     = -156,    // Driver is not running
    NVAPI_ERROR_DRIVER_RELOAD_REQUIRED          = -157,    // A driver reload is required to apply these settings
    NVAPI_SET_NOT_ALLOWED                       = -158,    // Intended Setting is not allowed
    NVAPI_ADVANCED_DISPLAY_TOPOLOGY_REQUIRED    = -159,    // Information can't be returned due to "advanced display topology"
    NVAPI_SETTING_NOT_FOUND                     = -160,    // Setting is not found
    NVAPI_SETTING_SIZE_TOO_LARGE                = -161,    // Setting size is too large
    NVAPI_TOO_MANY_SETTINGS_IN_PROFILE          = -162,    // There are too many settings for a profile
    NVAPI_PROFILE_NOT_FOUND                     = -163,    // Profile is not found
    NVAPI_PROFILE_NAME_IN_USE                   = -164,    // Profile name is duplicated
    NVAPI_PROFILE_NAME_EMPTY                    = -165,    // Profile name is empty
    NVAPI_EXECUTABLE_NOT_FOUND                  = -166,    // Application not found in Profile
    NVAPI_EXECUTABLE_ALREADY_IN_USE             = -167,    // Application already exists in the other profile
    NVAPI_DATATYPE_MISMATCH                     = -168,    // Data Type mismatch
    NVAPI_PROFILE_REMOVED                       = -169,    /// The profile passed as parameter has been removed and is no longer valid.
    NVAPI_UNREGISTERED_RESOURCE                 = -170,    // An unregistered resource was passed as a parameter
    NVAPI_ID_OUT_OF_RANGE                       = -171,    // The DisplayId corresponds to a display which is not within the normal outputId range
    NVAPI_DISPLAYCONFIG_VALIDATION_FAILED       = -172,    // Display topology is not valid so we can't do a mode set on this config
    NVAPI_DPMST_CHANGED                         = -173,    // Display Port Multi-Stream topology has been changed
    NVAPI_INSUFFICIENT_BUFFER                   = -174,    // Input buffer is insufficient to hold the contents    
    NVAPI_ACCESS_DENIED                         = -175,    // No access to the caller.
    NVAPI_MOSAIC_NOT_ACTIVE                     = -176,    // The requested action cannot be performed without Mosaic being enabled
    NVAPI_SHARE_RESOURCE_RELOCATED              = -177,    // The surface is relocated away from vidmem
    NVAPI_REQUEST_USER_TO_DISABLE_DWM           = -178,    // The user should disable DWM before calling NvAPI
    NVAPI_INVALID_CONFIGURATION                 = -180,    // The requested action cannot be performed in the current state
    NVAPI_STEREO_HANDSHAKE_NOT_DONE             = -181,    // Call failed as stereo handshake not completed.
} NvAPI_Status;

/* Initialises NvAPI (must be called first). Returns NV_OK if successful,
   otherwise can fail for several reasons (not using latest nVidia drivers,
   don't have an nVidia graphics card, etc). */
extern NvAPI_Status NvAPI_Initialize(void);

/* Unloads NvAPI (must be called last). This unloads nvapi.dll so forgetting
   to call this will result in a memory leak. */
extern NvAPI_Status NvAPI_Unload(void);


/* Sets pIsStereoEnabled to true if stereo enabled system wide. It will return
   false if 3D Vision is disabled in the nVidia control panel or 3D vision is
   not supported by the hardware. */
extern NvAPI_Status NvAPI_Stereo_IsEnabled(unsigned char *pIsStereoEnabled);

/* Creates a stereo handle from a Direct3D device. This may be a D3D 9, D3D 10,
   or D3D11 device. */
extern NvAPI_Status NvAPI_Stereo_CreateHandleFromIUnknown(void *pDevice,
	StereoHandle *pStereoHandle);

/* Destroys a stereo handle. Call this before unloading the NvAPI. */
extern NvAPI_Status NvAPI_Stereo_DestroyHandle(StereoHandle stereoHandle);


/* Sets isStereoActivated to true if stereo is activated at that particular
   point in time. This can be toggled using the Activate/Deactive calls,
   pressing the button on the front of the 3D Vision USB dongle, or using
   the nVidia hotkey (Ctrl+T by default). */
extern NvAPI_Status NvAPI_Stereo_IsActivated(StereoHandle stereoHandle,
	unsigned char *isStereoActivated);

/* Turns 3d Vision on in the application (as long as it is activated on the
   system and you have a valid handle). Note that the user can manually turn
   it off via the USB dongle or hotkey so you must constantly check if stereo
   is activated. */
extern NvAPI_Status NvAPI_Stereo_Activate(StereoHandle stereoHandle);

/* Turns 3d Vision off in the application. Note that the user can manually
   turn it on via the USB dongle or hotkey, so you must constantly check if
   stereo is activated, and turn it off if you're playing 2d content. */
extern NvAPI_Status NvAPI_Stereo_Deactivate(StereoHandle stereoHandle);