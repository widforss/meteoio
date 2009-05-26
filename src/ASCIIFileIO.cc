#include "ASCIIFileIO.h"

using namespace std;

//ASCIIFileIO::ASCIIFileIO(void (*delObj)(void*), const string& filename) : IOHandler(delObj), cfg(filename){}

//Main constructor
ASCIIFileIO::ASCIIFileIO(const std::string& configfile) : IOHandler(NULL), cfg(configfile) {
	//Nothing else so far
  
}

//Copy constructor
ASCIIFileIO::ASCIIFileIO(const ASCIIFileIO& aio) : IOHandler(NULL), cfg(aio.cfg) {
	//Nothing else so far
}

ASCIIFileIO::ASCIIFileIO(const ConfigReader& cfgreader) : IOHandler(NULL), cfg(cfgreader) {
	//Nothing else so far
}

ASCIIFileIO::~ASCIIFileIO() throw(){
	cleanup();
}

//Clone function
//ASCIIFileIO* ASCIIFileIO::clone() const { return new ASCIIFileIO(*this); }

void ASCIIFileIO::cleanup() throw(){
	if (fin.is_open()) //close fin if open
		fin.close();

	if (fout.is_open()) //close fout if open
		fout.close();
}

void ASCIIFileIO::get2DGridSize(int& nx, int& ny){
	string filename="";
	map<string, string> header; // A map to save key value pairs of the file header

	cfg.getValue("DEMFILE", filename); // cout << tmp << endl;

	if (!slfutils::validFileName(filename)) THROW InvalidFileNameException(filename, AT);
	if (!slfutils::fileExists(filename)) THROW FileNotFoundException(filename, AT);
  
	fin.clear();
	fin.open (filename.c_str(), ifstream::in);
	if (fin.fail()) THROW FileAccessException(filename, AT);    
   
	//Go through file, save key value pairs
	try {
		slfutils::readKeyValueHeader(header, fin, 2, " ");
		slfutils::getValueForKey(header, "ncols", nx);
		slfutils::getValueForKey(header, "nrows", ny);
	} catch(std::exception& e){
		cout << "[e] " << AT << ": "<< endl;
		nx=0;
		ny=0;
		cleanup();
		throw;
	}
	cleanup();
}

void ASCIIFileIO::read2DGrid(Grid2DObject& grid_out, const string& filename){
  
	unsigned int ncols, nrows;
	double xllcorner, yllcorner, cellsize, nodata;
	vector<string> tmpvec;
	string line="";
	map<string, string> header; // A map to save key value pairs of the file header

	if (!slfutils::validFileName(filename)) THROW InvalidFileNameException(filename, AT);
	if (!slfutils::fileExists(filename)) THROW FileNotFoundException(filename, AT);
  
	fin.clear();
	fin.open (filename.c_str(), ifstream::in);
	if (fin.fail()) THROW FileAccessException(filename, AT);
  
	char eoln = slfutils::getEoln(fin); //get the end of line character for the file
   
	//Go through file, save key value pairs
	try {
		slfutils::readKeyValueHeader(header, fin, 6, " ");
		slfutils::getValueForKey(header, "ncols", ncols);
		slfutils::getValueForKey(header, "nrows", nrows);
		slfutils::getValueForKey(header, "xllcorner", xllcorner);
		slfutils::getValueForKey(header, "yllcorner", yllcorner);
		slfutils::getValueForKey(header, "cellsize", cellsize);
		slfutils::getValueForKey(header, "NODATA_value", nodata);

		//cout << ncols << " " << nrows << " " << xllcorner<< " " << yllcorner<< " " << cellsize<< " " << nodata <<endl;
    
		//Initialize the 2D grid
		grid_out.set(ncols, nrows, xllcorner, yllcorner, cellsize, nodata);

		if ((ncols==0) || (nrows==0))
			THROW SLFException("Number of rows or columns in 2D Grid given is zero, in file: " + filename, AT);

		//Read one line after the other and parse values into Grid2DObject
		for (unsigned int kk=nrows-1; (kk < nrows); kk--){
			getline(fin, line, eoln); //read complete line
			//cout << "k:" << kk << "\n" << line << endl;

			if (slfutils::readLineToVec(line, tmpvec) != ncols)
				THROW InvalidFormatException("Premature End " + filename, AT);

			for (unsigned int ll=0; ll < ncols; ll++){
				if (!slfutils::convertString(grid_out.grid2D(ll, kk), tmpvec.at(ll), std::dec))
					THROW ConversionFailedException("For Grid2D value in line: " + line + " in file " + filename, AT);
			}
		}
	} catch(std::exception& e){
		cleanup();
		throw;
	}
	cleanup();
}

