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
#include "WSMDFIO.h"

using namespace std;

namespace mio {
/**
 * @page wsmdf WSMDF
 * @section template_format Format
 * *Put here the informations about the standard format that is implemented*
 *
 * @section template_units Units
 *
 *
 * @section template_keywords Keywords
 * This plugin uses the following keywords:
 * - COORDSYS: coordinate system (see Coords); [Input] and [Output] section
 * - COORDPARAM: extra coordinates parameters (see Coords); [Input] and [Output] section
 * - etc
 */

const std::string WSMDFIO::wsmdf_version = "0.9";
map<string, MeteoData::Parameters> WSMDFIO::mapParameterByName;
const bool WSMDFIO::__init = WSMDFIO::initStaticData();

bool WSMDFIO::initStaticData()
{
	for (unsigned int ii=0; ii<MeteoData::nrOfParameters; ii++){
		mapParameterByName[MeteoData::getParameterName(ii)] = MeteoData::Parameters(ii);
	}

	return true;
}

double& WSMDFIO::getParameter(const std::string& columnName, MeteoData& md)
{
	MeteoData::Parameters paramindex = mapParameterByName[columnName];
	return md.param(paramindex);
}

void WSMDFIO::checkColumnNames(const std::vector<std::string>& vecColumns, const bool& locationInHeader)
{
	/*
	 * This function checks whether the sequence of keywords specified in the 
	 * [HEADER] section (key 'fields') is valid
	 */
	vector<unsigned int> paramcounter = vector<unsigned int>(MeteoData::nrOfParameters, 0);

	for (unsigned int ii=0; ii<vecColumns.size(); ii++){
		if ((vecColumns[ii] == "timestamp") || (vecColumns[ii] == "longitude")
		    || (vecColumns[ii] == "latitude") || (vecColumns[ii] == "altitude")){
			//everything ok
		} else {
			map<string, MeteoData::Parameters>::iterator it = mapParameterByName.find(vecColumns[ii]);
			if (it == mapParameterByName.end())
				throw InvalidFormatException("Key 'fields' specified in [HEADER] section contains invalid names", AT);
			
			paramcounter.at(mapParameterByName[vecColumns[ii]])++;
		}
	}
	
	//Check for multiple usages of parameters
	for (unsigned int ii=0; ii<paramcounter.size(); ii++){
		if (paramcounter[ii] > 1)
			throw InvalidFormatException("In 'fields': Multiple use of " + MeteoData::getParameterName(ii), AT);
	}
	
	//If there is no location information in the [HEADER] section, then
	//location information must be part of fields
	if (!locationInHeader){
		unsigned int latcounter = 0, loncounter=0, altcounter=0;
		for (unsigned int ii=0; ii<vecColumns.size(); ii++){
			if (vecColumns[ii] == "longitude")
				loncounter++;
			else if (vecColumns[ii] == "latitude")
				latcounter++;
			else if (vecColumns[ii] == "altitude")
				altcounter++;
		}

		if ((latcounter != loncounter) && (loncounter != altcounter) && (altcounter != 1))
			throw InvalidFormatException("Key 'fields' must contain 'latitude', 'longitude' and 'altitude'", AT);
	}
}

//END STATIC SECTION


WSMDFIO::WSMDFIO(void (*delObj)(void*), const std::string& filename) : IOInterface(delObj), cfg(filename)
{
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
	parseInputOutputSection();
}

WSMDFIO::WSMDFIO(const std::string& configfile) : IOInterface(NULL), cfg(configfile)
{
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
	parseInputOutputSection();
}

WSMDFIO::WSMDFIO(const ConfigReader& cfgreader) : IOInterface(NULL), cfg(cfgreader)
{
	IOUtils::getProjectionParameters(cfg, coordin, coordinparam, coordout, coordoutparam);
	parseInputOutputSection();
}

WSMDFIO::~WSMDFIO() throw()
{
	cleanup();
}

void WSMDFIO::cleanup() throw()
{
	if (fin.is_open()) {//close fin if open
		fin.close();
	}
	if (fout.is_open()) {//close fout if open
		fout.close();
	}		
}

void WSMDFIO::read2DGrid(Grid2DObject& /*grid_out*/, const std::string& /*_name*/)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void WSMDFIO::readDEM(DEMObject& /*dem_out*/)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void WSMDFIO::readLanduse(Grid2DObject& /*landuse_out*/)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void WSMDFIO::readAssimilationData(const Date& /*date_in*/, Grid2DObject& /*da_out*/)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void WSMDFIO::readStationData(const Date&, std::vector<StationData>& /*vecStation*/)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void WSMDFIO::parseInputOutputSection()
{
	/*
	 * Parse the [Input] and [Output] sections within ConfigReader object cfg
	 */

	//Parse input section: extract number of files to read and store filenames in vecFiles
	unsigned int counter = 1;
	string filename = "";
	do {
		stringstream ss;
		filename = "";
		
		ss << "METEOFILE" << counter;
		cfg.getValue(ss.str(), "Input", filename, ConfigReader::nothrow);

		if (filename != ""){
			if (!IOUtils::validFileName(filename)) //Check whether filename is valid
				throw InvalidFileNameException(filename, AT);

			vecFiles.push_back(filename);
		}
		counter++;
	} while (filename != "");

	//Parse output section: extract info on whether to write ASCII or BINARY format, gzipped or not
	outpath = "";
	outputIsAscii = true; 
	outputIsGzipped = false;

	vector<string> vecArgs;
	cfg.getValue("METEOPATH", "Output", outpath, ConfigReader::nothrow);
	cfg.getValue("METEOPARAM", "Output", vecArgs, ConfigReader::nothrow); //"ASCII|BINARY GZIP"

	if (outpath == "")
		return;

	if (vecArgs.size() == 0)
		vecArgs.push_back("ASCII");

	if (vecArgs.size() > 2)
		throw InvalidFormatException("Too many values for key METEOPARAM", AT);

	if (vecArgs[0] == "BINARY")
		outputIsAscii = false;
	else if (vecArgs[0] == "ASCII")
		outputIsAscii = true;
	else 
		throw InvalidFormatException("The first value for key METEOPARAM may only be ASCII or BINARY", AT);

	if (vecArgs.size() == 2){
		if (vecArgs[1] != "GZIP")
			throw InvalidFormatException("The second value for key METEOPARAM may only be GZIP", AT);

		outputIsGzipped = true;
	}
}

void WSMDFIO::readMeteoData(const Date& dateStart, const Date& dateEnd,
                            std::vector< std::vector<MeteoData> >& vecMeteo, 
                            std::vector< std::vector<StationData> >& vecStation,
                            const unsigned int& stationindex)
{
	//Make sure that vecMeteo/vecStation have the correct dimension and stationindex is valid
	unsigned int startindex=0, endindex=vecFiles.size();	
	if (stationindex != IOUtils::npos){
		if ((stationindex < vecFiles.size()) || (stationindex < vecMeteo.size()) || (stationindex < vecStation.size())){
			startindex = stationindex;
			endindex = stationindex+1;
		} else {
			throw IndexOutOfBoundsException("Invalid stationindex", AT);
		} 

		vecMeteo[stationindex].clear();
		vecStation[stationindex].clear();
	} else {
		vecMeteo.clear();
		vecStation.clear();
		
		vecMeteo = vector< vector<MeteoData> >(vecFiles.size());
		vecStation = vector< vector<StationData> >(vecFiles.size());
	}

	//Now loop through all requested stations, open the respective files and parse them
	for (unsigned int ii=startindex; ii<endindex; ii++){
		bool isAscii = true;
		string filename = vecFiles.at(ii); //filename of current station
		
		if (!IOUtils::fileExists(filename))
			throw FileNotFoundException(filename, AT);

		fin.clear();
		fin.open (filename.c_str(), ios::in);
		if (fin.fail())
			throw FileAccessException(filename, AT);

		char eoln = IOUtils::getEoln(fin); //get the end of line character for the file

		//Go through file, save key value pairs
		string line="";
		std::vector<std::string> tmpvec, vecDataSequence;
		double timezone = 0.0;
		bool locationInHeader = false;
		StationData sd;

		try {
			//1. Read signature
			getline(fin, line, eoln); //read complete signature line
			IOUtils::stripComments(line);
			IOUtils::readLineToVec(line, tmpvec);
			checkSignature(tmpvec, filename, isAscii);

			//2. Read Header
			readHeader(eoln, filename, locationInHeader, timezone, sd, vecDataSequence);
			WSMDFIO::checkColumnNames(vecDataSequence, locationInHeader);

			//3. Read DATA
			if (isAscii){
				readDataAscii(eoln, filename, timezone, sd, vecDataSequence, dateStart, dateEnd,
						    vecMeteo[ii], vecStation[ii]);
			} else {
				streampos currpos = fin.tellg();
				fin.close();
				fin.open (filename.c_str(), ios::in|ios::binary);				
				if (fin.fail()) throw FileAccessException(filename, AT);
				fin.seekg(currpos); //jump to binary data section

				readDataBinary(eoln, filename, timezone, sd, vecDataSequence, dateStart, dateEnd,
							vecMeteo[ii], vecStation[ii]);
			}
			cleanup();
		} catch(std::exception& e) {
			cleanup();
			throw;
		}
	}
}

void WSMDFIO::readDataBinary(const char&, const std::string&, const double& timezone, 
					    const StationData& sd, const std::vector<std::string>& vecDataSequence,
					    const Date& dateStart, const Date& dateEnd,
					    std::vector<MeteoData>& vecMeteo, std::vector<StationData>& vecStation)
{
	const unsigned int nrOfColumns = vecDataSequence.size();

	while (!fin.eof()){
		MeteoData md;
		StationData tmpsd = sd;
		double lat=IOUtils::nodata, lon=IOUtils::nodata, alt=IOUtils::nodata;
		unsigned int poscounter = 0;

		for (unsigned int ii=0; ii<nrOfColumns; ii++){
			if (vecDataSequence[ii] == "timestamp"){
				double tmpval;
				fin.read(reinterpret_cast < char * > (&tmpval), sizeof(double));

				md.date.setDate(tmpval);

				if ((timezone != IOUtils::nodata) && (timezone != 0.0))
					md.date.setTimeZone(timezone);

				if (md.date < dateStart)
					continue;
				if (md.date > dateEnd)
					break;
			} else {
				float tmpval;
				fin.read(reinterpret_cast < char * > (&tmpval), sizeof(float));			
				double val = (double)tmpval;

				if (vecDataSequence[ii] == "latitude"){
					lat = val;
					poscounter++;
				} else if (vecDataSequence[ii] == "longitude"){
					lon = val;
					poscounter++;
				} else if (vecDataSequence[ii] == "altitude"){
					alt = val;
					poscounter++;
				} else {
					WSMDFIO::getParameter(vecDataSequence[ii], md) = val;
				}
			}
		}

		char c;
		fin.read(&c, sizeof(char));
		if (c != '\n')
			throw InvalidFormatException("Corrupted data in section [DATA]", AT);

		if (poscounter == 3)
			tmpsd.position.setLatLon(lat, lon, alt);

		//cout << "===" << endl;
		//cout << sd << endl;
		//cout << md.date.toString(Date::ISO) << endl;

		vecMeteo.push_back(md);
		vecStation.push_back(tmpsd);
	}	
}

void WSMDFIO::readDataAscii(const char& eoln, const std::string& filename, const double& timezone, 
					   const StationData& sd, const std::vector<std::string>& vecDataSequence,
					   const Date& dateStart, const Date& dateEnd,
					   std::vector<MeteoData>& vecMeteo, std::vector<StationData>& vecStation)
{
	string line = "";
	vector<string> tmpvec;
	const unsigned int nrOfColumns = vecDataSequence.size();

	while (!fin.eof()){
		getline(fin, line, eoln);
		IOUtils::stripComments(line);
		IOUtils::trim(line);
		if (line == "") continue; //Pure comment lines and empty lines are ignored

		unsigned int ncols = IOUtils::readLineToVec(line, tmpvec);

		if (ncols != nrOfColumns)
			throw InvalidFormatException("In "+ filename + ": Invalid amount of data in data line", AT);

		MeteoData md;
		StationData tmpsd = sd;
		double lat, lon, alt;
		unsigned int poscounter = 0;
		for (unsigned int ii=0; ii<nrOfColumns; ii++){
			if (vecDataSequence[ii] == "timestamp"){
				if (!IOUtils::convertString(md.date, tmpvec[ii]))
					throw InvalidFormatException("In "+filename+": Timestamp invalid in data line", AT);

				if ((timezone != IOUtils::nodata) && (timezone != 0.0))
					md.date.setTimeZone(timezone);

				if (md.date < dateStart)
					continue;
				if (md.date > dateEnd)
					break;

			} else if (vecDataSequence[ii] == "latitude"){
				if (!IOUtils::convertString(lat, tmpvec[ii])) 
					throw InvalidFormatException("In "+filename+": Latitude invalid", AT);
				poscounter++;
			} else if (vecDataSequence[ii] == "longitude"){
				if (!IOUtils::convertString(lon, tmpvec[ii])) 
					throw InvalidFormatException("In "+filename+": Longitude invalid", AT);
				poscounter++;
			} else if (vecDataSequence[ii] == "altitude"){
				if (!IOUtils::convertString(alt, tmpvec[ii])) 
					throw InvalidFormatException("In "+filename+": Altitude invalid", AT);
				poscounter++;
			} else {
				if (!IOUtils::convertString(WSMDFIO::getParameter(vecDataSequence[ii], md), tmpvec[ii]))
					throw InvalidFormatException("In "+filename+": Invalid value for param", AT);
			}
		}

		if (poscounter == 3)
			tmpsd.position.setLatLon(lat, lon, alt);

		vecMeteo.push_back(md);
		vecStation.push_back(tmpsd);
	}
}

void WSMDFIO::readHeader(const char& eoln, const std::string& filename, bool& locationInHeader,
                         double& timezone, StationData& sd, std::vector<std::string>& vecDataSequence)
{
	string line="";
	while (!fin.eof() && (fin.peek() != '['))
		getline(fin, line, eoln);
	
	getline(fin, line, eoln);
	IOUtils::stripComments(line);
	IOUtils::trim(line);
	IOUtils::toUpper(line);

	if (line != "[HEADER]")
		throw InvalidFormatException("Section " + line + " in "+ filename + " invalid", AT);

	std::map<std::string,std::string> mapHeader;
	while (!fin.eof() && (fin.peek() != '[')){
		getline(fin, line, eoln);

		IOUtils::stripComments(line);
		IOUtils::trim(line);

		if (line != "")
			if (!IOUtils::readKeyValuePair(line, "=", mapHeader))
				throw InvalidFormatException("Invalid key value pair in section [Header]", AT);
	}

	//Now extract info from mapHeader
	IOUtils::getValueForKey(mapHeader, "station_id", sd.stationID);
	IOUtils::getValueForKey(mapHeader, "station_name", sd.stationName, IOUtils::nothrow);
	IOUtils::getValueForKey(mapHeader, "tz", timezone, IOUtils::nothrow);

	double lat=IOUtils::nodata, lon=IOUtils::nodata, alt=IOUtils::nodata;	
	IOUtils::getValueForKey(mapHeader, "latitude", lat, IOUtils::nothrow);
	if (lat != IOUtils::nodata){ //HACK
		IOUtils::getValueForKey(mapHeader, "longitude", lon);
		IOUtils::getValueForKey(mapHeader, "altitude", alt);
		sd.position.setLatLon(lat, lon, alt);
		locationInHeader = true;
	} else {
		locationInHeader = false;
	}

	IOUtils::getValueForKey(mapHeader, "fields", vecDataSequence);
	//IOUtils::getValueForKey(mapHeader, "units_offset", vecUnitsOffset, IOUtils::nothrow);
	//IOUtils::getValueForKey(mapHeader, "units_multiplier", vecUnitsMultiplier, IOUtils::nothrow);

	//Read [DATA] section tag
	getline(fin, line, eoln);
	IOUtils::stripComments(line);
	IOUtils::trim(line);
	IOUtils::toUpper(line);

	if (line != "[DATA]")
		throw InvalidFormatException("Section " + line + " in "+ filename + " invalid, expected [DATA]", AT);
}

void WSMDFIO::checkSignature(const std::vector<std::string>& vecSignature, const std::string& filename, bool& isAscii)
{
	if ((vecSignature.size() != 3) || (vecSignature[0] != "WSMDF") || (vecSignature[1] != wsmdf_version))
		throw InvalidFormatException("The signature of file " + filename + " is invalid", AT);

	if (vecSignature[2] == "ASCII")
		isAscii = true;
	else if (vecSignature[2] == "BINARY")
		isAscii = false;
	else 
		throw InvalidFormatException("The 3rd column in the file " + filename + " must be either ASCII or BINARY", AT);
}

void WSMDFIO::writeMeteoData(const std::vector< std::vector<MeteoData> >& vecMeteo,
					    const std::vector< std::vector<StationData> >& vecStation,
					    const std::string&)
{

	//Loop through all stations
	for (unsigned int ii=0; ii<vecMeteo.size(); ii++){
		//1. check consitency of station data position -> write location in header or data section
		StationData sd;
		bool isConsistent = checkConsistency(vecStation.at(ii), sd);
		
		if (sd.stationID == ""){
			stringstream ss;
			ss << "Station" << ii+1;

			sd.stationID = ss.str();
		}

		string filename = outpath + "/" + sd.stationID + ".wsmdf";
		if (!IOUtils::validFileName(filename)) //Check whether filename is valid
			throw InvalidFileNameException(filename, AT);

		try {
			fout.open(filename.c_str());
			if (fout.fail()) throw FileAccessException(filename.c_str(), AT);

			fout << "WSMDF " << wsmdf_version << " ";
			if (outputIsAscii)
				fout << "ASCII" << endl;
			else 
				fout << "BINARY" << endl;

			//2. write header, but first check which meteo parameter fields are actually in use
			vector<bool> vecParamInUse = vector<bool>(MeteoData::nrOfParameters, false); 
			double timezone = IOUtils::nodata;
			checkForUsedParameters(vecMeteo[ii], timezone, vecParamInUse);
			writeHeaderSection(isConsistent, sd, timezone, vecParamInUse);
			
			//3. write data depending on ASCII/BINARY
			if (outputIsAscii) {
				writeDataAscii(isConsistent, vecMeteo[ii], vecStation[ii], vecParamInUse);
			} else {
				fout << "[DATA]" << endl;
				fout.close();
				fout.open(filename.c_str(), ios::out | ios::app | ios::binary); //reopen as binary file
				if (fout.fail()) throw FileAccessException(filename.c_str(), AT);
				writeDataBinary(isConsistent, vecMeteo[ii], vecStation[ii], vecParamInUse);
			}
			
			cleanup();

			//4. gzip file or not
		} catch (exception& e){
			cleanup();
			throw;
		}
	}
}

void WSMDFIO::writeDataBinary(const bool& writeLocationInHeader, const std::vector<MeteoData>& vecMeteo,
                              const std::vector<StationData>& vecStation, const std::vector<bool>& vecParamInUse)
{
	char eoln = '\n';

	for (unsigned int ii=0; ii<vecMeteo.size(); ii++){
		float val = 0;
		double julian = vecMeteo[ii].date.getJulianDate();
		
		fout.write((char*)&julian, sizeof(double));

		if (!writeLocationInHeader){ //Meta data changes
			val = (float)vecStation[ii].position.getLat();
			fout.write((char*)&val, sizeof(float));
			val = (float)vecStation[ii].position.getLon();
			fout.write((char*)&val, sizeof(float));
			val = (float)vecStation[ii].position.getAltitude();
			fout.write((char*)&val, sizeof(float));
		}

		for (unsigned int jj=0; jj<MeteoData::nrOfParameters; jj++){
			if (vecParamInUse[jj]){
				val = (float)vecMeteo[ii].param(jj);
				fout.write((char*)&val, sizeof(float));
			}
		}
		fout.write((char*)&eoln, sizeof(char));
	}
}

void WSMDFIO::writeDataAscii(const bool& writeLocationInHeader, const std::vector<MeteoData>& vecMeteo,
                             const std::vector<StationData>& vecStation, const std::vector<bool>& vecParamInUse)
{
	fout << "[DATA]" << endl;
	fout.width(12);
	fout.precision(4);

	for (unsigned int ii=0; ii<vecMeteo.size(); ii++){
		fout << vecMeteo[ii].date.toString(Date::ISO);

		if (!writeLocationInHeader){ //Meta data changes
			fout << " " << fixed << vecStation[ii].position.getLat();
			fout << " " << fixed << vecStation[ii].position.getLon();
			fout << " " << fixed << vecStation[ii].position.getAltitude();
		}

		for (unsigned int jj=0; jj<MeteoData::nrOfParameters; jj++){
			if (vecParamInUse[jj])
				fout << " " << fixed << vecMeteo[ii].param(jj);
		}
		fout << endl;
	}
}

void WSMDFIO::writeHeaderSection(const bool& writeLocationInHeader, const StationData& sd, 
                                 const double& timezone, const std::vector<bool>& vecParamInUse)
{
	fout << "[HEADER]" << endl;
	fout << "station_id = " << sd.getStationID() << endl;
	if (sd.getStationName() != "")
		fout << "station_name = " << sd.getStationName() << endl;
	
	if (writeLocationInHeader){
		fout << "latitude = " << sd.position.getLat() << endl;
		fout << "longitude = " << sd.position.getLon() << endl;
		fout << "altitude = " << sd.position.getAltitude() << endl;
	}

	fout << "nodata = " << IOUtils::nodata << endl;
	
	if ((timezone != IOUtils::nodata) && (timezone != 0.0))
		fout << "tz = " << timezone << endl;

	fout << "fields = timestamp";

	if (!writeLocationInHeader){
		fout << " latitude longitude altitude";
	}

	for (unsigned int ii=0; ii<MeteoData::nrOfParameters; ii++){
		if (vecParamInUse[ii])
			fout << " " << MeteoData::getParameterName(ii);
	}
	fout << endl;
}


void WSMDFIO::checkForUsedParameters(const std::vector<MeteoData>& vecMeteo, double& timezone, 
                                     std::vector<bool>& vecParamInUse)
{
	for (unsigned int ii=0; ii<vecMeteo.size(); ii++){
		for (unsigned int jj=0; jj<MeteoData::nrOfParameters; jj++){
			if (!vecParamInUse[jj])
				if (vecMeteo[ii].param(jj) != IOUtils::nodata)
					vecParamInUse[jj] = true;
		}
	}	

	if (vecMeteo.size() > 0)
		timezone = vecMeteo[0].date.getTimeZone();
}

bool WSMDFIO::checkConsistency(const std::vector<StationData>& vecStation, StationData& sd)
{
	for (unsigned int ii=1; ii<vecStation.size(); ii++){
		if (vecStation[ii].position != vecStation[ii-1].position)
			return false;
	}

	if (vecStation.size() > 0)
		sd = vecStation[0];

	return true;
}

void WSMDFIO::readSpecialPoints(std::vector<Coords>&)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}

void WSMDFIO::write2DGrid(const Grid2DObject& /*grid_in*/, const std::string& /*name*/)
{
	//Nothing so far
	throw IOException("Nothing implemented here", AT);
}


#ifndef _METEOIO_JNI
extern "C"
{
	void deleteObject(void* obj) {
		delete reinterpret_cast<PluginObject*>(obj);
	}

	void* loadObject(const string& classname, const string& filename) {
		if(classname == "WSMDFIO") {
			//cerr << "Creating dynamic handle for " << classname << endl;
			return new WSMDFIO(deleteObject, filename);
		}
		//cerr << "Could not load " << classname << endl;
		return NULL;
	}
}
#endif

} //namespace
