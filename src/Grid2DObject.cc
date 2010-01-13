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
#include "Grid2DObject.h"
#include "IOUtils.h"
#include "MapProj.h"
#include <cmath>

/*
 * Default constructor.
 * grid2D attribute is initialized by Array2D default constructor.
 */
Grid2DObject::Grid2DObject() : grid2D()
{
	ncols = 0;
	nrows = 0;
	xllcorner = 0.0;
	yllcorner = 0.0;
	latitude = IOUtils::nodata;
	longitude = IOUtils::nodata;
	cellsize = 0.0;
}

Grid2DObject::Grid2DObject(const unsigned int& _ncols, const unsigned int& _nrows,
				const double& _xllcorner, const double& _yllcorner,
				const double& _latitude, const double& _longitude,
				const double& _cellsize/*, 
				const MapProj& _proj*/) : grid2D(_ncols, _nrows, IOUtils::nodata)
{
	//set metadata, grid2D already successfully created
	setValues(_ncols, _nrows, _xllcorner, _yllcorner, _latitude, _longitude, _cellsize/*, _proj*/);
}

Grid2DObject::Grid2DObject(const unsigned int& _ncols, const unsigned int& _nrows,
				const double& _xllcorner, const double& _yllcorner,
				const double& _latitude, const double& _longitude,
				const double& _cellsize, const Array2D<double>& _grid2D/*, 
				const MapProj& _proj*/) : grid2D()
{
	set(_ncols, _nrows, _xllcorner, _yllcorner, _latitude, _longitude, _cellsize, _grid2D/*, _proj*/);
}

Grid2DObject::Grid2DObject(const Grid2DObject& _grid2Dobj, const unsigned int& _nx, const unsigned int& _ny,
				const unsigned int& _ncols, const unsigned int& _nrows) 
	: grid2D(_grid2Dobj.grid2D, _nx,_ny, _ncols,_nrows)
{

	setValues(_ncols, _nrows, (_grid2Dobj.xllcorner+_nx*_grid2Dobj.cellsize),
		(_grid2Dobj.yllcorner+_ny*_grid2Dobj.cellsize), 
		IOUtils::nodata, IOUtils::nodata, _grid2Dobj.cellsize);
}

void Grid2DObject::grid_to_WGS84(const unsigned int& i, const unsigned int& j, double& _latitude, double& _longitude)
{
	const double easting = ((double)i+.5) * cellsize;
	const double northing = ((double)j+.5) * cellsize;

	MapProj::local_to_WGS84(latitude, longitude, easting, northing, _latitude, _longitude, MapProj::GEO_COSINE);
}

int Grid2DObject::WGS84_to_grid(const double& _latitude, const double& _longitude, unsigned int& i, unsigned int& j)
{
	double easting, northing;
	MapProj::WGS84_to_local(latitude, longitude, _latitude, _longitude, easting, northing, MapProj::GEO_COSINE);
	
	double x = floor(easting/cellsize);
	double y = floor(northing/cellsize);

	int error_code=EXIT_SUCCESS;
	
	if(x<0.) {
		i=0;
		error_code=EXIT_FAILURE;
	} else if(x>(double)ncols) {
		i=ncols;
		error_code=EXIT_FAILURE;
	} else {
		i=(int)x;
	}
	
	if(y<0.) {
		j=0;
		error_code=EXIT_FAILURE;
	} else if(y>(double)nrows) {
		j=nrows;
		error_code=EXIT_FAILURE;
	} else {
		j=(int)y;
	}

	return error_code;
}

void Grid2DObject::set(const unsigned int& _ncols, const unsigned int& _nrows,
				const double& _xllcorner, const double& _yllcorner,
				const double& _latitude, const double& _longitude,
				const double& _cellsize/*, 
				const MapProj& _proj*/)
{
	setValues(_ncols, _nrows, _xllcorner, _yllcorner, _latitude, _longitude, _cellsize/*, _proj*/);
	grid2D.resize(ncols, nrows, IOUtils::nodata);
}