void ASCIIFileIO::readDEM(Grid2DObject& dem_out){
	string filename="";

	cfg.getValue("DEMFILE", filename); // cout << tmp << endl;

	read2DGrid(dem_out, filename);
}

void ASCIIFileIO::readLanduse(Grid2DObject& landuse_out){
	string filename="";

	cfg.getValue("LANDUSEFILE", filename); // cout << tmp << endl;

	read2DGrid(landuse_out, filename);
}

void ASCIIFileIO::readMeteoData(const Date& date_in, vector<MeteoData>& meteo_out){
	vector<StationData> vecStation;
	readMeteoData(date_in, meteo_out, vecStation);
}

void ASCIIFileIO::readMeteoData(const Date& date_in, vector<MeteoData>& vecMeteo, vector<StationData>& vecStation){
	//See whether data is already buffered
	//Filter
	//Resample if necessary
	//Filter resampled value
	//return that value
	/*
	  1. FilterFacade filterFacade("filters.ini");
	  2. filterFacade.getMinimalWindow(minNbPoints, minDeltaTime);
	  3. fiterFacade.doCheck(unfilteredMeteoBuffer, filteredMeteoBuffer,
	*/

	//FilterFacade filterFacade("filters.ini");

	unsigned int index = MeteoBuffer::npos;
	if (unfilteredMeteoBuffer.size()>0) //check whether meteo data for the date exists in buffer
		index = unfilteredMeteoBuffer[0].seek(date_in);

	//cerr << "Buffered MeteoBuffers: " << unfilteredMeteoBuffer.size() << "  Found data at index: " << index << endl;

	if (( index != MeteoBuffer::npos ) && (unfilteredMeteoBuffer[0].getMeteoData(index).date == date_in)){//in the buffer, return from buffer
		cerr << "[i] Buffered data found for date: " << date_in.toString() << endl;

	} else if (index == MeteoBuffer::npos) { //not in buffer, rebuffering needed
		cerr << "[i] Data for date " << date_in.toString() << " not found in buffer, rebuffering" << endl;
		unfilteredMeteoBuffer.clear();
		filteredMeteoBuffer.clear();

		MeteoData md;
		read1DMeteo(date_in, md);
		index = unfilteredMeteoBuffer[0].seek(date_in);
		//cerr << "read 1D meteo" << endl;
    
		try {
			read2DMeteo(date_in, vecMeteo, vecStation);
		} catch(exception& e){
			cerr << "[i] No meteo2d data found or error while reading it, using only Meteo1D data: " << e.what() << endl;
      
			if (unfilteredMeteoBuffer.size()>1)
				unfilteredMeteoBuffer.erase(unfilteredMeteoBuffer.begin()+1, unfilteredMeteoBuffer.end());
		}
	}

	// RESAMPLING
	if (( index != MeteoBuffer::npos ) && (unfilteredMeteoBuffer[0].getMeteoData(index).date != date_in)){//in the buffer, resampling needed 
		cerr << "[i] Resampling required for date: " << date_in.toString() << endl;
    
		Meteo1DResampler mresampler;
		for (unsigned int ii=0; ii<unfilteredMeteoBuffer.size(); ii++){
			mresampler.resample(index, date_in, unfilteredMeteoBuffer[ii]);
		}
	}

	//FILTERING

	//fill the vectors with data, previously gathered into the MeteoBuffer
	vecMeteo.clear();
	vecStation.clear();
	for (unsigned int ii=0; ii<unfilteredMeteoBuffer.size(); ii++){
		vecMeteo.push_back(unfilteredMeteoBuffer[ii].getMeteoData(index));
		vecStation.push_back(unfilteredMeteoBuffer[ii].getStationData(index));
	}
}


