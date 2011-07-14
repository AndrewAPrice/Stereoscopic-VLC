#include <d3d9.h>
#include <nvapi.h>

__declspec(dllexport) NvAPI_Status WINAPI NvAPIWrapper_Initialize()
{
	return NvAPI_Initialize();
}

__declspec(dllexport) NvAPI_Status WINAPI NvAPIWrapper_Unload()
{
	return NvAPI_Unload();
}

__declspec(dllexport) NvAPI_Status WINAPI NvAPIWrapper_Stereo_IsEnabled(
	NvU8 *pIsStereoEnabled)
{
	return NvAPI_Stereo_IsEnabled(pIsStereoEnabled);
}

__declspec(dllexport) NvAPI_Status WINAPI NvAPIWrapper_Stereo_Enable(void)
{
	return NvAPI_Stereo_Enable();
}

__declspec(dllexport) NvAPI_Status WINAPI NvAPIWrapper_Stereo_CreateHandleFromIUnknown(
	IUnknown *pDevice, StereoHandle *pStereoHandle)
{
	return NvAPI_Stereo_CreateHandleFromIUnknown(pDevice, pStereoHandle);
}

__declspec(dllexport) NvAPI_Status WINAPI NvAPIWrapper_Stereo_IsActivated(
	StereoHandle stereoHandle, NvU8 *pIsStereoOn)
{
	return NvAPI_Stereo_IsActivated(stereoHandle, pIsStereoOn);
}

__declspec(dllexport) NvAPI_Status WINAPI NvAPIWrapper_Stereo_DestroyHandle(
	StereoHandle stereoHandle)
{
    return NvAPI_Stereo_DestroyHandle(stereoHandle);
}