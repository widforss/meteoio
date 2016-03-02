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
#include <meteoio/meteoLaws/Meteoconst.h> //for PI
#include <meteoio/MathOptim.h>

#include <string.h>
#include <algorithm>

#include <meteoio/plugins/ARPSIO.h>

using namespace std;

namespace mio {
/**
 * @page arps ARPSIO
 * @section arps_format Format
 * This is for reading grid data in the ARPS grid format (it transparently supports both true ARPS ascii grids and 
 * grids modified by the ARPSGRID utility). DEM reading works well while reading meteo parameters might be a rough ride 
 * (since ARPS files do not always contain a consistent set of meteo fields).
 * 
 * It is possible to manually extract a whole section, for a given variable with the following script, in order to ease debugging:
 * @code
 * awk '/^ [a-z]/{isVar=0} /^ radsw/{isVar=1;next} {if(isVar) print $0}' {arps_file}
 * @endcode
 *
 * @section arps_units Units
 * All units are assumed to be MKSA.
 *
 * @section arps_keywords Keywords
 * This plugin uses the following keywords:
 * - COORDSYS: coordinate system (see Coords); [Input] and [Output] section
 * - COORDPARAM: extra coordinates parameters (see Coords); [Input] and [Output] section
 * - DEMFILE: path and file containing the DEM; [Input] section
 * - ARPS_XCOORD: x coordinate of the lower left corner of the grids; [Input] section
 * - ARPS_YCOORD: y coordinate of the lower left corner of the grids; [Input] section
 * - GRID2DPATH: path to the input directory where to find the arps files to be read as grids; [Input] section
 * - GRID2DEXT: arps file extension, or <i>none</i> for no file extension (default: .asc)
 */

const double ARPSIO::plugin_nodata = -999.; //plugin specific nodata value
const std::string ARPSIO::default_ext=".asc"; //filename extension

ARPSIO::ARPSIO(const std::string& configfile)
        : cfg(configfile),
          coordin(), coordinparam(), coordout(), coordoutparam(),
          grid2dpath_in(), ext(default_ext), dimx(0), dimy(0), dimz(0), cellsize(0.),
          xcoord(IOUtils::nodata), ycoord(IOUtils::nodata), zcoord(), is_true_arps(true)
{
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
	setOptions();
}

ARPSIO::ARPSIO(const Config& cfgreader)
        : cfg(cfgreader),
          coordin(), coordinparam(), coordout(), coordoutparam(),
          grid2dpath_in(), ext(default_ext), dimx(0), dimy(0), dimz(0), cellsize(0.),
          xcoord(IOUtils::nodata), ycoord(IOUtils::nodata), zcoord(), is_true_arps(true)
{
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
	setOptions();
}

ARPSIO& ARPSIO::operator=(const ARPSIO& source) {
	if (this != &source) {
		coordin = source.coordin;
		coordinparam = source.coordinparam;
		coordout = source.coordout;
		coordoutparam = source.coordoutparam;
		grid2dpath_in = source. grid2dpath_in;
		ext = source.ext;
		dimx = source.dimx;
		dimy = source.dimy;
		dimz = source.dimz;
		cellsize = source.cellsize;
		xcoord = source.xcoord;
		ycoord = source.ycoord;
		zcoord = source.zcoord;
		is_true_arps = source.is_true_arps;
	}
	return *this;
}

void ARPSIO::setOptions()
{
	const string grid_in = cfg.get("GRID2D", "Input", IOUtils::nothrow);
	if (grid_in == "ARPS") { //keep it synchronized with IOHandler.cc for plugin mapping!!
		cfg.getValue("GRID2DPATH", "Input", grid2dpath_in);
	}

	cfg.getValue("ARPS_XCOORD", "Input", xcoord, IOUtils::nothrow);
	cfg.getValue("ARPS_YCOORD", "Input", ycoord, IOUtils::nothrow);

	//default value has been set in constructor
	cfg.getValue("GRID2DEXT", "Input", ext, IOUtils::nothrow);
	if (ext=="none") ext.clear();
}

void ARPSIO::listFields(const std::string& i_name)
{
	const size_t pos = i_name.find_last_of(":");//a specific parameter can be provided as {filename}:{parameter}
	const std::string filename = (pos!=IOUtils::npos)? grid2dpath_in +"/" + i_name.substr(0, pos) : grid2dpath_in +"/" + i_name;
	
	FILE *fin;
	openGridFile(fin, filename);
	const std::string dem_marker = (is_true_arps)? "zp coordinat" : "zp_coordinat";
	moveToMarker(fin, filename, dem_marker);
	
	char dummy[ARPS_MAX_LINE_LENGTH];
	int nb_elems = 0;
	do {
		nb_elems = fscanf(fin," %[^\t\n] ",dummy); //HACK: possible buffer overflow
		if (dummy[0]>='A' && dummy[0]<='z') { //white spaces have been skipped above
			std::string tmp( dummy );
			IOUtils::trim(tmp);
			std::cout << "Found '" << tmp << "'\n";
		}
	} while (!feof(fin) && nb_elems!=0);
	
	fclose(fin);
}

void ARPSIO::read2DGrid(Grid2DObject& grid_out, const std::string& i_name)
{
	const size_t pos = i_name.find_last_of(":");//a specific parameter can be provided as {filename}:{parameter}
	const std::string filename = (pos!=IOUtils::npos)? grid2dpath_in +"/" + i_name.substr(0, pos) : grid2dpath_in +"/" + i_name;
	if (pos==IOUtils::npos) { //TODO: read by default the first data grid that is found?
		listFields(i_name);
		throw InvalidArgumentException("Please provide the parameter that has to be read!", AT);
	}
	
	const std::string param_str = (pos!=IOUtils::npos)?  i_name.substr(pos+1) : "";
	const size_t param = MeteoGrids::getParameterIndex( param_str );
	if (param==IOUtils::npos)
		throw InvalidArgumentException("Invalid MeteoGrids Parameter requested: '"+param_str+"'", AT);
	
	FILE *fin;
	openGridFile(fin, filename);
	read2DGrid_internal(fin, filename, grid_out, static_cast<MeteoGrids::Parameters>(param));
	if (grid_out.empty()) {
		ostringstream ss;
		ss << "No suitable data found for parameter " << MeteoGrids::getParameterName(param) << " ";
		ss << " in file \"" << filename << "\"";
		throw NoDataException(ss.str(), AT);
	}
	fclose(fin);
}

void ARPSIO::read2DGrid(Grid2DObject& grid_out, const MeteoGrids::Parameters& parameter, const Date& date)
{
	std::string date_str = date.toString(Date::ISO);
	std::replace( date_str.begin(), date_str.end(), ':', '.');
	const std::string filename = grid2dpath_in + "/" + date_str + ext;
	FILE *fin;
	openGridFile(fin, filename);
	read2DGrid_internal(fin, filename, grid_out, parameter);
	if (grid_out.empty()) {
		ostringstream ss;
		ss << "No suitable data found for parameter " << MeteoGrids::getParameterName(parameter) << " ";
		ss << "at time step " << date.toString(Date::ISO) << " in file \"" << filename << "\"";
		throw NoDataException(ss.str(), AT);
	}

	fclose(fin);
}

void ARPSIO::read2DGrid_internal(FILE* &fin, const std::string& filename, Grid2DObject& grid_out, const MeteoGrids::Parameters& parameter)
{
	//extra variables: 
	//kmh / kmv: horiz./vert. turbulent mixing coeff. m^2/s
	//tke: turbulent kinetic energy. (m/s)^2
	
	//Radiation parameters
	if (parameter==MeteoGrids::ISWR) readGridLayer(fin, filename, "radsw", 1, grid_out);
	if (parameter==MeteoGrids::RSWR) {
		Grid2DObject net;
		readGridLayer(fin, filename, "radsw", 1, grid_out);
		readGridLayer(fin, filename, "radswnet", 1, net);
		grid_out.grid2D -= net.grid2D;
	}
	if (parameter==MeteoGrids::ILWR) readGridLayer(fin, filename, "radlwin", 1, grid_out);
	if (parameter==MeteoGrids::ALB) {
		Grid2DObject rswr, iswr;
		readGridLayer(fin, filename, "radsw", 1, iswr);
		readGridLayer(fin, filename, "radswnet", 1, grid_out); //net radiation
		rswr.grid2D = iswr.grid2D - grid_out.grid2D;
		grid_out.grid2D = iswr.grid2D/rswr.grid2D;
	}

	//Wind grids
	if (parameter==MeteoGrids::U) readGridLayer(fin, filename, "u", 2, grid_out);
	if (parameter==MeteoGrids::V) readGridLayer(fin, filename, "v", 2, grid_out);
	if (parameter==MeteoGrids::W) readGridLayer(fin, filename, "w", 2, grid_out);
	if (parameter==MeteoGrids::VW) {
		Grid2DObject V;
		readGridLayer(fin, filename, "u", 2, grid_out); //U
		readGridLayer(fin, filename, "v", 2, V);
		for (size_t jj=0; jj<grid_out.getNy(); jj++) {
			for (size_t ii=0; ii<grid_out.getNx(); ii++) {
				grid_out(ii,jj) = sqrt( Optim::pow2(grid_out(ii,jj)) + Optim::pow2(V(ii,jj)) );
			}
		}
	}
	if (parameter==MeteoGrids::DW) {
		Grid2DObject V;
		readGridLayer(fin, filename, "u", 2, grid_out); //U
		readGridLayer(fin, filename, "v", 2, V);
		for (size_t jj=0; jj<grid_out.getNy(); jj++) {
			for (size_t ii=0; ii<grid_out.getNx(); ii++) {
				grid_out(ii,jj) = fmod( atan2( grid_out(ii,jj), V(ii,jj) ) * Cst::to_deg + 360., 360.); // turn into degrees [0;360)
			}
		}
	}

	//Basic meteo parameters
	if (parameter==MeteoGrids::P) readGridLayer(fin, filename, "p", 2, grid_out);
	if (parameter==MeteoGrids::TSG) readGridLayer(fin, filename, "tsoil", 1, grid_out); //or is it tss for us?
	/*if (parameter==MeteoGrids::RH) {
		//const double epsilon = Cst::gaz_constant_dry_air / Cst::gaz_constant_water_vapor;
		readGridLayer(filename, "qv", 2, grid_out); //water vapor mixing ratio
		//Atmosphere::waterSaturationPressure(T);
		//TODO: compute relative humidity out of it!
		//through potential temperature "pt" -> local temperature?
	}*/

	//Hydrological parameters
	if (parameter==MeteoGrids::HS) readGridLayer(fin, filename, "snowdpth", 1, grid_out);
	if (parameter==MeteoGrids::PSUM) {
		readGridLayer(fin, filename, "prcrate1", 1, grid_out); //in kg/m^2/s
		grid_out.grid2D *= 3600.; //we need kg/m^2/h
	}

	//DEM
	const std::string dem_marker = (is_true_arps)? "zp coordinat" : "zp_coordinat";
	if (parameter==MeteoGrids::DEM) readGridLayer(fin, filename, dem_marker, 1, grid_out);
	if (parameter==MeteoGrids::SLOPE) {
		DEMObject dem;
		dem.setUpdatePpt(DEMObject::SLOPE);
		readGridLayer(fin, filename, dem_marker, 1, dem);
		dem.update();
		grid_out.set(dem.cellsize, dem.llcorner, dem.slope);
	}
	if (parameter==MeteoGrids::AZI) {
		DEMObject dem;
		dem.setUpdatePpt(DEMObject::SLOPE);
		readGridLayer(fin, filename, dem_marker, 1, dem);
		dem.update();
		grid_out.set(dem.cellsize, dem.llcorner, dem.azi);
	}

	rewind(fin);
}

void ARPSIO::read3DGrid(Grid3DObject& grid_out, const std::string& i_name)
{
	const size_t pos = i_name.find_last_of(":");//a specific parameter can be provided as {filename}:{parameter}
	const std::string filename = (pos!=IOUtils::npos)? grid2dpath_in +"/" + i_name.substr(0, pos) : grid2dpath_in +"/" + i_name;
	if (pos==IOUtils::npos) { //TODO: read by default the first data grid that is found?
		listFields(i_name);
		throw InvalidArgumentException("Please provide the parameter that has to be read!", AT);
	}
	
	std::string parameter = (pos!=IOUtils::npos)?  i_name.substr(pos+1) : "";
	if (parameter=="DEM") { //this is called damage control... this is so ugly...
		parameter = (is_true_arps)? "zp coordinat" : "zp_coordinat";
	}
	
	FILE *fin;
	openGridFile(fin, filename);

	//resize the grid just in case
	Coords llcorner(coordin, coordinparam);
	llcorner.setXY(xcoord, ycoord, IOUtils::nodata);
	grid_out.set(dimx, dimy, dimz, cellsize, llcorner);
	grid_out.z = zcoord;

	// Read until the parameter is found
	moveToMarker(fin, filename, parameter);

	//read the data we are interested in
	for (size_t ix = 0; ix < dimx; ix++) {
		for (size_t iy = 0; iy < dimy; iy++) {
			for (size_t iz = 0; iz < dimz; iz++) {
				double tmp;
				if (fscanf(fin," %16lf%*[\n]",&tmp)==1) {
					grid_out.grid3D(ix,iy,iz) = tmp;
				} else {
					fclose(fin);
					throw InvalidFormatException("Failure in reading 3D grid in file "+filename, AT);
				}
			}
		}
	}

	fclose(fin);
}

void ARPSIO::readDEM(DEMObject& dem_out)
{
	const std::string filename = cfg.get("DEMFILE", "Input");
	FILE *fin;
	openGridFile(fin, filename);
	read2DGrid_internal(fin, filename, dem_out, MeteoGrids::DEM);
	fclose(fin);
}

void ARPSIO::readLanduse(Grid2DObject& /*landuse_out*/)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void ARPSIO::readAssimilationData(const Date& /*date_in*/, Grid2DObject& /*da_out*/)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void ARPSIO::readStationData(const Date&, std::vector<StationData>& /*vecStation*/)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void ARPSIO::readMeteoData(const Date& /*dateStart*/, const Date& /*dateEnd*/,
                           std::vector< std::vector<MeteoData> >& /*vecMeteo*/,
                           const size_t&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void ARPSIO::writeMeteoData(const std::vector< std::vector<MeteoData> >& /*vecMeteo*/,
                            const std::string&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void ARPSIO::readPOI(std::vector<Coords>&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void ARPSIO::write2DGrid(const Grid2DObject& /*grid_in*/, const std::string& /*name*/)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void ARPSIO::write2DGrid(const Grid2DObject&, const MeteoGrids::Parameters&, const Date&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void ARPSIO::initializeGRIDARPS(FILE* &fin, const std::string& filename)
{
	double v1, v2;

	//go to read the sizes
	moveToMarker(fin, filename, "nnx");
	//finish reading the line and move to the next one
	if (fscanf(fin,"%*[^\n]")!=0) {
		fclose(fin);
		throw InvalidFormatException("Error in file format of file "+filename, AT);
	}
	if (fscanf(fin," %u %u %u \n",&dimx,&dimy,&dimz)!=3) {
		fclose(fin);
		throw InvalidFormatException("Can not read dimx, dimy, dimz from file "+filename, AT);
	}
	if (dimx==0 || dimy==0 || dimz==0) {
		fclose(fin);
		throw IndexOutOfBoundsException("Invalid dimx, dimy, dimz from file "+filename, AT);
	}

	//initializing cell size
	moveToMarker(fin, filename, "x_coordinate");
	if (fscanf(fin,"%lg %lg",&v1,&v2)!=2) {
		fclose(fin);
		throw InvalidFormatException("Can not read first two x coordinates from file "+filename, AT);
	}
	const double cellsize_x = v2 - v1;
	moveToMarker(fin, filename, "y_coordinate");
	if (fscanf(fin,"%lg %lg",&v1,&v2)!=2) {
		fclose(fin);
		throw InvalidFormatException("Can not read first two y coordinates from file "+filename, AT);
	}
	const double cellsize_y = v2 - v1;
	if (cellsize_x!=cellsize_y) {
		fclose(fin);
		throw InvalidFormatException("Only square cells currently supported! Non compliance in file "+filename, AT);
	}
	cellsize = cellsize_y;

	//HACK: zcoords must be read from zp. But they are NOT constant...

}

void ARPSIO::initializeTrueARPS(FILE* &fin, const std::string& filename, const char curr_line[ARPS_MAX_LINE_LENGTH])
{
	double v1, v2;

	//go to read the sizes
	if (sscanf(curr_line," nx = %u, ny = %u, nz = %u ",&dimx,&dimy,&dimz)!=3) {
		fclose(fin);
		throw InvalidFormatException("Can not read dimx, dimy, dimz from file "+filename, AT);
	}
	if (dimx==0 || dimy==0 || dimz==0) {
		fclose(fin);
		throw IndexOutOfBoundsException("Invalid dimx, dimy, dimz from file "+filename, AT);
	}

	//initializing cell size
	moveToMarker(fin, filename, "x coordinate");
	if (fscanf(fin,"%lg %lg",&v1,&v2)!=2) {
		fclose(fin);
		throw InvalidFormatException("Can not read first two x coordinates from file "+filename, AT);
	}
	const double cellsize_x = v2 - v1;
	moveToMarker(fin, filename, "y coordinate");
	if (fscanf(fin,"%lg %lg",&v1,&v2)!=2) {
		fclose(fin);
		throw InvalidFormatException("Can not read first two y coordinates from file "+filename, AT);
	}
	const double cellsize_y = v2 - v1;
	if (cellsize_x!=cellsize_y) {
		fclose(fin);
		throw InvalidFormatException("Only square cells currently supported! Non compliance in file "+filename, AT);
	}
	cellsize = cellsize_y;

	moveToMarker(fin, filename, "z coordinate");
	while (fscanf(fin,"%lg",&v1)==1) {
		zcoord.push_back( v1 );
	}
	if (zcoord.size()!=dimz) {
		ostringstream ss;
		ss << "Expected " << dimz << " z coordinates in file \""+filename+"\", found " << zcoord.size();
		fclose(fin);
		throw InvalidFormatException(ss.str(), AT);
	}
}

void ARPSIO::openGridFile(FILE* &fin, const std::string& filename)
{
	unsigned int v1;

	if (!IOUtils::fileExists(filename)) throw AccessException(filename, AT); //prevent invalid filenames
	if ((fin=fopen(filename.c_str(),"r")) == NULL) {
		fclose(fin);
		throw AccessException("Can not open file "+filename, AT);
	}

	//identify if the file is an original arps file or a file modified by ARPSGRID
	char dummy[ARPS_MAX_LINE_LENGTH];
	for (unsigned char j=0; j<5; j++) {
		//the first easy difference in the structure happens at line 5
		if (fgets(dummy,ARPS_MAX_STRING_LENGTH,fin)==NULL) {
			fclose(fin);
			throw InvalidFormatException("Fail to read header lines of file "+filename, AT);
		}
	}
	zcoord.clear(); //reset the zcoord
	if (sscanf(dummy," nx = %u, ny = ", &v1)<1) {
		//this is an ASCII file modified by ARPSGRID
		is_true_arps=false;
		initializeGRIDARPS(fin, filename);
	} else {
		//this is a true ARPS file
		initializeTrueARPS(fin, filename, dummy);
	}
	//set xcoord, ycoord to a default value if not set by the user
	if (xcoord==IOUtils::nodata) xcoord = -cellsize;
	if (ycoord==IOUtils::nodata) ycoord = -cellsize;

	//come back to the begining of the file
	rewind(fin);
}

/** @brief Read a specific layer for a given parameter from the ARPS file
 * @param parameter The parameter to extract. This could be any of the following:
 *        - x_coordinate for getting the X coordinates of the mesh
 *        - y_coordinate for getting the Y coordinates of the mesh
 *        - zp_coordinat for getting the Z coordinates of the mesh
 *        - u for getting the u component of the wind field
 *        - v for getting the v component of the wind field
 *        - w for getting the w component of the wind field
 * @param layer     Index of the layer to extract (1 to dimz)
 * @param grid      [out] grid containing the values. The grid will be resized if necessary.
*/
void ARPSIO::readGridLayer(FILE* &fin, const std::string& filename, const std::string& parameter, const unsigned int& layer, Grid2DObject& grid)
{
	if (layer<1 || layer>dimz) {
		fclose(fin);
		ostringstream tmp;
		tmp << "Layer " << layer << " does not exist in ARPS file " << filename << " (nr layers=" << dimz << ")";
		throw IndexOutOfBoundsException(tmp.str(), AT);
	}

	//resize the grid just in case
	Coords llcorner(coordin, coordinparam);
	llcorner.setXY(xcoord, ycoord, IOUtils::nodata);
	grid.set(dimx, dimy, cellsize, llcorner);

	// Read until the parameter is found
	moveToMarker(fin, filename, parameter);

	// move to the begining of the layer of interest
	if (layer>1) {
		double tmp;
		const size_t jmax=dimx*dimy*(layer-1);
		for (size_t j = 0; j < jmax; j++) {
			if (fscanf(fin," %16lf%*[\n]",&tmp)==EOF) {
				fclose(fin);
				throw InvalidFormatException("Fail to skip data layers in file "+filename, AT);
			}
		}
	}

	//read the data we are interested in
	for (size_t iy = 0; iy < dimy; iy++) {
		for (size_t ix = 0; ix < dimx; ix++) {
			double tmp;
			const int readPts = fscanf(fin," %16lf%*[\n]",&tmp);
			if (readPts==1) {
				grid(ix,iy) = tmp;
			} else {
				char dummy[ARPS_MAX_LINE_LENGTH];
				const int status = fscanf(fin,"%s",dummy);
				fclose(fin);
				string msg = "Fail to read data layer for parameter '"+parameter+"' in file '"+filename+"'";
				if (status>0) msg += ", instead read: '"+string(dummy)+"'";
				
				throw InvalidFormatException(msg, AT);
			}
		}
	}
}

void ARPSIO::moveToMarker(FILE* &fin, const std::string& filename, const std::string& marker)
{
	char dummy[ARPS_MAX_LINE_LENGTH];
	do {
		const int nb_elems = fscanf(fin," %[^\t\n] ",dummy); //HACK: possible buffer overflow
		if (nb_elems==0) {
			fclose(fin);
			throw InvalidFormatException("Matching failure in file "+filename+" when looking for "+marker, AT);
		}
		if (dummy[0]>='A' && dummy[0]<='z') { //this is a "marker" line
			if (strncmp(dummy,marker.c_str(), marker.size()) == 0) return;
		}
	} while (!feof(fin));
	
	fclose(fin);
	throw InvalidFormatException("End of file "+filename+" should NOT have been reached when looking for "+marker, AT);
}

} //namespace