void ASCIIFileIO::read1DMeteo(const Date& date_in, MeteoData& meteo_out){
	StationData sd_tmp;
	read1DMeteo(date_in, meteo_out, sd_tmp);
}

void ASCIIFileIO::convertUnits(MeteoData& meteo){
	//converts C to Kelvin, converts RH to [0,1]
	if(meteo.ta!=meteo.nodata) meteo.ta=C_TO_K(meteo.ta);
	if(meteo.ts0!=meteo.nodata) meteo.ts0=C_TO_K(meteo.ts0);
	if(meteo.rh!=meteo.nodata) meteo.rh /= 100.;
}

void ASCIIFileIO::read1DMeteo(const Date& date_in, MeteoData& meteo_out, StationData& station_out){
	double latitude=StationData::nodata, longitude=StationData::nodata, 
		xcoord=StationData::nodata, ycoord=StationData::nodata, altitude=StationData::nodata;
	string tmp="", line="";
	Date tmp_date;
	vector<string> tmpvec;
	map<string, string> header; // A map to save key value pairs of the file header
	int parsedLines = 0;
	MeteoData tmpdata;

	unfilteredMeteoBuffer.push_back(MeteoBuffer(600));
	filteredMeteoBuffer.push_back(MeteoBuffer(600));

	cfg.getValue("METEOPATH", tmp); 
	tmp += "/meteo1d.txt";

	if (!slfutils::fileExists(tmp)) THROW FileNotFoundException(tmp, AT);

	fin.clear();
	fin.open (tmp.c_str(), ifstream::in);
	if (fin.fail()) THROW FileAccessException(tmp,AT);

	char eoln = slfutils::getEoln(fin); //get the end of line character for the file
   
	//Go through file, save key value pairs
	try {
		slfutils::readKeyValueHeader(header, fin, 5, "="); //Read in 5 lines as header
		slfutils::getValueForKey(header, "Latitude", latitude);
		slfutils::getValueForKey(header, "Longitude", longitude);
		slfutils::getValueForKey(header, "X_Coord", xcoord);
		slfutils::getValueForKey(header, "Y_Coord", ycoord);
		slfutils::getValueForKey(header, "Altitude", altitude);

		station_out.setStationData(xcoord, ycoord, altitude, "", latitude, longitude);

		//Read one line, construct Date object and see whether date is greater or equal than the date_in object
		slfutils::skipLines(fin, 1, eoln); //skip rest of line

		do {
			getline(fin, line, eoln); //read complete line
			parsedLines++;
			//cout << line << endl;

			readMeteoDataLine(line, date_in, tmpdata, tmp);
			tmpdata.cleanData();
			convertUnits(tmpdata);

			unfilteredMeteoBuffer[0].put(tmpdata, station_out);

			//} while(tmp_date<date_in);

		} while(tmpdata.date < date_in);

		meteo_out = tmpdata; //copy by value
		//meteo_out.cleanData();
		//convertUnits(meteo_out);

		//Lese weiter in den Buffer (bis zu 400 Werte)
		try {
			for (int ii=0; ii<400; ii++){
				getline(fin, line, eoln); //read complete line
				parsedLines++;
				//cout << line << endl;
	
				readMeteoDataLine(line, date_in, tmpdata, tmp);
				tmpdata.cleanData();
				convertUnits(tmpdata);
	
				unfilteredMeteoBuffer[0].put(tmpdata, station_out);
			}
		} catch(...){;} //just continue
    
		//cout << tmp_date.toString() << endl;
	} catch(...){
		cout << "[e] " << AT << ": "<< endl;
		cleanup();
		throw;
	}

	cleanup();
}

