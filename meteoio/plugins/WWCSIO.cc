// SPDX-License-Identifier: LGPL-3.0-or-later
/***********************************************************************************/
/*  Copyright 2014 Snow and Avalanche Study Establishment    SASE-CHANDIGARH       */
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

#ifdef _WIN32
	#include <winsock.h>
#endif // _WIN32

#include <mysql.h>
#include <stdio.h>
#include <cstring>
#include <algorithm>

using namespace std;

namespace mio {
/**
* @page wwcs WWCSIO
* @section WWCS_format Format
* This is the plugin required to get meteorological data from the WWCS MySQL database.
*
* @section WWCS_units Units
* The units are assumed to be the following:
* - __temperatures__ in celsius
* - __relative humidity__ in %
* - __wind speed__ in m/s
* - __precipitations__ in mm/h
* - __radiation__ in W/m²
*
* @section WWCS_keywords Keywords
* This plugin uses the following keywords:
* - COORDSYS: coordinate system (see Coords); [Input] and [Output] section
* - COORDPARAM: extra coordinates parameters (see Coords); [Input] and [Output] section
* - WWCS_HOST: MySQL Host Name (e.g. localhost or 191.168.145.20); [Input] section
* - WWCS_DB: MySQL Database (e.g. snowpack); [Input] section
* - WWCS_USER: MySQL User Name (e.g. root); [Input] section
* - WWCS_PASS: MySQL password; [Input] section
* - TIME_ZONE: For [Input] and [Output] sections
* - STATION#: station code for the given number #; [Input] section
*/

const double WWCSIO::plugin_nodata = -999.; //plugin specific nodata value. It can also be read by the plugin (depending on what is appropriate)
const string WWCSIO::MySQLQueryStationMetaData = "SELECT stationName, latitude, longitude, altitude, slope, azimuth FROM sites WHERE StationID=?";
const string WWCSIO::MySQLQueryMeteoData = "SELECT timestamp, ta, rh, p, logger_ta, logger_rh FROM meteoseries WHERE stationID='STATION_NUMBER' AND TimeStamp>='START_DATE' AND TimeStamp<='END_DATE' ORDER BY timestamp ASC";

WWCSIO::WWCSIO(const std::string& configfile)
        : cfg(configfile), vecStationIDs(), vecStationMetaData(),
          mysqlhost(), mysqldb(), mysqluser(), mysqlpass(),
          coordin(), coordinparam(), coordout(), coordoutparam(),
          in_dflt_TZ(5.), out_dflt_TZ(5.)
{
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
	readConfig();
}

WWCSIO::WWCSIO(const Config& cfgreader)
        : cfg(cfgreader), vecStationIDs(), vecStationMetaData(),
          mysqlhost(), mysqldb(), mysqluser(), mysqlpass(),
          coordin(), coordinparam(), coordout(), coordoutparam(),
          in_dflt_TZ(5.), out_dflt_TZ(5.)
{
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
	readConfig();
}

void WWCSIO::readConfig()
{
	cfg.getValue("WWCS_HOST", "Input", mysqlhost);
	cfg.getValue("WWCS_DB", "Input", mysqldb);
	cfg.getValue("WWCS_USER", "Input", mysqluser);
	cfg.getValue("WWCS_PASS", "Input", mysqlpass);
	cfg.getValue("TIME_ZONE","Input", in_dflt_TZ, IOUtils::nothrow);
	cfg.getValue("TIME_ZONE","Output", out_dflt_TZ, IOUtils::nothrow);
}

void WWCSIO::readStationIDs(std::vector<std::string>& vecStationID) const
{
	vecStationID.clear();
	cfg.getValues("STATION", "INPUT", vecStationID);

	if (vecStationID.empty()) {
		cerr << "\tNo stations specified for WWCSIO... is this what you want?\n";
	}
}

void WWCSIO::readStationMetaData()
{
	vecStationMetaData.clear();
	std::vector<std::string> vecStationID;
	readStationIDs( vecStationID );

	for (size_t ii=0; ii<vecStationID.size(); ii++) {
		// Retrieve the station meta data - this only needs to be done once per instance
		std::vector<std::string> stationMetaData;
		getStationMetaData(vecStationID[ii], MySQLQueryStationMetaData, stationMetaData);

		/*Coords location(coordin,coordinparam);
		location.setLatLon(lat, longi, alt);
		const std::string station_name = (!stao_name.empty())? vecStationID[ii] + ":" + stao_name : vecStationID[ii];
		StationData sd(location, vecStationID[ii], station_name);
		sd.setSlope(slope, azi);
		vecStationMetaData.push_back( sd );*/
	}

}

void WWCSIO::getStationMetaData(const std::string& stationID,
                                 const std::string& sqlQuery, std::vector<std::string>& vecMetaData)
{
	vecMetaData.clear();

	MYSQL *mysql = mysql_init(nullptr);
	//mysql_options(mysql, MYSQL_OPT_RECONNECT, &reconnect);
	if (!mysql_real_connect(mysql, mysqlhost.c_str(), mysqluser.c_str(), mysqlpass.c_str(), mysqldb.c_str(), 0, NULL, 0))
		throw IOException("Could not initiate connection to Mysql server "+mysqlhost+": "+std::string(mysql_error(mysql)), AT);

	MYSQL_STMT *stmt = mysql_stmt_init(mysql);
	if (!stmt) throw IOException("Could not allocate memory for mysql statement", AT);

	if (mysql_stmt_prepare(stmt, MySQLQueryStationMetaData.c_str(), MySQLQueryStationMetaData.size())) {
		throw IOException("Error preparing mysql statement", AT);
	} else {
		const long unsigned int param_count = mysql_stmt_param_count(stmt);
		if (param_count!=1) throw IOException("Wrong number of parameters in mysql statement", AT);
		
		MYSQL_BIND stmtParams[1];
		unsigned long argLength = stationID.size();
		memset(&stmtParams, 0, sizeof(stmtParams));
		stmtParams[0].buffer_type = MYSQL_TYPE_STRING;
		stmtParams[0].buffer_length = argLength;
		stmtParams[0].buffer = (char*)stationID.c_str();
		stmtParams[0].length = &argLength;
		stmtParams[0].is_null = 0;

		if (mysql_stmt_bind_param(stmt, stmtParams)) {
			throw IOException("Error binding parameters", AT);
		} else if (mysql_stmt_execute(stmt)) {
			throw IOException("Error executing statement", AT);
		} else {
			MYSQL_RES *prepare_meta_result = mysql_stmt_result_metadata(stmt);
			if (!prepare_meta_result) throw IOException("Error executing meta statement", AT);
			const int column_count= mysql_num_fields(prepare_meta_result);
			if (column_count!=6) throw IOException("Wrong number of columns returned", AT);
			
			static const int STRING_SIZE = 50;
			char statName[STRING_SIZE];
			double lat, lon, alt, slope, azi;
			MYSQL_BIND result[6];
			memset(result, 0, sizeof(result));
			unsigned long length[6];
			bool is_null[6];
			bool error[6];
			
			//stationname
			result[0].buffer_type= MYSQL_TYPE_STRING;
			result[0].buffer= (char *)statName;
			result[0].buffer_length= STRING_SIZE;
			result[0].is_null= &is_null[0];
			result[0].length= &length[0];
			result[0].error= &error[0];
			
			//latitude
			result[1].buffer_type= MYSQL_TYPE_DOUBLE;
			result[1].buffer= (char *)&lat;
			result[1].is_null= &is_null[1];
			result[1].length= &length[1];
			result[1].error= &error[1];
			
			//longitude
			result[2].buffer_type= MYSQL_TYPE_DOUBLE;
			result[2].buffer= (char *)&lon;
			result[2].is_null= &is_null[2];
			result[2].length= &length[2];
			result[2].error= &error[2];
			
			//altitude
			result[3].buffer_type= MYSQL_TYPE_DOUBLE;
			result[3].buffer= (char *)&alt;
			result[3].is_null= &is_null[3];
			result[3].length= &length[3];
			result[3].error= &error[3];
			
			//slope
			result[4].buffer_type= MYSQL_TYPE_DOUBLE;
			result[4].buffer= (char *)&slope;
			result[4].is_null= &is_null[4];
			result[4].length= &length[4];
			result[4].error= &error[4];
			
			//azi
			result[5].buffer_type= MYSQL_TYPE_DOUBLE;
			result[5].buffer= (char *)&azi;
			result[5].is_null= &is_null[5];
			result[5].length= &length[5];
			result[5].error= &error[5];

			if (mysql_stmt_bind_result(stmt, result)) throw IOException("Error binding results", AT);
			if (mysql_stmt_store_result(stmt)) throw IOException("mysql_stmt_store_result failed", AT);
			
			while (!mysql_stmt_fetch(stmt)) {
				std::cout << "Result returned: " << statName << " (" << lat << " , " << lon << " , " << alt << ") slope=" << slope << " azi=" << azi << "\n";
			}
			
			mysql_free_result(prepare_meta_result);
		}
	}
	if (mysql_stmt_close(stmt)) {
		throw IOException("Failed closing Mysql connection: "+std::string(mysql_error(mysql)), AT);
	}
	
	mysql_close(mysql);
}

void WWCSIO::readStationData(const Date&, std::vector<StationData>& vecStation)
{
	vecStation.clear();
	readStationMetaData(); //reads all the station meta data into the vecStationMetaData (member vector)
	vecStation = vecStationMetaData; //vecStationMetaData is a global vector holding all meta data
}

void WWCSIO::readMeteoData(const Date& dateStart , const Date& dateEnd,
                            std::vector<std::vector<MeteoData> >& vecMeteo)
{
	if (vecStationMetaData.empty()) readStationMetaData();

	vecMeteo.clear();
	vecMeteo.insert(vecMeteo.begin(), vecStationMetaData.size(), vector<MeteoData>());

	for (size_t ii=0; ii<vecStationMetaData.size(); ii++) { //loop through relevant stations
		readData(dateStart, dateEnd, vecMeteo, ii, vecStationMetaData);
	}
}

//read meteo data for one station
void WWCSIO::readData(const Date& dateStart, const Date& dateEnd, std::vector< std::vector<MeteoData> >& vecMeteo,
                       const size_t& stationindex, const std::vector<StationData>& vecMeta) const
{
	vecMeteo.at(stationindex).clear();

	Date dateS(dateStart), dateE(dateEnd);
	dateS.setTimeZone(in_dflt_TZ);
	dateE.setTimeZone(in_dflt_TZ);

	std::string stat_abk, stao_nr;
	std::vector<std::string> vecHTS1;
	std::vector< std::vector<std::string> > vecResult;
	//parseStationID(vecMeta.at(stationindex).getStationID(), stat_abk, stao_nr);

	getStationData(stat_abk, stao_nr, dateS, dateE, vecHTS1, vecResult);
	MeteoData tmpmd;
	tmpmd.meta = vecMeta.at(stationindex);

	for (size_t ii=0; ii<vecResult.size(); ii++){
		parseDataSet(vecResult[ii], tmpmd);
		convertUnits(tmpmd);
		vecMeteo.at(stationindex).push_back( tmpmd ); //Now insert tmpmd
	}
}

void WWCSIO::convertUnits(MeteoData& meteo)
{
	meteo.standardizeNodata(plugin_nodata);

	//converts C to Kelvin, converts RH to [0,1]
	double& ta = meteo(MeteoData::TA);
	ta = IOUtils::C_TO_K(ta);

	double& tss = meteo(MeteoData::TSS);
	tss = IOUtils::C_TO_K(tss);

	double& rh = meteo(MeteoData::RH);
	if (rh != IOUtils::nodata)
	rh /= 100.;

	/*double& hs = meteo(MeteoData::HS);
	if (hs != IOUtils::nodata)
	hs /= 100.0;

	double& rswr = meteo(MeteoData::RSWR);
	if (rswr != IOUtils::nodata)
	rswr /= 100.0;*/
}

void WWCSIO::parseDataSet(const std::vector<std::string>& i_meteo, MeteoData& md) const
{
	const std::string statID( md.meta.getStationID() );

	if (!IOUtils::convertString(md.date, i_meteo.at(0), in_dflt_TZ))
		throw ConversionFailedException("Invalid timestamp for station "+statID+": "+i_meteo.at(0), AT);
	if (!IOUtils::convertString(md(MeteoData::TA), i_meteo.at(1)))
		throw ConversionFailedException("Invalid TA for station "+statID+": "+i_meteo.at(1), AT);
	if (!IOUtils::convertString(md(MeteoData::RH), i_meteo.at(2)))
		throw ConversionFailedException("Invalid RH for station "+statID+": "+i_meteo.at(2), AT);
	if (!IOUtils::convertString(md(MeteoData::VW), i_meteo.at(3)))
		throw ConversionFailedException("Invalid VW for station "+statID+": "+i_meteo.at(3), AT);
	if (!IOUtils::convertString(md(MeteoData::HS), i_meteo.at(4)))
		throw ConversionFailedException("Invalid HS for station "+statID+": "+i_meteo.at(4), AT);
	if (!IOUtils::convertString(md(MeteoData::TSS), i_meteo.at(5)))
		throw ConversionFailedException("Invalid TSS for station "+statID+": "+i_meteo.at(5), AT);
	if (!IOUtils::convertString(md(MeteoData::RSWR), i_meteo.at(6)))
		throw ConversionFailedException("Invalid RSWR for station "+statID+": "+i_meteo.at(6), AT);
	if (!IOUtils::convertString(md(MeteoData::ISWR), i_meteo.at(7)))
		throw ConversionFailedException("Invalid ISWR for station "+statID+": "+i_meteo.at(7), AT);
	if (!IOUtils::convertString(md(MeteoData::ILWR), i_meteo.at(8)))
		throw ConversionFailedException("Invalid ILWR for station "+statID+": "+i_meteo.at(8), AT);
	if (!IOUtils::convertString(md(MeteoData::PSUM), i_meteo.at(9)))
		throw ConversionFailedException("Invalid PSUM for station "+statID+": "+i_meteo.at(9), AT);
	if (!IOUtils::convertString(md(MeteoData::P), i_meteo.at(10)))
		throw ConversionFailedException("Invalid P for station "+statID+": "+i_meteo.at(10), AT);
}

bool WWCSIO::getStationData(const std::string& stat_abk, const std::string& stao_nr,
                             const Date& dateS, const Date& dateE,
                             const std::vector<std::string>& vecHTS1,
                             std::vector< std::vector<std::string> >& vecMeteoData) const
{
	vecMeteoData.clear();
	bool fullStation = true;

	// Creating MySQL TimeStamps
	std::string sDate( dateS.toString(Date::ISO) );
	std::replace( sDate.begin(), sDate.end(), 'T', ' ');
	std::string eDate( dateE.toString(Date::ISO) );
	std::replace( eDate.begin(), eDate.end(), 'T', ' ');

	//SELECT timestamp, ta, rh, p, logger_ta, logger_rh FROM meteoseries WHERE stationID='STATION_NUMBER' AND TimeStamp>='START_DATE' AND TimeStamp<='END_DATE' ORDER BY timestamp ASC
	std::string Query2( MySQLQueryMeteoData );
	const std::string str1( "STATION_NAME" );
	const std::string str2( "STATION_NUMBER" );
	const std::string str3( "START_DATE" );
	const std::string str4( "END_DATE" );
	Query2.replace( Query2.find(str1), str1.length(), stat_abk ); // set 1st variable's value
	Query2.replace( Query2.find(str2), str2.length(), stao_nr ); // set 2nd variable's value
	Query2.replace( Query2.find(str3), str3.length(), sDate ); // set 3rd variable's value
	Query2.replace( Query2.find(str4), str4.length(), eDate ); // set 4th variable's value

	MYSQL *conn2 = mysql_init(nullptr);
	if (!mysql_real_connect(conn2, mysqlhost.c_str(), mysqluser.c_str(), mysqlpass.c_str(), mysqldb.c_str(), 0, nullptr, 0)) {
		throw IOException("Could not initiate connection to Mysql server "+mysqlhost, AT);
	}

	if (mysql_query(conn2, Query2.c_str())) {
		throw IOException("Query2 \""+Query2+"\" failed to execute", AT);
	}

	MYSQL_RES *res2 = mysql_use_result(conn2);
	const unsigned int column_no2 = mysql_num_fields(res2);
	MYSQL_ROW row2;
	while ( ( row2= mysql_fetch_row(res2) ) != nullptr ) {
		std::vector<std::string> vecData;
		for (unsigned int ii=0; ii<column_no2; ii++) {
			std::string row_02;
				if (!row2[ii]){
					IOUtils::convertString(row_02,"-999.0");// HARD CODED :(
				}else{
					IOUtils::convertString(row_02, row2[ii]);
				 }
			vecData.push_back(row_02);
		}
		if (fullStation) {
			for (unsigned int ii=0; ii<static_cast<unsigned int>(vecHTS1.size()); ii++) {
				vecData.push_back(vecHTS1.at(ii));
			}
		}
		vecMeteoData.push_back(vecData);
	}

	mysql_free_result(res2);
	mysql_close(conn2);
	return fullStation;
}

} //namespace
