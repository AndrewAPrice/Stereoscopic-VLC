/*****************************************************************************
 * direct3d.c: Windows Direct3D video output module
 *****************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
 *$Id$
 *
 * Authors: Damien Fouilleul <damienf@videolan.org>
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

/*****************************************************************************
 * Preamble:
 *
 * This plugin will use YUV surface if supported, using YUV will result in
 * the best video quality (hardware filering when rescaling the picture)
 * and the fastest display as it requires less processing.
 *
 * If YUV overlay is not supported this plugin will use RGB offscreen video
 * surfaces that will be blitted onto the primary surface (display) to
 * effectively display the pictures.
 *
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_vout_display.h>

#include <windows.h>
#include <d3d9.h>

#include "common.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenVideoXP(vlc_object_t *);
static int  OpenVideoVista(vlc_object_t *);
static void Close(vlc_object_t *);

#define DESKTOP_TEXT N_("Enable desktop mode ")
#define DESKTOP_LONGTEXT N_(\
    "The desktop mode allows you to display the video on the desktop.")

#define HW_BLENDING_TEXT N_("Use hardware blending support")
#define HW_BLENDING_LONGTEXT N_(\
    "Try to use hardware acceleration for subtitles/OSD blending.")

#define D3D_HELP N_("Recommended video output for Windows Vista and later versions")

vlc_module_begin ()
    set_shortname("Direct3D")
    set_description(N_("Direct3D video output"))
    set_help(D3D_HELP)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)

    add_bool("direct3d-desktop", false, DESKTOP_TEXT, DESKTOP_LONGTEXT, true)
    add_bool("direct3d-hw-blending", true, HW_BLENDING_TEXT, HW_BLENDING_LONGTEXT, true)
    

    set_capability("vout display", 240)
    add_shortcut("direct3d")
    set_callbacks(OpenVideoVista, Close)

    add_submodule()
        set_description(N_("Direct3D video output (XP)"))
        set_capability("vout display", 220)
        add_shortcut("direct3d_xp")
        set_callbacks(OpenVideoXP, Close)

vlc_module_end ()

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static const vlc_fourcc_t d3d_subpicture_chromas[] = {
    VLC_CODEC_RGBA,
    0
};

struct picture_sys_t
{
    LPDIRECT3DSURFACE9 surface;
    picture_t          *fallback;
};

static int  Open(vlc_object_t *);

static picture_pool_t *Pool  (vout_display_t *, unsigned);
static void           Prepare(vout_display_t *, picture_t *, subpicture_t *subpicture);
static void           Display(vout_display_t *, picture_t *, subpicture_t *subpicture);
static int            Control(vout_display_t *, int, va_list);
static void           Manage (vout_display_t *);

static int  Direct3DCreate (vout_display_t *);
static int  Direct3DReset  (vout_display_t *);
static void Direct3DDestroy(vout_display_t *);

static int  Direct3DOpen (vout_display_t *, video_format_t *);
static void Direct3DClose(vout_display_t *);

static int NvCreate  (vout_display_t *);
static void NvDestroy (vout_display_t *);

/* */
typedef struct
{
    FLOAT       x,y,z;      // vertex untransformed position
    FLOAT       rhw;        // eye distance
    D3DCOLOR    diffuse;    // diffuse color
    FLOAT       tu, tv;     // texture relative coordinates
} CUSTOMVERTEX;
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)

typedef struct d3d_region_t {
    D3DFORMAT          format;
    unsigned           width;
    unsigned           height;
    CUSTOMVERTEX       vertex[4];
    LPDIRECT3DTEXTURE9 texture;
} d3d_region_t;

static void Direct3DDeleteRegions(int, d3d_region_t *);

static int  Direct3DImportPicture(vout_display_t *vd, d3d_region_t *, LPDIRECT3DSURFACE9 surface, int i_eye);
static void Direct3DImportSubpicture(vout_display_t *vd, int *, d3d_region_t **, subpicture_t *);

static void Direct3DRenderScene(vout_display_t *vd, d3d_region_t *, int, d3d_region_t *);

/* */
static int DesktopCallback(vlc_object_t *, char const *, vlc_value_t, vlc_value_t, void *);

/**
 * It creates a Direct3D vout display.
 */
static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;

    /* Allocate structure */
    vd->sys = sys = calloc(1, sizeof(vout_display_sys_t));
    if (!sys)
        return VLC_ENOMEM;

    NvCreate(vd);

    if (Direct3DCreate(vd)) {
        msg_Err(vd, "Direct3D could not be initialized");
        Direct3DDestroy(vd);
        NvDestroy(vd);
        free(sys);
        return VLC_EGENERIC;
    }

    sys->use_desktop = var_CreateGetBool(vd, "direct3d-desktop");
    sys->reset_device = false;
    sys->reset_device = false;
    sys->allow_hw_yuv = var_CreateGetBool(vd, "directx-hw-yuv");
    sys->desktop_save.is_fullscreen = vd->cfg->is_fullscreen;
    sys->desktop_save.is_on_top     = false;
    sys->desktop_save.win.left      = var_InheritInteger(vd, "video-x");
    sys->desktop_save.win.right     = vd->cfg->display.width;
    sys->desktop_save.win.top       = var_InheritInteger(vd, "video-y");
    sys->desktop_save.win.bottom    = vd->cfg->display.height;

    if (CommonInit(vd))
        goto error;

    /* */
    video_format_t fmt;
    if (Direct3DOpen(vd, &fmt)) {
        msg_Err(vd, "Direct3D could not be opened");
        goto error;
    }

    /* */
    vout_display_info_t info = vd->info;
    info.is_slow = true;
    info.has_double_click = true;
    info.has_hide_mouse = false;
    info.has_pictures_invalid = true;
    info.has_event_thread = true;
    if (var_InheritBool(vd, "direct3d-hw-blending") &&
        (sys->d3dcaps.SrcBlendCaps  & D3DPBLENDCAPS_SRCALPHA) &&
        (sys->d3dcaps.DestBlendCaps & D3DPBLENDCAPS_INVSRCALPHA) &&
        (sys->d3dcaps.TextureCaps   & D3DPTEXTURECAPS_ALPHA) &&
        (sys->d3dcaps.TextureOpCaps & D3DTEXOPCAPS_SELECTARG1) &&
        (sys->d3dcaps.TextureOpCaps & D3DTEXOPCAPS_MODULATE))
        info.subpicture_chromas = d3d_subpicture_chromas;
    else
        info.subpicture_chromas = NULL;

    /* Interaction */
    vlc_mutex_init(&sys->lock);
    sys->ch_desktop = false;
    sys->desktop_requested = sys->use_desktop;

    vlc_value_t val;
    val.psz_string = _("Desktop");
    var_Change(vd, "direct3d-desktop", VLC_VAR_SETTEXT, &val, NULL);
    var_AddCallback(vd, "direct3d-desktop", DesktopCallback, NULL);

    /* Setup vout_display now that everything is fine */
    vd->fmt  = fmt;
    vd->info = info;

    vd->pool    = Pool;
    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = Control;
    vd->manage  = Manage;

    /* Fix state in case of desktop mode */
    if (sys->use_desktop && vd->cfg->is_fullscreen)
        vout_display_SendEventFullscreen(vd, false);

    return VLC_SUCCESS;
error:
    Direct3DClose(vd);
    CommonClean(vd);
    Direct3DDestroy(vd);
    NvDestroy(vd);
    free(vd->sys);
    return VLC_EGENERIC;
}

static bool IsVistaOrAbove(void)
{
    OSVERSIONINFO winVer;
    winVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    return GetVersionEx(&winVer) && winVer.dwMajorVersion > 5;
}

static int OpenVideoXP(vlc_object_t *obj)
{
    /* Windows XP or lower, make sure this module isn't the default */
    return IsVistaOrAbove() ? VLC_EGENERIC : Open(obj);
}

