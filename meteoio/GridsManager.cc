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

#include <meteoio/GridsManager.h>
#include <meteoio/dataClasses/Coords.h>
#include <meteoio/meteoLaws/Atmosphere.h>
#include <meteoio/MathOptim.h>

using namespace std;

namespace mio {

GridsManager::GridsManager(IOHandler& in_iohandler, const Config& in_cfg)
             : iohandler(in_iohandler), cfg(in_cfg), buffer(0), grids2d_list(), grids2d_start(), grids2d_end(),
               grid2d_list_buffer_size(370.), processing_level(IOUtils::filtered | IOUtils::resampled | IOUtils::generated)
{
	size_t max_grids = 10;
	cfg.getValue("BUFF_GRIDS", "General", max_grids, IOUtils::nothrow);
	buffer.setMaxGrids(max_grids);
	cfg.getValue("BUFFER_SIZE", "General", grid2d_list_buffer_size, IOUtils::nothrow);
}

/**
* @brief Set the desired ProcessingLevel
*        The processing level affects the way meteo data is read and processed
*        Three values are possible:
*        - IOUtils::raw data shall be read directly from the buffer
*        - IOUtils::filtered data shall be filtered before returned to the user
*        - IOUtils::resampled data shall be resampled before returned to the user
*          this only affects the function getMeteoData(const Date&, METEO_DATASET&);
*
*        The three values can be combined: e.g. IOUtils::filtered | IOUtils:resampled
* @param i_level The ProcessingLevel values that shall be used to process data
*/
void GridsManager::setProcessingLevel(const unsigned int& i_level)
{
	if (i_level >= IOUtils::num_of_levels)
		throw InvalidArgumentException("The processing level is invalid", AT);

	if (((i_level & IOUtils::raw) == IOUtils::raw)
	    && ((i_level & IOUtils::filtered) == IOUtils::filtered))
		throw InvalidArgumentException("The processing level is invalid (raw and filtered at the same time)", AT);

	processing_level = i_level;
}

void GridsManager::read2DGrid(Grid2DObject& grid2D, const std::string& filename)
{
	if (processing_level == IOUtils::raw){
		iohandler.read2DGrid(grid2D, filename);
	} else {
		if (buffer.get(grid2D, filename)) return;

		iohandler.read2DGrid(grid2D, filename);
		buffer.push(grid2D, filename);
	}
}

//do we have the requested parameter either in buffer or raw?
bool GridsManager::isAvailable(const std::set<size_t>& available_params, const MeteoGrids::Parameters& parameter, const Date& date) const
{
	const bool in_buffer = buffer.has(parameter, date);
	if (in_buffer) return true;
	
	const bool in_raw = (available_params.find( parameter ) != available_params.end());
	return in_raw;
}

//get the requested grid either from buffer or by a direct reading
//the goal is that subsequent calls to the same grid find it in the buffer transparently
void GridsManager::getGrid(Grid2DObject& grid2D, const MeteoGrids::Parameters& parameter, const Date& date)
{
	if (buffer.get(grid2D, parameter, date)) return;
	iohandler.read2DGrid(grid2D, parameter, date);
}

//Grids of meteo params are coming out of models, so we assume that all possible parameters
//are available at each time steps. So we don't need to search a combination of parameters and timesteps
bool GridsManager::read2DGrid(Grid2DObject& grid2D, const std::set<size_t>& available_params, const MeteoGrids::Parameters& parameter, const Date& date)
{
	if (parameter==MeteoGrids::VW || parameter==MeteoGrids::DW) {
		const bool hasU = isAvailable(available_params, MeteoGrids::U, date);
		const bool hasV = isAvailable(available_params, MeteoGrids::V, date);
		if (!hasU || !hasV) return false;
		
		Grid2DObject U,V;
		getGrid(U, MeteoGrids::U, date);
		buffer.push(U, MeteoGrids::U, date);
		getGrid(V, MeteoGrids::V, date);
		buffer.push(V, MeteoGrids::V, date);
		
		grid2D.set(U, IOUtils::nodata);
		if (parameter==MeteoGrids::VW) {
			for (size_t ii=0; ii<grid2D.size(); ii++)
				grid2D(ii) = sqrt( Optim::pow2(U(ii)) + Optim::pow2(V(ii)) );
		} else {
			for (size_t ii=0; ii<grid2D.size(); ii++)
				grid2D(ii) =  fmod( atan2( U(ii), V(ii) ) * Cst::to_deg + 360., 360.); // turn into degrees [0;360)
		}
		buffer.push(grid2D, parameter, date);
		return true;
	}
	
	if (parameter==MeteoGrids::RH) {
		const bool hasTA = isAvailable(available_params, MeteoGrids::TA, date);
		if (!hasTA) return false;
		Grid2DObject TA;
		getGrid(TA, MeteoGrids::TA, date);
		buffer.push(TA, MeteoGrids::TA, date);
		
		const bool hasDEM = isAvailable(available_params, MeteoGrids::DEM, date);
		const bool hasQI = isAvailable(available_params, MeteoGrids::QI, date);
		const bool hasTD = isAvailable(available_params, MeteoGrids::TD, date);
		
		if (hasTA && hasTD) {
			getGrid(grid2D, MeteoGrids::TD, date);
			buffer.push(grid2D, MeteoGrids::TD, date);
			
			for (size_t ii=0; ii<grid2D.size(); ii++)
				grid2D(ii) = Atmosphere::DewPointtoRh(grid2D(ii), TA(ii), false);
			return true;
		} else if (hasQI && hasDEM && hasTA) {
			Grid2DObject dem;
			getGrid(dem, MeteoGrids::DEM, date); //HACK use readDEM instead?
			getGrid(grid2D, MeteoGrids::QI, date);
			buffer.push(grid2D, MeteoGrids::QI, date);
			for (size_t ii=0; ii<grid2D.size(); ii++)
				grid2D(ii) = Atmosphere::specToRelHumidity(dem(ii), TA(ii), grid2D(ii));
			return true;
		}
		
		return false;
	}
	
	if (parameter==MeteoGrids::ISWR) {
		const bool hasISWR_DIFF = isAvailable(available_params, MeteoGrids::ISWR_DIFF, date);
		const bool hasISWR_DIR = isAvailable(available_params, MeteoGrids::ISWR_DIR, date);
		
		if (hasISWR_DIFF && hasISWR_DIR) {
			Grid2DObject iswr_diff;
			getGrid(iswr_diff, MeteoGrids::ISWR_DIFF, date);
			getGrid(grid2D, MeteoGrids::ISWR_DIR, date);
			grid2D += iswr_diff;
			buffer.push(grid2D, MeteoGrids::ISWR, date);
			return true;
		}
		
		const bool hasALB = isAvailable(available_params, MeteoGrids::ALB, date);
		const bool hasRSWR = isAvailable(available_params, MeteoGrids::ISWR, date);
		if (hasRSWR && hasALB) {
			Grid2DObject alb;
			getGrid(alb, MeteoGrids::ALB, date);
			getGrid(grid2D, MeteoGrids::RSWR, date);
			grid2D /= alb;
			buffer.push(grid2D, MeteoGrids::ISWR, date);
			return true;
		}
		
		return false;
	}
	
	if (parameter==MeteoGrids::RSWR) {
		const bool hasALB = isAvailable(available_params, MeteoGrids::ALB, date);
		if (!hasALB) return false;
		const bool hasISWR = isAvailable(available_params, MeteoGrids::ISWR, date);
		
		if (!hasISWR) {
			const bool hasISWR_DIFF = isAvailable(available_params, MeteoGrids::ISWR_DIFF, date);
			const bool hasISWR_DIR = isAvailable(available_params, MeteoGrids::ISWR_DIR, date);
			if (!hasISWR_DIFF || !hasISWR_DIR) return false;
			
			Grid2DObject iswr_diff;
			getGrid(iswr_diff, MeteoGrids::ISWR_DIFF, date);
			getGrid(grid2D, MeteoGrids::ISWR_DIR, date);
			grid2D += iswr_diff;
			buffer.push(grid2D, MeteoGrids::ISWR, date);
		} else {
			getGrid(grid2D, MeteoGrids::ISWR, date);
		}
		
		Grid2DObject alb;
		getGrid(alb, MeteoGrids::ALB, date);
		
		grid2D *= alb;
		buffer.push(grid2D, MeteoGrids::RSWR, date);
		return true;
	}
	
	if (parameter==MeteoGrids::HS) {
		const bool hasRSNO = isAvailable(available_params, MeteoGrids::RSNO, date);
		const bool hasSWE = isAvailable(available_params, MeteoGrids::SWE, date);
		
		if (hasRSNO && hasSWE) {
			Grid2DObject rsno;
			getGrid(rsno, MeteoGrids::RSNO, date);
			getGrid(grid2D, MeteoGrids::SWE, date);
			grid2D *= 1000.0; //convert mm=kg/m^3 into kg
			grid2D /= rsno;
			buffer.push(grid2D, MeteoGrids::HS, date);
			return true;
		}
		return false;
	}
	
	if (parameter==MeteoGrids::PSUM) {
		const bool hasPSUM_S = isAvailable(available_params, MeteoGrids::PSUM_S, date);
		const bool hasPSUM_L = isAvailable(available_params, MeteoGrids::PSUM_L, date);
		
		if (hasPSUM_S && hasPSUM_L) {
			Grid2DObject psum_l;
			getGrid(grid2D, MeteoGrids::PSUM_S, date);
			buffer.push(grid2D, MeteoGrids::PSUM_S, date);
			getGrid(psum_l, MeteoGrids::PSUM_L, date);
			buffer.push(psum_l, MeteoGrids::PSUM_L, date);
			grid2D += psum_l;
			buffer.push(grid2D, MeteoGrids::PSUM, date);
			return true;
		}
	}
	
	if (parameter==MeteoGrids::PSUM_PH) {
		const bool hasPSUM_S = isAvailable(available_params, MeteoGrids::PSUM_S, date);
		const bool hasPSUM_L = isAvailable(available_params, MeteoGrids::PSUM_L, date);
		
		if (hasPSUM_S && hasPSUM_L) {
			Grid2DObject psum_l;
			getGrid(grid2D, MeteoGrids::PSUM_S, date);
			buffer.push(grid2D, MeteoGrids::PSUM_S, date);
			getGrid(psum_l, MeteoGrids::PSUM_L, date);
			buffer.push(psum_l, MeteoGrids::PSUM_L, date);
			grid2D += psum_l;
			buffer.push(grid2D, MeteoGrids::PSUM, date);
			
			for (size_t ii=0; ii<grid2D.size(); ii++) {
				const double psum = grid2D(ii);
				if (psum!=IOUtils::nodata && psum>0)
					grid2D(ii) = psum_l(ii) / psum;
			}
			return true;
		}
		
		return false;
	}
	
	return false;
}

void GridsManager::read2DGrid(Grid2DObject& grid2D, const MeteoGrids::Parameters& parameter, const Date& date)
{
	if (processing_level == IOUtils::raw){
		iohandler.read2DGrid(grid2D, parameter, date);
	} else {
		if (buffer.get(grid2D, parameter, date)) return;
		
		//should we rebuffer the grids list?
		if (grids2d_list.empty() || date<grids2d_start || date>grids2d_end) {
			grids2d_start = date - 1.;
			grids2d_end = date + grid2d_list_buffer_size;
			const bool status = iohandler.list2DGrids(grids2d_start, grids2d_end, grids2d_list);
			if (status) {
				//the plugin might have returned a range larger than requested, so adjust the min/max dates if necessary
				if (!grids2d_list.empty()) {
					if (grids2d_start > grids2d_list.begin()->first) grids2d_start = grids2d_list.begin()->first;
					if (grids2d_end < grids2d_list.rbegin()->first) grids2d_end = grids2d_list.rbegin()->first;
				}
			} else { //this means that the call is not implemeted in the plugin
				iohandler.read2DGrid(grid2D, parameter, date);
				buffer.push(grid2D, parameter, date);
				return;
			}
		}
		
		const std::map<Date, std::set<size_t> >::const_iterator it = grids2d_list.find(date);
		if (it!=grids2d_list.end()) {
			if ( it->second.find(parameter) != it->second.end() ) {
				iohandler.read2DGrid(grid2D, parameter, date);
				buffer.push(grid2D, parameter, date);
			} else { //the right parameter could not be found, can we generate it?
				if (!read2DGrid(grid2D, it->second, parameter, date))
					throw NoDataException("Could not find or generate a grid of "+MeteoGrids::getParameterName( parameter )+" at time "+date.toString(Date::ISO), AT);
			}
		} else {
			throw NoDataException("Could not find any grids at time "+date.toString(Date::ISO), AT);
		}
	}
}

//HACK buffer 3D grids!
void GridsManager::read3DGrid(Grid3DObject& grid_out, const std::string& i_filename)
{
	iohandler.read3DGrid(grid_out, i_filename);
}

void GridsManager::read3DGrid(Grid3DObject& grid_out, const MeteoGrids::Parameters& parameter, const Date& date)
{
	iohandler.read3DGrid(grid_out, parameter, date);
}

void GridsManager::readDEM(DEMObject& grid2D)
{
	if (processing_level == IOUtils::raw){
		iohandler.readDEM(grid2D);
	} else {
		if (buffer.get(grid2D, "/:DEM"))
			return;

		iohandler.readDEM(grid2D);
		buffer.push(grid2D, "/:DEM");
	}
}

void GridsManager::readLanduse(Grid2DObject& grid2D)
{
	if (processing_level == IOUtils::raw){
		iohandler.readLanduse(grid2D);
	} else {
		if (buffer.get(grid2D, "/:LANDUSE"))
			return;

		iohandler.readLanduse(grid2D);
		buffer.push(grid2D, "/:LANDUSE");
	}
}

void GridsManager::readAssimilationData(const Date& date, Grid2DObject& grid2D)
{
	if (processing_level == IOUtils::raw){
		iohandler.readAssimilationData(date, grid2D);
	} else {
		const string grid_hash = "/:ASSIMILATIONDATA"+date.toString(Date::ISO);
		if (buffer.get(grid2D, grid_hash))
			return;

		iohandler.readAssimilationData(date, grid2D);
		buffer.push(grid2D, grid_hash);
	}
}

void GridsManager::write2DGrid(const Grid2DObject& grid2D, const std::string& name)
{
	iohandler.write2DGrid(grid2D, name);
}

void GridsManager::write2DGrid(const Grid2DObject& grid2D, const MeteoGrids::Parameters& parameter, const Date& date)
{
	iohandler.write2DGrid(grid2D, parameter, date);
}

void GridsManager::write3DGrid(const Grid3DObject& grid_out, const std::string& options)
{
	iohandler.write3DGrid(grid_out, options);
}

void GridsManager::write3DGrid(const Grid3DObject& grid_out, const MeteoGrids::Parameters& parameter, const Date& date)
{
	iohandler.write3DGrid(grid_out, parameter, date);
}

const std::string GridsManager::toString() const {
	ostringstream os;
	os << "<GridsManager>\n";
	os << "Config& cfg = " << hex << &cfg << dec << "\n";
	os << "IOHandler& iohandler = " << hex << &iohandler << dec << "\n";
	os << "Processing level = " << processing_level << "\n";
	os << buffer.toString();
	os << "</GridsManager>\n";
	return os.str();
}

} //namespace
