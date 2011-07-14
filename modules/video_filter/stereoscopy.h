/*****************************************************************************
 * sterescopy.h : sterescopy plugin for vlc
 *****************************************************************************
 * Copyright (C) 2011 the VideoLAN team
 *
 * Author: Andrew Price <andrewprice@andrewalexanderprice.com>
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

#ifndef VLC_STEREOSCOPY_H
#define VLC_STEREOSCOPY_H 1

/* do nothing, comes in as 2d, goes out as 2d */
#define STEREOSCOPY_2D                          0

#define STEREOSCOPY_ANAGLYPH_BLUE               1000
#define STEREOSCOPY_ANAGLYPH_CYAN               1001
#define STEREOSCOPY_ANAGLYPH_GREEN              1002
#define STEREOSCOPY_ANAGLYPH_MAGENTA            1003
#define STEREOSCOPY_ANAGLYPH_RED                1004
#define STEREOSCOPY_ANAGLYPH_YELLOW             1005

#define STEREOSCOPY_ANAGLYPH_BLUE_GRAY          1010
#define STEREOSCOPY_ANAGLYPH_CYAN_GRAY          1011
#define STEREOSCOPY_ANAGLYPH_GREEN_GRAY         1012
#define STEREOSCOPY_ANAGLYPH_MAGENTA_GRAY       1013
#define STEREOSCOPY_ANAGLYPH_RED_GRAY           1014
#define STEREOSCOPY_ANAGLYPH_YELLOW_GRAY        1015

#define STEREOSCOPY_ANAGLYPH_CYAN_FILL          1021
#define STEREOSCOPY_ANAGLYPH_MAGENTA_FILL       1023
#define STEREOSCOPY_ANAGLYPH_YELLOW_FILL        1025

#define STEREOSCOPY_SIDEBYSIDE_LEFT             2000
#define STEREOSCOPY_SIDEBYSIDE_RIGHT            2001
#define STEREOSCOPY_SIDEBYSIDE_BOTTOM           2002
#define STEREOSCOPY_SIDEBYSIDE_TOP              2003

#endif /* VLC_STEREOSCOPY_H */