void ASCIIFileIO::readMeteoDataLine(std::string& line, const Date& date_in, MeteoData& tmpdata, string filename){
	Date tmp_date;
	int tmp_ymdh[4];
	vector<string> tmpvec;
	double tmp_values[6];

	if (slfutils::readLineToVec(line, tmpvec) != 10){
		THROW InvalidFormatException("Premature End of Line or no data for date " + date_in.toString() + " found in File " + filename, AT);
	}
      
	for (int ii=0; ii<4; ii++){
		if (!slfutils::convertString(tmp_ymdh[ii], tmpvec.at(ii), std::dec))
			THROW InvalidFormatException(filename + ": " + line, AT);
	}

	tmp_date.setDate(tmp_ymdh[0],tmp_ymdh[1],tmp_ymdh[2],tmp_ymdh[3]);

	//Read rest of line with values ta, iswr, vw, rh, ea, nswc
  
	for (int ii=0; ii<6; ii++){ //go through the columns
		if (!slfutils::convertString(tmp_values[ii], tmpvec.at(ii+4), std::dec))
			THROW InvalidFormatException(filename + ": " + line, AT);
	}
      
	tmpdata.setMeteoData(tmp_date, tmp_values[0], tmp_values[1], tmp_values[2], tmp_values[3], tmp_values[4], tmp_values[5], MeteoData::nodata);
}


void ASCIIFileIO::read2DMeteo(const Date& date_in, vector<MeteoData>& meteo_out){
	vector<StationData> vecStation;
	read2DMeteo(date_in, meteo_out, vecStation);
}

/*
  Preamble: Files are in METEOFILE directory. 4 types of files: 
  prec????.txt == nswc
  rh????.txt == rh
  ta????.txt == ta
  wspd????.txt == vw
  
  Remarks: The headers of the files may defer - for each unique 
  StationData one MeteoData and one StationData object will be created
*/
void ASCIIFileIO::read2DMeteo(const Date& date_in, vector<MeteoData>& vecMeteo, vector<StationData>& vecStation){
	unsigned int stations=0, bufferindex=0;
	map<string, unsigned int> hashStations = map<string, unsigned int>();
	vector<string> filenames = vector<string>();

	vecMeteo.clear();
	vecStation.clear();
 
	constructMeteo2DFilenames(unfilteredMeteoBuffer[0].getMeteoData(0).date, filenames);
	stations = getNrOfStations(filenames, hashStations);
	cerr << "[i] Number of 2D meteo stations: " << stations << endl;
  
	if (stations < 1)
		THROW InvalidFormatException("No StationData found in 2D Meteo Files", AT); 
	else {
		vecMeteo.clear();
		vecStation.clear();

		MeteoBuffer tmpbuffer(600, unfilteredMeteoBuffer[0].size());

		for (unsigned int ii=0; ii<stations; ii++){
			StationData tmp_sd;
			MeteoData tmp_md;
      
			unfilteredMeteoBuffer.push_back(tmpbuffer);

			vecMeteo.push_back(tmp_md);
			vecStation.push_back(tmp_sd);
		}
	}

	try {
		read2DMeteoHeader(filenames[0], hashStations, vecStation);
		read2DMeteoHeader(filenames[1], hashStations, vecStation);
		read2DMeteoHeader(filenames[2], hashStations, vecStation);
		read2DMeteoHeader(filenames[3], hashStations, vecStation);

		for (unsigned int ii=1; ii<unfilteredMeteoBuffer.size(); ii++){//loop through all MeteoBuffers
			for (unsigned int jj=0; jj<unfilteredMeteoBuffer[ii].size(); jj++){ //loop through all allocated StationData within one MeteoBuffer
				StationData& bufferedStation = unfilteredMeteoBuffer[ii].getStationData(jj);
				bufferedStation = vecStation[ii-1];
			}
		}

		do {

			unsigned int currentindex = bufferindex;
			read2DMeteoData(filenames[0], "nswc", date_in, hashStations, vecMeteo, bufferindex);
			bufferindex = currentindex;
			read2DMeteoData(filenames[1], "rh", date_in, hashStations, vecMeteo, bufferindex);
			bufferindex = currentindex;
			read2DMeteoData(filenames[2], "ta", date_in, hashStations, vecMeteo, bufferindex);
			bufferindex = currentindex;
			read2DMeteoData(filenames[3], "vw", date_in, hashStations, vecMeteo, bufferindex);
			//cerr << "bufferindex: " << bufferindex << "  Expected size()" << unfilteredMeteoBuffer[0].size() << endl;

			if (bufferindex < (unfilteredMeteoBuffer[0].size()-1)){
				//construct new filenames for the continued buffering
				//cerr << unfilteredMeteoBuffer[0].getMeteoData(bufferindex).date.toString() << endl;
				constructMeteo2DFilenames(unfilteredMeteoBuffer[0].getMeteoData(bufferindex).date, filenames);
			}
		} while(bufferindex < (unfilteredMeteoBuffer[0].size()));
	} catch(...){
		vecMeteo.clear();
		vecStation.clear();
    
		//TODO: cleanup of buffer

		cleanup();
		throw;
	}
  
	for (unsigned int ii=0; ii<vecMeteo.size(); ii++) {
		vecMeteo[ii].cleanData();
		convertUnits(vecMeteo[ii]);
	}

	for (unsigned int ii=1; ii<unfilteredMeteoBuffer.size(); ii++){//loop through all MeteoBuffers
		for (unsigned int jj=0; jj<unfilteredMeteoBuffer[ii].size(); jj++){ //loop through all allocated MeteoData within one MeteoBuffer
			MeteoData& bufferedMeteo = unfilteredMeteoBuffer[ii].getMeteoData(jj);
			bufferedMeteo.cleanData();
			convertUnits(bufferedMeteo);
		}
	}
}

