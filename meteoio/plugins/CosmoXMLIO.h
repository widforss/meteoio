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
#ifndef __CosmoXMLIO_H__
#define __CosmoXMLIO_H__

#include <meteoio/IOInterface.h>
#include <meteoio/Config.h>
#include <meteoio/IOUtils.h>
#include <meteoio/Coords.h>
#include <meteoio/IOExceptions.h>

#include <string>
#include <sstream>
#include <iostream>

#include <libxml/parser.h>
#include <libxml/xpath.h>

namespace mio {

/**
* @class CosmoXMLIO
* @brief Reading of FieldExtra XML meteorological data.
* This is the plugin for reading the XML data genereated by FieldExtra, the post-processor
* of the MeteoSwiss COSMO meteorological model.
*
* @author Mathias Bavay
* @date   2014
*/
class CosmoXMLIO : public IOInterface {
	public:
		CosmoXMLIO(const std::string& configfile);
		CosmoXMLIO(const CosmoXMLIO&);
		CosmoXMLIO(const Config& cfg);
		~CosmoXMLIO() throw();

		CosmoXMLIO& operator=(const CosmoXMLIO&); ///<Assignement operator, required because of pointer member

		virtual void read2DGrid(Grid2DObject& grid_out, const std::string& parameter="");
		virtual void read2DGrid(Grid2DObject& grid_out, const MeteoGrids::Parameters& parameter, const Date& date);

		virtual void readDEM(DEMObject& dem_out);
		virtual void readLanduse(Grid2DObject& landuse_out);

		virtual void readStationData(const Date& date, std::vector<StationData>& vecStation);
		virtual void readMeteoData(const Date& dateStart, const Date& dateEnd, std::vector< std::vector<MeteoData> >& vecMeteo, const size_t& stationindex=IOUtils::npos);

		virtual void writeMeteoData(const std::vector< std::vector<MeteoData> >& vecMeteo, const std::string& name="");

		virtual void readAssimilationData(const Date&, Grid2DObject& da_out);
		virtual void readPOI(std::vector<Coords>& pts);
		virtual void write2DGrid(const Grid2DObject& grid_in, const std::string& filename);
		virtual void write2DGrid(const Grid2DObject& grid_in, const MeteoGrids::Parameters& parameter, const Date& date);

	private:
		typedef enum METEOREADSTATUS { read_ok, read_continue, read_stop } MeteoReadStatus;

		void init(const Config& cfg);
		static void scanMeteoPath(const std::string& meteopath_in, std::vector< std::pair<Date,std::string> > &meteo_files);
		size_t getFileIdx(const Date& start_date) const;
		void openIn_XML(const std::string& in_meteofile);
		void closeIn_XML() throw();
		bool parseStationData(const std::string& station_id, const xmlXPathContextPtr& xpathCtx, StationData &sd);
		MeteoReadStatus parseMeteoDataPoint(const Date& dateStart, const Date& dateEnd, const xmlNodePtr &element, MeteoData &md) const;

		bool parseMeteoData(const Date& dateStart, const Date& dateEnd, const std::string& station_id,
		                    const StationData& sd, const xmlXPathContextPtr& xpathCtx, std::vector<MeteoData> &vecMeteo) const;

		std::vector< std::pair<Date,std::string> > cache_meteo_files; //cache of meteo files in METEOPATH
		std::map<std::string, std::string> xml_stations_id; //mapping between true station ID and the messy id used in the xml
		std::vector<std::string> input_id; //user specified stations to read
		double plugin_nodata; //plugin specific no data value

		xmlDocPtr in_doc;
		xmlXPathContextPtr in_xpathCtx;

		static const double in_tz, out_tz; //plugin specific time zones
		static const std::string xml_namespace, StationData_xpath, MeteoData_xpath;
		static const std::string meteo_ext;

		std::string coordin, coordinparam, coordout, coordoutparam; //projection parameters
};

} //namespace
#endif
