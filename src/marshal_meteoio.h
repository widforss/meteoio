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
#ifdef _POPC_

#ifndef MARSHAL_METEOIO_H
#define MARSHAL_METEOIO_H

#include "Grid2DObject.h"
#include "Grid3DObject.h"
#include "DEMObject.h"
#include "StationData.h"
#include "MeteoData.h"
#include "Coords.h"
#include <vector>
#include <string>
#include "LegacyIO.ph"

typedef Array2D<double> TYPE_DOUBLE2D;
typedef Array3D<double> TYPE_DOUBLE3D;
typedef Array2D<int> TYPE_INT2D;
typedef std::vector<std::string> STR_VECTOR;

void marshal_uint(POPBuffer &buf,unsigned int &data, int maxsize, int flag, POPMemspool *temp);

void marshal_slope_type(POPBuffer &buf, DEMObject::slope_type &data, int maxsize, int flag, POPMemspool *temp);

void marshal_geo_distances(POPBuffer &buf, Coords::geo_distances &data, int maxsize, int flag, POPMemspool *temp);

void marshal_TYPE_DOUBLE2D(POPBuffer &buf, TYPE_DOUBLE2D &data,int maxsize, int flag, POPMemspool *temp);

void marshal_TYPE_DOUBLE3D(POPBuffer &buf, TYPE_DOUBLE3D &data,int maxsize, int flag, POPMemspool *temp);

void marshal_TYPE_INT2D(POPBuffer &buf, TYPE_INT2D &data,int maxsize, int flag, POPMemspool *temp);

void marshal_CDoubleArray(POPBuffer &buf, CDoubleArray &data,int maxsize, int flag, POPMemspool *temp);

void marshal_CNodeArray(POPBuffer &buf,CNodeArray &data,int maxsize, int flag, POPMemspool *temp);

void marshal_update_CNodeArray(POPBuffer &buf,CNodeArray &data, int maxsize, int flag, POPMemspool *temp);

void marshal_input_CNodeArray(POPBuffer &buf,CNodeArray &data, int maxsize, int flag, POPMemspool *temp);

void marshal_POINTSArray(POPBuffer &buf,POINTSArray &data, int maxsize, int flag, POPMemspool *temp);

void marshal_METEO_DATASET(POPBuffer &buf, METEO_DATASET &data, int maxsize, int flag, POPMemspool *temp);

void marshal_vector_METEO_DATASET(POPBuffer &buf, std::vector<METEO_DATASET> &data, int maxsize, int flag, POPMemspool *temp);

void marshal_vecstr(POPBuffer &buf, std::vector<std::string> &data, int maxsize, int flag, POPMemspool *temp);

void marshal_map_str_str(POPBuffer &buf, std::map<std::string, std::string> &data_map, int maxsize, int flag, POPMemspool *temp);

void marshal_map_str_vecstr(POPBuffer &buf, std::map<std::string, STR_VECTOR> &data_map, int maxsize, int flag, POPMemspool *temp);

void marshal_Coords(POPBuffer &buf, Coords &data, int maxsize, int flag, POPMemspool *temp);

void marshal_STATION_DATASET(POPBuffer &buf, STATION_DATASET &data, int maxsize, int flag, POPMemspool *temp);

void marshal_vector_STATION_DATASET(POPBuffer &buf, std::vector<STATION_DATASET> &data, int maxsize, int flag, POPMemspool *temp);

void marshal_vector_Grid2DObject(POPBuffer &buf, std::vector<Grid2DObject> &data, int maxsize, int flag, POPMemspool *temp);

#endif

#endif
