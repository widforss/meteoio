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
#include "CosmoXMLIO.h"
#include <meteoio/meteolaws/Atmosphere.h>
#include <sstream>

//To read a text file
#include <iostream>
#include <fstream>
#include <string>

//#include <libxml/parser.h>
#include <libxml/tree.h>
//#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#if !defined(LIBXML_XPATH_ENABLED) || !defined(LIBXML_SAX1_ENABLED)
	#error Please enable XPATH and SAX1 in your version of libxml!
#endif

using namespace std;

namespace mio {
/**
 * @page cosmoxml COSMOXML
 * @section cosmoxml_format Format
 * This plugin reads the XML files as generated by <A HREF="http://www.cosmo-model.org/">Cosmo</A>'s <A HREF="http://www.cosmo-model.org/content/support/software/default.htm#fieldextra">FieldExtra</A>.
 * The files are written out by COSMO in Grib format and preprocessed by FieldExtra (MeteoSwiss) to get XML files.
 * It requires <A HREF="http://xmlsoft.org/">libxml2</A> to compile and run.
 *
 * @section cosmo_partners COSMO Group
 * This plugin has been developed primarily for reading XML files produced by COSMO (http://www.cosmo-model.org/) at MeteoSwiss.
 * COSMO (COnsortium for Small scale MOdelling) represents a non-hydrostatic limited-area atmospheric model, to be used both for operational and for research applications by the members of the consortium. The Consortium has the following members:
 *  - Germany, DWD, Deutscher Wetterdienst
 *  - Switzerland, MCH, MeteoSchweiz
 *  - Italy, USAM, Ufficio Generale Spazio Aereo e Meteorologia
 *  - Greece, HNMS, Hellenic National Meteorological Service
 *  - Poland, IMGW, Institute of Meteorology and Water Management
 *  - Romania, NMA, National Meteorological Administration
 *  - Russia, RHM, Federal Service for Hydrometeorology and Environmental Monitoring
 *  - Germany, AGeoBw, Amt für GeoInformationswesen der Bundeswehr
 *  - Italy, CIRA, Centro Italiano Ricerche Aerospaziali
 *  - Italy, ARPA-SIMC, ARPA Emilia Romagna Servizio Idro Meteo Clima
 *  - Italy, ARPA Piemonte, Agenzia Regionale per la Protezione Ambientale Piemonte
 *
 * @section cosmoxml_units Units
 * The units are assumed to be the following:
 * - temperatures in K
 * - relative humidity in %
 * - wind speed in m/s
 * - precipitations in mm/h
 * - radiation in W/m²
 * - snow height in cm
 * - maximal wind speed in m/s
 *
 * @section cosmoxml_keywords Keywords
 * This plugin uses the following keywords:
 * - COORDSYS:  input coordinate system (see Coords) specified in the [Input] section
 * - METEO:     specify COSMOXML for [Input] section
 * - METEOPATH: string containing the path to the xml files to be read, specified in the [Input] section
 * - METEOFILE: specify the xml file to read the data from (optional)
 * - STATION#: ID of the station to read
 *
 * If no METEOFILE is provided, all ".xml" files in the METEOPATH directory will be read. They <i>must</i> contain the date of
 * the first data formatted as ISO8601 numerical UTC date in their file name. For example, a file containing simulated
 * meteorological fields from 2014-03-03T12:00 until 2014-03-05T00:00 could be named such as "cosmo_201403031200.xml"
 *
 * Example:
 * @code
 * [Input]
 * COORDSYS	= CH1903
 * METEO	= COSMOXML
 * METEOPATH	= ./input/meteoXMLdata
 * METEOFILE	= cosmo2.xml
 * STATION1	= ATT
 * STATION2	= EGH
 * @endcode
 */

const double CosmoXMLIO::in_tz = 0.; //Plugin specific timezone
const double CosmoXMLIO::out_tz = 0.; //Plugin specific time zone
const std::string CosmoXMLIO::xml_namespace = "http://www.meteoswiss.ch/xmlns/modeltemplate/2";
const std::string CosmoXMLIO::StationData_xpath = "//ch:datainformation/ch:data-tables/ch:data/ch:row/ch:col";
const std::string CosmoXMLIO::MeteoData_xpath = "//ch:valueinformation/ch:values-tables/ch:data/ch:row/ch:col";
const std::string CosmoXMLIO::meteo_ext = "xml";

CosmoXMLIO::CosmoXMLIO(const std::string& configfile)
           : cache_meteo_files(), xml_stations_id(), input_id(), plugin_nodata(-999.),
             in_doc(NULL), in_xpathCtx(NULL),
             coordin(), coordinparam(), coordout(), coordoutparam()
{
	Config cfg(configfile);
	init(cfg);
}

CosmoXMLIO::CosmoXMLIO(const Config& cfg)
           : cache_meteo_files(), xml_stations_id(), input_id(), plugin_nodata(-999.),
             in_doc(NULL), in_xpathCtx(NULL),
             coordin(), coordinparam(), coordout(), coordoutparam()
{
	init(cfg);
}

void CosmoXMLIO::init(const Config& cfg)
{
	LIBXML_TEST_VERSION

	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
	cfg.getValues("STATION", "INPUT", input_id);
	std::string meteopath;
	cfg.getValue("METEOPATH", "INPUT", meteopath);
	std::string meteofile;
	cfg.getValue("METEOFILE", "INPUT", meteofile, IOUtils::nothrow);

	if(!meteofile.empty()) {
		meteofile = meteopath + "/" + meteofile;
		const std::pair<Date,std::string> tmp(Date(), meteofile);
		cache_meteo_files.push_back( tmp );
	} else {
		scanMeteoPath(meteopath, cache_meteo_files);
	}
}

CosmoXMLIO& CosmoXMLIO::operator=(const CosmoXMLIO& source) {
	if(this != &source) {
		cache_meteo_files = source.cache_meteo_files;
		xml_stations_id = source.xml_stations_id;
		input_id = source.input_id;
		plugin_nodata = source.plugin_nodata;
		in_doc = NULL;
		in_xpathCtx = NULL;
		coordin = source.coordin;
		coordinparam = source.coordinparam;
		coordout = source.coordout;
		coordoutparam = source.coordoutparam;
	}
	return *this;
}

CosmoXMLIO::~CosmoXMLIO() throw()
{
	closeIn_XML();
}

void CosmoXMLIO::scanMeteoPath(const std::string& meteopath_in,  std::vector< std::pair<Date,std::string> > &meteo_files)
{
	meteo_files.clear();
	std::list<std::string> dirlist;
	IOUtils::readDirectory(meteopath_in, dirlist, meteo_ext);
	dirlist.sort();

	//Check date in every filename and cache it
	std::list<std::string>::const_iterator it = dirlist.begin();
	while ((it != dirlist.end())) {
		const std::string& filename = *it;
		const std::string::size_type spos = filename.find_first_of("0123456789");
		Date date;
		IOUtils::convertString(date, filename.substr(spos,10), in_tz);
		const std::pair<Date,std::string> tmp(date, filename);

		meteo_files.push_back(tmp);
		it++;
	}
}

void CosmoXMLIO::openIn_XML(const std::string& in_meteofile)
{
	if(in_doc!=NULL) return; //the file has already been read

	xmlInitParser();
	xmlKeepBlanksDefault(0);

	in_doc = xmlParseFile(in_meteofile.c_str());
	if (in_doc == NULL) {
		throw FileNotFoundException("Could not open/parse file \""+in_meteofile+"\"", AT);
	}

	if(in_xpathCtx!=NULL) xmlXPathFreeContext(in_xpathCtx); //free variable if this was not freed before
	in_xpathCtx = xmlXPathNewContext(in_doc);
	if(in_xpathCtx == NULL) {
		closeIn_XML();
		throw IOException("Unable to create new XPath context", AT);
	}

	if(xmlXPathRegisterNs(in_xpathCtx,  (const xmlChar*)"ch", (const xmlChar*)xml_namespace.c_str()) != 0) {
		throw IOException("Unable to register namespace with prefix", AT);
	}
}

void CosmoXMLIO::closeIn_XML() throw()
{
	if(in_xpathCtx!=NULL) {
		xmlXPathFreeContext(in_xpathCtx);
		in_xpathCtx = NULL;
	}
	if(in_doc!=NULL) {
		xmlFreeDoc(in_doc);
		in_doc = NULL;
	}
	xmlCleanupParser();
}

void CosmoXMLIO::read2DGrid(Grid2DObject& /*grid_out*/, const std::string& /*_name*/)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void CosmoXMLIO::read2DGrid(Grid2DObject&, const MeteoGrids::Parameters&, const Date&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void CosmoXMLIO::readDEM(DEMObject& /*dem_out*/)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void CosmoXMLIO::readLanduse(Grid2DObject& /*landuse_out*/)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void CosmoXMLIO::readAssimilationData(const Date& /*date_in*/, Grid2DObject& /*da_out*/)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

bool CosmoXMLIO::parseStationData(const std::string& station_id, const xmlXPathContextPtr& xpathCtx, StationData &sd)
{
	const std::string xpath = StationData_xpath+"[@id='station_abbreviation' and text()='"+station_id+"']/.."; //ie parent node containing the pattern
	const std::string attribute = "id";

	xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((const xmlChar*)xpath.c_str(), xpathCtx);
	if(xpathObj == NULL) return false;

	//check the number of matches
	const xmlNodeSetPtr &metadata = xpathObj->nodesetval;
	const int nr_metadata = (metadata) ? metadata->nodeNr : 0;
	if(nr_metadata==0)
		throw NoAvailableDataException("No metadata found for station \""+station_id+"\"", AT);
	if(nr_metadata>1)
		throw InvalidFormatException("Multiple definition of metadata for station \""+station_id+"\"", AT);

	//collect all the data fields
	std::string xml_id;
	double altitude = IOUtils::nodata, latitude = IOUtils::nodata, longitude = IOUtils::nodata;
	//start from the first child until the last one
	for (xmlNode *cur_node = metadata->nodeTab[0]->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			xmlChar *att = xmlGetProp(cur_node, (const xmlChar *)attribute.c_str());
			const std::string field( (char*)(att) );
			xmlFree(att);

			if (cur_node->children->type == XML_TEXT_NODE) {
				const std::string value( (char*)(cur_node->children->content) );

				if(field=="identifier") xml_id = value;
				else if(field=="station_abbreviation") sd.stationID = value;
				else if(field=="station_name") sd.stationName = value;
				else if(field=="model_station_height") IOUtils::convertString(altitude, value);
				else if(field=="model_station_latitude") IOUtils::convertString(latitude, value);
				else if(field=="model_station_longitude") IOUtils::convertString(longitude, value);
				else if(field=="missing_value_code") IOUtils::convertString(plugin_nodata, value);
			}
		}
	}

	if(latitude==IOUtils::nodata || longitude==IOUtils::nodata || altitude==IOUtils::nodata)
		throw NoAvailableDataException("Some station location information is missing for station \""+station_id+"\"", AT);
	sd.position.setProj(coordin, coordinparam);
	sd.position.setLatLon(latitude, longitude, altitude);

	if(xml_id.empty())
		throw NoAvailableDataException("XML station id missing for station \""+station_id+"\"", AT);
	xml_stations_id[station_id] = xml_id;

	xmlXPathFreeObject(xpathObj);
	return true;
}

size_t CosmoXMLIO::getFileIdx(const Date& start_date) const
{
	if(cache_meteo_files.empty())
		throw InvalidArgumentException("No input files found or configured!", AT);

	//find which file we should open
	if(cache_meteo_files.size()==1) {
		return 0;
	} else {
		size_t idx;
		for(idx=1; idx<cache_meteo_files.size(); idx++) {
			if(start_date>=cache_meteo_files[idx-1].first && start_date<cache_meteo_files[idx].first) {
				return idx--;
			}
		}

		//not found, we take the closest timestamp we have
		if(start_date<cache_meteo_files.front().first)
			return 0;
		else
			return cache_meteo_files.size()-1;
	}
}

void CosmoXMLIO::readStationData(const Date& station_date, std::vector<StationData>& vecStation)
{
	vecStation.clear();

	const std::string meteofile( cache_meteo_files[ getFileIdx(station_date) ].second );
	openIn_XML(meteofile);

	//read all the stations' metadata
	for(size_t ii=0; ii<input_id.size(); ii++) {
		StationData sd;
		if(!parseStationData(input_id[ii], in_xpathCtx, sd)) {
			closeIn_XML();
			throw IOException("Unable to evaluate xpath expression \""+input_id[ii]+"\"", AT);
		}
		vecStation.push_back(sd);
	}

	closeIn_XML();
}

CosmoXMLIO::MeteoReadStatus CosmoXMLIO::parseMeteoDataPoint(const Date& dateStart, const Date& dateEnd, const xmlNodePtr &element, MeteoData &md) const
{
	const std::string attribute = "id";
	double iswr_dir = IOUtils::nodata, iswr_diff = IOUtils::nodata;

	//collect all the data fields
	for (xmlNode *cur_node = element; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			xmlChar *att = xmlGetProp(cur_node, (const xmlChar *)attribute.c_str());
			const std::string field( (char*)(att) );
			xmlFree(att);

			if (cur_node->children->type == XML_TEXT_NODE) {
				const std::string value( (char*)(cur_node->children->content) );
				if(field=="reference_ts") {
					IOUtils::convertString(md.date, value, in_tz);
					if(md.date<dateStart) return read_continue;
					if(md.date>dateEnd) return read_stop;
				} else {
					double tmp;
					IOUtils::convertString(tmp, value);
					tmp = IOUtils::standardizeNodata(tmp, plugin_nodata);

					//HACK for now, we hard-code the fields mapping
					if(field=="108005") md(MeteoData::TA) = tmp;
					else if(field=="108014") md(MeteoData::RH) = tmp/100.;
					else if(field=="108015") md(MeteoData::VW) = tmp;
					else if(field=="108017") md(MeteoData::DW) = tmp;
					else if(field=="108018") md(MeteoData::VW_MAX) = tmp;
					else if(field=="108023") md(MeteoData::HNW) = tmp;
					else if(field=="108060") md(MeteoData::HS) = tmp/100.;
					else if(field=="108062") md(MeteoData::TSS) = tmp;
					else if(field=="108064") iswr_diff = tmp;
					else if(field=="108065") iswr_dir = tmp;
					else if(field=="108066") md(MeteoData::RSWR) = tmp;
					else if(field=="108067") md(MeteoData::ILWR) = tmp; //108068=olwr
				}
			}
		}
	}

