/***********************************************************************************/
/*  Copyright 2017 SLF                                                                                                                                */
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
#include <meteoio/plugins/DBO.h>

#include <meteoio/dataClasses/Coords.h>
#include <meteoio/IOExceptions.h>
#include <meteoio/meteoLaws/Meteoconst.h>

#include <algorithm>
#include <sstream>
#include <iostream>

#include <curl/curl.h>
#include <meteoio/plugins/picojson.h>

using namespace std;

namespace mio {
/**
 * @page dbo DBO
 * @section dbo_format Format
 * This plugin reads meteorological data from DBO
 * via the RESTful web service. To compile the plugin you need to have the <a href="http://curl.haxx.se/">CURL library</a> with its headers present.
 *
 * @section dbo_keywords Keywords
 * This plugin uses the following keywords:
 * - DBO_URL: The URL of the RESTful web service e.g.http://developwis.wsl.ch:8730/osper-api
 * - DBO_USER: The username to access the service (optional)
 * - DBO_PASS: The password to authenticate the USER (optional)
 * - STATION#: station code for the given station, e. g. la_fouly_1034 (case sensitive!)
 * - DBO_TIMEOUT: timeout (in seconds) for the connection to the server (default: 60s)
 * - DBO_DEBUG: print the full requests/answers from the server when something does not work as expected
 *
 * @code
 * METEO	= DBO
 * DBO_URL	= http://developwis.wsl.ch:8730/osper-api
 * DBO_USER	= mylogin
 * DBO_PASS	= mypasswd
 * STATION1	= wind_tunnel_meteo
 * @endcode
 *
 */

//we keep it as a simple function in order to avoid exposing picojson stuff in the header
void printJSON(const picojson::value& v, const unsigned int& depth)
{
	if (v.is<picojson::null>()) {
		for (unsigned int jj=0; jj<depth; jj++) std::cout << "\t";
		std::cout << "NULL\n";
		return;
	}

	if (v.is<picojson::object>()) {
		const picojson::value::object& obj = v.get<picojson::object>();
		for (picojson::value::object::const_iterator ii = obj.begin(); ii != obj.end(); ++ii) {
			for (unsigned int jj=0; jj<depth; jj++) std::cout << "\t";
			std::cout << ii->first << "\n";
			printJSON(ii->second, depth+1);
		}
	} else if (v.is<std::string>()){
		for (unsigned int jj=0; jj<depth; jj++) std::cout << "\t";
		std::cout << v.get<std::string>() << "\n";
	} else if (v.is<double>()){
		for (unsigned int jj=0; jj<depth; jj++) std::cout << "\t";
		std::cout << v.get<double>() << "\n";
	} else if (v.is<bool>()){
		for (unsigned int jj=0; jj<depth; jj++) std::cout << "\t";
		std::cout << std::boolalpha << v.get<bool>() << "\n";
	} else if (v.is<picojson::array>()){ //ie vector<picojson::value>
		for (unsigned int jj=0; jj<depth; jj++) std::cout << "\t";
		const picojson::array& array = v.get<picojson::array>();
		std::cout << "array " << array.size() << "\n";
		for (size_t jj=0; jj<array.size(); jj++)
			printJSON(array[jj], depth+1);
	}
}

void JSONQuery(const std::string& path, picojson::value& v, std::vector<picojson::value>& results)
{
	if (v.is<picojson::null>()) return;

	size_t start_pos = 0;
	if (path[0]=='$') start_pos++;
	if (path[1]=='.') start_pos++;

	const size_t end_pos = path.find(".", start_pos);
	const std::string local_path = (end_pos!=std::string::npos)? path.substr(start_pos, end_pos-start_pos) : path.substr(start_pos);
	const std::string remaining_path = (end_pos!=std::string::npos)? path.substr(end_pos+1) : "";

	if (v.is<picojson::object>()) {
		picojson::value::object& obj = v.get<picojson::object>();
		for (map<string,picojson::value>::iterator it = obj.begin(); it != obj.end(); ++it) {
			if (it->first==local_path) {
				if (!remaining_path.empty())
					JSONQuery(remaining_path, it->second, results);
				else
					results.push_back( it->second );
			}
		}
	}
}

void goToJSONPath(const std::string& path, picojson::value& v, std::vector<picojson::value>& results)
{
	if (v.is<picojson::null>()) return;

	size_t start_pos = 0;
	if (path[0]=='$') start_pos++;
	if (path[1]=='.') start_pos++;

	const size_t end_pos = path.find(".", start_pos);
	const std::string local_path = (end_pos!=std::string::npos)? path.substr(start_pos, end_pos-start_pos) : path.substr(start_pos);
	const std::string remaining_path = (end_pos!=std::string::npos)? path.substr(end_pos+1) : "";

	if (v.is<picojson::object>()) {
		picojson::value::object& obj = v.get<picojson::object>();
		for (map<string,picojson::value>::iterator it = obj.begin(); it != obj.end(); ++it) {
			if (it->first==local_path) {
				if (!remaining_path.empty())
					goToJSONPath(remaining_path, it->second, results);
				else
					results.push_back( it->second );
			}
		}
	}
}

std::string getString(const std::string& path, picojson::value& v)
{
	std::vector<picojson::value> results;
	goToJSONPath(path, v, results);
	if (!results.empty()) {
		if (! results.front().is<picojson::null>() &&  results.front().is<std::string>()) return  results.front().get<std::string>();
	}

	return std::string();
}

double getDouble(const std::string& path, picojson::value& v)
{
	std::vector<picojson::value> results;
	goToJSONPath(path, v, results);
	if (!results.empty()) {
		if (! results.front().is<picojson::null>() &&  results.front().is<double>()) return  results.front().get<double>();
	}

	return IOUtils::nodata;
}

std::vector<double> getDoubles(const std::string& path, picojson::value& v)
{
	std::vector<picojson::value> results;
	goToJSONPath(path, v, results);

	std::vector<double> vecDouble;
	for (size_t ii=0; ii<results.size(); ii++) {
		 if (results[ii].is<picojson::array>()){ //ie vector<picojson::value>
			const picojson::array& array = results[ii].get<picojson::array>();
			for (size_t jj=0; jj<array.size(); jj++) {
				if (! array[jj].is<picojson::null>() &&  array[jj].is<double>()) vecDouble.push_back( array[jj].get<double>() );
			}
		}
	}

	return vecDouble;
}

/*************************************************************************************************/
const int DBO::http_timeout_dflt = 60; // seconds until connect time out for libcurl
const std::string DBO::sensors_endpoint = "/osper-api/osper/stations/";
const std::string DBO::sensors_format = "format=csv";
const std::string DBO::null_string = "null";

DBO::DBO(const std::string& configfile)
      : cfg(configfile), vecStationName(), vecMeta(),
        coordin(), coordinparam(), coordout(), coordoutparam(),
        endpoint(), userid(), passwd(), default_timezone(1.),
        http_timeout(http_timeout_dflt), dbo_debug(false)
{
	initDBOConnection();
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
	cfg.getValues("STATION", "INPUT", vecStationName); //reads station names into vector<string> vecStationName
}

DBO::DBO(const Config& cfgreader)
      : cfg(cfgreader), vecStationName(), vecMeta(),
        coordin(), coordinparam(), coordout(), coordoutparam(),
        endpoint(), userid(), passwd(), default_timezone(1.),
        http_timeout(http_timeout_dflt), dbo_debug(false)
{
	initDBOConnection();
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
	cfg.getValues("STATION", "INPUT", vecStationName); //reads station names into vector<string> vecStationName
}

void DBO::initDBOConnection() {
	curl_global_init(CURL_GLOBAL_ALL);

	cfg.getValue("DBO_TIMEOUT", "Input", http_timeout, IOUtils::nothrow);
	cfg.getValue("TIME_ZONE", "Input", default_timezone, IOUtils::nothrow);

	cfg.getValue("DBO_URL", "Input", endpoint);
	if (*endpoint.rbegin() != '/') endpoint += "/";
	cerr << "[i] Using DBO URL: " << endpoint << endl;

	cfg.getValue("DBO_USER", "Input", userid, IOUtils::nothrow);
	cfg.getValue("DBO_PASS", "Input", passwd, IOUtils::nothrow);
	cfg.getValue("DBO_DEBUG", "INPUT", dbo_debug, IOUtils::nothrow);
}

void DBO::readStationData(const Date& /*date*/, std::vector<StationData>& vecStation)
{
	vecStation.clear();
	if (vecMeta.empty()) fillStationMeta();
	vecStation = vecMeta;
}

void DBO::readMeteoData(const Date& dateStart, const Date& dateEnd,
                          std::vector< std::vector<MeteoData> >& vecMeteo)
{
	vecMeteo.clear();

	for(size_t ii=0; ii<vecStationName.size(); ii++)
		readData(dateStart, dateEnd, vecMeteo[ii], ii);
}

void DBO::fillStationMeta()
{
	vecMeta.clear();

	for(size_t ii=0; ii<vecStationName.size(); ii++) {
		const std::string station_id = vecStationName[ii];
		const string anon_request = sensors_endpoint + IOUtils::strToLower( station_id );
		const string auth = "&username=" + userid + "&password=" + passwd;
		const string request = (!userid.empty())? anon_request+auth : anon_request;

		stringstream ss;
		if (curl_read(request, ss)) {
			picojson::value v;
			const std::string err( picojson::parse(v, ss.str()) );
			if (!err.empty()) throw IOException("Error while parsing JSON: "+err, AT);

			const std::vector<double> coordinates( getDoubles("$.geometry.coordinates", v) );
			if (coordinates.size()!=3)
				throw InvalidFormatException("Wrong coordinates specification!", AT);

			Coords position(coordin, coordinparam);
			position.setLatLon(coordinates[1], coordinates[0], coordinates[2]);
			const StationData sd(position, getString("$.properties.name", v), getString("$.properties.locationName", v));
			vecMeta.push_back( sd );
		} else {
			if (dbo_debug)
				std::cout << "****\nRequest: " << request << "\n****\n";
			throw IOException("Could not retrieve data for station " + station_id, AT);
		}
	}
}

void DBO::readData(const Date& /*dateStart*/, const Date& /*dateEnd*/, std::vector<MeteoData>& /*vecMeteo*/, const size_t& stationindex)
{
	const std::string station_id = vecStationName[stationindex];
	const string anon_request = sensors_endpoint + IOUtils::strToLower( station_id );
	const string auth = "&username=" + userid + "&password=" + passwd;
	const string request = (!userid.empty())? anon_request+auth : anon_request;

	std::cerr << "Request: " << request << "\n";
	stringstream ss;
	if (curl_read(request, ss)) {
		picojson::value v;

		const std::string err( picojson::parse(v, ss.str()) );
		if (!err.empty())
			throw IOException("Error while parsing JSON: "+err, AT);

		std::cout << "$.properties.name = " << getString("$.properties.name", v) << "\n";
		std::cout << "$.properties.id = " << getDouble("$.properties.id", v) << "\n";
		const std::vector<double> coordinates( getDoubles("$.geometry.coordinates", v) );
		for(size_t ii=0; ii<coordinates.size(); ii++) std::cerr << " " << coordinates[ii];
		std::cerr << "\n";
	} else {
		if (dbo_debug)
			std::cout << "****\nRequest: " << request << "\n****\n";
		throw IOException("Could not retrieve data for station " + station_id, AT);
	}
}

void DBO::convertUnits(MeteoData& meteo) const
{
	//converts C to Kelvin, converts RH to [0,1]
	double& ta = meteo(MeteoData::TA);
	ta = IOUtils::C_TO_K(ta);

	double& tsg = meteo(MeteoData::TSG);
	tsg = IOUtils::C_TO_K(tsg);

	double& tss = meteo(MeteoData::TSS);
	tss = IOUtils::C_TO_K(tss);

	double& rh = meteo(MeteoData::RH);
	if (rh != IOUtils::nodata)
		rh /= 100.;

	double& hs = meteo(MeteoData::HS);
	if (hs != IOUtils::nodata)
		hs /= 100.;
}

size_t DBO::data_write(void* buf, size_t size, size_t nmemb, void* userp)
{
	if (userp) {
		ostream& os = *static_cast<ostream*>(userp);
		const streamsize len = size * nmemb;

		if (os.write(static_cast<char*>(buf), len)) return len;
	}

	return 0;
}

bool DBO::curl_read(const std::string& url_query, std::ostream& os)
{
	CURLcode code(CURLE_FAILED_INIT);
	CURL* curl = curl_easy_init();

	const std::string url( endpoint + url_query );

	if (curl) {
		if (CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &data_write))
		   && CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L))
		   && CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L))
		   && CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_FILE, &os))
		   && CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_TIMEOUT, DBO::http_timeout))
		   && CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_URL, url.c_str())))
		{
			code = curl_easy_perform(curl);
		}
		curl_easy_cleanup(curl);
	}

	if (code!=CURLE_OK) {
		if (dbo_debug)
			std::cout << "****\nRequest: " << url_query << "\n****\n";
		std::cout << "[E] " << curl_easy_strerror(code) << "\t";
	}

	return (code==CURLE_OK);
}

} //namespace
