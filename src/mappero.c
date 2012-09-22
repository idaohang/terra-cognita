#include "mappero.h"
#ifndef PI
    #define PI atan(1)*4
#endif

/* The code below is copied from Mappero */

/*
 * Copyright (C) 2010 Max Lapan
 * Copyright (C) 2010 Alberto Mardegan <mardy@users.sourceforge.net>
 *
 * This file is part of Mappero.
 *
 * Mappero is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mappero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Mappero.  If not, see <http://www.gnu.org/licenses/>.
 */

void unit2latlon_google(gint unitx, gint unity, MapGeo *lat, MapGeo *lon)
{
    MapGeo tmp;
    *lon = (unitx * (360.0 / WORLD_SIZE_UNITS)) - 180.0;
    tmp = (unity * (MERCATOR_SPAN / WORLD_SIZE_UNITS)) + MERCATOR_TOP;
    *lat = (360.0 * (GATAN(GEXP(tmp)))) * (1.0 / PI) - 90.0;
}

void latlon2unit_google(MapGeo lat, MapGeo lon, gint *unitx, gint *unity)
{
    MapGeo tmp;

    *unitx = (lon + 180.0) * (WORLD_SIZE_UNITS / 360.0) + 0.5;
    tmp = GSIN(deg2rad(lat));
    *unity = 0.5 + (WORLD_SIZE_UNITS / MERCATOR_SPAN) *
        (GLOG((1.0 + tmp) / (1.0 - tmp)) * 0.5 - MERCATOR_TOP);
}
