/*****************************************************************************
 * stereoscopy.c : "steroscopy" video filter - splits one image into 2 eyes
 *****************************************************************************
 * Copyright (C) 2011 the VideoLAN team
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_rand.h>

#include <vlc_filter.h>
#include "filter_picture.h"
#include "stereoscopy.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static picture_t *Filter( filter_t *, picture_t * );
static void Destroy( vlc_object_t *p_this );

/* based on those from extract.c */
static void get_red_from_yuv420( picture_t *, picture_t *, int, int, int );
static void get_red_from_yuv422( picture_t *, picture_t *, int, int, int );
static void get_cyan_from_yuv420( picture_t *, picture_t *, int, int, int );
static void get_cyan_from_yuv422( picture_t *, picture_t *, int, int, int );
static void get_green_from_yuv420( picture_t *, picture_t *, int, int, int );
static void get_green_from_yuv422( picture_t *, picture_t *, int, int, int );
static void get_magenta_from_yuv420( picture_t *, picture_t *, int, int, int );
static void get_magenta_from_yuv422( picture_t *, picture_t *, int, int, int );
static void get_blue_from_yuv420( picture_t *, picture_t *, int, int, int );
static void get_blue_from_yuv422( picture_t *, picture_t *, int, int, int );
static void get_yellow_from_yuv420( picture_t *, picture_t *, int, int, int );
static void get_yellow_from_yuv422( picture_t *, picture_t *, int, int, int );

static void get_red_grayscale_from_yuv420( picture_t *, picture_t *, int, int,
    int );
static void get_red_grayscale_from_yuv422( picture_t *, picture_t *, int, int,
    int );
static void get_cyan_grayscale_from_yuv420( picture_t *, picture_t *, int, int,
    int );
static void get_cyan_grayscale_from_yuv422( picture_t *, picture_t *, int, int,
    int );
static void get_green_grayscale_from_yuv420( picture_t *, picture_t *, int, int,
    int );
static void get_green_grayscale_from_yuv422( picture_t *, picture_t *, int, int,
    int );
static void get_magenta_grayscale_from_yuv420( picture_t *, picture_t *, int,
    int, int );
static void get_magenta_grayscale_from_yuv422( picture_t *, picture_t *, int,
    int, int );
static void get_blue_grayscale_from_yuv420( picture_t *, picture_t *, int, int,
    int );
static void get_blue_grayscale_from_yuv422( picture_t *, picture_t *, int, int,
    int );
static void get_yellow_grayscale_from_yuv420( picture_t *, picture_t *, int,
    int, int );
static void get_yellow_grayscale_from_yuv422( picture_t *, picture_t *, int,
    int, int );

static void get_cyan_fill_from_yuv420( picture_t *, picture_t *, int, int,
   int );
static void get_cyan_fill_from_yuv422( picture_t *, picture_t *, int, int,
   int );
static void get_magenta_fill_from_yuv420( picture_t *, picture_t *, int, int,
   int );
static void get_magenta_fill_from_yuv422( picture_t *, picture_t *, int, int,
   int );
static void get_yellow_fill_from_yuv420( picture_t *, picture_t *, int, int,
   int );
static void get_yellow_fill_from_yuv422( picture_t *, picture_t *, int, int,
  int );

static void get_lefthalf_from_yuv420( picture_t *, picture_t *, int, int,
  int );
static void get_lefthalf_from_yuv422( picture_t *, picture_t *, int, int,
  int );
static void get_righthalf_from_yuv420( picture_t *, picture_t *, int, int,
  int );
static void get_righthalf_from_yuv422( picture_t *, picture_t *, int, int,
  int );
static void get_tophalf_from_yuv420( picture_t *, picture_t *, int, int,
  int );
static void get_tophalf_from_yuv422( picture_t *, picture_t *, int, int,
  int );
static void get_bottomhalf_from_yuv420( picture_t *, picture_t *, int, int,
  int );