	if(iswr_diff!=IOUtils::nodata && iswr_dir!=IOUtils::nodata)
		md(MeteoData::ISWR) = iswr_diff+iswr_dir;

	return read_ok;
}

bool CosmoXMLIO::parseMeteoData(const Date& dateStart, const Date& dateEnd, const std::string& station_id, const StationData& sd, const xmlXPathContextPtr& xpathCtx, std::vector<MeteoData> &vecMeteo) const
{
	const std::string xpath = MeteoData_xpath+"[@id='identifier' and text()='"+station_id+"']";

	xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((const xmlChar*)xpath.c_str(), xpathCtx);
	if(xpathObj == NULL) return false;

	//check the number of matches
	const xmlNodeSetPtr &data = xpathObj->nodesetval;
	const int nr_data = (data) ? data->nodeNr : 0;
	if(nr_data==0)
		throw NoAvailableDataException("No data found for station \""+station_id+"\"", AT);

	//loop over all data for this station_id
	for(int ii=0; ii<nr_data; ii++) {
		MeteoData md( Date(), sd);

		const MeteoReadStatus status = parseMeteoDataPoint(dateStart, dateEnd, data->nodeTab[ii], md);
		if(status==read_ok) vecMeteo.push_back( md );
		if(status==read_stop) break;
	}

	xmlXPathFreeObject(xpathObj);
	return true;
}

