#include <glib.h>

/* The code below is copied from Mappero */

/*
 * Copyright (C) 2010 Alberto Mardegan <mardy@users.sourceforge.net>
 *
 * This file is part of libMappero.
 *
 * libMappero is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libMappero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libMappero.  If not, see <http://www.gnu.org/licenses/>.
 */

#define deg2rad(deg) ((deg) * (PI / 180.0))
#define rad2deg(rad) ((rad) * (180.0 / PI))

#ifdef USE_DOUBLES_FOR_LATLON
#define GSIN(x) sin(x)
#define GCOS(x) cos(x)
#define GASIN(x) asin(x)
#define GTAN(x) tan(x)
#define GATAN(x) atan(x)
#define GATAN2(x, y) atan2(x, y)
#define GEXP(x) exp(x)
#define GLOG(x) log(x)
#define GPOW(x, y) pow(x, y)
#define GSQTR(x) sqrt(x)
#else
#define GSIN(x) sinf(x)
#define GCOS(x) cosf(x)
#define GASIN(x) asinf(x)
#define GTAN(x) tanf(x)
#define GATAN(x) atanf(x)
#define GATAN2(x, y) atan2f(x, y)
#define GEXP(x) expf(x)
#define GLOG(x) logf(x)
#define GPOW(x, y) powf(x, y)
#define GSQTR(x) sqrtf(x)
#endif

#define MAX_ZOOM (20)
#define TILE_SIZE_P2 (8)

#define WORLD_SIZE_UNITS (2 << (MAX_ZOOM + TILE_SIZE_P2))

#define MERCATOR_SPAN (-6.28318377773622)
#define MERCATOR_TOP (3.14159188886811)

#ifdef USE_DOUBLES_FOR_LATLON
typedef gdouble MapGeo;
#else
typedef gfloat MapGeo;
#endif