static void get_bottomhalf_from_yuv422( picture_t *, picture_t *, int, int,
  int );

#define FILTER_PREFIX "stereoscopy-"

#define LEFT_EYE_METHOD_TEXT N_("Left eye stereoscopy encoding")
#define RIGHT_EYE_METHOD_TEXT N_("Right eye stereoscopy enconding")
#define LEFT_EYE_METHOD_LONGTEXT N_("Represents the form of steroscopic "\
                                    "encoding used for the image of the "\
                                    "respective eye. " \
                                    "0 - The image is intended to be 2D " \
                                    "and should be displayed as such. " \
                                    "1000 - The eye is encoded in the " \
                                    "blue channel. " \
                                    "1001 - The eye is encoded in the " \
                                    "green and blue channels. " \
                                    "1002 - The eye is encoded in the " \
                                    "green channel. " \
                                    "1003 - The eye is encoded in the " \
                                    "red and blue channels. " \
                                    "1004 - The eye is encoded in the " \
                                    "red channel. " \
                                    "1005 - The eye is encoded in the " \
                                    "red and green channels. " \
                                    "1010 - The eye is encoded in the " \
                                    "blue channel as a gray scale image. " \
                                    "1011 - The eye is encoded in the " \
                                    "green and blue channels as a gray " \
                                    "scale image. " \
                                    "1012 - The eye is encoded in the " \
                                    "green channel as a gray scale image. " \
                                    "1013 - The eye is encoded in the " \
                                    "red and blue channels as a gray " \
                                    "scale image. " \
                                    "1014 - The eye is encoded in the " \
                                    "red channel as a gray scale image. " \
                                    "1015 - The eye is encoded in the " \
                                    "red and green channels as a gray " \
                                    "scale image. " \
                                    "1021 - The eye is encoded in the " \
                                    "green and blue channels interpolating" \
                                    "the red channel. " \
                                    "1023 - The eye is encoded in the " \
                                    "red and blue channels interpolating" \
                                    "the green channel. " \
                                    "1025 - The eye is encoded in the " \
                                    "red and green channels interpolating" \
                                    "the blue channel. " \
                                    "2000 - The eye is encoded in the " \
                                    "left half of the image. " \
                                    "2001 - The eye is encoded in the " \
                                    "right half of the image. " \
                                    "2002 - The eye is encoded in the " \
                                    "bottom half of the image. " \
                                    "2003 - The eye is encoded in the " \
                                    "top half of the image. ")
#define RIGHT_EYE_METHOD_LONGTEXT LEFT_EYE_METHOD_LONGTEXT

