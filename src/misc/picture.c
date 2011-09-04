/*****************************************************************************
 * picture.c : picture management functions
 *****************************************************************************
 * Copyright (C) 2000-2010 the VideoLAN team
 * Copyright (C) 2009-2010 Laurent Aimar
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#include <assert.h>

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_image.h>
#include <vlc_block.h>

/**
 * Allocate a new picture in the heap.
 *
 * This function allocates a fake direct buffer in memory, which can be
 * used exactly like a video buffer. The video output thread then manages
 * how it gets displayed.
 */
static int vout_AllocatePicture( picture_t *p_pic,
                                 vlc_fourcc_t i_chroma,
                                 int i_width, int i_height,
                                 int i_sar_num, int i_sar_den )
{
    /* Make sure the real dimensions are a multiple of 16 */
    if( picture_Setup( p_pic, i_chroma, i_width, i_height,
                       i_sar_num, i_sar_den ) != VLC_SUCCESS )
        return VLC_EGENERIC;

    /* Calculate how big the new image should be */
    size_t i_bytes = 0;
    for( int i = 0; i < p_pic->i_planes; i++ )
    {
        const plane_t *p = &p_pic->p[i];

        if( p->i_pitch <= 0 || p->i_lines <= 0 ||
            p->i_pitch > (SIZE_MAX - i_bytes)/p->i_lines )
        {
            p_pic->i_planes = 0;
            return VLC_ENOMEM;
        }
        i_bytes += p->i_pitch * p->i_lines;
    }

    uint8_t *p_data = vlc_memalign( &p_pic->p_data_orig, 16, i_bytes );
    if( !p_data )
    {
        p_pic->i_planes = 0;
        return VLC_EGENERIC;
    }

    /* Fill the p_pixels field for each plane */
    p_pic->p[0].p_pixels = p_data;
    for( int i = 1; i < p_pic->i_planes; i++ )
    {
        p_pic->p[i].p_pixels = &p_pic->p[i-1].p_pixels[ p_pic->p[i-1].i_lines *
                                                        p_pic->p[i-1].i_pitch ];
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void PictureReleaseCallback( picture_t *p_picture )
{
    if( --p_picture->i_refcount > 0 )
        return;
    picture_Delete( p_picture );
}

/*****************************************************************************
 *
 *****************************************************************************/
void picture_Reset( picture_t *p_picture )
{
    /* */
    p_picture->date = VLC_TS_INVALID;
    p_picture->b_force = false;
    p_picture->b_progressive = false;
    p_picture->i_nb_fields = 2;
    p_picture->b_top_field_first = false;
    picture_CleanupQuant( p_picture );
}

/*****************************************************************************
 *
 *****************************************************************************/
static int LCM( int a, int b )
{
    return a * b / GCD( a, b );
}

int picture_Setup( picture_t *p_picture, vlc_fourcc_t i_chroma,
                   int i_width, int i_height, int i_sar_num, int i_sar_den )
{
    /* Store default values */
    p_picture->i_planes = 0;
    for( unsigned i = 0; i < VOUT_MAX_PLANES; i++ )
    {
        plane_t *p = &p_picture->p[i];
        p->p_pixels = NULL;
        p->i_pixel_pitch = 0;
    }

    p_picture->pf_release = NULL;
    p_picture->p_release_sys = NULL;
    p_picture->i_refcount = 0;

    p_picture->i_nb_fields = 2;

    p_picture->i_qtype = QTYPE_NONE;
    p_picture->i_qstride = 0;
    p_picture->p_q = NULL;

    video_format_Setup( &p_picture->format, i_chroma, i_width, i_height,
                        i_sar_num, i_sar_den );

    const vlc_chroma_description_t *p_dsc =
        vlc_fourcc_GetChromaDescription( p_picture->format.i_chroma );
    if( !p_dsc )
        return VLC_EGENERIC;

    /* We want V (width/height) to respect:
        (V * p_dsc->p[i].w.i_num) % p_dsc->p[i].w.i_den == 0
        (V * p_dsc->p[i].w.i_num/p_dsc->p[i].w.i_den * p_dsc->i_pixel_size) % 16 == 0
       Which is respected if you have
       V % lcm( p_dsc->p[0..planes].w.i_den * 16) == 0
    */
    int i_modulo_w = 1;
    int i_modulo_h = 1;
    unsigned int i_ratio_h  = 1;
    for( unsigned i = 0; i < p_dsc->plane_count; i++ )
    {
        i_modulo_w = LCM( i_modulo_w, 16 * p_dsc->p[i].w.den );
        i_modulo_h = LCM( i_modulo_h, 16 * p_dsc->p[i].h.den );
        if( i_ratio_h < p_dsc->p[i].h.den )
            i_ratio_h = p_dsc->p[i].h.den;
    }

    const int i_width_aligned  = ( i_width  + i_modulo_w - 1 ) / i_modulo_w * i_modulo_w;
    const int i_height_aligned = ( i_height + i_modulo_h - 1 ) / i_modulo_h * i_modulo_h;
    const int i_height_extra   = 2 * i_ratio_h; /* This one is a hack for some ASM functions */
    for( unsigned i = 0; i < p_dsc->plane_count; i++ )
    {
        plane_t *p = &p_picture->p[i];

        p->i_lines         = (i_height_aligned + i_height_extra ) * p_dsc->p[i].h.num / p_dsc->p[i].h.den;
        p->i_visible_lines = i_height * p_dsc->p[i].h.num / p_dsc->p[i].h.den;
        p->i_pitch         = i_width_aligned * p_dsc->p[i].w.num / p_dsc->p[i].w.den * p_dsc->pixel_size;
        p->i_visible_pitch = i_width * p_dsc->p[i].w.num / p_dsc->p[i].w.den * p_dsc->pixel_size;
        p->i_pixel_pitch   = p_dsc->pixel_size;

        assert( (p->i_pitch % 16) == 0 );
    }
    p_picture->i_planes  = p_dsc->plane_count;

    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
picture_t *picture_NewFromResource( const video_format_t *p_fmt, const picture_resource_t *p_resource )
{
    video_format_t fmt = *p_fmt;

    /* It is needed to be sure all information are filled */
    video_format_Setup( &fmt, p_fmt->i_chroma,
                              p_fmt->i_width, p_fmt->i_height,
                              p_fmt->i_sar_num, p_fmt->i_sar_den );
    if( p_fmt->i_x_offset < p_fmt->i_width &&
        p_fmt->i_y_offset < p_fmt->i_height &&
        p_fmt->i_visible_width  > 0 && p_fmt->i_x_offset + p_fmt->i_visible_width  <= p_fmt->i_width &&
        p_fmt->i_visible_height > 0 && p_fmt->i_y_offset + p_fmt->i_visible_height <= p_fmt->i_height )
        video_format_CopyCrop( &fmt, p_fmt );

    /* */
    picture_t *p_picture = calloc( 1, sizeof(*p_picture) );
    if( !p_picture )
        return NULL;

    if( p_resource )
    {
        if( picture_Setup( p_picture, fmt.i_chroma, fmt.i_width, fmt.i_height,
                           fmt.i_sar_num, fmt.i_sar_den ) )
        {
            free( p_picture );
            return NULL;
        }
        p_picture->p_sys = p_resource->p_sys;

        for( int i = 0; i < p_picture->i_planes; i++ )
        {
            p_picture->p[i].p_pixels = p_resource->p[i].p_pixels;
            p_picture->p[i].i_lines  = p_resource->p[i].i_lines;
            p_picture->p[i].i_pitch  = p_resource->p[i].i_pitch;
        }
    }
    else
    {
        if( vout_AllocatePicture( p_picture,
                                  fmt.i_chroma, fmt.i_width, fmt.i_height,
                                  fmt.i_sar_num, fmt.i_sar_den ) )
        {
            free( p_picture );
            return NULL;
        }
    }
    /* */
    p_picture->format = fmt;
    p_picture->i_refcount = 1;
    p_picture->pf_release = PictureReleaseCallback;

    return p_picture;
}
picture_t *picture_NewFromFormat( const video_format_t *p_fmt )
{
    return picture_NewFromResource( p_fmt, NULL );
}
picture_t *picture_New( vlc_fourcc_t i_chroma, int i_width, int i_height, int i_sar_num, int i_sar_den )
{
    video_format_t fmt;

    memset( &fmt, 0, sizeof(fmt) );
    video_format_Setup( &fmt, i_chroma, i_width, i_height,
                        i_sar_num, i_sar_den );

    return picture_NewFromFormat( &fmt );
}

/*****************************************************************************
 *
 *****************************************************************************/
void picture_Delete( picture_t *p_picture )
{
        /* while( p_picture != NULL ) { */
        picture_t *p_next = p_picture->p_next;

        assert( p_picture && p_picture->i_refcount == 0 );
        assert( p_picture->p_release_sys == NULL );

        free( p_picture->p_q );
        free( p_picture->p_data_orig );
        free( p_picture->p_sys );
	    free( p_picture );
		p_picture = p_next;
        /*}*/
}

/*****************************************************************************
 *
 *****************************************************************************/
void picture_CopyPixels( picture_t *p_dst, const picture_t *p_src )
{
    int i;

    for( i = 0; i < p_src->i_planes ; i++ )
        plane_CopyPixels( p_dst->p+i, p_src->p+i );
}

void plane_CopyPixels( plane_t *p_dst, const plane_t *p_src )
{
    const unsigned i_width  = __MIN( p_dst->i_visible_pitch,
                                     p_src->i_visible_pitch );
    const unsigned i_height = __MIN( p_dst->i_visible_lines,
                                     p_src->i_visible_lines );

    /* The 2x visible pitch check does two things:
       1) Makes field plane_t's work correctly (see the deinterlacer module)
       2) Moves less data if the pitch and visible pitch differ much.
    */
    if( p_src->i_pitch == p_dst->i_pitch  &&
        p_src->i_pitch < 2*p_src->i_visible_pitch )
    {
        /* There are margins, but with the same width : perfect ! */
        vlc_memcpy( p_dst->p_pixels, p_src->p_pixels,
                    p_src->i_pitch * i_height );
    }
    else
    {
        /* We need to proceed line by line */
        uint8_t *p_in = p_src->p_pixels;
        uint8_t *p_out = p_dst->p_pixels;
        int i_line;

        assert( p_in );
        assert( p_out );

        for( i_line = i_height; i_line--; )
        {
            vlc_memcpy( p_out, p_in, i_width );
            p_in += p_src->i_pitch;
            p_out += p_dst->i_pitch;
        }
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
int picture_Export( vlc_object_t *p_obj,
                    block_t **pp_image,
                    video_format_t *p_fmt,
                    picture_t *p_picture,
                    vlc_fourcc_t i_format,
                    int i_override_width, int i_override_height )
{
    /* */
    video_format_t fmt_in = p_picture->format;
    if( fmt_in.i_sar_num <= 0 || fmt_in.i_sar_den <= 0 )
    {
        fmt_in.i_sar_num =
        fmt_in.i_sar_den = 1;
    }

    /* */
    video_format_t fmt_out;
    memset( &fmt_out, 0, sizeof(fmt_out) );
    fmt_out.i_sar_num =
    fmt_out.i_sar_den = 1;
    fmt_out.i_chroma  = i_format;

    /* compute original width/height */
    unsigned int i_original_width;
    unsigned int i_original_height;
    if( fmt_in.i_sar_num >= fmt_in.i_sar_den )
    {
        i_original_width = (int64_t)fmt_in.i_width * fmt_in.i_sar_num / fmt_in.i_sar_den;
        i_original_height = fmt_in.i_height;
    }
    else
    {
        i_original_width =  fmt_in.i_width;
        i_original_height = (int64_t)fmt_in.i_height * fmt_in.i_sar_den / fmt_in.i_sar_num;
    }

    /* */
    fmt_out.i_width  = ( i_override_width < 0 ) ?
                       i_original_width : i_override_width;
    fmt_out.i_height = ( i_override_height < 0 ) ?
                       i_original_height : i_override_height;

    /* scale if only one direction is provided */
    if( fmt_out.i_height == 0 && fmt_out.i_width > 0 )
    {
        fmt_out.i_height = fmt_in.i_height * fmt_out.i_width
                     * fmt_in.i_sar_den / fmt_in.i_width / fmt_in.i_sar_num;
    }
    else if( fmt_out.i_width == 0 && fmt_out.i_height > 0 )
    {
        fmt_out.i_width  = fmt_in.i_width * fmt_out.i_height
                     * fmt_in.i_sar_num / fmt_in.i_height / fmt_in.i_sar_den;
    }

    image_handler_t *p_image = image_HandlerCreate( p_obj );

    block_t *p_block = image_Write( p_image, p_picture, &fmt_in, &fmt_out );

    image_HandlerDelete( p_image );

    if( !p_block )
        return VLC_EGENERIC;

    p_block->i_pts =
    p_block->i_dts = p_picture->date;

    if( p_fmt )
        *p_fmt = fmt_out;
    *pp_image = p_block;

    return VLC_SUCCESS;
}

void picture_BlendSubpicture(picture_t *dst,
                             filter_t *blend, subpicture_t *src)
{
    assert(blend && dst && blend->fmt_out.video.i_chroma == dst->format.i_chroma);
    assert(src && !src->b_fade && src->b_absolute);

    for (subpicture_region_t *r = src->p_region; r != NULL; r = r->p_next) {
        assert(r->p_picture && r->i_align == 0);
        if (filter_ConfigureBlend(blend, dst->format.i_width, dst->format.i_height,
                                  &r->fmt) ||
            filter_Blend(blend, dst,
                         r->i_x, r->i_y, r->p_picture,
                         src->i_alpha * r->i_alpha / 255)) {
            msg_Err(blend, "blending %4.4s to %4.4s failed",
                    (char *)&blend->fmt_in.video.i_chroma,
                    (char *)&blend->fmt_out.video.i_chroma );
        }
    }
}
