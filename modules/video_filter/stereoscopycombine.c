/*****************************************************************************
 * stereoscopycombine.c : stereoscopy combine video filter - takes two
 * images (one for each eye) and combines them into a single image
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

#include <vlc_fixups.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static picture_t *Filter( filter_t *, picture_t * );
static void Destroy( vlc_object_t *p_this );

static int OutputModeStrToValue(const char *str);

static picture_t *CombinePictures( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int chromaformat );


/* combiner methods, each take two pictures and return one */
static picture_t *combine_red_cyan_yuv411( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_red_cyan_yuv420( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_red_cyan_yuv422( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_red_cyan_yuv444( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_red_green_yuv411( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_red_green_yuv420( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_red_green_yuv422( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_red_green_yuv444( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_red_blue_yuv411( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_red_blue_yuv420( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_red_blue_yuv422( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_red_blue_yuv444( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_green_magenta_yuv411( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_green_magenta_yuv420( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_green_magenta_yuv422( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_green_magenta_yuv444( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_green_blue_yuv411( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_green_blue_yuv420( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_green_blue_yuv422( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_green_blue_yuv444( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_blue_yellow_yuv411( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_blue_yellow_yuv420( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_blue_yellow_yuv422( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );
static picture_t *combine_blue_yellow_yuv444( filter_t *p_filter, picture_t *, picture_t *t, int, int, int );


/* output modes */
#define STEREOSCOPY_COMBINE_MODE_RED_CYAN                    0
#define STEREOSCOPY_COMBINE_MODE_CYAN_RED                    1
#define STEREOSCOPY_COMBINE_MODE_RED_GREEN                   2
#define STEREOSCOPY_COMBINE_MODE_GREEN_RED                   3
#define STEREOSCOPY_COMBINE_MODE_RED_BLUE                    4
#define STEREOSCOPY_COMBINE_MODE_BLUE_RED                    5
#define STEREOSCOPY_COMBINE_MODE_GREEN_MAGENTA               6
#define STEREOSCOPY_COMBINE_MODE_MAGENTA_GREEN               7
#define STEREOSCOPY_COMBINE_MODE_GREEN_BLUE                  8
#define STEREOSCOPY_COMBINE_MODE_BLUE_GREEN                  9
#define STEREOSCOPY_COMBINE_MODE_BLUE_YELLOW                 10
#define STEREOSCOPY_COMBINE_MODE_YELLOW_BLUE                 11

#define FILTER_PREFIX "stereoscopy-combine-"


#define OUTPUT_METHOD_TEXT N_("Output stereoscopy encoding")
#define OUTPUT_METHOD_LONGTEXT N_("Represents the form of steroscopic "\
                                    "encoding used for the image of the "\
                                    "respective eye. " \
									"rc - Red/Cyan (Anaglyph). " \
									"cr - Cyan/Red (Anaglyph). " \
									"rg - Red/Green (Anaglyph). " \
									"gr - Green/Red (Anaglyph). " \
									"rb - Red/Blue (Anaglyph). " \
									"br - Blue/Red (Anaglyph). " \
									"gm - Green/Magenta (Anaglyph). " \
									"mg - Magenta/Green (Anaglyph). " \
									"gb - Green/Blue (Anaglyph). " \
									"bg - Blue/Green (Anaglyph). " \
									"yb - Yellow/Blue (Anaglyph). " \
									"by - Blue/Yellow (Anaglyph). ")


static const char * const type_list_text[] = { N_("Red/Cyan (Anaglyph)"),
	N_("Cyan/Red (Anaglyph)"), N_("Red/Green (Anaglyph)"), N_("Green/Red (Anaglyph)"),
    N_("Red/Blue (Anaglyph)"), N_("Blue/Red (Anaglyph)"), N_("Green/Magenta (Anaglyph)"),
    N_("Magenta/Green (Anaglyph)"), N_("Green/Blue (Anaglyph)"),
    N_("Blue/Green (Anaglyph)"), N_("Blue/Yellow (Anaglyph)"),
    N_("Yellow/Blue (Anaglyph)") };

static const char * const type_list[] = { "rc", "cr", "rg",
	"gr", "rb", "br", "gm", "mg", "gb",
	"bg", "by", "yb"};

struct filter_sys_t
{
	picture_t *p_lastLeftEye;
	picture_t *p_lastRightEye;
    int    i_outputMethod;
};


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Stereoscopy combine video filter") )
    set_shortname( N_( "Stereoscopy Combine" ))
    set_capability( "video filter2", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
	
    add_string("stereoscopic-output", "rg", OUTPUT_METHOD_TEXT, OUTPUT_METHOD_LONGTEXT, false)
        change_string_list(type_list, type_list_text, 0)

    add_shortcut( "stereoscopy-combine" )
    set_callbacks( Create, Destroy )

vlc_module_end ()

/*****************************************************************************
 * Create: initialises stereoscopy combine video filter
 *****************************************************************************
 * This function allocates and initializes a stereoscopy combine filter.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter;
    filter_sys_t *p_sys;
    char *outputMethod;

    p_filter = (filter_t *)p_this;
    p_filter->pf_video_filter = Filter;
    p_sys = p_filter->p_sys = malloc(sizeof(*p_sys));

    if( !p_sys )
        return VLC_ENOMEM;

	/* read in method for left eye */
	outputMethod = var_InheritString( p_filter, "stereoscopic-output" );
	p_sys->i_outputMethod = OutputModeStrToValue(outputMethod);
	free(outputMethod);

	p_sys->p_lastLeftEye = NULL;
	p_sys->p_lastRightEye = NULL;

	return VLC_SUCCESS;
}


/*****************************************************************************
 * Destroy: destroy  stereoscopy combine video filter
 *****************************************************************************
 * Destroys sterescopy combine video filter
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;

	filter_sys_t *p_sys = p_filter->p_sys;
	
	/* ensure any saved pictures are released */
	if( p_sys->p_lastLeftEye != NULL )
        picture_Release( p_sys->p_lastLeftEye );
	
	if( p_sys->p_lastRightEye != NULL )
        picture_Release( p_sys->p_lastRightEye );


    free( p_filter->p_sys );
}

/*****************************************************************************
 * InputModeStrToValue: input mode string to value method
 *****************************************************************************
 * Converts the string of an input mode (e.g. "left") to it's value
 * (e.g. STEREOSCOPY_SIDEBYSIDE_LEFT).
 *****************************************************************************/
static int OutputModeStrToValue(const char *str) {
    if(strcmp(str, "rc") == 0)
        return STEREOSCOPY_COMBINE_MODE_RED_CYAN;
    if(strcmp(str, "cr") == 0)
        return STEREOSCOPY_COMBINE_MODE_CYAN_RED;
    if(strcmp(str, "rg") == 0)
        return STEREOSCOPY_COMBINE_MODE_RED_GREEN;
    if(strcmp(str, "gr") == 0)
        return STEREOSCOPY_COMBINE_MODE_GREEN_RED;
    if(strcmp(str, "rb") == 0)
        return STEREOSCOPY_COMBINE_MODE_RED_BLUE;
    if(strcmp(str, "br") == 0)
        return STEREOSCOPY_COMBINE_MODE_BLUE_RED;
    if(strcmp(str, "gm") == 0)
        return STEREOSCOPY_COMBINE_MODE_GREEN_MAGENTA;
    if(strcmp(str, "mg") == 0)
        return STEREOSCOPY_COMBINE_MODE_MAGENTA_GREEN;
    if(strcmp(str, "gb") == 0)
        return STEREOSCOPY_COMBINE_MODE_GREEN_BLUE;
    if(strcmp(str, "bg") == 0)
        return STEREOSCOPY_COMBINE_MODE_BLUE_GREEN;
    if(strcmp(str, "by") == 0)
        return STEREOSCOPY_COMBINE_MODE_BLUE_YELLOW;
    if(strcmp(str, "yb") == 0)
        return STEREOSCOPY_COMBINE_MODE_YELLOW_BLUE;

    /* nothing left */
    return STEREOSCOPY_COMBINE_MODE_RED_CYAN;
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
    int chromaformat;

    if( !p_inpic ) return NULL;

    if( (p_filter->fmt_in.video.i_height == 0) ||
        (p_filter->fmt_in.video.i_width == 0) )
        return NULL;

    if( (p_filter->fmt_out.video.i_height == 0) ||
        (p_filter->fmt_out.video.i_width == 0) )
        return NULL;

    p_sys = p_filter->p_sys;

    /* already a 2d image so do nothing */
    if( p_inpic->i_eye == 0 )
        return p_inpic;

	/* if left eye */
	if( p_inpic->i_eye == 1 )
	{
		if( p_sys->p_lastLeftEye != NULL )
            picture_Release( p_sys->p_lastLeftEye );

		p_sys->p_lastLeftEye = p_inpic;
	}

	/* if right eye */
	if( p_inpic->i_eye == 2 )
	{
		if( p_sys->p_lastRightEye != NULL )
            picture_Release( p_sys->p_lastRightEye );

		p_sys->p_lastRightEye = p_inpic;
	}

	/* needs at least one of each eye to continue */
	if( p_sys->p_lastLeftEye == NULL || p_sys->p_lastRightEye == NULL )
	{
		return NULL;
	}

    switch( p_inpic->format.i_chroma )
    {
        case VLC_CODEC_I411:
            chromaformat = 411;
            break;
        case VLC_CODEC_I420:
        case VLC_CODEC_J420:
            chromaformat = 420;
            break;
        case VLC_CODEC_YV12:
        case VLC_CODEC_I422:
        case VLC_CODEC_J422:
            chromaformat = 422;
            break;
        case VLC_CODEC_I444:
        case VLC_CODEC_J444:
            chromaformat = 444;
            break;
        default:
            msg_Err( p_filter, "Unsupported input chroma (%4.4s)",
                      (char*)&(p_inpic->format.i_chroma) );
            picture_Release( p_inpic );
            return NULL;
    }
	
	/* combine images */
    picture_t *p_output = CombinePictures( p_filter, p_sys->p_lastLeftEye,
        p_sys->p_lastRightEye, chromaformat);

    if( !p_output )
    {
        msg_Warn( p_filter, "can't get output picture" );
            return NULL;
    }

    p_output->i_eye = 0;
    return p_output;
}


/*****************************************************************************
 * CombinePictures: combines two images into a stereo image
 *****************************************************************************
 * Combines two seperate images into a single stereoscopic image based on the
 * desired output format.
 *****************************************************************************/
static picture_t *CombinePictures( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int chromaformat )
{
	filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;

	switch(p_sys->i_outputMethod)
	{
		case STEREOSCOPY_COMBINE_MODE_RED_CYAN:
			switch(chromaformat)
			{
				case 411:
					return combine_red_cyan_yuv411(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 420:
					return combine_red_cyan_yuv420(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 422:
					return combine_red_cyan_yuv422(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 444:
					return combine_red_cyan_yuv444(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
			}
		break;
		case STEREOSCOPY_COMBINE_MODE_CYAN_RED:
			switch(chromaformat)
			{
				case 411:
					return combine_red_cyan_yuv411(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 420:
					return combine_red_cyan_yuv420(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 422:
					return combine_red_cyan_yuv422(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 444:
					return combine_red_cyan_yuv444(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
			}
			break;
	case STEREOSCOPY_COMBINE_MODE_RED_GREEN:
			switch(chromaformat)
			{
				case 411:
					return combine_red_green_yuv411(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 420:
					return combine_red_green_yuv420(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 422:
					return combine_red_green_yuv422(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 444:
					return combine_red_green_yuv444(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
			}
			break;
	case STEREOSCOPY_COMBINE_MODE_GREEN_RED:
			switch(chromaformat)
			{
				case 411:
					return combine_red_green_yuv411(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 420:
					return combine_red_green_yuv420(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 422:
					return combine_red_green_yuv422(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 444:
					return combine_red_green_yuv444(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
			}
			break;
	case STEREOSCOPY_COMBINE_MODE_RED_BLUE:
			switch(chromaformat)
			{
				case 411:
					return combine_red_blue_yuv411(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 420:
					return combine_red_blue_yuv420(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 422:
					return combine_red_blue_yuv422(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 444:
					return combine_red_blue_yuv444(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
			}
			break;
	case STEREOSCOPY_COMBINE_MODE_BLUE_RED:
			switch(chromaformat)
			{
				case 411:
					return combine_red_blue_yuv411(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 420:
					return combine_red_blue_yuv420(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 422:
					return combine_red_blue_yuv422(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 444:
					return combine_red_blue_yuv444(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
			}
			break;
	case STEREOSCOPY_COMBINE_MODE_GREEN_MAGENTA:
			switch(chromaformat)
			{
				case 411:
					return combine_green_magenta_yuv411(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 420:
					return combine_green_magenta_yuv420(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 422:
					return combine_green_magenta_yuv422(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 444:
					return combine_green_magenta_yuv444(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
			}
			break;
	case STEREOSCOPY_COMBINE_MODE_MAGENTA_GREEN:
			switch(chromaformat)
			{
				case 411:
					return combine_green_magenta_yuv411(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 420:
					return combine_green_magenta_yuv420(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 422:
					return combine_green_magenta_yuv422(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 444:
					return combine_green_magenta_yuv444(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
			}
			break;
	case STEREOSCOPY_COMBINE_MODE_GREEN_BLUE:
			switch(chromaformat)
			{
				case 411:
					return combine_green_blue_yuv411(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 420:
					return combine_green_blue_yuv420(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 422:
					return combine_green_blue_yuv422(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 444:
					return combine_green_blue_yuv444(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
			}
			break;
	case STEREOSCOPY_COMBINE_MODE_BLUE_GREEN:
			switch(chromaformat)
			{
				case 411:
					return combine_green_blue_yuv411(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 420:
					return combine_green_blue_yuv420(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 422:
					return combine_green_blue_yuv422(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 444:
					return combine_green_blue_yuv444(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
			}
			break;
	case STEREOSCOPY_COMBINE_MODE_BLUE_YELLOW:
			switch(chromaformat)
			{
				case 411:
					return combine_blue_yellow_yuv411(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 420:
					return combine_blue_yellow_yuv420(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 422:
					return combine_blue_yellow_yuv422(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 444:
					return combine_blue_yellow_yuv444(p_filter, p_in1, p_in2,
                        Y_PLANE, U_PLANE, V_PLANE);
			}
			break;
	case STEREOSCOPY_COMBINE_MODE_YELLOW_BLUE:
			switch(chromaformat)
			{
				case 411:
					return combine_blue_yellow_yuv411(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 420:
					return combine_blue_yellow_yuv420(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 422:
					return combine_blue_yellow_yuv422(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
				case 444:
					return combine_blue_yellow_yuv444(p_filter, p_in2, p_in1,
                        Y_PLANE, U_PLANE, V_PLANE);
			}
			break;
	};

	return NULL;
}

/* combining methods */
static picture_t *combine_red_cyan_yuv411( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_red_cyan_yuv411 not implemented");
	return NULL;
}

static picture_t *combine_red_cyan_yuv420( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_red_cyan_yuv420 not implemented");
	return NULL;
}

static picture_t *combine_red_cyan_yuv422( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_red_cyan_yuv422 not implemented");
	return NULL;
}

static picture_t *combine_red_cyan_yuv444( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_red_cyan_yuv444 not implemented");
	return NULL;
}

static picture_t *combine_red_green_yuv411( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_red_green_yuv411 not implemented");
	return NULL;
}

static picture_t *combine_red_green_yuv420( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_red_green_yuv420 not implemented");
	return NULL;
}

static picture_t *combine_red_green_yuv422( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_red_green_yuv422 not implemented");
	return NULL;
}

static picture_t *combine_red_green_yuv444( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_red_green_yuv444 not implemented");
	return NULL;
}

static picture_t *combine_red_blue_yuv411( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_red_blue_yuv411 not implemented");
	return NULL;
}

static picture_t *combine_red_blue_yuv420( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_red_blue_yuv420 not implemented");
	return NULL;
}

static picture_t *combine_red_blue_yuv422( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_red_blue_yuv422 not implemented");
	return NULL;
}

static picture_t *combine_red_blue_yuv444( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_red_blue_yuv444 not implemented");
	return NULL;
}

static picture_t *combine_green_magenta_yuv411( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_green_magenta_yuv411 not implemented");
	return NULL;
}

static picture_t *combine_green_magenta_yuv420( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_green_magenta_yuv420 not implemented");
	return NULL;
}

static picture_t *combine_green_magenta_yuv422( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_green_magenta_yuv422 not implemented");
	return NULL;
}

static picture_t *combine_green_magenta_yuv444( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_green_magenta_yuv444 not implemented");
	return NULL;
}

static picture_t *combine_green_blue_yuv411( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_green_blue_yuv411 not implemented");
	return NULL;
}

static picture_t *combine_green_blue_yuv420( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_green_blue_yuv420 not implemented");
	return NULL;
}

static picture_t *combine_green_blue_yuv422( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_green_blue_yuv422 not implemented");
	return NULL;
}

static picture_t *combine_green_blue_yuv444( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_green_blue_yuv444 not implemented");
	return NULL;
}

static picture_t *combine_blue_yellow_yuv411( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_blue_yellow_yuv411 not implemented");
	return NULL;
}

static picture_t *combine_blue_yellow_yuv420( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_blue_yellow_yuv420 not implemented");
	return NULL;
}

static picture_t *combine_blue_yellow_yuv422( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_blue_yellow_yuv422 not implemented");
	return NULL;
}

static picture_t *combine_blue_yellow_yuv444( filter_t *p_filter, picture_t *p_in1, picture_t *p_in2, int yp, int up, int vp)
{
    VLC_UNUSED(p_in1);
    VLC_UNUSED(p_in2);
    VLC_UNUSED(yp);
    VLC_UNUSED(up);
    VLC_UNUSED(vp);
    msg_Err( p_filter, "stereoscopy.c: combine_blue_yellow_yuv444 not implemented");
	return NULL;
}