struct filter_sys_t
{
    int    i_leftEyeMethod;      /* stereoscopy encoding for left eye */
    int    i_rightEyeMethod;     /* stereoscopy encoding for right eye */
    bool   b_leftEyeLast;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Stereoscopy video filter") )
    set_shortname( N_( "Stereoscopy" ))
    set_capability( "video filter2", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_shortcut( "stereoscopy" )
    set_callbacks( Create, Destroy )

    add_integer( FILTER_PREFIX "left", STEREOSCOPY_ANAGLYPH_RED,
                 LEFT_EYE_METHOD_TEXT, LEFT_EYE_METHOD_LONGTEXT, false )
    add_integer( FILTER_PREFIX "right", STEREOSCOPY_ANAGLYPH_CYAN,
                 RIGHT_EYE_METHOD_TEXT, RIGHT_EYE_METHOD_LONGTEXT, false )
vlc_module_end ()

/*****************************************************************************
 * Create: initialises stereoscopy video filter
 *****************************************************************************
 * This function allocates and initializes a Distort vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter;
    filter_sys_t *p_sys;

    p_filter = (filter_t *)p_this;
    p_filter->pf_video_filter = Filter;
    p_sys = p_filter->p_sys = malloc(sizeof(*p_sys));

    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->i_leftEyeMethod = var_InheritInteger( p_filter,
        FILTER_PREFIX "left" );
    p_sys->i_rightEyeMethod = var_InheritInteger( p_filter,
        FILTER_PREFIX "right" );
    p_sys->b_leftEyeLast = false;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy Crop video thread output method
 *****************************************************************************
 * Terminate an output method created by CropCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    free( p_filter->p_sys );
}


/*****************************************************************************
 * DecodeImageYUV: extracts an image from a greater RGB image
 *****************************************************************************
 * Extracts the image for a single eye from a stereoscopic image containing
 * both eyes in YUV420 or YUV422 format.
 *****************************************************************************/
static picture_t *DecodeImageYUV( filter_t *p_filter, picture_t *p_inpic,
    int i_method, bool yuv422)
{
    picture_t *p_output = filter_NewPicture( p_filter );

    if( !p_output )
        return NULL;

    switch( i_method) 
    {
        case STEREOSCOPY_ANAGLYPH_BLUE:
            if(yuv422)
                get_blue_from_yuv422(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            else
                get_blue_from_yuv420(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_ANAGLYPH_CYAN:
            if(yuv422)
                get_cyan_from_yuv422(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            else
                get_cyan_from_yuv420(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_ANAGLYPH_GREEN:
            if(yuv422)
                get_green_from_yuv422(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            else
                get_green_from_yuv420(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_ANAGLYPH_MAGENTA:
            if(yuv422)
                get_magenta_from_yuv422(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            else
                get_magenta_from_yuv420(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_ANAGLYPH_RED:
            if(yuv422)
                get_red_from_yuv422(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            else
                get_red_from_yuv420(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_ANAGLYPH_YELLOW:
            if(yuv422)
                get_yellow_from_yuv422(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            else
                get_yellow_from_yuv420(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_ANAGLYPH_BLUE_GRAY:
            if(yuv422)
                get_blue_grayscale_from_yuv422(p_inpic,
                    p_output, Y_PLANE, U_PLANE, V_PLANE);
            else
                get_blue_grayscale_from_yuv420(p_inpic,
                    p_output, Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_ANAGLYPH_CYAN_GRAY:
            if(yuv422)
                get_cyan_grayscale_from_yuv422(p_inpic,
                    p_output, Y_PLANE, U_PLANE, V_PLANE);
            else
                get_cyan_grayscale_from_yuv420(p_inpic,
                    p_output, Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_ANAGLYPH_GREEN_GRAY:
            if(yuv422)
                get_green_grayscale_from_yuv422(p_inpic,
                    p_output, Y_PLANE, U_PLANE, V_PLANE);
            else
                get_green_grayscale_from_yuv420(p_inpic,
                    p_output, Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_ANAGLYPH_MAGENTA_GRAY:
            if(yuv422)
                get_magenta_grayscale_from_yuv422(p_inpic,
                    p_output, Y_PLANE, U_PLANE, V_PLANE);
            else
                get_magenta_grayscale_from_yuv420(p_inpic,
                    p_output, Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_ANAGLYPH_RED_GRAY:
            if(yuv422)
                get_red_grayscale_from_yuv422(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            else
                get_red_grayscale_from_yuv420(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_ANAGLYPH_YELLOW_GRAY:
            if(yuv422)
                get_yellow_grayscale_from_yuv422(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            else
                get_yellow_grayscale_from_yuv420(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_ANAGLYPH_CYAN_FILL:
            if(yuv422)
                get_cyan_fill_from_yuv422(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            else
                get_cyan_fill_from_yuv420(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_ANAGLYPH_MAGENTA_FILL:
            if(yuv422)
                get_magenta_fill_from_yuv422(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            else
                get_magenta_fill_from_yuv420(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_ANAGLYPH_YELLOW_FILL:
            if(yuv422)
                get_yellow_fill_from_yuv422(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            else
                get_yellow_fill_from_yuv420(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_SIDEBYSIDE_LEFT:
            if(yuv422)
                get_lefthalf_from_yuv422(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            else
                get_lefthalf_from_yuv420(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_SIDEBYSIDE_RIGHT:
            if(yuv422)
                get_righthalf_from_yuv422(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            else
                get_righthalf_from_yuv420(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_SIDEBYSIDE_BOTTOM:
            if(yuv422)
                get_bottomhalf_from_yuv422(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            else
                get_bottomhalf_from_yuv420(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            break;
        case STEREOSCOPY_SIDEBYSIDE_TOP:
            if(yuv422)
                get_tophalf_from_yuv422(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            else
                get_tophalf_from_yuv420(p_inpic, p_output,
                    Y_PLANE, U_PLANE, V_PLANE);
            break;
    }

    picture_CopyProperties( p_output, p_inpic );
    return p_output;
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function takes a single frame and divides the image up into seperate
 * eyes, returning two frames.
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_inpic )
{
    filter_sys_t *p_sys;
    bool yuv422;

    if( !p_inpic ) return NULL;

    if( (p_filter->fmt_in.video.i_height == 0) ||
        (p_filter->fmt_in.video.i_width == 0) )
        return NULL;

    if( (p_filter->fmt_out.video.i_height == 0) ||
        (p_filter->fmt_out.video.i_width == 0) )
        return NULL;

    p_sys = p_filter->p_sys;

    /* already a 3d image so do nothing */
    if( p_sys->i_leftEyeMethod == STEREOSCOPY_2D || p_inpic->i_eye > 0 ||
        p_sys->i_rightEyeMethod == STEREOSCOPY_2D )
        return p_inpic;
    switch( p_inpic->format.i_chroma )
    {
        case VLC_CODEC_I420:
        case VLC_CODEC_J420:
        case VLC_CODEC_YV12:
            yuv422 = false;
            break;
        case VLC_CODEC_I422:
        case VLC_CODEC_J422:
            yuv422 = true;
            break;
        default:
            msg_Warn( p_filter, "Unsupported input chroma (%4.4s)",
                      (char*)&(p_inpic->format.i_chroma) );
            picture_Release( p_inpic );
            return NULL;
    }

    /* CORRECT METHOD BUT COMMENTED OUT WHILE 'DROPPING PICTURES' ISSUE REMAINS

    picture_t *p_leftOut;
    picture_t *p_rightOut;
    p_leftOut = DecodeImageYUV( p_filter, p_inpic, p_sys->i_leftEyeMethod, yuv422);
    if( !p_leftOut )
    {
        msg_Warn( p_filter, "can't get left output picture" );
        picture_Release( p_inpic );
        return NULL;
    }

    p_leftOut->i_eye = 1;

    p_leftOut->p_next = p_rightOut = DecodeImageYUV( p_filter, p_inpic,
        p_sys->i_rightEyeMethod, yuv422);

    if( !p_rightOut )
    {
        msg_Warn( p_filter, "can't get right output picture" );
        picture_Release( p_leftOut );
        picture_Release( p_inpic );
        return NULL;
    }

    p_rightOut->i_eye = 2;

    picture_Release(p_inpic);
    return p_leftOut;
    */

    /* DECODES LEFT AND RIGHT FRAMES ALTERNATIVELY, DROPS A FRAME BUT IS A
       WORKAROUND UNTIL DROPPING PICTURES IS FIXED */

    if(p_sys->b_leftEyeLast)
    {
        p_sys->b_leftEyeLast = false;

        /* do right eye */
        picture_t *p_rightOut = DecodeImageYUV( p_filter, p_inpic,
            p_sys->i_rightEyeMethod, yuv422);

        if( !p_rightOut )
        {
            msg_Warn( p_filter, "can't get right output picture" );
            picture_Release( p_inpic );
            return NULL;
        }

        p_rightOut->i_eye = 2;
        picture_Release( p_inpic );
        return p_rightOut;
    }
    else
    {
        p_sys->b_leftEyeLast = true;

        /* do left eye */
        picture_t *p_leftOut = DecodeImageYUV( p_filter, p_inpic,
            p_sys->i_leftEyeMethod, yuv422);

        if( !p_leftOut )
        {
            msg_Warn( p_filter, "can't get left output picture" );
            picture_Release( p_inpic );
            return NULL;
        }

        p_leftOut->i_eye = 1;
        picture_Release( p_inpic );
        return p_leftOut;
    }
}

/*****************************************************************************
 * Colour conversion methods. These methods are based on those in extract.c.
 *****************************************************************************/
static void get_red_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, r, 0, 0);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, r, 0, 0);
            
            y1in++;
            y2in++;

            y1out++;
            y2out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, r, 0, 0);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, r, 0, 0);

            y1in++;
            y2in++;

            y1out++;
            y2out++;

            uin++;
            vin++;
            uout++;
            vout++;
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_red_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, r, 0, 0);

            y1in++;
            y1out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, r, 0, 0);

            y1in++;
            y1out++;

            uin++;
            vin++;

            uout++;
            vout++;
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_cyan_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, 0, g, b);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, 0, g, b);
            
            y1in++;
            y2in++;

            y1out++;
            y2out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, 0, g, b);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, 0, g, b);

            y1in++;
            y2in++;

            y1out++;
            y2out++;

            uin++;
            vin++;
            uout++;
            vout++;
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_cyan_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, 0, g, b);

            y1in++;
            y1out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, 0, g, b);

            y1in++;
            y1out++;

            uin++;
            vin++;

            uout++;
            vout++;
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_green_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, 0, g, 0);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, 0, g, 0);
            
            y1in++;
            y2in++;

            y1out++;
            y2out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, 0, g, 0);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, 0, g, 0);

            y1in++;
            y2in++;

            y1out++;
            y2out++;

            uin++;
            vin++;
            uout++;
            vout++;
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_green_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                   int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, 0, g, 0);

            y1in++;
            y1out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, 0, g, 0);

            y1in++;
            y1out++;

            uin++;
            vin++;

            uout++;
            vout++;
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_magenta_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, r, 0, b);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, r, 0, b);
            
            y1in++;
            y2in++;

            y1out++;
            y2out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, r, 0, b);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, r, 0, b);

            y1in++;
            y2in++;

            y1out++;
            y2out++;

            uin++;
            vin++;
            uout++;
            vout++;
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_magenta_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, r, 0, b);

            y1in++;
            y1out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, r, 0, b);

            y1in++;
            y1out++;

            uin++;
            vin++;

            uout++;
            vout++;
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_blue_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, 0, 0, b);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, 0, 0, b);
            
            y1in++;
            y2in++;

            y1out++;
            y2out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, 0, 0, b);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, 0, 0, b);

            y1in++;
            y2in++;

            y1out++;
            y2out++;

            uin++;
            vin++;
            uout++;
            vout++;
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_blue_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, 0, 0, b);

            y1in++;
            y1out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, 0, 0, b);

            y1in++;
            y1out++;

            uin++;
            vin++;

            uout++;
            vout++;
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_yellow_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, r, g, 0);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, r, g, 0);
            
            y1in++;
            y2in++;

            y1out++;
            y2out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, r, g, 0);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, r, g, 0);

