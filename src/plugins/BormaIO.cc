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
#include "BormaIO.h"

using namespace std;
using namespace mio;

namespace mio {
/**
 * @page borma BORMA
 * @section borma_format Format
 * This plugin reads the XML files as generated by the Borma system. It requires libxml to compile and run.
 *
 * @section borma_units Units
 * The units are assumed to be the following:
 * - temperatures in celsius
 * - relative humidity in %
 * - wind speed in m/s
 * - precipitations in mm/h
 * - radiation in W/m²
 *
 * @section borma_keywords Keywords
 * This plugin uses the following keywords:
 * - COORDSYS: input coordinate system (see Coords) specified in the [Input] section
 * - COORDPARAM: extra input coordinates parameters (see Coords) specified in the [Input] section
 * - COORDSYS: output coordinate system (see Coords) specified in the [Output] section
 * - COORDPARAM: extra output coordinates parameters (see Coords) specified in the [Output] section
 * - XMLPATH: string containing the path to the xml files
 * - NROFSTATIONS: total number of stations listed for use
 * - STATION#: station id for the given number #
 */
}

const double BormaIO::plugin_nodata = -999.0; //plugin specific nodata value

BormaIO::BormaIO(void (*delObj)(void*), const std::string& filename) : IOInterface(delObj), cfg(filename)
{
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
}

BormaIO::BormaIO(const std::string& configfile) : IOInterface(NULL), cfg(configfile)
{
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
}

BormaIO::BormaIO(const ConfigReader& cfgreader) : IOInterface(NULL), cfg(cfgreader)
{
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
}

BormaIO::~BormaIO() throw()
{
	cleanup();
}

void BormaIO::cleanup() throw()
{
	if (fin.is_open()) {//close fin if open
		fin.close();
	}
}

//Clone function
//BormaIO* BormaIO::clone() const { return new BormaIO(*this); }

void BormaIO::read2DGrid(Grid2DObject&, const std::string& filename)
{
	//Nothing so far
	(void)filename;
	throw IOException("Nothing implemented here", AT);
}