static int OpenVideoVista(vlc_object_t *obj)
{
    /* Windows Vista or above, make this module the default */
    return IsVistaOrAbove() ? Open(obj) : VLC_EGENERIC;
}

/**
 * It destroyes a Direct3D vout display.
 */
static void Close(vlc_object_t *object)
{
    vout_display_t * vd = (vout_display_t *)object;

    var_DelCallback(vd, "direct3d-desktop", DesktopCallback, NULL);
    vlc_mutex_destroy(&vd->sys->lock);

    Direct3DClose(vd);

    CommonClean(vd);

    Direct3DDestroy(vd);
    NvDestroy(vd);

    free(vd->sys);
}

/* */
static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    VLC_UNUSED(count);
    return vd->sys->pool;
}

static int  Direct3DLockSurface(picture_t *);
static void Direct3DUnlockSurface(picture_t *);

static void Prepare(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    LPDIRECT3DSURFACE9 surface = picture->p_sys->surface;
    int i_eye = picture->i_eye; /* which eye is this picture from */
#if 0
    picture_Release(picture);
    VLC_UNUSED(subpicture);
#else
    /* FIXME it is a bit ugly, we need the surface to be unlocked for
     * rendering.
     *  The clean way would be to release the picture (and ensure that
     * the vout doesn't keep a reference). But because of the vout
     * wrapper, we can't */

    Direct3DUnlockSurface(picture);
    VLC_UNUSED(subpicture);
#endif

    /* check if device is still available */
    HRESULT hr = IDirect3DDevice9_TestCooperativeLevel(sys->d3ddev);
    if (FAILED(hr)) {
        if (hr == D3DERR_DEVICENOTRESET && !sys->reset_device) {
            vout_display_SendEventPicturesInvalid(vd);
            sys->reset_device = true;
        }
        return;
    }

    d3d_region_t picture_region;
    if (!Direct3DImportPicture(vd, &picture_region, surface, i_eye)
       /*!(i_eye & STEREO_WAIT_FOR_NEXT_FRAME_BIT)*/ /* causes it to flicker,
                                                         perhapes stopping Display
                                                         will fix it */) {
        int subpicture_region_count     = 0;
        d3d_region_t *subpicture_region = NULL;
        if (subpicture)
            Direct3DImportSubpicture(vd, &subpicture_region_count, &subpicture_region,
                                     subpicture);

        Direct3DRenderScene(vd, &picture_region,
                            subpicture_region_count, subpicture_region);

        Direct3DDeleteRegions(sys->d3dregion_count, sys->d3dregion);
        sys->d3dregion_count = subpicture_region_count;
        sys->d3dregion       = subpicture_region;
    }
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    LPDIRECT3DDEVICE9 d3ddev = sys->d3ddev;

    // Present the back buffer contents to the display
    // No stretching should happen here !
    const RECT src = sys->rect_dest_clipped;
    const RECT dst = sys->rect_dest_clipped;
    HRESULT hr = IDirect3DDevice9_Present(d3ddev, &src, &dst, NULL, NULL);
    if (FAILED(hr)) {
        msg_Dbg(vd, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
    }

#if 0
    VLC_UNUSED(picture);
    VLC_UNUSED(subpicture);
#else
    /* XXX See Prepare() */
    Direct3DLockSurface(picture);
    picture_Release(picture);
#endif
    if (subpicture)
        subpicture_Delete(subpicture);

    CommonDisplay(vd);
}
static int ControlResetDevice(vout_display_t *vd)
{
    return Direct3DReset(vd);
}
static int ControlReopenDevice(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->use_desktop) {
        /* Save non-desktop state */
        sys->desktop_save.is_fullscreen = vd->cfg->is_fullscreen;
        sys->desktop_save.is_on_top     = sys->is_on_top;

        WINDOWPLACEMENT wp = { .length = sizeof(wp), };
        GetWindowPlacement(sys->hparent ? sys->hparent : sys->hwnd, &wp);
        sys->desktop_save.win = wp.rcNormalPosition;
    }

    /* */
    Direct3DClose(vd);
    EventThreadStop(sys->event);

    /* */
    vlc_mutex_lock(&sys->lock);
    sys->use_desktop = sys->desktop_requested;
    sys->ch_desktop = false;
    vlc_mutex_unlock(&sys->lock);

    /* */
    event_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.use_desktop = sys->use_desktop;
    if (!sys->use_desktop) {
        cfg.win.type   = VOUT_WINDOW_TYPE_HWND;
        cfg.win.x      = sys->desktop_save.win.left;
        cfg.win.y      = sys->desktop_save.win.top;
        cfg.win.width  = sys->desktop_save.win.right  - sys->desktop_save.win.left;
        cfg.win.height = sys->desktop_save.win.bottom - sys->desktop_save.win.top;
    }

    event_hwnd_t hwnd;
    if (EventThreadStart(sys->event, &hwnd, &cfg)) {
        msg_Err(vd, "Failed to restart event thread");
        return VLC_EGENERIC;
    }
    sys->parent_window = hwnd.parent_window;
    sys->hparent       = hwnd.hparent;
    sys->hwnd          = hwnd.hwnd;
    sys->hvideownd     = hwnd.hvideownd;
    sys->hfswnd        = hwnd.hfswnd;
    SetRectEmpty(&sys->rect_parent);

    /* */
    video_format_t fmt;
    if (Direct3DOpen(vd, &fmt)) {
        CommonClean(vd);
        msg_Err(vd, "Failed to reopen device");
        return VLC_EGENERIC;
    }
    vd->fmt = fmt;
    sys->is_first_display = true;

    if (sys->use_desktop) {
        /* Disable fullscreen/on_top while using desktop */
        if (sys->desktop_save.is_fullscreen)
            vout_display_SendEventFullscreen(vd, false);
        if (sys->desktop_save.is_on_top)
            vout_display_SendWindowState(vd, VOUT_WINDOW_STATE_NORMAL);
    } else {
        /* Restore fullscreen/on_top */
        if (sys->desktop_save.is_fullscreen)
            vout_display_SendEventFullscreen(vd, true);
        if (sys->desktop_save.is_on_top)
            vout_display_SendWindowState(vd, VOUT_WINDOW_STATE_ABOVE);
    }
    return VLC_SUCCESS;
}
static int Control(vout_display_t *vd, int query, va_list args)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query) {
    case VOUT_DISPLAY_RESET_PICTURES:
        /* FIXME what to do here in case of failure */
        if (sys->reset_device) {
            if (ControlResetDevice(vd)) {
                msg_Err(vd, "Failed to reset device");
                return VLC_EGENERIC;
            }
            sys->reset_device = false;
        } else if(sys->reopen_device) {
            if (ControlReopenDevice(vd)) {
                msg_Err(vd, "Failed to reopen device");
                return VLC_EGENERIC;
            }
            sys->reopen_device = false;
        }
        return VLC_SUCCESS;
    default:
        return CommonControl(vd, query, args);
    }
}
static void Manage (vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    CommonManage(vd);

    /* Desktop mode change */
    vlc_mutex_lock(&sys->lock);
    const bool ch_desktop = sys->ch_desktop;
    sys->ch_desktop = false;
    vlc_mutex_unlock(&sys->lock);

    if (ch_desktop) {
        sys->reopen_device = true;
        vout_display_SendEventPicturesInvalid(vd);
    }

    /* Position Change */
    if (sys->changes & DX_POSITION_CHANGE) {
#if 0 /* need that when bicubic filter is available */
        RECT rect;
        UINT width, height;

        GetClientRect(p_sys->hvideownd, &rect);
        width  = rect.right-rect.left;
        height = rect.bottom-rect.top;

        if (width != p_sys->d3dpp.BackBufferWidth || height != p_sys->d3dpp.BackBufferHeight)
        {
            msg_Dbg(vd, "resizing device back buffers to (%lux%lu)", width, height);
            // need to reset D3D device to resize back buffer
            if (VLC_SUCCESS != Direct3DResetDevice(vd, width, height))
                return VLC_EGENERIC;
        }
#endif
        sys->clear_scene = true;
        sys->changes &= ~DX_POSITION_CHANGE;
    }
}

