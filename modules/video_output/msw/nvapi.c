/*****************************************************************************
 * nvapi.c: nVidia stereoscopic API
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

#include <windows.h>
#include "nvapi.h"

HMODULE h_nvapi; /* reference to nvapi dll */
void *(__cdecl *p_queryInterface)(unsigned __int32);
void (__cdecl *p_callStart)(unsigned __int32, unsigned __int64 *);
int (__cdecl *p_callReturn)(unsigned __int32, unsigned __int32,
	unsigned __int32, unsigned __int32);
int (__cdecl *p_NvAPI_StereoIsEnabled)(unsigned char *);
int (__cdecl *p_NvAPI_StereoCreateHandleFromIUnknown)(void *, StereoHandle *);
int (__cdecl *p_NvAPI_StereoDestroyHandle)(StereoHandle);
int (__cdecl *p_NvAPI_StereoIsActivated)(StereoHandle, unsigned char *);
int (__cdecl *p_NvAPI_StereoActivate)(StereoHandle);
int (__cdecl *p_NvAPI_StereoDeactivate)(StereoHandle);

/* Initialises NvAPI (must be called first). Returns NV_OK if successful,
   otherwise can fail for several reasons (not using latest nVidia drivers,
   don't have an nVidia graphics card, etc). */
NvAPI_Status NvAPI_Initialize(void)
{
	int (*p_NvAPIInitialise)(void); // [sp+0h] [bp-4h]@6

	/* try to load nvapi */
	h_nvapi = LoadLibraryA("nvapi.dll");
	if( !h_nvapi )
		return NVAPI_LIBRARY_NOT_FOUND;

	p_queryInterface = (void *(__cdecl *)(unsigned __int32))GetProcAddress(h_nvapi, "nvapi_QueryInterface");
	if( !p_queryInterface )
	{
		FreeLibrary(h_nvapi);
		return NVAPI_ERROR;
	}

	p_NvAPIInitialise = (int (*)(void))p_queryInterface(0x150E828u);

	if ( !p_NvAPIInitialise )
	{
		FreeLibrary(h_nvapi);
		return NVAPI_ERROR;
	}

	if( p_NvAPIInitialise() != NVAPI_OK )
	{
		FreeLibrary(h_nvapi);
		return NVAPI_ERROR;
	}
	
	p_callStart = (void (__cdecl *)(unsigned __int32, unsigned __int64 *))p_queryInterface(0x33C7358Cu);
	p_callReturn = (__int32 (__cdecl *)(unsigned __int32, unsigned __int32,
		unsigned __int32, unsigned __int32))p_queryInterface(0x593E8644u);

	if ( !p_callStart && !p_callReturn)
	{
		p_callStart = 0;
		p_callReturn = 0;
	}

	/* load function pointers */
	p_NvAPI_StereoIsEnabled = (int (__cdecl *)(unsigned char *))p_queryInterface(0x348FF8E1u);
	p_NvAPI_StereoCreateHandleFromIUnknown = (int (__cdecl *)(void *, StereoHandle *))p_queryInterface(0xAC7E37F4u);
	p_NvAPI_StereoDestroyHandle = (int (__cdecl *)(StereoHandle))p_queryInterface(0x3A153134u);
	p_NvAPI_StereoIsActivated = (int (__cdecl *)(StereoHandle, unsigned char *))p_queryInterface(0x1FB0BC30u);
	p_NvAPI_StereoActivate = (int (__cdecl *)(StereoHandle))p_queryInterface(0xF6A1AD68u);
	p_NvAPI_StereoDeactivate = (int (__cdecl *)(StereoHandle))p_queryInterface(0x2D68DE96u);

	return NVAPI_OK;
}

/* Unloads NvAPI (must be called last). This unloads nvapi.dll so forgetting
   to call this will result in a memory leak. */
NvAPI_Status NvAPI_Unload(void)
{
	/*signed int i;*/
	int (*p_NvAPI_Unload)(void);

	p_NvAPI_Unload = (int (*)(void))p_queryInterface(0xD22BDD7Eu);
	if ( !p_NvAPI_Unload )
		return NVAPI_NO_IMPLEMENTATION;

	if ( p_NvAPI_Unload() != NVAPI_OK )
		return NVAPI_ERROR;

	p_queryInterface = 0;
	h_nvapi = 0;
	p_callStart = 0;
	p_callReturn = 0;
	FreeLibrary(h_nvapi);
	return NVAPI_OK;
}

/* Sets pIsStereoEnabled to true if stereo enabled system wide. It will return
   false if 3D Vision is disabled in the nVidia control panel or 3D vision is
   not supported by the hardware. */