void CosmoXMLIO::readMeteoData(const Date& dateStart, const Date& dateEnd,
                               std::vector< std::vector<MeteoData> >& vecMeteo,
                               const size_t&)
{
	vecMeteo.clear();
	const size_t nr_files = cache_meteo_files.size();
	size_t file_idx = getFileIdx(dateStart);
	Date nextDate;

	do {
		//since files contain overlapping data, we will only read the non-overlapping part
		//ie from start to the start date of the next file
		nextDate = ((file_idx+1)<nr_files)? cache_meteo_files[file_idx+1].first : dateEnd;

		const std::string meteofile( cache_meteo_files[file_idx].second );
		openIn_XML(meteofile);

		//read all the stations' metadata
		std::vector<StationData> vecStation;
		for(size_t ii=0; ii<input_id.size(); ii++) {
			StationData sd;
			if(!parseStationData(input_id[ii], in_xpathCtx, sd)) {
				closeIn_XML();
				throw IOException("Unable to evaluate xpath expression \""+input_id[ii]+"\"", AT);
			}
			vecStation.push_back(sd);
		}

		//read all the stations' data
		for(size_t ii=0; ii<input_id.size(); ii++) {
			const string station_id = xml_stations_id[ input_id[ii] ];
			vector<MeteoData> vecTmp;
			if(!parseMeteoData(dateStart, nextDate, station_id, vecStation[ii], in_xpathCtx, vecTmp)) {
				closeIn_XML();
				throw IOException("Unable to evaluate xpath expression \""+input_id[ii]+"\"", AT);
			}
			vecMeteo.push_back(vecTmp);
		}

		closeIn_XML();

		file_idx++;
	} while (file_idx<nr_files && nextDate<=dateEnd);
}

void CosmoXMLIO::writeMeteoData(const std::vector< std::vector<MeteoData> >& /*vecMeteo*/,
                                const std::string&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void CosmoXMLIO::readPOI(std::vector<Coords>&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void CosmoXMLIO::write2DGrid(const Grid2DObject& /*grid_in*/, const std::string& /*name*/)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void CosmoXMLIO::write2DGrid(const Grid2DObject&, const MeteoGrids::Parameters&, const Date&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

} //namespace