void ASCIIFileIO::constructMeteo2DFilenames(const Date& date_in, vector<string>& filenames){
	int year=0, dummy=0;
	string tmp;
	stringstream ss;

	filenames.clear();
 
	date_in.getDate(year, dummy, dummy);
	ss << year;

	cfg.getValue("METEOPATH", tmp); 

	string precFilename = tmp + "/prec" + ss.str() + ".txt";
	string rhFilename = tmp + "/rhum" + ss.str() + ".txt";
	string taFilename = tmp + "/tair" + ss.str() + ".txt";
	string wspdFilename = tmp + "/wspd" + ss.str() + ".txt";

	filenames.push_back(precFilename);
	filenames.push_back(rhFilename);
	filenames.push_back(taFilename);
	filenames.push_back(wspdFilename);

	for (unsigned int ii=0; ii<filenames.size(); ii++){
		if (!slfutils::fileExists(filenames[ii])) THROW FileNotFoundException(filenames[ii], AT);
	}
}


unsigned int ASCIIFileIO::getNrOfStations(vector<string>& filenames, map<string, unsigned int>& hashStations){
	vector<string> tmpvec;
	string line_in="";
  
	for (unsigned int ii=0; ii<filenames.size(); ii++){
		//cout << *it << endl;
		string filename = filenames[ii];

		fin.clear();
		fin.open (filename.c_str(), ifstream::in);
		if (fin.fail()) THROW FileAccessException(filename, AT);
  
		char eoln = slfutils::getEoln(fin); //get the end of line character for the file

		slfutils::skipLines(fin, 4, eoln); 
		getline(fin, line_in, eoln); //5th line holds the names of the stations
		unsigned int cols = slfutils::readLineToVec(line_in, tmpvec);
		if ( cols > 4){ // if there are any stations
			//check each station name and whether it's already hashed, otherwise: hash!
			for (unsigned int ii=4; ii<cols; ii++){
				unsigned int tmp_int = hashStations.count(tmpvec.at(ii));
				//cout << tmp_int << endl;
				if (tmp_int == 0) {
					hashStations[tmpvec.at(ii)] = hashStations.size();
					//cout << "adding hash for station " + tmpvec.at(ii) + ": " << hashStations[tmpvec.at(ii)] << endl;
				}
			} 
		}
		cleanup();      
	}  
  
	return (hashStations.size());
}