void BormaIO::readDEM(DEMObject&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void BormaIO::readLanduse(Grid2DObject&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void BormaIO::writeMeteoData(const std::vector< std::vector<MeteoData> >&,
					    const std::vector< std::vector<StationData> >&,
					    const std::string&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void BormaIO::readStationData(const Date&, std::vector<StationData>&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void BormaIO::readMeteoData(const Date& dateStart, const Date& dateEnd,
							  std::vector< std::vector<MeteoData> >& vecMeteo,
							  std::vector< std::vector<StationData> >& vecStation,
							  const unsigned int& stationindex)
{
	if (vecStationName.size() == 0)
		readStationNames(); //reads station names into vector<string> vecStationName

	unsigned int indexStart=0, indexEnd=vecStationName.size();

	//The following part decides whether all the stations are rebuffered or just one station
	if (stationindex == IOUtils::npos){
		vecMeteo.clear();
		vecStation.clear();

		vecMeteo.insert(vecMeteo.begin(), vecStationName.size(), std::vector<MeteoData>());
		vecStation.insert(vecStation.begin(), vecStationName.size(), std::vector<StationData>());
	} else {
		if ((stationindex < vecMeteo.size()) && (stationindex < vecStation.size())){
			indexStart = stationindex;
			indexEnd   = stationindex+1;
		} else {
			throw IndexOutOfBoundsException("", AT);
		}
	}

	for (unsigned int ii=indexStart; ii<indexEnd; ii++){ //loop through stations
		//cout << vecStationName[ii] << endl;
		bufferData(dateStart, dateEnd, vecMeteo, vecStation, ii);
	}
}

void BormaIO::readStationNames()
{
	vecStationName.clear();

	//Read in the StationNames
	std::string xmlpath="", str_stations="";
	int stations=0;

	cfg.getValue("NROFSTATIONS", "Input", str_stations);

	if (!IOUtils::convertString(stations, str_stations, std::dec)) {
		throw ConversionFailedException("Error while reading value for NROFSTATIONS", AT);
	}

	for (int ii=0; ii<stations; ii++) {
		std::stringstream tmp_stream;
		std::string stationname="", tmp_file="";

		tmp_stream << (ii+1); //needed to construct key name
		cfg.getValue(std::string("STATION"+tmp_stream.str()), "Input", stationname);

		vecStationName.push_back(stationname);
	}
}

void BormaIO::getFiles(const std::string& stationname, const Date& start_date, const Date& end_date,
					 std::vector<std::string>& vecFiles, std::vector<Date>& vecDate)
{
	std::list<std::string> dirlist = std::list<std::string>();
	Date tmp_date;
	std::string xmlpath="";

	cfg.getValue("XMLPATH", "Input", xmlpath);
	vecFiles.clear();
	IOUtils::readDirectory(xmlpath, dirlist, "_" + stationname + ".xml");

	//Sort dirlist in ascending order
	dirlist.sort();

	//Check date in every filename
	std::list<std::string>::iterator it = dirlist.begin();

	if (start_date > end_date){ //Special case return first data >= dateStart
		while ((it != dirlist.end())) {
			//check validity of filename
			if (validFilename(*it)) {
				std::string filename_out = *it;
				stringToDate(filename_out, tmp_date);

				if (tmp_date > start_date) {
					vecFiles.push_back(xmlpath + "/" + filename_out);
					vecDate.push_back(tmp_date);
					return;
				}
			}

			it++;
		}
		return;
	}

	while ((it != dirlist.end()) && (tmp_date < end_date)) {
		//check validity of filename
		if (validFilename(*it)) {
			std::string filename_out = *it;
			stringToDate(filename_out, tmp_date);

			if ((tmp_date >= start_date) && (tmp_date <= end_date)) {
				vecFiles.push_back(xmlpath + "/" + filename_out);
				vecDate.push_back(tmp_date);
			}
		}

		it++;
	}
}

bool BormaIO::bufferData(const Date& dateStart, const Date& dateEnd,
					   std::vector< std::vector<MeteoData> >& vecMeteo,
					   std::vector< std::vector<StationData> >& vecStation,
					   const unsigned int& stationnr)
{
	std::vector<std::string> vecFiles;
	std::vector<Date> vecDate;

	if (stationnr >= vecMeteo.size()) {
		throw IndexOutOfBoundsException("", AT);
	}

	vecMeteo[stationnr].clear();
	vecStation[stationnr].clear();

	getFiles(vecStationName[stationnr], dateStart, dateEnd, vecFiles, vecDate);
	//cout << "[i] Buffering station number: " << vecStationName[stationnr] << "  " << vecFiles.size() << " files" << endl;

	if (vecFiles.size()==0) { //No files in range between dateStart and dateEnd
		return false;
	}

	for (unsigned int ii=0; ii<vecFiles.size(); ii++) {
		MeteoData meteoData;
		StationData stationData;
		xmlExtractData(vecFiles[ii], vecDate[ii], meteoData, stationData);
		vecMeteo[stationnr].push_back(meteoData);
		vecStation[stationnr].push_back(stationData);
	}

	return true;
}


void BormaIO::checkForMeteoFiles(const std::string& xmlpath, const std::string& stationname, const Date& date_in,
					std::string& filename_out, Date& date_out)
{
	std::list<std::string> dirlist = std::list<std::string>();
	IOUtils::readDirectory(xmlpath, dirlist, "_" + stationname + ".xml");

	//Sort dirlist in ascending order
	dirlist.sort();

	//Check date in every filename
	std::list<std::string>::iterator it = dirlist.begin();

	while ((it != dirlist.end()) && (date_out < date_in)) {
		//check validity of filename
		if (validFilename(*it)) {
			filename_out = *it;
			stringToDate(filename_out, date_out);
		}

		it++;
	}
}

void BormaIO::xmlExtractData(const std::string& filename, const Date& date_in, MeteoData& md, StationData& sd)
{
	double ta=IOUtils::nodata, iswr=IOUtils::nodata, vw=IOUtils::nodata, dw=IOUtils::nodata;
	double rh=IOUtils::nodata, ilwr=IOUtils::nodata, hnw=IOUtils::nodata, tsg=IOUtils::nodata;
	double tss=IOUtils::nodata, hs=IOUtils::nodata, rswr=IOUtils::nodata;
	double longitude=IOUtils::nodata, latitude=IOUtils::nodata, altitude=IOUtils::nodata;

	//Try to read xml file
	xmlpp::DomParser parser;
	//parser.set_validate(); provide DTD to check syntax
	parser.set_substitute_entities(); //We just want the text to be resolved/unescaped automatically.
	parser.parse_file(filename);
	if(parser) {
		//Walk the tree: ROOT NODE
		xmlpp::Node* pNode = parser.get_document()->get_root_node(); //deleted by DomParser.

		//Read in StationData
		const std::string stationName = xmlGetNodeContent(pNode, "stationsId");
		const std::string str_long = xmlGetNodeContent(pNode, "stationsLon");
		const std::string str_lati = xmlGetNodeContent(pNode, "stationsLat");
		const std::string str_alti = xmlGetNodeContent(pNode, "stationsAlt");

		xmlParseStringToDouble(str_long, longitude, "stationsLon");
		xmlParseStringToDouble(str_lati, latitude, "stationsLat");
		xmlParseStringToDouble(str_alti, altitude, "stationsAlt");

		//HACK!! would it be possible for getValueForKey() to do this transparently? (with a user flag)
		latitude = IOUtils::standardizeNodata(latitude, plugin_nodata);
		longitude = IOUtils::standardizeNodata(longitude, plugin_nodata);
		altitude = IOUtils::standardizeNodata(altitude, plugin_nodata);

		//compute/check WGS coordinates (considered as the true reference) according to the projection as defined in cfg
		Coords location(coordin, coordinparam);
		location.setLatLon(latitude, longitude, altitude);
		sd.setStationData(location, stationName);

		//lt = ta
		const std::string str_lt = xmlGetNodeContent(pNode, "lt");
		xmlParseStringToDouble(str_lt, ta, "lt");

		//gs = iswr
		const std::string str_gs = xmlGetNodeContent(pNode, "gs");
		xmlParseStringToDouble(str_gs, iswr, "gs");

		//Wind velocity
		const std::string str_vw = xmlGetNodeContent(pNode, "wgm");
		xmlParseStringToDouble(str_vw, vw, "wgm");

		//rlf = rh
		const std::string str_rh = xmlGetNodeContent(pNode, "rlf");
		xmlParseStringToDouble(str_rh, rh, "rlf");

		//ni = hnw //TODO: not sure that this field really contains what we want...
		const std::string str_ns = xmlGetNodeContent(pNode, "ni");
		xmlParseStringToDouble(str_ns, hnw, "ni");

		//sb = ilwr
		const std::string str_sb = xmlGetNodeContent(pNode, "sb");
		xmlParseStringToDouble(str_sb, ilwr, "sb");

		//fbt = tss
		const std::string str_fbt = xmlGetNodeContent(pNode, "fbt");
		xmlParseStringToDouble(str_fbt, tss, "fbt");

		//sh = hs
		const std::string str_sh = xmlGetNodeContent(pNode, "sh");
		xmlParseStringToDouble(str_sh, hs, "sh");

		md.setDate(date_in);
		md.setData(MeteoData::TA, ta);
		md.setData(MeteoData::ISWR, iswr);
		md.setData(MeteoData::VW, vw);
		md.setData(MeteoData::DW, dw);
		md.setData(MeteoData::RH, rh);
		md.setData(MeteoData::ILWR, ilwr);
		md.setData(MeteoData::HNW, hnw);
		md.setData(MeteoData::TSG, tsg);
		md.setData(MeteoData::TSS, tss);
		md.setData(MeteoData::HS, hs);
		md.setData(MeteoData::RSWR, rswr);

		convertUnits(md);

	} else {
		throw IOException("Error parsing XML", AT);
	}
}

void BormaIO::xmlParseStringToDouble(const std::string& str_in, double& d_out, const std::string& parname)
{
	if (str_in!="") {//do nothing if empty content for a node was read
		if (!IOUtils::convertString(d_out, str_in, std::dec)) {//but if content of node is not empty, try conversion
			throw ConversionFailedException("Error while reading value for " + parname, AT);
		}
	}
}

std::string BormaIO::xmlGetNodeContent(xmlpp::Node* pNode, const std::string& nodename)
{
	xmlpp::Node* tmpNode= xmlGetNode(pNode, nodename);

	if (tmpNode!=NULL) {
		xmlpp::Node* tmpNode2= xmlGetNode(tmpNode, "text"); //Try to retrieve text content
		if (tmpNode2!=NULL) {
			const xmlpp::TextNode* nodeText = dynamic_cast<const xmlpp::TextNode*>(tmpNode2);
			return std::string(nodeText->get_content());
		}
	}

	return std::string("");
}


xmlpp::Node* BormaIO::xmlGetNode(xmlpp::Node* parentNode, const std::string& nodename)
{
	if (xmlGetNodeName(parentNode)==nodename) {
		return parentNode;
	}

	xmlpp::Node::NodeList list = parentNode->get_children();

	for(xmlpp::Node::NodeList::iterator iter = list.begin(); iter != list.end(); ++iter) {
		//xmlpp::Node* tmpNode= *iter;
		//cout << tmpNode->get_name() << endl;

		xmlpp::Node* tmpNode2= (xmlGetNode(*iter, nodename));
		if (tmpNode2!=NULL) {
			return tmpNode2;
		}
	}

	return NULL;
}

std::string BormaIO::xmlGetNodeName(xmlpp::Node* pNode)
{
	std::string nodename = pNode->get_name();
	return nodename;
}

void BormaIO::stringToDate(const std::string& instr, Date& date_out) const
{
	//TODO: use IOUtil::convertString() for that!
	int tmp[5];

	std::string year = "20" + instr.substr(0,2);
	std::string month = instr.substr(2,2);
	std::string day = instr.substr(4,2);
	std::string hour = instr.substr(6,2);
	std::string minute = instr.substr(8,2);

	IOUtils::convertString(tmp[0], year, std::dec);
	IOUtils::convertString(tmp[1], month, std::dec);
	IOUtils::convertString(tmp[2], day, std::dec);
	IOUtils::convertString(tmp[3], hour, std::dec);
	IOUtils::convertString(tmp[4], minute, std::dec);

	date_out.setDate(tmp[0],tmp[1],tmp[2],tmp[3],tmp[4]);
}

bool BormaIO::validFilename(const std::string& tmp) const
{
	size_t pos = tmp.find_first_not_of("0123456789");//Filename must start with 10 numbers
	if (pos!=10) {
		return false;
	}

	return true;
}

void BormaIO::read2DMeteo(const Date& date_in, std::vector<MeteoData>& meteo_out)
{
	std::vector<StationData> vecStation;
	read2DMeteo(date_in, meteo_out, vecStation);
}

void BormaIO::read2DMeteo(const Date& date_in, std::vector<MeteoData>& vecMeteo, std::vector<StationData>& vecStation)
{
	vecMeteo.clear();
	vecStation.clear();

	//Read in the StationNames
	std::string xmlpath="", str_stations="";
	int stations=0;

	cfg.getValue("NROFSTATIONS", "Input", str_stations);
	cfg.getValue("XMLPATH", "Input", xmlpath);

	if (!IOUtils::convertString(stations, str_stations, std::dec)) {
		throw ConversionFailedException("Error while reading value for NROFSTATIONS", AT);
	}

	for (int ii=0; ii<stations; ii++) {
		std::stringstream tmp_stream;
		std::string stationname="", tmp_file="";
		Date tmp_date(0.0);
		MeteoData md;
		StationData sd;

		tmp_stream << (ii+1); //needed to construct key name
		cfg.getValue(std::string("STATION"+tmp_stream.str()), "Input", stationname);

		checkForMeteoFiles(xmlpath, stationname, date_in, tmp_file, tmp_date);
		//Check whether file was found
		if (tmp_date<date_in) {
			throw FileNotFoundException("No XML file in path '" + xmlpath
								   + "' found for date " + date_in.toString(Date::ISO) + " for station " + stationname, AT);
		}

		//Read in data from XML File
		xmlExtractData(xmlpath+"/"+tmp_file, tmp_date, md, sd);

		vecMeteo.push_back(md);
		vecStation.push_back(sd);
	}
}

void BormaIO::readAssimilationData(const Date&, Grid2DObject&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void BormaIO::readSpecialPoints(std::vector<Coords>&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void BormaIO::write2DGrid(const Grid2DObject&, const std::string& name)
{
	//Nothing so far
	(void)name;
	throw IOException("Nothing implemented here", AT);
}

void BormaIO::convertUnits(MeteoData& meteo)
{
	meteo.standardizeNodata(plugin_nodata);

	//converts C to Kelvin, converts ilwr to ea, converts RH to [0,1]
	if(meteo.ta!=IOUtils::nodata) {
		meteo.ta=C_TO_K(meteo.ta);
	}

	if(meteo.tsg!=IOUtils::nodata) {
		meteo.tsg=C_TO_K(meteo.tss);
	}

	if(meteo.tss!=IOUtils::nodata) {
		meteo.tss=C_TO_K(meteo.tss);
	}

	if(meteo.rh!=IOUtils::nodata) {
		meteo.rh /= 100.;
	}
}

#ifndef _METEOIO_JNI
extern "C"
{
	void deleteObject(void* obj) {
		delete reinterpret_cast<PluginObject*>(obj);
	}

	void* loadObject(const std::string& classname, const std::string& filename) {
		if(classname == "BormaIO") {
			//cerr << "Creating dynamic handle for " << classname << endl;
			return new BormaIO(deleteObject, filename);
		}
		//cerr << "Could not load " << classname << endl;
		return NULL;
	}
}
#endif