            y1in++;
            y2in++;

            y1out++;
            y2out++;

            uin++;
            vin++;
            uout++;
            vout++;
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_yellow_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, r, g, 0);

            y1in++;
            y1out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, r, g, 0);

            y1in++;
            y1out++;

            uin++;
            vin++;

            uout++;
            vout++;
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

/****** grayscale variations ******/
static void get_red_grayscale_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, r, r, r);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, r, r, r);
            
            y1in++;
            y2in++;

            y1out++;
            y2out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, r, r, r);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, r, r, r);

            y1in++;
            y2in++;

            y1out++;
            y2out++;

            uin++;
            vin++;
            uout++;
            vout++;
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_red_grayscale_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, r, r, r);

            y1in++;
            y1out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, r, r, r);

            y1in++;
            y1out++;

            uin++;
            vin++;

            uout++;
            vout++;
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_cyan_grayscale_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    int avg;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (g + b) / 2;
            rgb_to_yuv(y1out, uout, vout, avg, avg, avg);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            avg = (g + b) / 2;
            rgb_to_yuv(y2out, uout, vout, avg, avg, avg);
            
            y1in++;
            y2in++;

            y1out++;
            y2out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (g + b) / 2;
            rgb_to_yuv(y1out, uout, vout, avg, avg, avg);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            avg = (g + b) / 2;
            rgb_to_yuv(y2out, uout, vout, avg, avg, avg);

            y1in++;
            y2in++;

            y1out++;
            y2out++;

            uin++;
            vin++;
            uout++;
            vout++;
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_cyan_grayscale_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    int avg;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (g + b) / 2;
            rgb_to_yuv(y1out, uout, vout, avg, avg, avg);

            y1in++;
            y1out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (g + b) / 2;
            rgb_to_yuv(y1out, uout, vout, avg, avg, avg);

            y1in++;
            y1out++;

            uin++;
            vin++;

            uout++;
            vout++;
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_green_grayscale_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, g, g, g);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, g, g, g);
            
            y1in++;
            y2in++;

            y1out++;
            y2out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, g, g, g);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, g, g, g);

            y1in++;
            y2in++;

            y1out++;
            y2out++;

            uin++;
            vin++;
            uout++;
            vout++;
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_green_grayscale_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                   int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, g, g, g);

            y1in++;
            y1out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, g, g, g);

            y1in++;
            y1out++;

            uin++;
            vin++;

            uout++;
            vout++;
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_magenta_grayscale_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    int avg;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (r + b) / 2;
            rgb_to_yuv(y1out, uout, vout, avg, avg, avg);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            avg = (r + b) / 2;
            rgb_to_yuv(y2out, uout, vout, avg, avg, avg);
            
            y1in++;
            y2in++;

            y1out++;
            y2out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (r + b) / 2;
            rgb_to_yuv(y1out, uout, vout, avg, avg, avg);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            avg = (r + b) / 2;
            rgb_to_yuv(y2out, uout, vout, avg, avg, avg);

            y1in++;
            y2in++;

            y1out++;
            y2out++;

            uin++;
            vin++;
            uout++;
            vout++;
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_magenta_grayscale_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    int avg;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (r + b) / 2;
            rgb_to_yuv(y1out, uout, vout, avg, avg, avg);

            y1in++;
            y1out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (r + b) / 2;
            rgb_to_yuv(y1out, uout, vout, avg, avg, avg);

            y1in++;
            y1out++;

            uin++;
            vin++;

            uout++;
            vout++;
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_blue_grayscale_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, b, b, b);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, b, b, b);
            
            y1in++;
            y2in++;

            y1out++;
            y2out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, b, b, b);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            rgb_to_yuv(y2out, uout, vout, b, b, b);

            y1in++;
            y2in++;

            y1out++;
            y2out++;

            uin++;
            vin++;
            uout++;
            vout++;
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_blue_grayscale_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, b, b, b);

            y1in++;
            y1out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            rgb_to_yuv(y1out, uout, vout, b, b, b);

            y1in++;
            y1out++;

            uin++;
            vin++;

            uout++;
            vout++;
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_yellow_grayscale_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    int avg;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (r + g) / 2;
            rgb_to_yuv(y1out, uout, vout, avg, avg, avg);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            avg = (r + g) / 2;
            rgb_to_yuv(y2out, uout, vout, avg, avg, avg);
            
            y1in++;
            y2in++;

            y1out++;
            y2out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (r + g) / 2;
            rgb_to_yuv(y1out, uout, vout, avg, avg, avg);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            avg = (r + g) / 2;
            rgb_to_yuv(y2out, uout, vout, avg, avg, avg);

            y1in++;
            y2in++;

            y1out++;
            y2out++;

            uin++;
            vin++;
            uout++;
            vout++;
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_yellow_grayscale_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    int avg;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (r + g) / 2;
            rgb_to_yuv(y1out, uout, vout, avg, avg, avg);

            y1in++;
            y1out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (r + g) / 2;
            rgb_to_yuv(y1out, uout, vout, avg, avg, avg);

            y1in++;
            y1out++;

            uin++;
            vin++;

            uout++;
            vout++;
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