void ASCIIFileIO::read2DMeteoData(const string& filename, const string& parameter, const Date& date_in, 
						    map<string,unsigned int>& hashStations, vector<MeteoData>& vecM, unsigned int& bufferindex)
{
	string line_in = "";
	unsigned int columns;
	vector<string> tmpvec, vec_names;
	Date tmp_date;
	int tmp_ymdh[4];
	//unsigned int bufferindex=0;
	bool old = true;

	fin.clear();
	fin.open (filename.c_str(), ifstream::in);
	if (fin.fail()) THROW FileAccessException(filename, AT);

	char eoln = slfutils::getEoln(fin); //get the end of line character for the file
    
	slfutils::skipLines(fin, 4, eoln); //skip first 4 lines
	getline(fin, line_in, eoln); //line containing UNIQUE station names
	columns = slfutils::readLineToVec(line_in, vec_names);
	if (columns < 4)
		THROW InvalidFormatException("Premature end of line in file " + filename, AT);

	MeteoData& lastMeteoData = unfilteredMeteoBuffer[0].getMeteoData(unfilteredMeteoBuffer[0].size()-1); //last time stamp in buffer of 1D meteo
	do {
		getline(fin, line_in, eoln); 
		string tmpline = line_in;
		slfutils::trim(tmpline);

		if (tmpline=="")
			break;

		if (slfutils::readLineToVec(line_in, tmpvec)!=columns)
			THROW InvalidFormatException("Premature End of Line or no data for date " + date_in.toString() 
								    + " found in File " + filename, AT);
    
		for (int ii=0; ii<4; ii++){
			if (!slfutils::convertString(tmp_ymdh[ii], tmpvec[ii], std::dec)) 
				THROW InvalidFormatException("Check date columns in " + filename, AT);
		}
		tmp_date.setDate(tmp_ymdh[0],tmp_ymdh[1],tmp_ymdh[2],tmp_ymdh[3]);

		MeteoData& currentMeteoData = unfilteredMeteoBuffer[0].getMeteoData(bufferindex); //1D Element to synchronize date
		if (tmp_date == currentMeteoData.date){
			//cout << "parsing data: Last Date: " << currentMeteoData.date.toString() << "   Meteobufferindex: " << bufferindex<< endl;    
      
			//Read in data
			for (unsigned int ii=4; ii<columns; ii++){
				unsigned int stationnr = hashStations[vec_names.at(ii)]; 
				MeteoData& tmpmd = unfilteredMeteoBuffer[stationnr].getMeteoData(bufferindex);
				tmpmd.date = tmp_date;

				if (parameter == "nswc") {
					if (!slfutils::convertString(tmpmd.nswc, tmpvec[ii], std::dec)) 
						THROW ConversionFailedException("For nswc value in " + filename + "  for date " + tmpmd.date.toString(), AT);
	  
				} else if (parameter == "rh") {
					if (!slfutils::convertString(tmpmd.rh, tmpvec[ii], std::dec)) 
						THROW ConversionFailedException("For rh value in " + filename + "  for date " + tmpmd.date.toString(), AT);
	  
				} else if (parameter == "ta") {
					if (!slfutils::convertString(tmpmd.ta, tmpvec[ii], std::dec)) 
						THROW ConversionFailedException("For ta value in " + filename + "  for date " + tmpmd.date.toString(), AT);
    
				} else if (parameter == "vw") {
					if (!slfutils::convertString(tmpmd.vw, tmpvec[ii], std::dec)) 
						THROW ConversionFailedException("For vw value in " + filename + "  for date " + tmpmd.date.toString(), AT);
				} 
			}

			bufferindex++;
		}
		//} while(tmp_date<date_in);
		if ((tmp_date >= date_in) && (old==true)){
			old=false;
			//cout << tmp_date.toString() << endl;
			//Write Data into MeteoData objects; which data is in the file determined by switch 'parameter'
			for (unsigned int ii=4; ii<columns; ii++){
				unsigned int stationnr = hashStations[vec_names.at(ii)]; 
				vecM.at(stationnr-1).date = tmp_date;

				if (parameter == "nswc") {
					if (!slfutils::convertString(vecM.at(stationnr-1).nswc, tmpvec[ii], std::dec)) 
						THROW ConversionFailedException("For nswc value in " + filename + "  for date " + tmp_date.toString(), AT);
    
				} else if (parameter == "rh") {
					if (!slfutils::convertString(vecM.at(stationnr-1).rh, tmpvec[ii], std::dec)) 
						THROW ConversionFailedException("For rh value in " + filename + "  for date " + tmp_date.toString(), AT);
	  
				} else if (parameter == "ta") {
					if (!slfutils::convertString(vecM.at(stationnr-1).ta, tmpvec[ii], std::dec)) 
						THROW ConversionFailedException("For ta value in " + filename + "  for date " + tmp_date.toString(), AT);
	  
				} else if (parameter == "vw") {
					if (!slfutils::convertString(vecM.at(stationnr-1).vw, tmpvec[ii], std::dec)) 
						THROW ConversionFailedException("For vw value in " + filename + "  for date " + tmp_date.toString(), AT);
				} 
			}
		}

	} while((tmp_date<lastMeteoData.date) && (!fin.eof()));

	//cout << "Found 2D meteo data for " << parameter << " for date: " << tmp_date.toString() << endl;
	//for (int ii=0; ii<columns; ii++){
	//  cout << tmpvec[ii] << " ";
	//}
	//cout << endl;
  
	cleanup();
}

