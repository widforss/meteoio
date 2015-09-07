/***********************************************************************************/
/*  Copyright 2015 WSL Institute for Snow and Avalanche Research    SLF-DAVOS      */
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
#ifndef __ALPUG_H__
#define __ALPUG_H__

#include <meteoio/IOInterface.h>
#include <meteoio/Config.h>

#include <string>
#include <deque>

namespace mio {

/**
 * @class ALPUG
 * @brief This plugin reads data as they come out of ALPUG stations, in ASCII
 *
 * @ingroup plugins
 * @author Mathias Bavay
 * @date   2015-03-31
 */
class ALPUG : public IOInterface {
	public:
		ALPUG(const std::string& configfile);
		ALPUG(const ALPUG&);
		ALPUG(const Config& cfgreader);
		~ALPUG() throw() {}

		virtual void read2DGrid(Grid2DObject& grid_out, const std::string& parameter="");
		virtual void read2DGrid(Grid2DObject& grid_out, const MeteoGrids::Parameters& parameter, const Date& date);
		virtual void readDEM(DEMObject& dem_out);
		virtual void readLanduse(Grid2DObject& landuse_out);

		virtual void readStationData(const Date& date, std::vector<StationData>& vecStation);
		virtual void readMeteoData(const Date& dateStart, const Date& dateEnd,
		                           std::vector< std::vector<MeteoData> >& vecMeteo,
		                           const size_t& stationindex=IOUtils::npos);

		virtual void writeMeteoData(const std::vector< std::vector<MeteoData> >& vecMeteo,
		                            const std::string& name="");

		virtual void readAssimilationData(const Date&, Grid2DObject& da_out);
		virtual void readPOI(std::vector<Coords>& pts);
		virtual void write2DGrid(const Grid2DObject& grid_in, const std::string& filename);
		virtual void write2DGrid(const Grid2DObject& grid_in, const MeteoGrids::Parameters& parameter, const Date& date);

	private:
		void parseInputOutputSection();
		void readMetaData();
		bool isDuplicate(const std::string& line) ;
		Date parseDINDate(const std::string& datum) const;
		bool parseLine(const std::string& filename, const size_t& nr_of_data_fields, const Date& dateStart, const Date& dateEnd, const std::string& line, MeteoData &md, bool &isValid) const;
		void readMetoFile(const size_t& station_index, const Date& dateStart, const Date& dateEnd, 
                                              std::vector<MeteoData>& vecM);
		
		const Config cfg;
		std::vector<StationData> vecMeta;
		std::deque<std::string> LinesBuffer;
		std::vector<std::string> vecIDs, vecFields;
		std::string coordin, coordinparam, coordout, coordoutparam; //projection parameters
		std::string inpath, outpath;
		double in_dflt_TZ, out_dflt_TZ;
		unsigned short wrap_month;
		
		static const double plugin_nodata; //plugin specific nodata value, e.g. -999
		static const size_t max_buffered_lines; //how many lines to keep in buffer to check for duplicates?
		static const std::string dflt_extension;
};

} //namespace
#endif