/**
 * Initialises an instance of 3D vision
 */
static int NvCreate(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    
    /* try initialising nvapi */
    if(NvAPI_Initialize() == NVAPI_OK)
    {
        /* not returning NVAPI_OK for Initialise() could mean bad drivers or
           not using an nVidia card */
        unsigned char stereoEnabled;
        if(NvAPI_Stereo_IsEnabled(&stereoEnabled) != NVAPI_OK)
        {
            /* not returning NVAPI_OK for Stereo_IsEnabled could possibly mean
               an unsupported video card or not running Windows Vista/7 */
            msg_Warn(vd, "Cannot check if NVAPI 3d Vision is enabled. Bad drive or not using compatible card.");
            NvAPI_Unload();
            sys->use_nvidia_3d = false;
            return VLC_EGENERIC;
        }
        else
        {
			if(!stereoEnabled)
			{
			    msg_Warn(vd, "NVAPI 3d Vision is disabled. Enable in nVidia Control Panel or not using compatible card.");
			    NvAPI_Unload();
			    sys->use_nvidia_3d = false;
			    return VLC_EGENERIC;
			}
        }
    }
    else
    {
        msg_Warn(vd, "NvAPI did not want to initialize. Bad driver or not using nVidia card.");
        sys->use_nvidia_3d = false;
        return VLC_EGENERIC;
    }

    sys->use_nvidia_3d = true;
    return VLC_SUCCESS;
}

/**
 * It initializes an instance of Direct3D9
 */
static int Direct3DCreate(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    sys->hd3d9_dll = LoadLibrary(TEXT("D3D9.DLL"));
    if (!sys->hd3d9_dll) {
        msg_Warn(vd, "cannot load d3d9.dll, aborting");
        return VLC_EGENERIC;
    }

    LPDIRECT3D9 (WINAPI *OurDirect3DCreate9)(UINT SDKVersion);
    OurDirect3DCreate9 =
        (void *)GetProcAddress(sys->hd3d9_dll, TEXT("Direct3DCreate9"));
    if (!OurDirect3DCreate9) {
        msg_Err(vd, "Cannot locate reference to Direct3DCreate9 ABI in DLL");
        return VLC_EGENERIC;
    }

    /* Create the D3D object. */
    LPDIRECT3D9 d3dobj = OurDirect3DCreate9(D3D_SDK_VERSION);
    if (!d3dobj) {
       msg_Err(vd, "Could not create Direct3D9 instance.");
       return VLC_EGENERIC;
    }
    sys->d3dobj = d3dobj;

    /*
    ** Get device capabilities
    */
    ZeroMemory(&sys->d3dcaps, sizeof(sys->d3dcaps));
    HRESULT hr = IDirect3D9_GetDeviceCaps(d3dobj, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &sys->d3dcaps);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not read adapter capabilities. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    /* TODO: need to test device capabilities and select the right render function */

    return VLC_SUCCESS;
}

/**
 * It releases an instance of nVidia 3D Vision
 */
static void NvDestroy(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if(!sys->use_nvidia_3d)
        return;

    sys->use_nvidia_3d = false;

    NvAPI_Unload();
}

/**
 * It releases an instance of Direct3D9
 */
static void Direct3DDestroy(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->d3dobj)
       IDirect3D9_Release(sys->d3dobj);
    if (sys->hd3d9_dll)
        FreeLibrary(sys->hd3d9_dll);

    sys->d3dobj = NULL;
    sys->hd3d9_dll = NULL;
}


/**
 * It setup vout_display_sys_t::d3dpp and vout_display_sys_t::rect_display
 * from the default adapter.
 */
static int Direct3DFillPresentationParameters(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    /*
    ** Get the current desktop display mode, so we can set up a back
    ** buffer of the same format
    */
    D3DDISPLAYMODE d3ddm;
    HRESULT hr = IDirect3D9_GetAdapterDisplayMode(sys->d3dobj,
                                                  D3DADAPTER_DEFAULT, &d3ddm);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not read adapter display mode. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    /* Set up the structure used to create the D3DDevice. */
    D3DPRESENT_PARAMETERS *d3dpp = &vd->sys->d3dpp;
    ZeroMemory(d3dpp, sizeof(D3DPRESENT_PARAMETERS));
    d3dpp->Flags                  = D3DPRESENTFLAG_VIDEO;
    d3dpp->Windowed               = TRUE;
    d3dpp->hDeviceWindow          = vd->sys->hvideownd;
    d3dpp->BackBufferWidth        = __MAX((unsigned int)GetSystemMetrics(SM_CXVIRTUALSCREEN),
                                          d3ddm.Width);
    d3dpp->BackBufferHeight       = __MAX((unsigned int)GetSystemMetrics(SM_CYVIRTUALSCREEN),
                                          d3ddm.Height);
    d3dpp->SwapEffect             = D3DSWAPEFFECT_COPY;
    d3dpp->MultiSampleType        = D3DMULTISAMPLE_NONE;
    d3dpp->PresentationInterval   = D3DPRESENT_INTERVAL_DEFAULT;
    d3dpp->BackBufferFormat       = d3ddm.Format;
    d3dpp->BackBufferCount        = 1;
    d3dpp->EnableAutoDepthStencil = FALSE;

    /* */
    RECT *display = &vd->sys->rect_display;
    display->left   = 0;
    display->top    = 0;
    display->right  = d3dpp->BackBufferWidth;
    display->bottom = d3dpp->BackBufferHeight;

    return VLC_SUCCESS;
}

/* */
static int  Direct3DCreateResources (vout_display_t *, video_format_t *);
static void Direct3DDestroyResources(vout_display_t *);

/**
 * It creates a Direct3D device and the associated resources.
 */