void ASCIIFileIO::read2DMeteoHeader(const string& filename, map<string,unsigned int>& hashStations, vector<StationData>& vecS){
	string line_in = "";
	unsigned int columns = 0;
	vector<string> vec_altitude, vec_xcoord, vec_ycoord, vec_names;

	fin.clear();
	fin.open (filename.c_str(), ifstream::in);
	if (fin.fail()) THROW FileAccessException(filename, AT);
  
	char eoln = slfutils::getEoln(fin); //get the end of line character for the file

	slfutils::skipLines(fin, 1, eoln);

	//Read all relevant lines in
	getline(fin, line_in, eoln); //Altitude
	columns = slfutils::readLineToVec(line_in, vec_altitude);

	getline(fin, line_in, eoln); //xcoord
	if (slfutils::readLineToVec(line_in, vec_xcoord) != columns)
		THROW InvalidFormatException("Column count doesn't match from line to line in " + filename, AT);

	getline(fin, line_in, eoln); //ycoord
	if (slfutils::readLineToVec(line_in, vec_ycoord) != columns)
		THROW InvalidFormatException("Column count doesn't match from line to line in " + filename, AT);

	getline(fin, line_in, eoln); //names
	if (slfutils::readLineToVec(line_in, vec_names) != columns)
		THROW InvalidFormatException("Column count doesn't match from line to line in " + filename, AT);

	for (unsigned int ii=4; ii<columns; ii++){
		unsigned int stationnr = hashStations[vec_names.at(ii)]; 
		if ((!slfutils::convertString(vecS.at(stationnr-1).altitude, vec_altitude.at(ii), std::dec))
		    || (!slfutils::convertString(vecS.at(stationnr-1).eastCoordinate, vec_xcoord.at(ii), std::dec))
		    || (!slfutils::convertString(vecS.at(stationnr-1).northCoordinate, vec_ycoord.at(ii), std::dec))
		    || (!slfutils::convertString(vecS.at(stationnr-1).stationName, vec_names.at(ii), std::dec)))
			THROW ConversionFailedException("Conversion of station description failed in " + filename, AT);  

		//Now convert the swiss grid coordinates into latitude and longitude
		slfutils::CH1903_to_WGS84(vecS[stationnr-1].eastCoordinate, vecS[stationnr-1].northCoordinate, 
							 vecS[stationnr-1].latitude, vecS[stationnr-1].longitude);

		//cout << stationnr << endl;
	}

	cleanup();
}

