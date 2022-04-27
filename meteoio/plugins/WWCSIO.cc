// SPDX-License-Identifier: LGPL-3.0-or-later
/***********************************************************************************/
/*  Copyright 2022 WSL Institute for Snow and Avalanche Research    SLF-DAVOS      */
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
#include <meteoio/plugins/WWCSIO.h>
#include <meteoio/plugins/libMysqlWrapper.h> //this includes mysql.h

#ifdef _WIN32
	#include <winsock.h>
#endif // _WIN32

#include <stdio.h>
#include <algorithm>

using namespace std;

namespace mio {
/**
* @page wwcs WWCSIO
* @section WWCS_format Format
* This is the plugin required to get meteorological data from the WWCS MySQL database. The connection 
* to the database is encrypted (with SSL) and compressed (with zlib) if supported by the server.
* 
* @section WWCS_dependencies Plugin dependencies and compilation
* This plugin requires the Mysql C API. This must be installed before attempting to compile the plugin. This can be installed
* from source and recompiled (for example getting it from 
* <a href="https://mariadb.org/download/?t=connector&p=connector-c&r=3.1.13&os=source">MariaDB</a>) or from precompiled binaries
* for your plateform.
* 
* @subsection WWCS_linux_install Linux
* On Linux, you need to install the *libmysqlclient-dev* (Debian, Ubuntu) or *mysql-devel* (RedHat, Centos, Fedora, Suse) 
* package. Then CMake will find it and you'll be able to compile the plugin.
* 
* @subsection WWCS_mac_install Mac
* If you have *brew* on your system, you can simply install the *mysql-connector-c* package from brew and then CMake fill
* find it and you'll be able to compile the plugin.
* 
* Otherwise, you can download <a href="https://dev.mysql.com/downloads/mysql/">the MySQL server package</a> (in the 
* <a href="https://downloads.mysql.com/archives/community/">archives</a> you can find packages for earlier versions of Macos, 
* for example Mysql 8.0.23 that is the latest version compatible with macOS 10.15 Catalina). Then install and cancel 
* the installation when the installer tries to configure a MySQL server (as this is not needed and it keeps everything that it has
* installed so far in place). CMake will then be able to find the libmysqlclient that MeteoIO needs to compile the plugin.
* 
* @subsection WWCS_windows_install Windows
* First, download the <a href="https://dev.mysql.com/downloads/installer/">Mysql installer</a> (yes, you can use the 32 bits version, 
* this only applies to the installer itself). Run the installer and select to install the Mysql server package. When asked to configure 
* the server, skip this step.
* 
* In CMake, select the *include* sub-directory of the Mysql install directory and select the *libmysql.dll*
* library within the *lib* sub-directory. You can then compile the plugin. Please do not forget to copy libmysql.dll as well as 
* *libcrypto-1_1-x64.dll* and *libssl-1_1-x64.dll* into the bin sub-directory of MeteoIO before running meteoio_timeseries.
* 
* @section WWCS_units Units
* The units are assumed to be the following:
* - __temperatures__ in celsius
* - __relative humidity__ in %
* - __pressures__ in Pa
*
* @section WWCS_keywords Keywords
* This plugin uses the following keywords:
* - COORDSYS: coordinate system (see Coords); [Input] and [Output] section
* - COORDPARAM: extra coordinates parameters (see Coords); [Input] and [Output] section
* - TIME_ZONE: For [Input] and [Output] sections (Input::TIME_ZONE should describe the timezone 
* of the data in the database while the resulting data will be converted to Output::TIME_ZONE )
* - WWCS_HOST: MySQL Host Name (e.g. localhost or 191.168.145.20); [Input] section
* - WWCS_DB: MySQL Database (e.g. wwcs); [Input] section
* - WWCS_USER: MySQL User Name (e.g. wwcs); [Input] section
* - WWCS_PASS: MySQL password; [Input] section
* - WWCS_PROFILE: which database profile to use. Currently supported: WWCS. [Input] section
* - STATION#: station code for the given number #; [Input] section
* 
* Example configuration:
* @code
* [Input]
* COORDSYS  = CH1903
* TIME_ZONE = 1
* 
* METEO     = WWCS
* WWCS_HOST = nesthorn.slf.ch
* WWCS_DB   = wwcs
* WWCS_USER = wwcs
* WWCS_PASS = XXX
* WWCS_PROFILE = WWCS
* 
* STATION1  = SLF01
* STATION2  = WWCS_BAL01
* STATION3  = WWCS_LUC
* @endcode
* 
* 
*/

/* the metadata query and the meteodata queries must be defined as follow: 
 *    - the query itself is a string with the marker <i>?</i> used as placeholder for parameters that will be bound when preparing the query;
 *    - the parsing of the query result is defined by the parsing specification vector that follows the query. It must provide 
 * the MySQL data type as well as a string that describes what MeteoIO should do with the resulting field.
 * 
 * The following field are specialy handled: STATNAME, LAT, LON, ALT, SLOPE, AZI, DATETIME. All other fields will be directly used as filed names in the 
 * populated MeteData object (fields not already existing will be created as needed). 
 * 
 * In order to support multiple kind of queries, there is the concept of profile. A profile is a set of metadata and meteodata queries together 
 * with the parsing specifications vectors. In order to add a new profile, simply provide the queries and parsing specifications vectors, define a 
 * profile name (add it to the documentation) and attribute the profile to the template queries and parsing specifications in WWCSIO::readConfig().
 * 
 * The architecture is a little suprising (relying on file-scope variables) in order to avoid exposing <mysql.h> content to the rest of MeteoIO
 * as this file does not wrap its content in a namespace and therefore would pollute everything else with its definitions.
*/
const std::string WWCS_metaDataQuery = "SELECT stationName, latitude, longitude, altitude, slope, azimuth FROM sites WHERE StationID=?"; //this query is supposed to only take the stationID as parameter (for now)
const std::vector<SQL_FIELD> WWCS_metaDataFields{ SQL_FIELD("STATNAME", MYSQL_TYPE_STRING), SQL_FIELD("LAT", MYSQL_TYPE_DOUBLE), SQL_FIELD("LON", MYSQL_TYPE_DOUBLE), SQL_FIELD("ALT", MYSQL_TYPE_DOUBLE), SQL_FIELD("SLOPE", MYSQL_TYPE_DOUBLE), SQL_FIELD("AZI", MYSQL_TYPE_DOUBLE) };
const std::string WWCS_meteoDataQuery = "SELECT timestamp, ta, rh, p, logger_ta, logger_rh FROM v_meteoseries WHERE stationID=? and timestamp>=? AND timestamp<=? ORDER BY timestamp ASC";
const std::vector<SQL_FIELD> WWCS_meteoDataFields{ SQL_FIELD("DATETIME", MYSQL_TYPE_DATETIME), SQL_FIELD("TA", MYSQL_TYPE_DOUBLE, SQL_FIELD::C_TO_K), SQL_FIELD("RH", MYSQL_TYPE_DOUBLE, SQL_FIELD::NORMALIZE_PC), SQL_FIELD("P", MYSQL_TYPE_DOUBLE), SQL_FIELD("LOGGER_TA", MYSQL_TYPE_DOUBLE, SQL_FIELD::C_TO_K), SQL_FIELD("LOGGER_RH", MYSQL_TYPE_DOUBLE, SQL_FIELD::NORMALIZE_PC) };

//template queries and parsing specification vectors
std::string metaDataQuery, meteoDataQuery;
std::vector<SQL_FIELD> metaDataFields, meteoDataFields;

WWCSIO::WWCSIO(const std::string& configfile)
        : cfg(configfile), vecStationIDs(), vecStationMetaData(),
          mysqlhost(), mysqldb(), mysqluser(), mysqlpass(),
          coordin(), coordinparam(), coordout(), coordoutparam(),
          in_dflt_TZ(1.), out_dflt_TZ(1.), mysql_options(mysql_wrp::COMPRESSION | mysql_wrp::ENCRYPTION)
{
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
	readConfig();
}

WWCSIO::WWCSIO(const Config& cfgreader)
        : cfg(cfgreader), vecStationIDs(), vecStationMetaData(),
          mysqlhost(), mysqldb(), mysqluser(), mysqlpass(),
          coordin(), coordinparam(), coordout(), coordoutparam(),
          in_dflt_TZ(1.), out_dflt_TZ(1.), mysql_options(mysql_wrp::COMPRESSION | mysql_wrp::ENCRYPTION)
{
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
	readConfig();
}

void WWCSIO::readConfig()
{
	cfg.getValue("TIME_ZONE","Input", in_dflt_TZ, IOUtils::nothrow);
	cfg.getValue("TIME_ZONE","Output", out_dflt_TZ, IOUtils::nothrow);
	
	cfg.getValue("WWCS_HOST", "Input", mysqlhost);
	cfg.getValue("WWCS_DB", "Input", mysqldb);
	cfg.getValue("WWCS_USER", "Input", mysqluser);
	cfg.getValue("WWCS_PASS", "Input", mysqlpass);
	
	std::string profile = cfg.get("WWCS_PROFILE", "Input");
	IOUtils::toUpper( profile );
	if (profile=="WWCS") {
		metaDataQuery = WWCS_metaDataQuery;
		meteoDataQuery = WWCS_meteoDataQuery;
		metaDataFields = WWCS_metaDataFields;
		meteoDataFields = WWCS_meteoDataFields;
	} else {
		throw InvalidArgumentException("Unknown profile '"+profile+"' selected for the MySQL plugin", AT);
	}
}

/**
* @brief Build the list of user provided station IDs to read
* @return list of station IDs
*/
std::vector<std::string> WWCSIO::readStationIDs() const
{
	std::vector<std::string> vecStationID;
	cfg.getValues("STATION", "INPUT", vecStationID);

	if (vecStationID.empty())
		cerr << "\tNo stations specified for WWCSIO... is this what you want?\n";
	
	return vecStationID;
}

/**
* @brief Retrieve the stations' metadata from the database.
* This metadata can then be used either to return only the metadata as a vector of StationData or
* later when reading the meteo data, in order to provide the necessary metadata.
* The results are stored in the vecStationMetaData member.
*/
void WWCSIO::readStationMetaData()
{
	const size_t nrMetadataFields( metaDataFields.size() ); //this is given by the metadata parsing vector metaDataFields
	vecStationMetaData.clear();
	const std::vector<std::string> vecStationID( readStationIDs() );
	
	MYSQL *mysql = mysql_wrp::initMysql(mysqlhost, mysqluser, mysqlpass, mysqldb, mysql_options);
	MYSQL_STMT *stmt = mysql_wrp::initStmt(&mysql, metaDataQuery, 1);
	
	for (size_t ii=0; ii<vecStationID.size(); ii++) {
		const std::string stationID( vecStationID[ii] );
		std::vector<SQL_FIELD> params_fields{ SQL_FIELD(stationID) };
		mysql_wrp::bindParams(&stmt, params_fields);
		
		if (mysql_stmt_execute(stmt)) {
			throw IOException("Error executing statement", AT);
		} else { //retrieve results
			mysql_wrp::bindResults(&stmt, metaDataFields);
			if (mysql_stmt_num_rows(stmt)!=1) throw IOException("stationID is not unique in the database!", AT);
			mysql_stmt_fetch(stmt); //we only have one result, see check above
			
			//loop over all fields that we should find
			std::string station_name;
			double lat=IOUtils::nodata, lon=IOUtils::nodata, alt=IOUtils::nodata, slope=IOUtils::nodata, azi=IOUtils::nodata;
			for (size_t ii=0; ii<nrMetadataFields; ++ii) {
				if (metaDataFields[ii].param=="STATNAME") station_name = metaDataFields[ii].str;
				else if (metaDataFields[ii].param=="LAT") lat = metaDataFields[ii].val;
				else if (metaDataFields[ii].param=="LON") lon = metaDataFields[ii].val;
				else if (metaDataFields[ii].param=="ALT") alt = metaDataFields[ii].val;
				else if (metaDataFields[ii].param=="SLOPE") slope = metaDataFields[ii].val;
				else if (metaDataFields[ii].param=="AZI") azi = metaDataFields[ii].val;
			}
			
			Coords location(coordin,coordinparam);
			location.setLatLon(lat, lon, alt);
			StationData sd(location, stationID, station_name);
			sd.setSlope(slope, azi);
			vecStationMetaData.push_back( sd );
		}
	}
	
	if (mysql_stmt_close(stmt)) {
		throw IOException("Failed closing Mysql connection: "+std::string(mysql_error(mysql)), AT);
	}
	
	mysql_close(mysql);
}

//standard call to get the metadata as defined in IOInterface.h
void WWCSIO::readStationData(const Date&, std::vector<StationData>& vecStation)
{
	vecStation.clear();
	readStationMetaData(); //reads all the station meta data into the vecStationMetaData (member vector)
	vecStation = vecStationMetaData; //vecStationMetaData is a global vector holding all meta data
}

//standard call to get the meteo data as defined in IOInterface.h
void WWCSIO::readMeteoData(const Date& dateStart , const Date& dateEnd,
                            std::vector<std::vector<MeteoData> >& vecMeteo)
{
	if (vecStationMetaData.empty()) readStationMetaData();

	vecMeteo.clear();
	vecMeteo.insert(vecMeteo.begin(), vecStationMetaData.size(), vector<MeteoData>());

	for (size_t ii=0; ii<vecStationMetaData.size(); ii++) { //loop through relevant stations
		readData(dateStart, dateEnd, vecMeteo, ii);
	}
}

/**
* @brief Retrieve the meteo data for one given station from the database
* @param[in] dateStart start date of the data to retrieve
* @param[in] dateEnd end date of the data to retrieve
* @param[out] vecMeteo vector to store the data for all stations
* @param[in] stationindex index of the current station of interest in both vecStationMetaData and vecMeteo
*/
void WWCSIO::readData(const Date& dateStart, const Date& dateEnd, std::vector< std::vector<MeteoData> >& vecMeteo,
                       const size_t& stationindex) const
{
	const size_t nrMeteoFields( meteoDataFields.size() ); //this is given by the meteodata parsing vector meteoDataFields
	vecMeteo.at(stationindex).clear();

	Date dateS(dateStart), dateE(dateEnd);
	dateS.setTimeZone(in_dflt_TZ);
	dateE.setTimeZone(in_dflt_TZ);
	
	MYSQL *mysql = mysql_wrp::initMysql(mysqlhost, mysqluser, mysqlpass, mysqldb, mysql_options);
	MYSQL_STMT *stmt = mysql_wrp::initStmt(&mysql, meteoDataQuery, 3);
	
	const StationData sd( vecStationMetaData[stationindex] );
	const std::string stationID( sd.getStationID() );
	std::vector<SQL_FIELD> params_fields{ SQL_FIELD(stationID), SQL_FIELD(dateStart), SQL_FIELD(dateEnd)};
	mysql_wrp::bindParams(&stmt, params_fields);
	
	if (mysql_stmt_execute(stmt)) {
		throw IOException("Error executing statement", AT);
	} else { //retrieve results
		mysql_wrp::bindResults(&stmt, meteoDataFields);
		
		//create the MeteoData template
		MeteoData md(sd);
		for (size_t ii=0; ii<nrMeteoFields; ++ii) {
			if (meteoDataFields[ii].isDate) continue;
			if (!md.param_exists(meteoDataFields[ii].param)) md.addParameter(meteoDataFields[ii].param);
		}
		
		do {
			//reset the MeteoData object
			md.reset();
			md.date.setUndef(true);
			
			const int status = mysql_stmt_fetch(stmt);
			if (status==1 || status==MYSQL_NO_DATA) break;
			
			//loop over all fields that we should find
			for (size_t ii=0; ii<nrMeteoFields; ++ii) {
				if (meteoDataFields[ii].isDate) {
					md.setDate( meteoDataFields[ii].getDate(in_dflt_TZ) ); //get from Mysql without TZ, set it to input TZ
					md.date.setTimeZone(out_dflt_TZ); //set to requested TZ
					continue;
				} else {
					md( meteoDataFields[ii].param ) = mysql_wrp::retrieveData(meteoDataFields[ii], meteoDataFields[ii].processing);
				}
			}
			
			vecMeteo[stationindex].push_back( md );
		} while (true);
	}
	
	if (mysql_stmt_close(stmt)) {
		throw IOException("Failed closing Mysql connection: "+std::string(mysql_error(mysql)), AT);
	}
	
	mysql_close(mysql);
}

} //namespace