static int Direct3DOpen(vout_display_t *vd, video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    LPDIRECT3D9 d3dobj = sys->d3dobj;

    if (Direct3DFillPresentationParameters(vd))
        return VLC_EGENERIC;

    // Create the D3DDevice
    LPDIRECT3DDEVICE9 d3ddev;

    UINT AdapterToUse = D3DADAPTER_DEFAULT;
    D3DDEVTYPE DeviceType = D3DDEVTYPE_HAL;

#ifndef NDEBUG
    // Look for 'NVIDIA PerfHUD' adapter
    // If it is present, override default settings
    for (UINT Adapter=0; Adapter< IDirect3D9_GetAdapterCount(d3dobj); ++Adapter) {
        D3DADAPTER_IDENTIFIER9 Identifier;
        HRESULT Res;
        Res = IDirect3D9_GetAdapterIdentifier(d3dobj,Adapter,0,&Identifier);
        if (strstr(Identifier.Description,"PerfHUD") != 0) {
            AdapterToUse = Adapter;
            DeviceType = D3DDEVTYPE_REF;
            break;
        }
    }
#endif

    HRESULT hr = IDirect3D9_CreateDevice(d3dobj, AdapterToUse,
                                         DeviceType, sys->hvideownd,
                                         D3DCREATE_SOFTWARE_VERTEXPROCESSING|
                                         D3DCREATE_MULTITHREADED,
                                         &sys->d3dpp, &d3ddev);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not create the D3D device! (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    sys->d3ddev = d3ddev;

    UpdateRects(vd, NULL, NULL, true);

    /* attempt to create a d3d device */
    if(sys->use_nvidia_3d)
    {
        if(NvAPI_Stereo_CreateHandleFromIUnknown(sys->d3ddev,
            &sys->nvStereoHandle) != NVAPI_OK)
        {
        msg_Err(vd, "Could not create a stereo handle.");
            NvDestroy(vd);
        }
    }

    if (Direct3DCreateResources(vd, fmt)) {
        msg_Err(vd, "Failed to allocate resources");
        return VLC_EGENERIC;
    }
    
    /* Change the window title bar text */
    EventThreadUpdateTitle(sys->event, VOUT_TITLE " (Direct3D output)");

    msg_Dbg(vd, "Direct3D device adapter successfully initialized");
    
    return VLC_SUCCESS;
}

/**
 * It releases the Direct3D9 device and its resources.
 */
static void Direct3DClose(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3DDestroyResources(vd);

    if(sys->use_nvidia_3d)
    {
        /* destroy stereo handle */
        NvAPI_Stereo_DestroyHandle(sys->nvStereoHandle);
    }

    if (sys->d3ddev)
       IDirect3DDevice9_Release(sys->d3ddev);

    sys->d3ddev = NULL;
}

/**
 * It reset the Direct3D9 device and its resources.
 */
static int Direct3DReset(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    LPDIRECT3DDEVICE9 d3ddev = sys->d3ddev;

    if (Direct3DFillPresentationParameters(vd))
        return VLC_EGENERIC;

    /* release all D3D objects */
    Direct3DDestroyResources(vd);

    if(sys->use_nvidia_3d)
    {
        /* destroy stereo handle */
        NvAPI_Stereo_DestroyHandle(sys->nvStereoHandle);
    }

    /* */
    HRESULT hr = IDirect3DDevice9_Reset(d3ddev, &sys->d3dpp);
    if (FAILED(hr)) {
        msg_Err(vd, "%s failed ! (hr=%08lX)", __FUNCTION__, hr);
        return VLC_EGENERIC;
    }

    /* attempt to create a d3d device */
    if(sys->use_nvidia_3d)
    {
        if(NvAPI_Stereo_CreateHandleFromIUnknown(sys->d3ddev,
            &sys->nvStereoHandle) != NVAPI_OK)
        {
        msg_Err(vd, "Could not create a stereo handle.");
            NvDestroy(vd);
        }
    }

    UpdateRects(vd, NULL, NULL, true);

    /* re-create them */
    if (Direct3DCreateResources(vd, &vd->fmt)) {
        msg_Dbg(vd, "%s failed !", __FUNCTION__);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/* */
static int  Direct3DCreatePool(vout_display_t *vd, video_format_t *fmt);
static void Direct3DDestroyPool(vout_display_t *vd);

static int  Direct3DCreateScene(vout_display_t *vd, const video_format_t *fmt);
static void Direct3DDestroyScene(vout_display_t *vd);

/**
 * It creates the picture and scene resources.
 */
static int Direct3DCreateResources(vout_display_t *vd, video_format_t *fmt)
{
    if (Direct3DCreatePool(vd, fmt)) {
        msg_Err(vd, "Direct3D picture pool initialization failed");
        return VLC_EGENERIC;
    }
    if (Direct3DCreateScene(vd, fmt)) {
        msg_Err(vd, "Direct3D scene initialization failed !");
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
/**
 * It destroys the picture and scene resources.
 */
static void Direct3DDestroyResources(vout_display_t *vd)
{
    Direct3DDestroyScene(vd);
    Direct3DDestroyPool(vd);
}

/**
 * It tests if the conversion from src to dst is supported.
 */
static int Direct3DCheckConversion(vout_display_t *vd,
                                   D3DFORMAT src, D3DFORMAT dst)
{
    vout_display_sys_t *sys = vd->sys;
    LPDIRECT3D9 d3dobj = sys->d3dobj;
    HRESULT hr;

    /* test whether device can create a surface of that format */
    hr = IDirect3D9_CheckDeviceFormat(d3dobj, D3DADAPTER_DEFAULT,
                                      D3DDEVTYPE_HAL, dst, 0,
                                      D3DRTYPE_SURFACE, src);
    if (SUCCEEDED(hr)) {
        /* test whether device can perform color-conversion
        ** from that format to target format
        */
        hr = IDirect3D9_CheckDeviceFormatConversion(d3dobj,
                                                    D3DADAPTER_DEFAULT,
                                                    D3DDEVTYPE_HAL,
                                                    src, dst);
    }
    if (!SUCCEEDED(hr)) {
        if (D3DERR_NOTAVAILABLE != hr)
            msg_Err(vd, "Could not query adapter supported formats. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

typedef struct
{
    const char   *name;
    D3DFORMAT    format;    /* D3D format */
    vlc_fourcc_t fourcc;    /* VLC fourcc */
    uint32_t     rmask;
    uint32_t     gmask;
    uint32_t     bmask;
} d3d_format_t;

static const d3d_format_t d3d_formats[] = {
    /* YV12 is always used for planar 420, the planes are then swapped in Lock() */
    { "YV12",       MAKEFOURCC('Y','V','1','2'),    VLC_CODEC_YV12,  0,0,0 },
    { "YV12",       MAKEFOURCC('Y','V','1','2'),    VLC_CODEC_I420,  0,0,0 },
    { "YV12",       MAKEFOURCC('Y','V','1','2'),    VLC_CODEC_J420,  0,0,0 },
    { "UYVY",       D3DFMT_UYVY,    VLC_CODEC_UYVY,  0,0,0 },
    { "YUY2",       D3DFMT_YUY2,    VLC_CODEC_YUYV,  0,0,0 },
    { "X8R8G8B8",   D3DFMT_X8R8G8B8,VLC_CODEC_RGB32, 0xff0000, 0x00ff00, 0x0000ff },
    { "A8R8G8B8",   D3DFMT_A8R8G8B8,VLC_CODEC_RGB32, 0xff0000, 0x00ff00, 0x0000ff },
    { "8G8B8",      D3DFMT_R8G8B8,  VLC_CODEC_RGB24, 0xff0000, 0x00ff00, 0x0000ff },
    { "R5G6B5",     D3DFMT_R5G6B5,  VLC_CODEC_RGB16, 0x1f<<11, 0x3f<<5,  0x1f<<0 },
    { "X1R5G5B5",   D3DFMT_X1R5G5B5,VLC_CODEC_RGB15, 0x1f<<10, 0x1f<<5,  0x1f<<0 },

    { NULL, 0, 0, 0,0,0}
};

/**
 * It returns the format (closest to chroma) that can be converted to target */
static const d3d_format_t *Direct3DFindFormat(vout_display_t *vd, vlc_fourcc_t chroma, D3DFORMAT target)
{
    vout_display_sys_t *sys = vd->sys;

    for (unsigned pass = 0; pass < 2; pass++) {
        const vlc_fourcc_t *list;

        if (pass == 0 && sys->allow_hw_yuv && vlc_fourcc_IsYUV(chroma))
            list = vlc_fourcc_GetYUVFallback(chroma);
        else if (pass == 1)
            list = vlc_fourcc_GetRGBFallback(chroma);
        else
            continue;

        for (unsigned i = 0; list[i] != 0; i++) {
            for (unsigned j = 0; d3d_formats[j].name; j++) {
                const d3d_format_t *format = &d3d_formats[j];

                if (format->fourcc != list[i])
                    continue;

                msg_Warn(vd, "trying surface pixel format: %s",
                         format->name);
                if (!Direct3DCheckConversion(vd, format->format, target)) {
                    msg_Dbg(vd, "selected surface pixel format is %s",
                            format->name);
                    return format;
                }
            }
        }
    }
    return NULL;
}

/**
 * It locks the surface associated to the picture and get the surface
 * descriptor which amongst other things has the pointer to the picture
 * data and its pitch.
 */
static int Direct3DLockSurface(picture_t *picture)
{
    /* Lock the surface to get a valid pointer to the picture buffer */
    D3DLOCKED_RECT d3drect;
    HRESULT hr = IDirect3DSurface9_LockRect(picture->p_sys->surface, &d3drect, NULL, 0);
    if (FAILED(hr)) {
        //msg_Dbg(vd, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        return CommonUpdatePicture(picture, &picture->p_sys->fallback, NULL, 0);
    }

    CommonUpdatePicture(picture, NULL, d3drect.pBits, d3drect.Pitch);
    return VLC_SUCCESS;
}
/**
 * It unlocks the surface associated to the picture.
 */
static void Direct3DUnlockSurface(picture_t *picture)
{
    /* Unlock the Surface */
    HRESULT hr = IDirect3DSurface9_UnlockRect(picture->p_sys->surface);
    if (FAILED(hr)) {
        //msg_Dbg(vd, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
    }
}

/**
 * It creates the pool of picture (only 1).
 *
 * Each picture has an associated offscreen surface in video memory
 * depending on hardware capabilities the picture chroma will be as close
 * as possible to the orginal render chroma to reduce CPU conversion overhead
 * and delegate this work to video card GPU
 */
static int Direct3DCreatePool(vout_display_t *vd, video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    LPDIRECT3DDEVICE9 d3ddev = sys->d3ddev;

    /* */
    *fmt = vd->source;

    /* Find the appropriate D3DFORMAT for the render chroma, the format will be the closest to
     * the requested chroma which is usable by the hardware in an offscreen surface, as they
     * typically support more formats than textures */
    const d3d_format_t *d3dfmt = Direct3DFindFormat(vd, fmt->i_chroma, sys->d3dpp.BackBufferFormat);
    if (!d3dfmt) {
        msg_Err(vd, "surface pixel format is not supported.");
        return VLC_EGENERIC;
    }
    fmt->i_chroma = d3dfmt->fourcc;
    fmt->i_rmask  = d3dfmt->rmask;
    fmt->i_gmask  = d3dfmt->gmask;
    fmt->i_bmask  = d3dfmt->bmask;

    /* We create one picture.
     * It is useless to create more as we can't be used for direct rendering */

    /* Create a surface */
    LPDIRECT3DSURFACE9 surface;
    HRESULT hr = IDirect3DDevice9_CreateOffscreenPlainSurface(d3ddev,
                                                              fmt->i_width,
                                                              fmt->i_height,
                                                              d3dfmt->format,
                                                              D3DPOOL_DEFAULT,
                                                              &surface,
                                                              NULL);
    if (FAILED(hr)) {
        msg_Err(vd, "Failed to create picture surface. (hr=0x%lx)", hr);
        return VLC_EGENERIC;
    }
    /* fill surface with black color */
    IDirect3DDevice9_ColorFill(d3ddev, surface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0));

    /* Create the associated picture */
    picture_resource_t *rsc = &sys->resource;
    rsc->p_sys = malloc(sizeof(*rsc->p_sys));
    if (!rsc->p_sys) {
        IDirect3DSurface9_Release(surface);
        return VLC_ENOMEM;
    }
    rsc->p_sys->surface = surface;
    rsc->p_sys->fallback = NULL;
    for (int i = 0; i < PICTURE_PLANE_MAX; i++) {
        rsc->p[i].p_pixels = NULL;
        rsc->p[i].i_pitch = 0;
        rsc->p[i].i_lines = fmt->i_height / (i > 0 ? 2 : 1);
    }
    picture_t *picture = picture_NewFromResource(fmt, rsc);
    if (!picture) {
        IDirect3DSurface9_Release(surface);
        free(rsc->p_sys);
        return VLC_ENOMEM;
    }

    /* Wrap it into a picture pool */
    picture_pool_configuration_t pool_cfg;
    memset(&pool_cfg, 0, sizeof(pool_cfg));
    pool_cfg.picture_count = 1;
    pool_cfg.picture       = &picture;
    pool_cfg.lock          = Direct3DLockSurface;
    pool_cfg.unlock        = Direct3DUnlockSurface;

    sys->pool = picture_pool_NewExtended(&pool_cfg);
    if (!sys->pool) {
        picture_Release(picture);
        IDirect3DSurface9_Release(surface);
        return VLC_ENOMEM;
    }
    return VLC_SUCCESS;
}
/**
 * It destroys the pool of picture and its resources.
 */
static void Direct3DDestroyPool(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool) {
        picture_resource_t *rsc = &sys->resource;
        IDirect3DSurface9_Release(rsc->p_sys->surface);
        if (rsc->p_sys->fallback)
            picture_Release(rsc->p_sys->fallback);
        picture_pool_Delete(sys->pool);
    }
    sys->pool = NULL;
}

/**
 * It allocates and initializes the resources needed to render the scene.
 */
static int Direct3DCreateScene(vout_display_t *vd, const video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    LPDIRECT3DDEVICE9       d3ddev = sys->d3ddev;
    HRESULT hr;

    /*
     * Create a texture for use when rendering a scene
     * for performance reason, texture format is identical to backbuffer
     * which would usually be a RGB format
     */
    LPDIRECT3DTEXTURE9 d3dtex;
    hr = IDirect3DDevice9_CreateTexture(d3ddev,
                                        fmt->i_width,
                                        fmt->i_height,
                                        1,
                                        D3DUSAGE_RENDERTARGET,
                                        sys->d3dpp.BackBufferFormat,
                                        D3DPOOL_DEFAULT,
                                        &d3dtex,
                                        NULL);
    if (FAILED(hr)) {
        msg_Err(vd, "Failed to create texture. (hr=0x%lx)", hr);
        return VLC_EGENERIC;
    }

    /*
    ** Create a vertex buffer for use when rendering scene
    */
    LPDIRECT3DVERTEXBUFFER9 d3dvtc;
    hr = IDirect3DDevice9_CreateVertexBuffer(d3ddev,
                                             sizeof(CUSTOMVERTEX)*4,
                                             D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
                                             D3DFVF_CUSTOMVERTEX,
                                             D3DPOOL_DEFAULT,
                                             &d3dvtc,
                                             NULL);
    if (FAILED(hr)) {
        msg_Err(vd, "Failed to create vertex buffer. (hr=0x%lx)", hr);
        IDirect3DTexture9_Release(d3dtex);
        return VLC_EGENERIC;
    }

    LPDIRECT3DSURFACE9 d3dleft_surface;
    hr = IDirect3DTexture9_GetSurfaceLevel(d3dtex, 0, &d3dleft_surface);
    if (FAILED(hr)) {
        msg_Err(vd, "Failed to get surface of left texture. (hr=0x%lx)", hr);
        IDirect3DTexture9_Release(d3dtex);
        IDirect3DVertexBuffer9_Release(d3dvtc);
        return VLC_EGENERIC;
    }

    if (sys->use_nvidia_3d)
    {
        /* secondary texture for right eye */
        LPDIRECT3DTEXTURE9 d3dtex_right;
        hr = IDirect3DDevice9_CreateTexture(d3ddev,
                                            fmt->i_width,
                                            fmt->i_height,
                                            1,
                                            D3DUSAGE_RENDERTARGET,
                                            sys->d3dpp.BackBufferFormat,
                                            D3DPOOL_DEFAULT,
                                            &d3dtex_right,
                                            NULL);
        if (FAILED(hr)) {
            msg_Err(vd, "Failed to create secondary texture. (hr=0x%lx)", hr);
            IDirect3DSurface9_Release(d3dleft_surface);
            IDirect3DTexture9_Release(d3dtex);
            IDirect3DVertexBuffer9_Release(d3dvtc);
            return VLC_EGENERIC;
        }

        /* nvidia stereo surface */
        LPDIRECT3DSURFACE9 d3dnv_surface;
        hr = IDirect3DDevice9_CreateRenderTarget(d3ddev,
                                                 sys->rect_display.right * 2,
                                                 sys->rect_display.bottom + 1,
                                                 D3DFMT_A8R8G8B8,
                                                 D3DMULTISAMPLE_NONE,
                                                 0,
                                                 TRUE,
                                                 &d3dnv_surface,
                                                 NULL);
        if (FAILED(hr)) {
            msg_Err(vd, "Failed to create stereo surface. (hr=0x%lx)", hr);
            IDirect3DTexture9_Release(d3dtex_right);
            IDirect3DSurface9_Release(d3dleft_surface);
            IDirect3DTexture9_Release(d3dtex);
            IDirect3DVertexBuffer9_Release(d3dvtc);
            return VLC_EGENERIC;
        }                                       

        LPDIRECT3DSURFACE9 d3dright_surface;
        hr = IDirect3DTexture9_GetSurfaceLevel(d3dtex_right, 0, &d3dright_surface);
        if (FAILED(hr)) {
            msg_Err(vd, "Failed to get surface of right texture. (hr=0x%lx)", hr);
            IDirect3DSurface9_Release(d3dnv_surface);
            IDirect3DTexture9_Release(d3dtex_right);
            IDirect3DSurface9_Release(d3dleft_surface);
            IDirect3DTexture9_Release(d3dtex);
            IDirect3DVertexBuffer9_Release(d3dvtc);
            return VLC_EGENERIC;
        }                                             
       

        LPDIRECT3DSURFACE9 d3dback_buffer;
        hr = IDirect3DDevice9_GetBackBuffer(d3ddev,
                                            0,
                                            0,
                                            D3DBACKBUFFER_TYPE_MONO,
                                            &d3dback_buffer);
        if (FAILED(hr)) {
            msg_Err(vd, "Failed to get device back buffer. (hr=0x%lx)", hr);
            IDirect3DSurface9_Release(d3dright_surface);
            IDirect3DSurface9_Release(d3dleft_surface);
            IDirect3DSurface9_Release(d3dnv_surface);
            IDirect3DTexture9_Release(d3dtex_right);
            IDirect3DTexture9_Release(d3dtex);
            IDirect3DVertexBuffer9_Release(d3dvtc);
            return VLC_EGENERIC;
        }                        

        sys->d3dtex_right = d3dtex_right;
        sys->d3dnv_surface = d3dnv_surface;
        sys->d3dback_buffer = d3dback_buffer;
        sys->d3dright_surface = d3dright_surface;

        sys->nvRectLeft.left = 0;
        sys->nvRectLeft.top = 0;
        sys->nvRectLeft.right = fmt->i_width;
        sys->nvRectLeft.bottom = fmt->i_height;

        sys->nvRectRight.left = fmt->i_width;
        sys->nvRectRight.top = 0;
        sys->nvRectRight.right = fmt->i_width * 2;
        sys->nvRectRight.bottom = fmt->i_height;

        sys->nvRectHeader.left = 0;
        sys->nvRectHeader.top = 0;
        sys->nvRectHeader.right = fmt->i_width * 2;
        sys->nvRectHeader.bottom = fmt->i_height + 1;
    }


    /* */
    sys->d3dtex = d3dtex;
    sys->d3dvtc = d3dvtc;
    sys->d3dregion_count = 0;
    sys->d3dregion       = NULL;
    sys->d3dleft_surface = d3dleft_surface;
    sys->left_tex_filled = false;
    sys->right_tex_filled = false;

    sys->clear_scene = true;

    // Texture coordinates outside the range [0.0, 1.0] are set
    // to the texture color at 0.0 or 1.0, respectively.
    IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    // Set linear filtering quality
    if (sys->d3dcaps.TextureFilterCaps & D3DPTFILTERCAPS_MINFLINEAR) {
        msg_Dbg(vd, "Using D3DTEXF_LINEAR for minification");
        IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    } else {
        msg_Dbg(vd, "Using D3DTEXF_POINT for minification");
        IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    }
    if (sys->d3dcaps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFLINEAR) {
        msg_Dbg(vd, "Using D3DTEXF_LINEAR for magnification");
        IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    } else {
        msg_Dbg(vd, "Using D3DTEXF_POINT for magnification");
        IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    }

    // set maximum ambient light
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_AMBIENT, D3DCOLOR_XRGB(255,255,255));

    // Turn off culling
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_CULLMODE, D3DCULL_NONE);

    // Turn off the zbuffer
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ZENABLE, D3DZB_FALSE);

    // Turn off lights
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_LIGHTING, FALSE);

    // Enable dithering
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_DITHERENABLE, TRUE);

    // disable stencil
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_STENCILENABLE, FALSE);

    // manage blending
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHABLENDENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);

    if (sys->d3dcaps.AlphaCmpCaps & D3DPCMPCAPS_GREATER) {
        IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHATESTENABLE,TRUE);
        IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHAREF, 0x00);
        IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHAFUNC,D3DCMP_GREATER);
    }

    // Set texture states
    IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_COLORARG1,D3DTA_TEXTURE);

    IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);

    msg_Dbg(vd, "Direct3D scene created successfully");

    return VLC_SUCCESS;
}