void ASCIIFileIO::readAssimilationData(const Date& date_in, Grid2DObject& da_out){
	int yyyy, mm, dd, hh;
	date_in.getDate(yyyy, mm, dd, hh);
	string filepath="";

	cfg.getValue("DAPATH", filepath); // cout << tmp << endl;
  
	stringstream ss;
	ss.fill('0');
	ss << filepath << "/" << setw(4) << yyyy << setw(2) << mm << setw(2) <<  dd << setw(2) <<  hh << ".sca";

	read2DGrid(da_out, ss.str());
}

void ASCIIFileIO::readSpecialPoints(CSpecialPTSArray& pts){
	string filename="", line_in="";
	vector<string> tmpvec;
	vector< pair<int,int> > mypts;

	cfg.getValue("SPECIALPTSFILE", filename); // cout << tmp << endl;
	if (!slfutils::fileExists(filename)) THROW FileNotFoundException(filename, AT);

	fin.clear();
	fin.open (filename.c_str(), ifstream::in);
	if (fin.fail()) THROW FileAccessException(filename,AT);

	char eoln = slfutils::getEoln(fin); //get the end of line character for the file

	while (!fin.eof()){
		getline(fin, line_in, eoln); 

		if (slfutils::readLineToVec(line_in, tmpvec)==2){ //Try to convert
			int x, y;
			if (!slfutils::convertString(x, tmpvec.at(0), std::dec))
				THROW ConversionFailedException("Conversion of a value failed in " + filename + " line: " + line_in, AT);        

			if (!slfutils::convertString(y, tmpvec.at(1), std::dec))
				THROW ConversionFailedException("Conversion of a value failed in " + filename + " line: " + line_in, AT);        

			pair<int,int> tmppair(x,y);
			mypts.push_back(tmppair);
		}
	}
	cleanup();

	//Now put everything into that legacy struct CSpecialPTSArray
	pts.SetSize(mypts.size());

	for (unsigned int jj=0; jj<mypts.size(); jj++){
		pts[jj].ix = mypts.at(jj).first;
		pts[jj].iy = mypts.at(jj).second;
		//cout << mypts.at(jj).first << "/" << mypts.at(jj).second << endl;
	}
}

void ASCIIFileIO::write2DGrid(const Grid2DObject& grid_in, const string& filename){
  
	if (slfutils::fileExists(filename)) THROW SLFException("File " + filename + " already exists!", AT);

	fout.open(filename.c_str());
	if (fout.fail()) THROW FileAccessException(filename, AT);


	try {
		fout << "ncols \t\t" << grid_in.ncols << endl;
		fout << "nrows \t\t" << grid_in.nrows << endl;
		fout << "xllcorner \t" << grid_in.xllcorner << endl;
		fout << "yllcorner \t" << grid_in.yllcorner << endl;    
		fout << "cellsize \t" << grid_in.cellsize << endl;
		fout << "NODATA_value \t" << (int)(grid_in.nodata) << endl;

		for (unsigned int kk=grid_in.nrows-1; kk < grid_in.nrows; kk--){
			for (unsigned int ll=0; ll < grid_in.ncols; ll++){
				fout << grid_in.grid2D(ll, kk) << "\t";
			}
			fout << endl;
		}
	} catch(...){
		cout << "[e] " << AT << ": "<< endl;
		cleanup();
		throw;
	}

	cleanup();
}


/*extern "C"
  {
  void deleteObject(void* obj) {
  delete reinterpret_cast<PluginObject*>(obj);
  }
  
  void* loadObject(const string& classname, const string& filename) {
  if(classname == "ASCIIFileIO") {
  cerr << "Creating handle to " << classname << endl;
  //return new ASCIIFileIO(deleteObject);
  return new ASCIIFileIO(deleteObject, filename);
  }
  cerr << "Could not load " << classname << endl;
  return NULL;
  }
  }

*/
