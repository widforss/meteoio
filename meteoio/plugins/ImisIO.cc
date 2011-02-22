/***********************************************************************************/
/*  Copyright 2009, 2010 WSL Institute for Snow and Avalanche Research   SLF-DAVOS */
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
#include "ImisIO.h"

using namespace std;
using namespace oracle;
using namespace oracle::occi;

namespace mio {
/**
 * @page imis IMIS
 * @section imis_format Format
 * This plugin reads data directly from the IMIS network database (Oracle database). 
 * It retrieves standard IMIS data as well as ENETZ and ANETZ data.
 *
 * @section imis_units Units
 * The units are assumed to be the following:
 * - temperatures in celsius
 * - relative humidity in %
 * - wind speed in m/s
 * - precipitations in mm/h
 * - radiation in W/m²
 *
 * @section imis_keywords Keywords
 * This plugin uses the following keywords:
 * - COORDSYS: input coordinate system (see Coords) specified in the [Input] section
 * - COORDPARAM: extra input coordinates parameters (see Coords) specified in the [Input] section
 * - COORDSYS: output coordinate system (see Coords) specified in the [Output] section
 * - COORDPARAM: extra output coordinates parameters (see Coords) specified in the [Output] section
 * - DBNAME: name of the database to use (exemple: sdbo)
 * - DBUSER: user name to use when connecting to the database
 * - DBPASS: password to use when connecting to the database
 * - NROFSTATIONS: total number of stations listed for use
 * - STATION#: station code for the given number #
 * - USEANETZ: use ANETZ stations to provide precipitations for normal IMIS stations. Each IMIS station is associated with one or two ANETZ stations and does a weighted average to get what should be its local precipitations
 */

const double ImisIO::plugin_nodata = -999.0; //plugin specific nodata value
const double ImisIO::in_tz = 1.; //All IMIS data is in gmt+1

const string ImisIO::sqlQueryMeteoData = "SELECT to_char(datum, 'YYYY-MM-DD HH24:MI') as datum, avg(ta) as ta, avg(iswr) as iswr, avg(vw) as vw, avg(dw) as dw, avg(rh) as rh, avg(ilwr) as ilwr, avg(hnw) as hnw, avg(tsg) as tsg, avg(tss) as tss, avg(hs) as hs, avg(rswr) as rswr FROM ams.v_ams_raw WHERE stat_abk=:1 AND stao_nr=:2 AND datum>=:3 AND datum<=:4 GROUP BY datum ORDER BY datum asc";

const string ImisIO::sqlQueryStationData = "SELECT stao_name,stao_x,stao_y,stao_h,hts1_1,hts1_2,hts1_3 FROM station2.standort WHERE stat_abk like :1 AND stao_nr=:2";

const string ImisIO::sqlQueryDriftStation = "SELECT drift_stat_abk, drift_stao_nr FROM station2.v_snow_drift_standort WHERE application_code='snowpack' AND snow_stat_abk=:1 AND snow_stao_nr=:2";

const string ImisIO::sqlQueryMeteoDataDrift = "SELECT  TO_CHAR(a.datum, 'YYYY-MM-DD HH24:MI') as datum, avg(a.ta) as ta, avg(a.iswr) as iswr, avg(a.vw) as vw, avg(a.dw) as dw, avg(a.rh) as rh, avg(a.ilwr) as ilwr, avg(a.hnw) as hnw, avg(a.tsg) as tsg, avg(a.tss) as tss, avg(a.hs) as hs, avg(a.rswr) as rswr, avg(b.vw) as VW_DRIFT, avg(b.dw) as DW_DRIFT FROM (SELECT * FROM ams.v_ams_raw where stat_abk=:1 and stao_nr=:2 and datum >=:3 AND datum <= :4) a LEFT OUTER JOIN (SELECT * FROM ams.v_ams_raw WHERE stat_abk=:5 and stao_nr=:6 AND datum > :3 AND datum < :4) b ON a.datum = b.datum GROUP BY a.datum ORDER BY a.datum asc";

std::map<std::string, AnetzData> ImisIO::mapAnetz;
const bool ImisIO::__init = ImisIO::initStaticData();

bool ImisIO::initStaticData()
{
	//Associate string with AnetzData
	mapAnetz["AMD2"] = AnetzData(2,"*GLA","*SAE","",3,1.2417929,0.548411708,-0.0692799);
	mapAnetz["ANV2"] = AnetzData(2,"*EVO","*MVE","",2,0.7920454,0.771111962,IOUtils::nodata);
	mapAnetz["ANV3"] = AnetzData(1,"*EVO","","",1,1.6468,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["ARO2"] = AnetzData(2,"*EVO","*GSB","",2,0.9692294,0.218384531,IOUtils::nodata);
	mapAnetz["ARO3"] = AnetzData(2,"*EVO","*ZER","",3,1.0748285,1.649860092,-0.0728015);
	mapAnetz["BED2"] = AnetzData(2,"*PIO","*ULR","",3,0.9934869,1.047586006,-0.05489259);
	mapAnetz["BED3"] = AnetzData(2,"*PIO","*ULR","",2,0.6999,0.4122,IOUtils::nodata);
	mapAnetz["BER2"] = AnetzData(2,"*ROB","*COV","",3,1.4454061,0.558775717,-0.05063568);
	mapAnetz["BER3"] = AnetzData(2,"*ROB","*COV","",2,0.378476,0.817976734,IOUtils::nodata);
	mapAnetz["BEV2"] = AnetzData(2,"*SAM","*COV","",3,1.8237643,0.853292298,-0.33642156);
	mapAnetz["BOG2"] = AnetzData(1,"*ROE","","",1,1.0795,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["BOR2"] = AnetzData(1,"*VIS","","",1,1.0662264,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["BOV2"] = AnetzData(2,"*GSB","*EVO","",2,0.3609309,0.934922978,IOUtils::nodata);
	mapAnetz["CAM2"] = AnetzData(2,"*PIO","*COM","",2,0.750536,0.426864157,IOUtils::nodata);
	mapAnetz["CHA2"] = AnetzData(2,"*AIG","*SIO","",2,0.7107216,0.99869915,IOUtils::nodata);
	mapAnetz["CON2"] = AnetzData(2,"*SIO","*MVE","",3,3.5344378,1.952708399,-0.74509918);
	mapAnetz["DAV2"] = AnetzData(2,"*WFJ","*DAV","",3,0.594108,1.091565634,-0.12150025);
	mapAnetz["DAV3"] = AnetzData(2,"*WFJ","*DAV","",3,0.9266618,0.815816241,-0.06248703);
	mapAnetz["DAV4"] = AnetzData(2,"*WFJ","*DAV","",3,0.9266618,0.815816241,-0.06248703);
	mapAnetz["DAV5"] = AnetzData(2,"*WFJ","*DAV","",3,0.9266618,0.815816241,-0.06248703);
	mapAnetz["DTR2"] = AnetzData(2,"*PIO","*COM","",2,0.0384,0.9731,IOUtils::nodata);
	mapAnetz["DVF2"] = AnetzData(1,"*WFJ","","",1,1,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["ELM2"] = AnetzData(1,"*GLA","","",1,1.4798048,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["ELS2"] = AnetzData(2,"*ABO","*INT","",3,1.0886792,0.568730457,-0.07758286);
	mapAnetz["FAE2"] = AnetzData(1,"*ABO","","",1,2.1132038,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["FIR2"] = AnetzData(2,"*INT","*GRH","",3,1.2416838,0.243226327,-0.02392287);
	mapAnetz["FIS2"] = AnetzData(1,"*ABO","","",1,1.1991,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["FNH2"] = AnetzData(2,"*AIG","*GSB","",2,1.3949428,0.297933922,IOUtils::nodata);
	mapAnetz["FOU2"] = AnetzData(1,"*GSB","","",1,0.8448844,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["FUL2"] = AnetzData(2,"*FEY","*AIG","",2,1.070156,0.587972864,IOUtils::nodata);
	mapAnetz["FUS2"] = AnetzData(1,"*PIO","","",1,1.3557753,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["GAD2"] = AnetzData(2,"*ENG","*GUE","",3,0.9764334,0.814293499,-0.07074082);
	mapAnetz["GAN2"] = AnetzData(2,"*ABO","*VIS","",2,0.520224,0.825813298,IOUtils::nodata);
	mapAnetz["GLA2"] = AnetzData(1,"*GLA","","",1,1.7186314,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["GOM2"] = AnetzData(2,"*ULR","*GRH","",2,0.4413,0.4235,IOUtils::nodata);
	mapAnetz["GOM3"] = AnetzData(2,"*ULR","*GRH","",2,0.3269755,0.62995601,IOUtils::nodata);
	mapAnetz["GUT2"] = AnetzData(2,"*GRH","*ENG","",2,0.3977985,0.463100458,IOUtils::nodata);
	mapAnetz["GUT3"] = AnetzData(2,"*GRH","*ENG","",2,0.3977985,0.463100458,IOUtils::nodata);
	mapAnetz["HTR2"] = AnetzData(2,"*HIR","*COM","",2,0.8668,0.5939,IOUtils::nodata);
	mapAnetz["HTR3"] = AnetzData(2,"*SBE","*COM","",2,1.3023275,-0.663411226,IOUtils::nodata);
	mapAnetz["ILI2"] = AnetzData(1,"*AIG","","",1,1.2341516,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["JUL2"] = AnetzData(2,"*COV","*SAM","",2,0.4900961,0.871078269,IOUtils::nodata);
	mapAnetz["KES2"] = AnetzData(2,"*SAM","*DAV","",2,0.847596,1.112635571,IOUtils::nodata);
	mapAnetz["KLO2"] = AnetzData(1,"*DAV","","",1,1.585,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["KLO3"] = AnetzData(2,"*DAV","*WFJ","",3,0.8352,0.9493,-0.0526);
	mapAnetz["LAU2"] = AnetzData(2,"*ABO","*SIO","",2,0.3037172,0.791695555,IOUtils::nodata);
	mapAnetz["LUK2"] = AnetzData(2,"*DIS","*PIO","",3,0.8593029,0.378261758,0.85930291);
	mapAnetz["MEI2"] = AnetzData(3,"*ENG","*GUE","*ALT",3,0.3882119,0.399244859,0.3298324);
	mapAnetz["MES2"] = AnetzData(2,"*HIR","*COM","",2,1.3552818,-0.393843912,IOUtils::nodata);
	mapAnetz["MUN2"] = AnetzData(1,"*VIS","","",1,0.8624804,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["NAR2"] = AnetzData(2,"*PIO","*COM","",3,0.4089981,0.873419792,-0.028464);
	mapAnetz["NEN2"] = AnetzData(2,"*SIO","*EVO","",3,0.9352699,1.312867984,-0.14543389);
	mapAnetz["OBM2"] = AnetzData(2,"*AIG","*MLS","",3,1.9413387,1.64250639,-0.37210579);
	mapAnetz["OBW2"] = AnetzData(2,"*GRH","*ULR","",3,0.2471352,1.219258485,-0.02153657);
	mapAnetz["OBW3"] = AnetzData(2,"*GRH","*ULR","",2,0.5274,0.4815,IOUtils::nodata);
	mapAnetz["OFE2"] = AnetzData(1,"*SCU","","",1,1.8758744,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["ORT2"] = AnetzData(1,"*GLA","","",1,1.6214,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["OTT2"] = AnetzData(1,"*ABO","","",1,1.3759903,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["PAR2"] = AnetzData(1,"*WFJ","","",1,1.6252986,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["PUZ2"] = AnetzData(2,"*DIS","*GUE","",2,0.9481811,0.1490937,IOUtils::nodata);
	mapAnetz["ROA2"] = AnetzData(2,"*INT","*NAP","",3,1.748338,0.574491521,-0.1670437);
	mapAnetz["SAA2"] = AnetzData(2,"*ZER","*VIS","",3,0.6316695,1.210149675,-0.11760175);
	mapAnetz["SAA3"] = AnetzData(1,"*VIS","","",1,1.2905,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["SCA2"] = AnetzData(2,"*ALT","*DIS","",2,0.8118627,0.360141586,IOUtils::nodata);
	mapAnetz["SCA3"] = AnetzData(2,"*ALT","*GLA","",2,0.4768725,0.819642544,IOUtils::nodata);
	mapAnetz["SCB2"] = AnetzData(2,"*ENG","*INT","",3,1.0535332,1.21234263,-0.1307221);
	mapAnetz["SCH2"] = AnetzData(1,"*INT","","",1,1.54557,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["SHE2"] = AnetzData(1,"*INT","","",1,1.1065938,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["SIM2"] = AnetzData(2,"*COM","*SBE","",2,0.6861131,0.296215066,IOUtils::nodata);
	mapAnetz["SLF2"] = AnetzData(1,"*WFJ","","",1,0.9585787,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["SMN2"] = AnetzData(1,"*SCU","","",1,0.6979953,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["SPN2"] = AnetzData(2,"*VIS","*ZER","",2,1.1049,1.4598,IOUtils::nodata);
	mapAnetz["SPN3"] = AnetzData(1,"*VIS","","",1,1.0244902,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["STH2"] = AnetzData(2,"*PLF","*ABO","",3,1.1252659,0.893324895,-0.13194965);
	mapAnetz["STN2"] = AnetzData(2,"*EVO","*MVE","",2,0.9042348,0.687519213,IOUtils::nodata);
	mapAnetz["TAM2"] = AnetzData(2,"*VAD","*GLA","",2,0.6304286,0.738150034,IOUtils::nodata);
	mapAnetz["TAM3"] = AnetzData(2,"*VAD","*GLA","",3,1.5515584,0.407868299,-0.0800763);
	mapAnetz["TRU2"] = AnetzData(2,"*MVE","*VIS","",2,1.1359,0.6577,IOUtils::nodata);
	mapAnetz["TUJ2"] = AnetzData(2,"*GUE","*DIS","",2,0.3636322,0.591777057,IOUtils::nodata);
	mapAnetz["TUJ3"] = AnetzData(2,"*GUE","*DIS","",2,0.4742,0.7791,IOUtils::nodata);
	mapAnetz["TUM2"] = AnetzData(1,"*DIS","","",1,1.752091,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["URS2"] = AnetzData(2,"*GUE","*GRH","",3,0.6847615,0.277707092,-0.03085219);
	mapAnetz["VAL2"] = AnetzData(2,"*PIO","*GUE","",3,1.2130704,0.508735389,-0.02905053);
	mapAnetz["VDS2"] = AnetzData(1,"*MVE","","",1,1.8282525,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["VIN2"] = AnetzData(1,"*SCU","","",1,0.8245,IOUtils::nodata,IOUtils::nodata);
	mapAnetz["VLS2"] = AnetzData(2,"*DIS","*HIR","",2,0.5764952,0.613916765,IOUtils::nodata);
	mapAnetz["ZER2"] = AnetzData(2,"*ZER","*EVO","",2,0.8707182,0.988158355,IOUtils::nodata);
	mapAnetz["ZER4"] = AnetzData(2,"*ZER","*EVO","",2,0.8707182,0.988158355,IOUtils::nodata);
	mapAnetz["ZNZ2"] = AnetzData(1,"*WFJ","","",1,0.9980525,IOUtils::nodata,IOUtils::nodata);

	return true;
}

void ImisIO::getDBParameters()
{
	cfg.getValue("DBNAME", "Input", oracleDBName_in);
	cfg.getValue("DBUSER", "Input", oracleUserName_in);
	cfg.getValue("DBPASS", "Input", oraclePassword_in);

	string tmp = cfg.get("USEANETZ", "Input", Config::nothrow);
	if (tmp != "") {
		useAnetz = cfg.get("USEANETZ", "Input");
	} else {
		useAnetz = false;
	}

	/*cfg.getValue("DBNAME", "Output", oracleDBName_out);
	cfg.getValue("DBUSER", "Output", oracleUserName_out);
	cfg.getValue("DBPASS", "Output", oraclePassword_out);*/
}

ImisIO::ImisIO(void (*delObj)(void*), const Config& i_cfg) : IOInterface(delObj), cfg(i_cfg)
{
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
	getDBParameters();
}

ImisIO::ImisIO(const std::string& configfile) : IOInterface(NULL), cfg(configfile)
{
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
	getDBParameters();
}

ImisIO::ImisIO(const Config& cfgreader) : IOInterface(NULL), cfg(cfgreader)
{
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
	getDBParameters();
}

ImisIO::~ImisIO() throw()
{
	cleanup();
}

void ImisIO::read2DGrid(Grid2DObject&, const std::string&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void ImisIO::readDEM(DEMObject&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void ImisIO::readLanduse(Grid2DObject&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void ImisIO::readAssimilationData(const Date&, Grid2DObject&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void ImisIO::readSpecialPoints(std::vector<Coords>&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void ImisIO::write2DGrid(const Grid2DObject&, const std::string&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void ImisIO::writeMeteoData(const std::vector< std::vector<MeteoData> >&,
                            const std::string&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void ImisIO::openDBConnection(oracle::occi::Environment*& env, oracle::occi::Connection*& conn)
{
	env  = Environment::createEnvironment();// static OCCI function
	conn = env->createConnection(oracleUserName_in, oraclePassword_in, oracleDBName_in);
}

void ImisIO::closeDBConnection(oracle::occi::Environment*& env, oracle::occi::Connection*& conn)
{
	try {
		if (conn != NULL)
			env->terminateConnection(conn);
		Environment::terminateEnvironment(env); // static OCCI function
	} catch (exception& e){
		Environment::terminateEnvironment(env); // static OCCI function
	}
}

void ImisIO::readStationData(const Date&, std::vector<StationData>& vecStation)
{
	vecStation.clear();

	if (vecMyStation.size() == 0){//Imis station meta data cannot change between time steps
		Environment *env = NULL;
		Connection *conn = NULL;

		try {
			openDBConnection(env, conn);

			readStationMetaData(conn); //reads all the station meta data into the vecMyStation (member vector)

			closeDBConnection(env, conn);
		} catch (exception& e){
			closeDBConnection(env, conn);
			throw IOException("Oracle Error: " + string(e.what()), AT); //Translation of OCCI exception to IOException
		}
	}

	vecStation = vecMyStation; //vecMyStation is a global vector holding all meta data
}

/**
 * @brief A meta function that extracts all station names from the Config,
 *        parses them and retrieves all meta data from the IMIS database
 */
void ImisIO::readStationMetaData(oracle::occi::Connection*& conn)
{
	vector<string> vecStationName;
	readStationNames(vecStationName);

	for (unsigned int ii=0; ii<vecStationName.size(); ii++){

		const string& stationName = vecStationName[ii];
		string stName = "", stationNumber = "";
		vector<string> resultset;

		//the stationName consists of the STAT_ABK and the STAO_NR, e.g. "KLO2" consists of "KLO" and "2"
		parseStationName(stationName, stName, stationNumber);

		//Now connect to the database and retrieve the meta data - this only needs to be done once per instance
		getStationData(stName, stationNumber, resultset, conn);

		if (resultset.size() < 4)
			throw IOException("Could not read enough meta data for station "+stName+stationNumber, AT);

		double east, north, alt;
		if ((!IOUtils::convertString(east, resultset.at(1), std::dec))
		    || (!IOUtils::convertString(north, resultset.at(2), std::dec))
		    || (!IOUtils::convertString(alt, resultset.at(3), std::dec)))
			throw ConversionFailedException("Error while converting station coordinate from Imis DB", AT);

		Coords myCoord(coordin, coordinparam);
		myCoord.setXY(east, north, alt);
		vecMyStation.push_back(StationData(myCoord, stationName, resultset.at(0)));

		//Check whether there is a special drift station for the current station (only IMIS stations)
		string driftstation = getDriftStation(stName, stationNumber, conn);
		if (driftstation != "") mapDriftStation[vecStationName[ii]] = driftstation;
	}
}

std::string ImisIO::getDriftStation(std::string stationName, std::string stationNumber, oracle::occi::Connection*& conn)
{
	if (stationNumber == "") //not an IMIS station, thus no further querying necessary
		return "";

	string driftstation = "";

	try {
		Statement *stmt = conn->createStatement(sqlQueryDriftStation);
		ResultSet *rs = NULL;

		stmt->setString(1, stationName);    // set 1st variable's value
		stmt->setString(2, stationNumber);  // set 2nd variable's value
		rs = stmt->executeQuery();

		while (rs->next() == true) {
			driftstation = "";
			driftstation += rs->getString(1);
			driftstation += rs->getString(2);
		}

		stmt->closeResultSet(rs);
		conn->terminateStatement(stmt);
	} catch (exception& e){
		throw IOException(string(e.what()), AT); //Translation of OCCI exception to IOException
	}

	return driftstation;
}

/**
 * @brief This function breaks up the station name into two components (a string and a number e.g. KLO2 -> "KLO","2")
 * @param stationName The full name of the station (e.g. "KLO2")
 * @param stName      The string part of the name  (e.g. "KLO")
 * @param stNumber    The integer part of the name (e.g. "2")
 */
void ImisIO::parseStationName(const std::string& stationName, std::string& stName, std::string& stNumber)
{
	stName    = stationName.substr(0, stationName.length()-1); //The station name: e.g. KLO
	stNumber  = stationName.substr(stationName.length()-1, 1); //The station number: e.g. 2
	if(!std::isdigit(stNumber[0])) {
		//the station is one of these non-imis stations that don't contain a number...
		stName = stationName;
		stNumber = "0";
	}
}

/**
 * @brief This function extracts all info about the stations that are to be used from global Config object
 * @param vecStationName A vector that will hold all relevant stations as std::strings
 */
void ImisIO::readStationNames(std::vector<std::string>& vecStationName)
{
	vecStationName.clear();

	//Read in the StationNames
	string xmlpath="", str_stations="";
	unsigned int stations=0;

	cfg.getValue("NROFSTATIONS", "Input", str_stations);

	if (str_stations == "")
		throw ConversionFailedException("Error while reading value for NROFSTATIONS", AT);

	if (!IOUtils::convertString(stations, str_stations, std::dec))
		throw ConversionFailedException("Error while reading value for NROFSTATIONS", AT);

	for (unsigned int ii=0; ii<stations; ii++) {
		stringstream tmp_stream;
		string stationname="", tmp_file="";

		tmp_stream << (ii+1); //needed to construct key name
		cfg.getValue(string("STATION"+tmp_stream.str()), "Input", stationname);
		std::cout << "\tRead io.ini stationname: '" << stationname << "'" << std::endl;
		vecStationName.push_back(stationname);
	}
}


void ImisIO::readMeteoData(const Date& dateStart, const Date& dateEnd, std::vector< std::vector<MeteoData> >& vecMeteo,
                           const unsigned int& stationindex)
{
	Environment *env = NULL;
	Connection *conn = NULL;

	try {
		if (vecMyStation.size() == 0){
			openDBConnection(env, conn);
			readStationMetaData(conn); //reads all the station meta data into the vecMyStation (member vector)
		}

		if (vecMyStation.size() == 0){ //if there are no stations -> return
			if ((env != NULL) || (conn != NULL)) closeDBConnection(env, conn);
			return;
		}

		unsigned int indexStart=0, indexEnd=vecMyStation.size();

		//The following part decides whether all the stations are rebuffered or just one station
		if (stationindex == IOUtils::npos){
			vecMeteo.clear();
			vecMeteo.insert(vecMeteo.begin(), vecMyStation.size(), vector<MeteoData>());
		} else {
			if (stationindex < vecMeteo.size()){
				indexStart = stationindex;
				indexEnd   = stationindex+1;
			} else {
				throw IndexOutOfBoundsException("You tried to access a stationindex in readMeteoData that is out of bounds", AT);
			}
		}

		if ((env == NULL) || (conn == NULL))
			openDBConnection(env, conn);

		for (unsigned int ii=indexStart; ii<indexEnd; ii++) //loop through stations
			readData(dateStart, dateEnd, vecMeteo, ii, vecMyStation, env, conn);

		if (useAnetz){ //Important: we don't care about the metadata for ANETZ stations
			vector<StationData> vecAnetzStation;       //holds the unique ANETZ stations that need to be read
			vector< vector<MeteoData> > vecMeteoAnetz; //holds the meteo data of the ANETZ stations
			map<string, unsigned int> mapAnetzNames;   //associates an ANETZ station with an index within vecMeteoAnetz

			findAnetzStations(indexStart, indexEnd, mapAnetzNames, vecAnetzStation);
			vecMeteoAnetz.insert(vecMeteoAnetz.begin(), vecAnetzStation.size(), vector<MeteoData>());

			//date_anetz_start/end must be changed to be a multiple of 6h before the original dateStart, dateEnd
			Date date_anetz_start = Date(floor(dateStart.getJulianDate(true) * 4.0) / 4.0, 0.);
			date_anetz_start.setTimeZone(in_tz);
			Date date_anetz_end   = Date(floor(dateEnd.getJulianDate(true) * 4.0) / 4.0, 0.);
			date_anetz_end.setTimeZone(in_tz);

			//read Anetz Data
			for (unsigned int ii=0; ii<vecAnetzStation.size(); ii++)
				readData(date_anetz_start, dateEnd, vecMeteoAnetz, ii, vecAnetzStation, env, conn);

			//We got all the data, now calc psum for all ANETZ stations
			vector< vector<double> > vec_of_psums; //6 hour accumulations of hnw
			calculatePsum(date_anetz_start, date_anetz_end, vecMeteoAnetz, vec_of_psums);

			for (unsigned int ii=indexStart; ii<indexEnd; ii++){ //loop through relevant stations
				map<string,AnetzData>::const_iterator it = mapAnetz.find(vecMyStation.at(ii).getStationID());
				if (it != mapAnetz.end())
					assimilateAnetzData(date_anetz_start, it->second, vec_of_psums, mapAnetzNames, ii, vecMeteo);
			}
		}

		closeDBConnection(env, conn);
	} catch (exception& e){
		closeDBConnection(env, conn);		
		throw IOException("Oracle Error: " + string(e.what()), AT); //Translation of OCCI exception to IOException
	}
}

void ImisIO::assimilateAnetzData(const Date& dateStart, const AnetzData& ad,
                                 const std::vector< std::vector<double> > vec_of_psums,
                                 const std::map<std::string, unsigned int>& mapAnetzNames, const unsigned int& stationindex,
                                 std::vector< std::vector<MeteoData> >& vecMeteo)
{
	//Do coefficient calculation (getHNW) for every single station and data point
	vector<double> current_station_psum;
	getAnetzHNW(ad, mapAnetzNames, vec_of_psums, current_station_psum);

	unsigned int counter = 0;
	Date current_slice_date = dateStart;
	current_slice_date.setTimeZone(in_tz);
	for (unsigned int jj=0; jj<vecMeteo[stationindex].size(); jj++){
		while (vecMeteo[stationindex][jj].date > (current_slice_date+0.2485)){
			counter++;
			double julian = floor((current_slice_date.getJulianDate(true) +0.25001) * 4.0) / 4.0;
			current_slice_date = Date(julian, 0.);
			current_slice_date.setTimeZone(in_tz);
		}

		if (counter >= current_station_psum.size()) { break; } //should never happen
		
		//cout << "Current slice date: " << current_slice_date.toString(Date::ISO) 
		//	<< "  value: " << current_station_psum.at(counter) << endl;
		
		double& hnw = vecMeteo[stationindex][jj].hnw;
		//cout << vecMeteo[stationindex][jj].date.toString(Date::ISO) << ": " << hnw;
		if ((hnw == IOUtils::nodata) || (IOUtils::checkEpsilonEquality(hnw, 0.0, 0.001))){
			//replace by psum if there is no own value measured
			hnw = current_station_psum.at(counter);
		}
		//cout << "  ---> " << hnw << endl;
	}
}

void ImisIO::getAnetzHNW(const AnetzData& ad, const std::map<std::string, unsigned int>& mapAnetzNames,
                         const std::vector< std::vector<double> >& vec_of_psums, std::vector<double>& psum)
{
	map<string, unsigned int>::const_iterator it;

	vector<unsigned int> vecIndex; //this vector will hold up to three indexes for the Anetz stations (position in vec_of_psums)
	for (unsigned int ii=0; ii<ad.nrOfAnetzStations; ii++){
		it = mapAnetzNames.find(ad.anetzstations[ii]);
		vecIndex.push_back(it->second);
	}

	if (ad.nrOfAnetzStations == ad.nrOfCoefficients){
		//1, 2, or 3 ANETZ stations without interaction
		for (unsigned int kk=0; kk<vec_of_psums.at(vecIndex.at(0)).size(); kk++){
			double sum = 0.0;
			for (unsigned int ii=0; ii<ad.nrOfCoefficients; ii++){
				sum += ad.coeffs[ii] * vec_of_psums.at(vecIndex[ii])[kk]; 
			}
			psum.push_back(sum/12.0);

			//cout << kk << " --> sum: " << sum/12 << endl;
		}
	} else {
		if (ad.nrOfCoefficients != 3)
			throw IOException("Misconfiguration in ANETZ data", AT);

		// Exactly two ANETZ stations with one interaction term
		for (unsigned int kk=0; kk<vec_of_psums.at(vecIndex.at(0)).size(); kk++){
			double sum = 0.0;
			const double& hnw0 = vec_of_psums.at(vecIndex.at(0))[kk];
			const double& hnw1 = vec_of_psums.at(vecIndex.at(1))[kk];
			//cout << "0: Using " << ad.anetzstations[0] << " with hnw: " << hnw0 << endl;
			//cout << "1: Using " << ad.anetzstations[1] << " with hnw: " << hnw1 << endl;
			sum += ad.coeffs[0] * hnw0;
			sum += ad.coeffs[1] * hnw1;
			sum += ad.coeffs[2] * hnw0 * hnw1;

			psum.push_back(sum/12.0);
		}
	}
}

void ImisIO::calculatePsum(const Date& dateStart, const Date& dateEnd,
                           const std::vector< std::vector<MeteoData> >& vecMeteoAnetz,
                           std::vector< std::vector<double> >& vec_of_psums)
{
	unsigned int nr_of_slices = (unsigned int)((dateEnd.getJulianDate(true) - dateStart.getJulianDate(true) + 0.00001) * 4.0) + 1;

	for (unsigned int ii=0; ii<vecMeteoAnetz.size(); ii++){
		double tmp_psum = 0.0;
		Date current_date = dateStart;
		current_date.setTimeZone(in_tz);

		vector<double> vec_current_station;
		unsigned int counter_of_elements = 0;
		for (unsigned int jj=0; jj<vecMeteoAnetz[ii].size(); jj++){
			const Date& anetzdate = vecMeteoAnetz[ii][jj].date;
			const double& hnw = vecMeteoAnetz[ii][jj].hnw;

			if ((current_date < anetzdate) && ((current_date+0.25) > anetzdate)){
				;
			} else {
				if ((counter_of_elements > 0) && (counter_of_elements < 6)) //this is mystical, but kind of a guess of the future
					tmp_psum = tmp_psum * 6.0 / (double)counter_of_elements;

				vec_current_station.push_back(tmp_psum);

				current_date += 0.25;
				tmp_psum = 0.0;
				counter_of_elements = 0;
			}

			if (hnw != IOUtils::nodata){
				tmp_psum += hnw;
				counter_of_elements++;
			}

		}

		if ((counter_of_elements > 0) && (counter_of_elements < 6)) //this is mystical, but kind of a guess of the future
			tmp_psum = tmp_psum*6/counter_of_elements;

		vec_current_station.push_back(tmp_psum);

		for (unsigned int jj=vec_current_station.size(); jj<nr_of_slices; jj++){ //To fill up the vector
			vec_current_station.push_back(0.0);
		}

		vec_of_psums.push_back(vec_current_station);
	}

	for (unsigned int ii=1; ii<vec_of_psums.size(); ii++){
		if (vec_of_psums[ii].size() != vec_of_psums[ii-1].size())
			throw IOException("Error while summing up the precipitation data for the ANETZ stations", AT);
	}
}

void ImisIO::findAnetzStations(const unsigned int& indexStart, const unsigned int& indexEnd,
                               std::map<std::string, unsigned int>& mapAnetzNames, std::vector<StationData>& vecAnetzStation)
{
	set<string> uniqueStations;
	
	for (unsigned int ii=indexStart; ii<indexEnd; ii++){ //loop through stations
		map<string,AnetzData>::const_iterator it = mapAnetz.find(vecMyStation.at(ii).getStationID());
		if (it != mapAnetz.end()){
			for (unsigned int jj=0; jj<it->second.nrOfAnetzStations; jj++){
				uniqueStations.insert(it->second.anetzstations[jj]);
			}
		}
	}

	unsigned int pp = 0;
	for (set<string>::const_iterator ii=uniqueStations.begin(); ii!=uniqueStations.end(); ii++){
		mapAnetzNames[*ii] = pp;
		pp++;

		StationData sd;
		sd.stationID = *ii;
		vecAnetzStation.push_back(sd);
	}
}

/**
 * @brief A meta function to read meteo data for one specific station (specified by the stationindex)
 * @param dateStart    The beginning of the interval to retrieve data for
 * @param dateEnd      The end of the interval to retrieve data for
 * @param vecMeteo     The vector that will hold all MeteoData for each station
 * @param stationindex The index of the station as specified in the Config
 */
void ImisIO::readData(const Date& dateStart, const Date& dateEnd, std::vector< std::vector<MeteoData> >& vecMeteo,
                      const unsigned int& stationindex, const std::vector<StationData>& vecStationNames, 
                      oracle::occi::Environment*& env, oracle::occi::Connection*& conn)
{
	vecMeteo.at(stationindex).clear();

	string stationName="", stationNumber="";
	vector< vector<string> > vecResult;
	vector<int> datestart = vector<int>(5);
	vector<int> dateend   = vector<int>(5);

	parseStationName(vecStationNames.at(stationindex).getStationID(), stationName, stationNumber);

	//IMIS is in TZ=+1, so moving back to this timezone
	Date dateS(dateStart), dateE(dateEnd);
	dateS.setTimeZone(in_tz);
	dateE.setTimeZone(in_tz);
	dateS.getDate(datestart[0], datestart[1], datestart[2], datestart[3], datestart[4]);
	dateE.getDate(dateend[0], dateend[1], dateend[2], dateend[3], dateend[4]);

	//Oracle can't deal with an integer for the hour of 24, hence the following workaround
	if (datestart[3] == 24){
		Date tmpDate = dateS + 3.0/(24.*3600.); //add three seconds to omit 24 for 00
		tmpDate.getDate(datestart[0], datestart[1], datestart[2], datestart[3], datestart[4]);
	}
	if (dateend[3] == 24){
		Date tmpDate = dateE + 3.0/(24.*3600.); //add three seconds to omit 24 for 00
		tmpDate.getDate(dateend[0], dateend[1], dateend[2], dateend[3], dateend[4]);
	}

	//get data for one specific station 
	getImisData(stationName, stationNumber, datestart, dateend, vecResult, env, conn);

	MeteoData tmpmd;
	tmpmd.meta = vecStationNames.at(stationindex);
	for (unsigned int ii=0; ii<vecResult.size(); ii++){
		parseDataSet(vecResult[ii], tmpmd);
		convertUnits(tmpmd);

		//For IMIS stations the hnw value is a rate (mm/h), therefore we need to 
		//divide it by two to conjure the accumulated value for the half hour
		if (tmpmd.meta.stationID.length() > 0){
			if (tmpmd.meta.stationID[0] != '*') //excludes ANETZ stations, they come in hourly sampling
				if (tmpmd.hnw != IOUtils::nodata)
					tmpmd.hnw /= 2; //half hour accumulated value for IMIS stations only
		}

		vecMeteo.at(stationindex).push_back(tmpmd); //Now insert tmpmd 
	}
}

/**
 * @brief Puts the data that has been retrieved from the database into a MeteoData object
 * @param _meteo a row of meteo data from the database (note: order important, matches SQL query)
 * @param md     the object to copy the data to
 */
void ImisIO::parseDataSet(const std::vector<std::string>& _meteo, MeteoData& md)
{
	IOUtils::convertString(md.date, _meteo.at(0), in_tz, dec);
	IOUtils::convertString(md.param(MeteoData::TA),   _meteo.at(1),  dec);
	IOUtils::convertString(md.param(MeteoData::ISWR), _meteo.at(2),  dec);
	IOUtils::convertString(md.param(MeteoData::VW),   _meteo.at(3),  dec);
	IOUtils::convertString(md.param(MeteoData::DW),   _meteo.at(4),  dec);
	IOUtils::convertString(md.param(MeteoData::RH),   _meteo.at(5),  dec);
	IOUtils::convertString(md.param(MeteoData::ILWR), _meteo.at(6),  dec);
	IOUtils::convertString(md.param(MeteoData::HNW),  _meteo.at(7),  dec);
	IOUtils::convertString(md.param(MeteoData::TSG),  _meteo.at(8),  dec);
	IOUtils::convertString(md.param(MeteoData::TSS),  _meteo.at(9),  dec);
	IOUtils::convertString(md.param(MeteoData::HS),   _meteo.at(10), dec);
	IOUtils::convertString(md.param(MeteoData::RSWR), _meteo.at(11), dec);

	if (_meteo.size() == 14){ //extra drift data
		if (!md.param_exists("VW_DRIFT")) md.addParameter("VW_DRIFT");
		if (!md.param_exists("DW_DRIFT")) md.addParameter("DW_DRIFT");
		IOUtils::convertString(md.param("VW_DRIFT"), _meteo.at(13), dec);
		IOUtils::convertString(md.param("DW_DRIFT"), _meteo.at(12), dec);
	}
}

/**
 * @brief This function gets back data from table station2 and fills vector with station data
 * @param stat_abk :      a string key of table station2
 * @param stao_nr :       a string key of table station2
 * @param vecStationData: string vector in which data will be filled
 */
void ImisIO::getStationData(const std::string& stat_abk, const std::string& stao_nr,
                            std::vector<std::string>& vecStationData, oracle::occi::Connection*& conn)
{
	vecStationData.clear();

	try {
		Statement *stmt = conn->createStatement(sqlQueryStationData);
		ResultSet *rs = NULL;

		stmt->setString(1, stat_abk); // set 1st variable's value
		stmt->setString(2, stao_nr);  // set 2nd variable's value
		rs = stmt->executeQuery();    // execute the statement stmt

		while (rs->next() == true) {
			for (unsigned int ii=0; ii<4; ii++) {
				vecStationData.push_back(rs->getString(ii+1));
			}
		}

		stmt->closeResultSet(rs);
		conn->terminateStatement(stmt);
	} catch (exception& e){
		throw IOException(string(e.what()), AT); //Translation of OCCI exception to IOException
	}
}

/**
 * @brief This is a private function. It gets back data from ams.v_imis which is a table of the database
 * and fill them in a vector of vector of string. Each record returned is a string vector.
 * @param stat_abk :     a string key of ams.v_imis
 * @param stao_nr :      a string key of ams.v_imis
 * @param datestart :    a vector of five(5) integer corresponding to the recording date
 * @param dateend :      a vector of five(5) integer corresponding to the recording date
 * @param vecMeteoData : a vector of vector of string in which data will be filled
 */
void ImisIO::getImisData (const std::string& stat_abk, const std::string& stao_nr,
                          const std::vector<int>& datestart, const std::vector<int>& dateend,
                          std::vector< std::vector<std::string> >& vecMeteoData,
                          oracle::occi::Environment*& env, oracle::occi::Connection*& conn)
{
	unsigned int nrOfColumns = 12;

	vecMeteoData.clear();

	//First check whether this is a station with separate wind drift data
	bool separateDriftStation = false;
	map<string,string>::const_iterator it = mapDriftStation.find(stat_abk+stao_nr);
	if (it != mapDriftStation.end())
		separateDriftStation = true;

	try {
		Statement *stmt = NULL;
		ResultSet *rs = NULL;
		if (!separateDriftStation){
			stmt = conn->createStatement(sqlQueryMeteoData);
		} else {
			stmt = conn->createStatement(sqlQueryMeteoDataDrift);
		}

		// construct the oracle specific Date object: year, month, day, hour, minutes
		occi::Date begindate(env, datestart[0], datestart[1], datestart[2], datestart[3], datestart[4]);
		occi::Date enddate(env, dateend[0], dateend[1], dateend[2], dateend[3], dateend[4]);
		stmt->setString(1, stat_abk); // set 1st variable's value (station name)
		stmt->setString(2, stao_nr);  // set 2nd variable's value (station number)
		stmt->setDate(3, begindate);  // set 3rd variable's value (begin date)
		stmt->setDate(4, enddate);    // set 4th variable's value (enddate)

		if (separateDriftStation){
			string drift_stat_abk="", drift_stao_nr=""; 
			parseStationName(it->second, drift_stat_abk, drift_stao_nr);
			nrOfColumns += 2; //two extra columns for drift wind and direction
			stmt->setString(5, drift_stat_abk); 
			stmt->setString(6, drift_stao_nr);  
		}

		rs = stmt->executeQuery(); // execute the statement stmt

		rs->setMaxColumnSize(7,22);
		vector<string> vecTmpMeteoData;
		while (rs->next() == true) {
			vecTmpMeteoData.clear();
			for (unsigned int ii=1; ii<=nrOfColumns; ii++) { // 12 or 14 columns
				vecTmpMeteoData.push_back(rs->getString(ii));
			}
			vecMeteoData.push_back(vecTmpMeteoData);
		}

		stmt->closeResultSet(rs);
		conn->terminateStatement(stmt);
	} catch (exception& e){
		throw IOException(string(e.what()), AT); //Translation of OCCI exception to IOException
	}
}

void ImisIO::convertUnits(MeteoData& meteo)
{
	meteo.standardizeNodata(plugin_nodata);

	//converts degC to Kelvin, converts ilwr to ea, converts RH to [0,1]
	if(meteo.ta!=IOUtils::nodata) {
		meteo.ta=C_TO_K(meteo.ta);
	}

	if(meteo.tsg!=IOUtils::nodata) {
		meteo.tsg=C_TO_K(meteo.tsg);
	}

	if(meteo.tss!=IOUtils::nodata) {
		meteo.tss=C_TO_K(meteo.tss);
	}

	if(meteo.rh!=IOUtils::nodata) {
		meteo.rh /= 100.;
	}

	if(meteo.hs!=IOUtils::nodata)
		meteo.hs /= 100.0;

}

void ImisIO::cleanup() throw()
{
}

#ifndef _METEOIO_JNI
extern "C"
{
	//using namespace MeteoIO;
	void deleteObject(void* obj) {
		delete reinterpret_cast<PluginObject*>(obj);
	}

	void* loadObject(const string& classname, const Config& cfg) {
		if(classname == "ImisIO") {
			//cerr << "Creating dynamic handle for " << classname << endl;
			return new ImisIO(deleteObject, cfg);
		}
		//cerr << "Could not load " << classname << endl;
		return NULL;
	}
}
#endif

} //namespace