/**
 * It releases the scene resources.
 */
static void Direct3DDestroyScene(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3DDeleteRegions(sys->d3dregion_count, sys->d3dregion);


    if (sys->use_nvidia_3d)
    {
        LPDIRECT3DSURFACE9 d3dright_surface = sys->d3dright_surface;
        if(d3dright_surface)
            IDirect3DSurface9_Release(d3dright_surface);
        sys->d3dright_surface = NULL;

        LPDIRECT3DTEXTURE9 d3dtex_right = sys->d3dtex_right;
        if(d3dtex_right)
            IDirect3DTexture9_Release(d3dtex_right);
        sys->d3dtex_right = NULL;

        LPDIRECT3DSURFACE9 d3dnv_surface = sys->d3dnv_surface;
        if(d3dnv_surface)
            IDirect3DSurface9_Release(d3dnv_surface);
        sys->d3dnv_surface = NULL;

        LPDIRECT3DSURFACE9 d3dback_buffer = sys->d3dback_buffer;
        if(d3dback_buffer)
            IDirect3DSurface9_Release(d3dback_buffer);
        sys->d3dback_buffer = NULL;
    }


    LPDIRECT3DSURFACE9 d3dleft_surface = sys->d3dleft_surface;
    if(d3dleft_surface)
        IDirect3DSurface9_Release(d3dleft_surface);
    sys->d3dleft_surface = NULL;


    LPDIRECT3DVERTEXBUFFER9 d3dvtc = sys->d3dvtc;
    if (d3dvtc)
        IDirect3DVertexBuffer9_Release(d3dvtc);

    LPDIRECT3DTEXTURE9 d3dtex = sys->d3dtex;
    if (d3dtex)
        IDirect3DTexture9_Release(d3dtex);

    sys->d3dvtc = NULL;
    sys->d3dtex = NULL;

    sys->d3dregion_count = 0;
    sys->d3dregion       = NULL;

    msg_Dbg(vd, "Direct3D scene released successfully");
}

