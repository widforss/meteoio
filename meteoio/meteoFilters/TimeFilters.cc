/***********************************************************************************/
/*  Copyright 2013 WSL Institute for Snow and Avalanche Research    SLF-DAVOS      */
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
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <algorithm>

#include <meteoio/meteoFilters/TimeFilters.h>
#include <meteoio/FileUtils.h>

using namespace std;

namespace mio {

static inline bool IsUndef (const MeteoData& md) { return md.date.isUndef(); }

TimeSuppr::TimeSuppr(const std::vector< std::pair<std::string, std::string> >& vecArgs, const std::string& name, const std::string& root_path, const double& TZ)
          : ProcessingBlock(vecArgs, name), suppr_dates(), range(IOUtils::nodata)
{
	const std::string where( "Filters::"+block_name );
	properties.stage = ProcessingProperties::first; //for the rest: default values
	const size_t nrArgs = vecArgs.size();

	if (nrArgs!=1)
		throw InvalidArgumentException("Wrong number of arguments for " + where, AT);

	if (vecArgs[0].first=="FRAC") {
		if (!IOUtils::convertString(range, vecArgs[0].second))
			throw InvalidArgumentException("Invalid range \""+vecArgs[0].second+"\" specified for "+where, AT);
		if (range<0. || range>1.)
			throw InvalidArgumentException("Wrong range for " + where + ", it should be between 0 and 1", AT);
	} else if (vecArgs[0].first=="SUPPR") {
		const std::string in_filename( vecArgs[0].second );
		const std::string prefix = ( FileUtils::isAbsolutePath(in_filename) )? "" : root_path+"/";
		const std::string path( FileUtils::getPath(prefix+in_filename, true) );  //clean & resolve path
		const std::string filename( path + "/" + FileUtils::getFilename(in_filename) );

		suppr_dates = ProcessingBlock::readDates(block_name, filename, TZ);
	} else
		throw UnknownValueException("Unknown option '"+vecArgs[0].first+"' for "+where, AT);
}

void TimeSuppr::process(const unsigned int& param, const std::vector<MeteoData>& ivec,
                        std::vector<MeteoData>& ovec)
{
	if (param!=IOUtils::unodata)
		throw InvalidArgumentException("The filter "+block_name+" can only be applied to TIME", AT);
	
	ovec = ivec;
	if (ovec.empty()) return;
	
	if (!suppr_dates.empty()) {
		supprByDates(ovec);
	} else { //only remove a given fraction
		supprFrac(ovec);
	}
}

//this assumes that the DATES_RANGEs in suppr_dates have been sorted by increasing starting dates
void TimeSuppr::supprByDates(std::vector<MeteoData>& ovec) const
{
	const std::string station_ID( ovec[0].meta.stationID ); //we know it is not empty
	const std::map< std::string, std::vector<dates_range> >::const_iterator station_it( suppr_dates.find( station_ID ) );
	if (station_it==suppr_dates.end()) return;

	const std::vector<dates_range> &suppr_specs = station_it->second;
	const size_t Nset = suppr_specs.size();
	size_t curr_idx = 0; //we know there is at least one
	for (size_t ii=0; ii<ovec.size(); ii++) {
		if (ovec[ii].date<suppr_specs[curr_idx].start) continue;

		if (ovec[ii].date<=suppr_specs[curr_idx].end) { //suppress the interval
			ovec[ii].date.setUndef(true); //mark the point to be removed
			continue;
		} else { //look for the next interval
			curr_idx++;
			if (curr_idx>=Nset) break; //all the suppression points have been processed
		}
	}
	
	//now really remove the points from the vector
	ovec.erase( std::remove_if(ovec.begin(), ovec.end(), IsUndef), ovec.end());
}

void TimeSuppr::supprFrac(std::vector<MeteoData>& ovec) const
{
	const size_t set_size = ovec.size();
	const size_t nrRemove = static_cast<size_t>( round( (double)set_size*range ) );

	srand( static_cast<unsigned int>(time(NULL)) );
	size_t ii=1;
	while (ii<nrRemove) {
		const size_t idx = rand() % set_size;
		if (ovec[idx].date.isUndef()) continue; //the point was already removed

		ovec[idx].date.setUndef(true);
		ii++;
	}
	
	//now really remove the points from the vector
	ovec.erase( std::remove_if(ovec.begin(), ovec.end(), IsUndef), ovec.end());
}


TimeUnDST::TimeUnDST(const std::vector< std::pair<std::string, std::string> >& vecArgs, const std::string& name, const std::string& root_path, const double& TZ)
        : ProcessingBlock(vecArgs, name), dst_changes()
{
	const std::string where( "Filters::"+block_name );
	properties.stage = ProcessingProperties::first; //for the rest: default values
	const size_t nrArgs = vecArgs.size();
	
	if (nrArgs!=1)
		throw InvalidArgumentException("Wrong number of arguments for " + where, AT);

	if (vecArgs[0].first=="CORRECTIONS") {
		const std::string in_filename( vecArgs[0].second );
		const std::string prefix = ( FileUtils::isAbsolutePath(in_filename) )? "" : root_path+"/";
		const std::string path( FileUtils::getPath(prefix+in_filename, true) );  //clean & resolve path
		const std::string filename( path + "/" + FileUtils::getFilename(in_filename) );

		dst_changes = ProcessingBlock::readCorrections(block_name, filename, TZ, 2);
		if (dst_changes.empty())
			throw InvalidArgumentException("Please provide at least one DST correction for " + where, AT);
	} else
		throw UnknownValueException("Unknown option '"+vecArgs[0].first+"' for "+where, AT);
}

void TimeUnDST::process(const unsigned int& param, const std::vector<MeteoData>& ivec, std::vector<MeteoData>& ovec)
{
	if (param!=IOUtils::unodata)
		throw InvalidArgumentException("The filter "+block_name+" can only be applied to TIME", AT);
	
	ovec = ivec;
	if (ovec.empty()) return;
	
	const size_t Nset = dst_changes.size();
	size_t ii=0, next_idx=0; //we know there is at least one
	double offset = 0.;
	for (; ii<ovec.size(); ii++) {
		if (ovec[ii].date>=dst_changes[next_idx].date) {
			offset = dst_changes[next_idx].offset * 1./(24.*3600.);
			next_idx++;
			if (next_idx==Nset) break; //no more new corrections to expect
		}
		if (offset!=0.) ovec[ii].date += offset;
	}
	
	if (offset==0) return; //no more corrections to apply
	
	//if some points remained after the last DST correction date, process them
	for (; ii<ovec.size(); ii++) {
		ovec[ii].date += offset;
	}
}

} //end namespace