NvAPI_Status NvAPI_Stereo_IsEnabled(unsigned char *pIsStereoEnabled)
{
	if(!p_NvAPI_StereoIsEnabled)
		return NVAPI_NO_IMPLEMENTATION;
	
	int callStartReturn = 0;
	int callStartReturn2 = 0;
	if ( p_callStart )
		p_callStart(0x348FF8E1u, (unsigned __int64 *)&callStartReturn);

	NvAPI_Status result = p_NvAPI_StereoIsEnabled(pIsStereoEnabled);

	if ( p_callReturn )
		p_callReturn(881850593, callStartReturn, callStartReturn2, result);
	return result;
}

/* Creates a stereo handle from a Direct3D device. This may be a D3D 9, D3D 10,
   or D3D11 device. */
extern NvAPI_Status NvAPI_Stereo_CreateHandleFromIUnknown(void *pDevice,
	StereoHandle *pStereoHandle)
{
	if( !p_NvAPI_StereoCreateHandleFromIUnknown )
		return NVAPI_NO_IMPLEMENTATION;

	int callStartReturn = 0;
	int callStartReturn2 = 0;
	    if ( p_callStart )
	       p_callStart(0xAC7E37F4u, (unsigned __int64 *)&callStartReturn);

	    NvAPI_Status result = p_NvAPI_StereoCreateHandleFromIUnknown(pDevice, pStereoHandle);

	    if ( p_callReturn )
	      p_callReturn(-1401014284, callStartReturn, callStartReturn2, result);
	    return result;
}

/* Destroys a stereo handle. Call this before unloading the NvAPI. */
NvAPI_Status NvAPI_Stereo_DestroyHandle(StereoHandle stereoHandle)
{
	if( !p_NvAPI_StereoDestroyHandle )
		return NVAPI_NO_IMPLEMENTATION;

	int callStartReturn = 0;
	int callStartReturn2 = 0;

	if (  p_callStart )
	       p_callStart(0x3A153134u, (unsigned __int64 *)&callStartReturn);

	    NvAPI_Status result = p_NvAPI_StereoDestroyHandle(stereoHandle);

	    if ( p_callReturn )
	      p_callReturn(974467380, callStartReturn, callStartReturn2, result);
	    return result;
}


/* Sets isStereoActivated to true if stereo is activated at that particular
   point in time. This can be toggled using the Activate/Deactive calls,
   pressing the button on the front of the 3D Vision USB dongle, or using
   the nVidia hotkey (Ctrl+T by default). */
extern NvAPI_Status NvAPI_Stereo_IsActivated(StereoHandle stereoHandle,
	unsigned char *isStereoActivated)
{
	if( !p_NvAPI_StereoIsActivated )
		return NVAPI_NO_IMPLEMENTATION;

	int callStartReturn = 0;
	int callStartReturn2 = 0;
	if (  p_callStart )
	       p_callStart(0x1FB0BC30u, (unsigned __int64 *)&callStartReturn);
	    NvAPI_Status result = p_NvAPI_StereoIsActivated(stereoHandle, isStereoActivated);
	    if ( p_callReturn )
	      p_callReturn(531676208, callStartReturn, callStartReturn2, result);
	return result;
}

/* Turns 3d Vision on in the application (as long as it is activated on the
   system and you have a valid handle). Note that the user can manually turn
   it off via the USB dongle or hotkey so you must constantly check if stereo
   is activated. */
NvAPI_Status NvAPI_Stereo_Activate(StereoHandle stereoHandle)
{
	if( !p_NvAPI_StereoActivate )
		return NVAPI_NO_IMPLEMENTATION;

	int callStartReturn = 0;
	int callStartReturn2 = 0;
    if (  p_callStart )
       p_callStart(0xF6A1AD68u, (unsigned __int64 *)&callStartReturn);

    NvAPI_Status result = p_NvAPI_StereoActivate(stereoHandle);

    if ( p_callReturn )
      p_callReturn(-157176472, callStartReturn, callStartReturn2, result);
    return result;
}

/* Turns 3d Vision off in the application. Note that the user can manually
   turn it on via the USB dongle or hotkey, so you must constantly check if
   stereo is activated, and turn it off if you're playing 2d content. */
NvAPI_Status NvAPI_Stereo_Deactivate(StereoHandle stereoHandle)
{
  if( !p_NvAPI_StereoDeactivate )
	  return NVAPI_NO_IMPLEMENTATION;

  int callStartReturn = 0;
  int callStartReturn2 = 0;

    if (  p_callStart )
       p_callStart(0x2D68DE96u, (unsigned __int64 *)&callStartReturn);

    NvAPI_Status result = p_NvAPI_StereoDeactivate(stereoHandle);

    if ( p_callReturn )
      p_callReturn(761847446, callStartReturn, callStartReturn2, result);

    return result;
}