void Grid2DObject::set(const unsigned int& _ncols, const unsigned int& _nrows,
				const double& _xllcorner, const double& _yllcorner,
				const double& _latitude, const double& _longitude,
				const double& _cellsize, const Array2D<double>& _grid2D/*, 
				const MapProj& _proj*/)
{
	//Test for equality in size: Only compatible Array2D<double> grids are permitted
	unsigned int nx,ny;
	_grid2D.size(nx,ny);
	if ((_ncols != nx) || (_nrows != ny)) {
		throw IOException("Mismatch in size of Array2D<double> parameter _grid2D and size of Grid2DObject", AT);
	}

	setValues(_ncols, _nrows, _xllcorner, _yllcorner, _latitude, _longitude, _cellsize/*, _proj*/);

	//Copy by value, after destroying the old grid
	grid2D = _grid2D;
}

void Grid2DObject::setValues(const unsigned int& _ncols, const unsigned int& _nrows,
				const double& _xllcorner, const double& _yllcorner,
				const double& _latitude, const double& _longitude, const double& _cellsize/*, 
				const MapProj& proj*/)
{
	ncols = _ncols;
	nrows = _nrows;
	cellsize = _cellsize;
	xllcorner = _xllcorner;
	yllcorner = _yllcorner;
	latitude = _latitude;
	longitude = _longitude;

	//const MapProj dummy;
	//if(proj!=dummy) {
	//	checkCoordinates(proj);
	//}
}

void Grid2DObject::checkCoordinates(const MapProj& proj)
{
	//calculate/check coordinates if necessary
	if(latitude==IOUtils::nodata || longitude==IOUtils::nodata) {
		if(xllcorner==IOUtils::nodata || yllcorner==IOUtils::nodata) {
			throw InvalidArgumentException("missing positional parameters (xll,yll) or (lat,long) for Grid2DObject", AT);
		}
		proj.convert_to_WGS84(xllcorner, yllcorner, latitude, longitude);
	} else {
		if(xllcorner==IOUtils::nodata || yllcorner==IOUtils::nodata) {
			proj.convert_from_WGS84(latitude, longitude, xllcorner, yllcorner);
		} else {
			double tmp_lat, tmp_lon;
			proj.convert_to_WGS84(xllcorner, yllcorner, tmp_lat, tmp_lon);
			if(!IOUtils::checkEpsilonEquality(latitude, tmp_lat, 1.e-4) || !IOUtils::checkEpsilonEquality(longitude, tmp_lon, 1.e-4)) {
				throw InvalidArgumentException("Latitude/longitude and xllcorner/yllcorner don't match for Grid2DObject", AT);
			}
		}
	}
}

bool Grid2DObject::isSameGeolocalization(const Grid2DObject& target)
{
	if( ncols==target.ncols && nrows==target.nrows &&
		IOUtils::checkEpsilonEquality(latitude, target.latitude, 1.e-4) && 
		IOUtils::checkEpsilonEquality(longitude, target.longitude, 1.e-4) &&
		cellsize==target.cellsize) {
		return true;
	} else {
		return false;
	}
}

#ifdef _POPC_
#include "marshal_meteoio.h"
void Grid2DObject::Serialize(POPBuffer &buf, bool pack)
{
	if (pack)
	{
		buf.Pack(&ncols,1);
		buf.Pack(&nrows,1);
		buf.Pack(&xllcorner,1);
		buf.Pack(&yllcorner,1);
		buf.Pack(&latitude,1);
		buf.Pack(&longitude,1);
		buf.Pack(&cellsize,1);
		unsigned int x,y;
		//DEBUG("ncols%d,nrows%d,xllcorner%d,yllcorner%d,celsize%d",ncols,nrows,xllcorner,yllcorner,cellsize);
		grid2D.size(x,y);
		marshal_TYPE_DOUBLE2D(buf, grid2D, 0, FLAG_MARSHAL, NULL);
	}
	else
	{
		buf.UnPack(&ncols,1);
		buf.UnPack(&nrows,1);
		buf.UnPack(&xllcorner,1);
		buf.UnPack(&yllcorner,1);
		buf.UnPack(&latitude,1);
		buf.UnPack(&longitude,1);
		buf.UnPack(&cellsize,1);
		//DEBUG("ncols%d,nrows%d,xllcorner%d,yllcorner%d,celsize%d",ncols,nrows,xllcorner,yllcorner,cellsize);
		grid2D.clear();//if(grid2D!=NULL)delete(grid2D);
		marshal_TYPE_DOUBLE2D(buf, grid2D, 0, !FLAG_MARSHAL, NULL);
	}
}
#endif