static void Direct3DSetupVertices(CUSTOMVERTEX *vertices,
                                  const RECT src_full,
                                  const RECT src_crop,
                                  const RECT dst,
                                  int alpha)
{
    const float src_full_width  = src_full.right  - src_full.left;
    const float src_full_height = src_full.bottom - src_full.top;
    vertices[0].x  = dst.left;
    vertices[0].y  = dst.top;
    vertices[0].tu = src_crop.left / src_full_width;
    vertices[0].tv = src_crop.top  / src_full_height;

    vertices[1].x  = dst.right;
    vertices[1].y  = dst.top;
    vertices[1].tu = src_crop.right / src_full_width;
    vertices[1].tv = src_crop.top   / src_full_height;

    vertices[2].x  = dst.right;
    vertices[2].y  = dst.bottom;
    vertices[2].tu = src_crop.right  / src_full_width;
    vertices[2].tv = src_crop.bottom / src_full_height;

    vertices[3].x  = dst.left;
    vertices[3].y  = dst.bottom;
    vertices[3].tu = src_crop.left   / src_full_width;
    vertices[3].tv = src_crop.bottom / src_full_height;

    for (int i = 0; i < 4; i++) {
        /* -0.5f is a "feature" of DirectX and it seems to apply to Direct3d also */
        /* http://www.sjbrown.co.uk/2003/05/01/fix-directx-rasterisation/ */
        vertices[i].x -= 0.5;
        vertices[i].y -= 0.5;

        vertices[i].z       = 0.0f;
        vertices[i].rhw     = 1.0f;
        vertices[i].diffuse = D3DCOLOR_ARGB(alpha, 255, 255, 255);
    }
}