/****** fill in missing channel variations ******/
static void get_cyan_fill_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    int avg;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (g + b) / 2;
            rgb_to_yuv(y1out, uout, vout, avg, g, b);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            avg = (g + b) / 2;
            rgb_to_yuv(y2out, uout, vout, avg, g, b);
            
            y1in++;
            y2in++;

            y1out++;
            y2out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (g + b) / 2;
            rgb_to_yuv(y1out, uout, vout, avg, g, b);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            avg = (g + b) / 2;
            rgb_to_yuv(y2out, uout, vout, avg, g, b);

            y1in++;
            y2in++;

            y1out++;
            y2out++;

            uin++;
            vin++;
            uout++;
            vout++;
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_cyan_fill_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    int avg;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (g + b) / 2;
            rgb_to_yuv(y1out, uout, vout, avg, g, b);

            y1in++;
            y1out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (g + b) / 2;
            rgb_to_yuv(y1out, uout, vout, avg, g, b);

            y1in++;
            y1out++;

            uin++;
            vin++;

            uout++;
            vout++;
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_magenta_fill_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    int avg;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (r + b) / 2;
            rgb_to_yuv(y1out, uout, vout, r, avg, b);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            avg = (r + b) / 2;
            rgb_to_yuv(y2out, uout, vout, r, avg, b);
            
            y1in++;
            y2in++;

            y1out++;
            y2out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (r + b) / 2;
            rgb_to_yuv(y1out, uout, vout, r, avg, b);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            avg = (r + b) / 2;
            rgb_to_yuv(y2out, uout, vout, r, avg, b);

            y1in++;
            y2in++;

            y1out++;
            y2out++;

            uin++;
            vin++;
            uout++;
            vout++;
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_magenta_fill_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    int avg;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (r + b) / 2;
            rgb_to_yuv(y1out, uout, vout, r, avg, b);

            y1in++;
            y1out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (r + b) / 2;
            rgb_to_yuv(y1out, uout, vout, r, avg, b);

            y1in++;
            y1out++;

            uin++;
            vin++;

            uout++;
            vout++;
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_yellow_fill_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    int avg;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (r + g) / 2;
            rgb_to_yuv(y1out, uout, vout, r, g, avg);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            avg = (r + g) / 2;
            rgb_to_yuv(y2out, uout, vout, r, g, avg);
            
            y1in++;
            y2in++;

            y1out++;
            y2out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (r + g) / 2;
            rgb_to_yuv(y1out, uout, vout, r, g, avg);

            yuv_to_rgb(&r, &g, &b, *y2in, *uin, *vin);
            avg = (r + g) / 2;
            rgb_to_yuv(y2out, uout, vout, r, g, avg);

            y1in++;
            y2in++;

            y1out++;
            y2out++;

            uin++;
            vin++;
            uout++;
            vout++;
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_yellow_fill_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;
    int r, g, b;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    int avg;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (r + g) / 2;
            rgb_to_yuv(y1out, uout, vout, r, g, avg);

            y1in++;
            y1out++;

            yuv_to_rgb(&r, &g, &b, *y1in, *uin, *vin);
            avg = (r + g) / 2;
            rgb_to_yuv(y1out, uout, vout, r, g, avg);

            y1in++;
            y1out++;

            uin++;
            vin++;

            uout++;
            vout++;
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_lefthalf_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
    int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    int avg;
    while( y1in < yend )
    {
        /* only go up to half way */
        const uint8_t *y1end = y1in + (i_visible_pitch / 2);
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            *uout = *uin;
            uout++;
            *uout = *uin;
            uout++;
            uin++;

            *vout = *vin;
            vout++;
            *vout = *vin;
            vout++;
            vin++;

            *y1out = *y1in;
            y1out++;
            *y1out = *y1in;
            y1out++;
            y1in++;

            *y1out = *y1in;
            y1out++;
            *y1out = *y1in;
            y1out++;
            y1in++;

            *y2out = *y2in;
            y2out++;
            *y2out = *y2in;
            y2out++;
            y2in++;

            *y2out = *y2in;
            y2out++;
            *y2out = *y2in;
            y2out++;
            y2in++;

        }
        y1in  += (i_in_pitch / 2) + 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += (p_inpic->p[up].i_pitch / 2) + p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += (p_inpic->p[vp].i_pitch / 2) + p_inpic->p[vp].i_pitch - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_lefthalf_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
    int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    int avg;
    while( y1in < yend )
    {
        /* only go up to half way */
        const uint8_t *y1end = y1in + (i_visible_pitch / 2);
        while( y1in < y1end )
        {
            *uout = *uin;
            uout++;
            *uout = *uin;
            uout++;
            uin++;

            *vout = *vin;
            vout++;
            *vout = *vin;
            vout++;
            vin++;

            *y1out = *y1in;
            y1out++;
            *y1out = *y1in;
            y1out++;
            y1in++;

            *y1out = *y1in;
            y1out++;
            *y1out = *y1in;
            y1out++;
            y1in++;
        }
        y1in  += (i_in_pitch / 2) + i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += (p_inpic->p[up].i_pitch / 2) + p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += (p_inpic->p[vp].i_pitch / 2) + p_inpic->p[vp].i_pitch - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_righthalf_from_yuv420( picture_t *p_inpic,
    picture_t *p_outpic, int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    int avg;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        /* start at half way */
        y1in += i_visible_pitch / 2;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;

        uin   += p_inpic->p[up].i_pitch / 2;
        vin   += p_inpic->p[vp].i_pitch  / 2;
        while( y1in < y1end )
        {
            *uout = *uin;
            uout++;
            *uout = *uin;
            uout++;
            uin++;

            *vout = *vin;
            vout++;
            *vout = *vin;
            vout++;
            vin++;

            *y1out = *y1in;
            y1out++;
            *y1out = *y1in;
            y1out++;
            y1in++;

            *y1out = *y1in;
            y1out++;
            *y1out = *y1in;
            y1out++;
            y1in++;

            *y2out = *y2in;
            y2out++;
            *y2out = *y2in;
            y2out++;
            y2in++;

            *y2out = *y2in;
            y2out++;
            *y2out = *y2in;
            y2out++;
            y2in++;

        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_righthalf_from_yuv422( picture_t *p_inpic,
    picture_t *p_outpic, int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    int avg;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        /* start at half way */
        y1in += i_visible_pitch / 2;

        uin   += p_inpic->p[up].i_pitch / 2;
        vin   += p_inpic->p[vp].i_pitch  / 2;
        while( y1in < y1end )
        {
            *uout = *uin;
            uout++;
            *uout = *uin;
            uout++;
            uin++;

            *vout = *vin;
            vout++;
            *vout = *vin;
            vout++;
            vin++;

            *y1out = *y1in;
            y1out++;
            *y1out = *y1in;
            y1out++;
            y1in++;

            *y1out = *y1in;
            y1out++;
            *y1out = *y1in;
            y1out++;
            y1in++;
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_tophalf_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
    int yp, int up, int vp )
{
}

static void get_tophalf_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
    int yp, int up, int vp )
{
}

static void get_bottomhalf_from_yuv420( picture_t *p_inpic,
    picture_t *p_outpic, int yp, int up, int vp )
{
}

static void get_bottomhalf_from_yuv422( picture_t *p_inpic,
    picture_t *p_outpic, int yp, int up, int vp )
{
}

