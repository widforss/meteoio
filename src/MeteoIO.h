/***********************************************************************************/
/*  Copyright 2009 WSL Institute for Snow and Avalanche Research    SLF-DAVOS      */
/***********************************************************************************/
/* This file is part of MeteoIO.
    MeteoIO is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    MeteoIO is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with MeteoIO.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __METEOIO_H__
#define __METEOIO_H__

#include "Array.h"
#include "Array2D.h"
#include "Array3D.h"

#include "StationData.h"
#include "MeteoData.h"
#include "Grid2DObject.h"
#include "DEMObject.h"
#include "Date_IO.h"
#include "DynamicLibrary.h"
#include "BufferedIOHandler.h"
#include "Coords.h"
#include "InterpolationAlgorithms.h"
#include "Meteo2DInterpolator.h"
#include "IOInterface.h"

#ifdef _POPC_
#include "IOHandler.ph"
#else
#include "IOHandler.h"
#endif

#endif