/**
 * It copies picture surface into a texture and setup the associated d3d_region_t.
 */
static int Direct3DImportPicture(vout_display_t *vd,
                                 d3d_region_t *region,
                                 LPDIRECT3DSURFACE9 source,
                                 int i_eye)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

    if (!source) {
        msg_Dbg(vd, "no surface to render ?");
        return VLC_EGENERIC;
    }

	/* mask out all flags so i_eye only stores the eye number */
	i_eye &= STEREO_EYE_MASK;

    /* msg_Dbg(vd, "Receiving eye (i_eye=%i)", i_eye); */

    /* retrieve texture top-level surface */
    LPDIRECT3DSURFACE9 destination;
    /* either 2d, left, or no 3d */
    if(!sys->use_nvidia_3d || i_eye < 2 )
    {
        destination = sys->d3dleft_surface;
        if(i_eye == 0)
        {
            /* clear both eyes if we just received a 2d image
               so we go back to drawing in 2d */
            sys->right_tex_filled = false;
            sys->left_tex_filled = false;
        }
        else
            sys->left_tex_filled = true;
    }
    else
    {
        destination = sys->d3dright_surface;
        sys->right_tex_filled = true;
    }

    /* Copy picture surface into texture surface
     * color space conversion happen here */
    hr = IDirect3DDevice9_StretchRect(sys->d3ddev, source, NULL, destination, NULL, D3DTEXF_NONE);

    if (FAILED(hr)) {
        msg_Dbg(vd, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        return VLC_EGENERIC;
    }

    /* */
/*    if(sys->left_tex_filled && sys->right_tex_filled &&
        sys->use_nvidia_3d)
        return VLC_SUCCESS;*/

    /* only needed for 3d, but if we rapidly turn off 2d we require this to exist */
    region->texture = sys->d3dtex;
    Direct3DSetupVertices(region->vertex,
                          vd->sys->rect_src,
                          vd->sys->rect_src_clipped,
                          vd->sys->rect_dest_clipped, 255);
    return VLC_SUCCESS;
}

static void Direct3DDeleteRegions(int count, d3d_region_t *region)
{
    for (int i = 0; i < count; i++) {
        if (region[i].texture)
            IDirect3DTexture9_Release(region[i].texture);
    }
    free(region);
}

static void Direct3DImportSubpicture(vout_display_t *vd,
                                     int *count_ptr, d3d_region_t **region,
                                     subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    int count = 0;
    for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next)
        count++;

    *count_ptr = count;
    *region    = calloc(count, sizeof(**region));
    if (*region == NULL) {
        *count_ptr = 0;
        return;
    }

    int i = 0;
    for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next, i++) {
        d3d_region_t *d3dr = &(*region)[i];
        HRESULT hr;

        d3dr->texture = NULL;
        for (int j = 0; j < sys->d3dregion_count; j++) {
            d3d_region_t *cache = &sys->d3dregion[j];
            if (cache->texture &&
                cache->format == D3DFMT_A8R8G8B8 &&
                cache->width  == r->fmt.i_visible_width &&
                cache->height == r->fmt.i_visible_height) {
                msg_Dbg(vd, "Reusing %dx%d texture for OSD",
                        cache->width, cache->height);
                *d3dr = *cache;
                memset(cache, 0, sizeof(*cache));
            }
        }
        if (!d3dr->texture) {
            d3dr->format = D3DFMT_A8R8G8B8;
            d3dr->width  = r->fmt.i_visible_width;
            d3dr->height = r->fmt.i_visible_height;
            hr = IDirect3DDevice9_CreateTexture(sys->d3ddev,
                                                d3dr->width, d3dr->height,
                                                1,
                                                D3DUSAGE_DYNAMIC,
                                                d3dr->format,
                                                D3DPOOL_DEFAULT,
                                                &d3dr->texture,
                                                NULL);
            if (FAILED(hr)) {
                d3dr->texture = NULL;
                msg_Err(vd, "Failed to create %dx%d texture for OSD",
                        d3dr->width, d3dr->height);
                continue;
            }
            msg_Dbg(vd, "Created %dx%d texture for OSD",
                    r->fmt.i_visible_width, r->fmt.i_visible_height);
        }

        D3DLOCKED_RECT lock;
        hr = IDirect3DTexture9_LockRect(d3dr->texture, 0, &lock, NULL, 0);
        if (SUCCEEDED(hr)) {
            uint8_t *dst_data  = lock.pBits;
            int      dst_pitch = lock.Pitch;
            for (unsigned y = 0; y < r->fmt.i_visible_height; y++) {
                int copy_pitch = __MIN(dst_pitch, r->p_picture->p->i_visible_pitch);
                memcpy(&dst_data[y * dst_pitch],
                       &r->p_picture->p->p_pixels[y * r->p_picture->p->i_pitch],
                       copy_pitch);
            }
            hr = IDirect3DTexture9_UnlockRect(d3dr->texture, 0);
            if (FAILED(hr))
                msg_Err(vd, "Failed to unlock the texture");
        } else {
            msg_Err(vd, "Failed to lock the texture");
        }

        /* Map the subpicture to sys->rect_dest */
        RECT src;
        src.left   = 0;
        src.right  = src.left + r->fmt.i_visible_width;
        src.top    = 0;
        src.bottom = src.top  + r->fmt.i_visible_height;

        const RECT video = sys->rect_dest;
        const float scale_w = (float)(video.right  - video.left) / subpicture->i_original_picture_width;
        const float scale_h = (float)(video.bottom - video.top)  / subpicture->i_original_picture_height;

        RECT dst;
        dst.left   = video.left + scale_w * r->i_x,
        dst.right  = dst.left + scale_w * r->fmt.i_visible_width,
        dst.top    = video.top  + scale_h * r->i_y,
        dst.bottom = dst.top  + scale_h * r->fmt.i_visible_height,
        Direct3DSetupVertices(d3dr->vertex,
                              src, src, dst,
                              subpicture->i_alpha * r->i_alpha / 255);
    }
}

static int Direct3DRenderRegion(vout_display_t *vd,
                                d3d_region_t *region)
{
    vout_display_sys_t *sys = vd->sys;

    LPDIRECT3DDEVICE9 d3ddev = vd->sys->d3ddev;
    bool drawInStereo;

    /* check if we can draw in stereo */    
    if(sys->left_tex_filled && sys->right_tex_filled &&
        sys->use_nvidia_3d)
    {
        /* check to see if stereo is turned on */
        unsigned char stereoOn;
        if(NvAPI_Stereo_IsActivated(sys->nvStereoHandle, &stereoOn) == NVAPI_OK)
		{
			if(!stereoOn)
			{
				/* activate 3d vision because it's not yet turned on */
				NvAPI_Stereo_Activate(sys->nvStereoHandle);
			}

			/* check again */
			if(NvAPI_Stereo_IsActivated(sys->nvStereoHandle, &stereoOn) == NVAPI_OK)
			{
				drawInStereo = (bool)stereoOn;
			}
			else
				drawInStereo = false;
		}
        else
            drawInStereo = false;
    }
    else
	{
		if(sys->use_nvidia_3d)
		{
			/* check if stereo is on */
            unsigned char stereoOn;
			if(NvAPI_Stereo_IsActivated(sys->nvStereoHandle, &stereoOn) == NVAPI_OK)
			{
				if(stereoOn)
				{
					/* deactivate 3d vision because it's turned on (saves wireless glass's power
					  to disable when not viewing 3d) */
					NvAPI_Stereo_Deactivate(sys->nvStereoHandle);
				}
			}

		}
        drawInStereo = false;
	}

    if(drawInStereo)
    {
        /* draw stereoscopic image */

        RECT dest = sys->rect_dest_clipped;
        /* work around for anyone reading this code:
            showing right image on left half of stereo surface, then showing
            left image on right half of stereo surface, and using the
            DXNV_SWAP_EYES flag

            For some reason stereomode wouldn't work correctly without
            that flag.
        */
        /* copy left and right image to stereo surface*/
        IDirect3DDevice9_StretchRect(sys->d3ddev, sys->d3dright_surface,
            &sys->nvRectLeft/*&sys->rect_src_clipped*/, sys->d3dnv_surface, &dest,
            D3DTEXF_NONE);

        dest.left = sys->rect_display.right;
        dest.right += sys->rect_display.right;
        IDirect3DDevice9_StretchRect(sys->d3ddev, sys->d3dleft_surface,
            &sys->nvRectLeft/*&sys->rect_src_clipped*/, sys->d3dnv_surface, &dest,
            D3DTEXF_NONE);

        /* write in the nvidia header */
        D3DLOCKED_RECT lr;
        IDirect3DSurface9_LockRect(sys->d3dnv_surface, &lr, NULL, 0);
        LPNVSTEREOIMAGEHEADER pSIH =
            (LPNVSTEREOIMAGEHEADER)(((unsigned char *) lr.pBits) +
            (lr.Pitch * sys->rect_display.bottom));

        /* write signature into stereo surface */
        pSIH->dwSignature = NVSTEREO_IMAGE_SIGNATURE;
        pSIH->dwBPP = 32;
        pSIH->dwFlags = DXNV_SWAP_EYES;
        pSIH->dwWidth = sys->rect_display.right * 2; 
        pSIH->dwHeight = sys->rect_display.bottom;

        IDirect3DSurface9_UnlockRect(sys->d3dnv_surface);

        /* copy stereo surface into back buffer */
        RECT stereo_source;
        stereo_source.top = 0;
        stereo_source.left = 0;
        stereo_source.right = sys->rect_display.right * 2;
        stereo_source.bottom = sys->rect_display.bottom + 1;
        IDirect3DDevice9_StretchRect(sys->d3ddev, sys->d3dnv_surface,
            &stereo_source, sys->d3dback_buffer,
            &sys->rect_display, D3DTEXF_NONE);
    }
    else
    {
        /* draw 2d image */

        LPDIRECT3DVERTEXBUFFER9 d3dvtc = sys->d3dvtc;
        LPDIRECT3DTEXTURE9      d3dtex = region->texture;

        HRESULT hr;

        /* Import vertices */
        void *vertex;
        hr = IDirect3DVertexBuffer9_Lock(d3dvtc, 0, 0, &vertex, D3DLOCK_DISCARD);
        if (FAILED(hr)) {
            msg_Dbg(vd, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
            return -1;
        }
        memcpy(vertex, region->vertex, sizeof(region->vertex));
        hr = IDirect3DVertexBuffer9_Unlock(d3dvtc);
        if (FAILED(hr)) {
            msg_Dbg(vd, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
            return -1;
        }

        // Setup our texture. Using textures introduces the texture stage states,
        // which govern how textures get blended together (in the case of multiple
        // textures) and lighting information. In this case, we are modulating
        // (blending) our texture with the diffuse color of the vertices.
        hr = IDirect3DDevice9_SetTexture(d3ddev, 0, (LPDIRECT3DBASETEXTURE9)d3dtex);
        if (FAILED(hr)) {
            msg_Dbg(vd, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
            return -1;
        }

        // Render the vertex buffer contents
        hr = IDirect3DDevice9_SetStreamSource(d3ddev, 0, d3dvtc, 0, sizeof(CUSTOMVERTEX));
        if (FAILED(hr)) {
            msg_Dbg(vd, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
            return -1;
        }

        // we use FVF instead of vertex shader
        hr = IDirect3DDevice9_SetFVF(d3ddev, D3DFVF_CUSTOMVERTEX);
        if (FAILED(hr)) {
            msg_Dbg(vd, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
            return -1;
        }

        // draw rectangle
        hr = IDirect3DDevice9_DrawPrimitive(d3ddev, D3DPT_TRIANGLEFAN, 0, 2);
        if (FAILED(hr)) {
            msg_Dbg(vd, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
            return -1;
        }
    }
    return 0;
}

/**
 * It renders the scene.
 *
 * This function is intented for higher end 3D cards, with pixel shader support
 * and at least 64 MiB of video RAM.
 */
static void Direct3DRenderScene(vout_display_t *vd,
                                d3d_region_t *picture,
                                int subpicture_count,
                                d3d_region_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    LPDIRECT3DDEVICE9 d3ddev = sys->d3ddev;
    HRESULT hr;

    if (sys->clear_scene) {
        /* Clear the backbuffer and the zbuffer */
        hr = IDirect3DDevice9_Clear(d3ddev, 0, NULL, D3DCLEAR_TARGET,
                                  D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        if (FAILED(hr)) {
            msg_Dbg(vd, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
            return;
        }
        sys->clear_scene = false;
    }

    // Begin the scene
    hr = IDirect3DDevice9_BeginScene(d3ddev);
    if (FAILED(hr)) {
        msg_Dbg(vd, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        return;
    }

    Direct3DRenderRegion(vd, picture);

    if (subpicture_count > 0)
        IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHABLENDENABLE, TRUE);
    for (int i = 0; i < subpicture_count; i++) {
        d3d_region_t *r = &subpicture[i];
        if (r->texture)
            Direct3DRenderRegion(vd, r);
    }
    if (subpicture_count > 0)
        IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHABLENDENABLE, FALSE);

    // End the scene
    hr = IDirect3DDevice9_EndScene(d3ddev);
    if (FAILED(hr)) {
        msg_Dbg(vd, "%s:%d (hr=0x%0lX)", __FUNCTION__, __LINE__, hr);
        return;
    }
}

/*****************************************************************************
 * DesktopCallback: desktop mode variable callback
 *****************************************************************************/
static int DesktopCallback(vlc_object_t *object, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(psz_cmd);
    VLC_UNUSED(oldval);
    VLC_UNUSED(p_data);

    vlc_mutex_lock(&sys->lock);
    const bool ch_desktop = !sys->desktop_requested != !newval.b_bool;
    sys->ch_desktop |= ch_desktop;
    sys->desktop_requested = newval.b_bool;
    vlc_mutex_unlock(&sys->lock);

    /* FIXME we should have a way to export variable to be saved */
    if (ch_desktop) {
        playlist_t *p_playlist = pl_Get(vd);
        /* Modify playlist as well because the vout might have to be
         * restarted */
        var_Create(p_playlist, "direct3d-desktop", VLC_VAR_BOOL);
        var_SetBool(p_playlist, "direct3d-desktop", newval.b_bool);
    }
    return VLC_SUCCESS;
}
