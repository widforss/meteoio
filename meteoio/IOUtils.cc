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

#include <meteoio/IOUtils.h>
#include <meteoio/Config.h>    // to avoid forward declaration hell
#include <meteoio/MeteoData.h> // to avoid forward declaration hell

namespace mio {

bool IOUtils::checkEpsilonEquality(const double& val1, const double& val2, const double& epsilon)
{
	if (((val1-epsilon) < val2) && ((val1+epsilon) > val2)) {
		return true;
	}

	return false;
}

double IOUtils::pow2(const double& val)
{
	return (val*val);
}

double IOUtils::pow3(const double& val)
{
	return (val*val*val);
}

double IOUtils::pow4(const double& val)
{
	return (val*val*val*val);
}

double IOUtils::bearing_to_angle(const double& bearing) {
	const double to_rad = M_PI/180.;
	return (fmod(360.-bearing+90., 360.)*to_rad);
}

double IOUtils::angle_to_bearing(const double& angle) {
	const double to_deg = 180.0 / M_PI;
	return (fmod( 90.-angle*to_deg+360. , 360. ));
}

void IOUtils::stripComments(std::string& str)
{
	size_t found = str.find_first_of("#;");
	if (found != std::string::npos){
		str.erase(found); //rest of line disregarded
	}
}

std::string IOUtils::cleanPath(const std::string& in_path) {
	std::string out_path(in_path);

	size_t curr = out_path.find('\\', 0);
	while(curr!=std::string::npos){
		out_path.replace(curr, 1, "/");
		curr = out_path.find('\\', curr);
	}
	//out_path.replace(out_path.begin(), out_path.end(), '\\', '/');
	return out_path;
}

void IOUtils::trim(std::string& str)
{
	const std::string whitespaces (" \t\f\v\n\r");
	size_t startpos = str.find_first_not_of(whitespaces); // Find the first character position after excluding leading blank spaces
	size_t endpos = str.find_last_not_of(whitespaces); // Find the first character position from reverse af

	// if all spaces or empty return an empty string
	if(( std::string::npos == startpos ) || ( std::string::npos == endpos)) {
		str = "";
	} else {
		str = str.substr( startpos, endpos-startpos+1 );
	}
}

void IOUtils::toUpper(std::string& str){
	for(unsigned int t=0; t<str.length(); t++) {
		str[t] = toupper(str[t]);
	}
}

bool IOUtils::readKeyValuePair(const std::string& in_line, const std::string& delimiter,
						 std::map<std::string,std::string>& out_map, const std::string& keyprefix, const bool& setToUpperCase)
{
	//size_t pos = in_line.find(delimiter); //first occurence of '='

	size_t pos = std::string::npos;
	if ((delimiter==" ") || (delimiter=="\t")) {
		pos = in_line.find_first_of(" \t", 0);
	} else {
		pos = in_line.find(delimiter); //first occurence of '='
	}


	if(pos != std::string::npos) { //ignore in_lines that are empty or without '='
		std::string key = in_line.substr(0, pos);
		std::string value = in_line.substr(pos + 1);

		IOUtils::trim(key);
		IOUtils::trim(value);
		//cerr << "key:" << key << " val:" << value << endl;

		if ((key == "") || (value=="")) {
			return false;
		}

		if (setToUpperCase)
			IOUtils::toUpper(key);


		out_map[keyprefix + key] = value;
	} else {
		return false;
		//cerr << "line:" << in_line << "delimiter" << endl;
	}

	return true;
}

bool IOUtils::fileExists(const std::string& filename)
{
	struct stat buffer ;

	if ((stat( filename.c_str(), &buffer))==0) {//File exists if stat returns 0
		return true ;
	}

	return false;
}

bool IOUtils::validFileName(const std::string& filename)
{
	size_t startpos = filename.find_first_not_of(" \t\n"); // Find the first character position after excluding leading blank spaces
	if((startpos!=0) || (filename==".") || (filename=="..")) {
		return false;
	}

	return true;
}

void IOUtils::readDirectory(const std::string& path, std::list<std::string>& dirlist, const std::string& pattern)
{
	DIR *dp;
	struct dirent *dirp;

	if((dp  = opendir(path.c_str())) == NULL) {
		throw FileAccessException("Error opening directory " + path, AT);
	}

	while ((dirp = readdir(dp)) != NULL) {
		std::string tmp = std::string(dirp->d_name);
		if( tmp.compare(".")!=0 && tmp.compare("..")!=0 ) { //skip "." and ".."
			if (pattern=="") {
				dirlist.push_back(tmp);
			} else {
				size_t pos = tmp.find(pattern);
				if (pos!=std::string::npos) {
					dirlist.push_back(tmp);
				}
			}
		}
	}
	closedir(dp);
}

void IOUtils::readKeyValueHeader(std::map<std::string,std::string>& headermap,
				  std::istream& fin,
				  const unsigned int& linecount,
				  const std::string& delimiter)
{
	int linenr = 0;
	std::string line="";

	//make a test for end of line encoding:
	char eol = IOUtils::getEoln(fin);

	for (unsigned int ii=0; ii< linecount; ii++){
		if (std::getline(fin, line, eol)) {
			//cout << line <<endl;
			linenr++;

			bool result = IOUtils::readKeyValuePair(line, delimiter, headermap);

			if (!result) { //  means if ((key == "") || (value==""))
				std::stringstream out;
				out << "Invalid key value pair in line: " << linenr << " of header";
				throw IOException(out.str(), AT);
			}
		} else {
			throw InvalidFormatException("Premature EOF while reading Header", AT);
		}
	}
}

char IOUtils::getEoln(std::istream& fin)
{
	std::streambuf* pbuf;
	char tmp = '0';
	int chars = 0;

	int position = fin.tellg();

	do {
		fin.get(tmp);
		chars++;

		if ((tmp == '\r') || (tmp == '\n')) {
			char peekc = tmp;
			//cout << (int)tmp << endl;
			while ((!fin.eof() && ((peekc=='\r') || (peekc=='\n')))) {
				tmp = peekc;
				fin.get(peekc);
				chars++;
			}
			pbuf = fin.rdbuf();
			pbuf->pubseekpos(position); //rewind
			fin.clear(); //reset eof flag, etc
			return tmp;
		}
	} while ((chars < 3000) && (!fin.eof()));

	pbuf = fin.rdbuf();
	pbuf->pubseekpos(position); //rewind
	fin.clear(); //reset eof flag, etc

	return '\n';
}

void IOUtils::skipLines(std::istream& fin, const unsigned int& nbLines, const char& eoln)
{
	std::string dummy;
	for (unsigned int ii=0; ii<nbLines; ii++) {
		if(!getline(fin, dummy, eoln)) {
			throw InvalidFormatException("Premature EOF while skipping lines", AT);
		}
	}
}

unsigned int IOUtils::readLineToVec(const std::string& line_in, std::vector<std::string>& vecString)
{
	vecString.clear();
	std::istringstream iss(line_in); //construct inputstream with the string line as input

	std::string tmp_string;
	while (!iss.eof()) {
		iss >> std::skipws >> tmp_string;

		if (tmp_string != "") {
			vecString.push_back(tmp_string);
		}
		tmp_string="";
	}

	return vecString.size();
}

unsigned int IOUtils::readLineToVec(const std::string& line_in, std::vector<std::string>& vecString, const char& delim)
{
	vecString.clear();
	std::string tmp_string;
	std::istringstream iss(line_in);

	while (getline(iss, tmp_string, delim)){
		vecString.push_back(tmp_string);
	}

	return vecString.size();
}

// generic template function convertString must be defined in the header

const char ALPHANUM[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const char NUM[] = "0123456789";

template<> bool IOUtils::convertString<std::string>(std::string& t, const std::string& str, std::ios_base& (*f)(std::ios_base&))
{
	(void)f;
	std::string s = str;
	trim(s); //delete trailing and leading whitespaces and tabs

	t = s;
	return true;
}

template<> bool IOUtils::convertString<bool>(bool& t, const std::string& str, std::ios_base& (*f)(std::ios_base&))
{
	std::string s = str;
	trim(s); //delete trailing and leading whitespaces and tabs

	if (toupper(s[0])=='T' || toupper(s[0])=='Y' ) {
		t = true;
	} else if (toupper(s[0])=='F' || toupper(s[0])=='N' ) {
		t = false;
	} else {
		std::istringstream iss(s);
		int i;
		iss >> f >> i; //Convert first part of stream with the formatter (e.g. std::dec, std::oct)
		if (iss.fail()) {//Conversion failed
			return false;
		}
		t = (i != 0);
	}

	std::string::size_type pos = s.find_first_not_of(ALPHANUM);
	if (pos != std::string::npos) {
		std::string tmp = s.substr(pos);
		trim(tmp);
		if ((tmp.length() > 0) && tmp[0] != '#' && tmp[0] != ';') {//if line holds more than one value it's invalid
			return false;
		}
	}

	return true;
}

bool IOUtils::convertString(Date& t, const std::string& str, const double& time_zone, std::ios_base& (*f)(std::ios_base&))
{
	std::string s = str;
	trim(s); //delete trailing and leading whitespaces and tabs

	(void)f;
	unsigned int year, month, day, hour, minute, second;
	char rest[32] = "";

	//HACK: we read the seconds, but we ignore them...
	if (sscanf(s.c_str(), "%u-%u-%u %u:%u:%u%31s", &year, &month, &day, &hour, &minute, &second, rest) >= 6) {
		t.setDate(year, month, day, hour, minute, time_zone);
	} else if (sscanf(s.c_str(), "%u-%u-%uT%u:%u:%u%31s", &year, &month, &day, &hour, &minute, &second, rest) >= 6) {
		t.setDate(year, month, day, hour, minute, time_zone);
	} else if (sscanf(s.c_str(), "%u-%u-%u %u:%u%31s", &year, &month, &day, &hour, &minute, rest) >= 5) {
		t.setDate(year, month, day, hour, minute, time_zone);
	} else if (sscanf(s.c_str(), "%u-%u-%uT%u:%u%31s", &year, &month, &day, &hour, &minute, rest) >= 5) {
		t.setDate(year, month, day, hour, minute, time_zone);
	} else if (sscanf(s.c_str(), "%u-%u-%u%31s", &year, &month, &day, rest) >= 3) {
		t.setDate(year, month, day, 0, 0, time_zone);
	} else if (sscanf(s.c_str(), "%u:%u%31s", &hour, &minute, rest) >= 2) {
		t.setDate( ((double)hour)/24. + ((double)minute)/24./60. , time_zone);
	} else {
		//try to read purely numerical date, potentially surrounded by other chars
		const unsigned int in_len = str.length();
		size_t beg = str.find_first_of(NUM);
		if(beg==npos || beg==in_len) return false;
		size_t end = str.find_first_not_of(NUM, beg+1);
		if(end==npos) end = in_len;

		const std::string datum = str.substr(beg, end-beg);
		const unsigned int d_len = datum.length();
		if(d_len<8 || d_len>14) return false;
		if( convertString(year,datum.substr(0,4))==false ) return false;
		if( convertString(month,datum.substr(4,2))==false ) return false;
		if( convertString(day,datum.substr(6,2))==false ) return false;
		if( convertString(hour,datum.substr(8,2))==false ) return false;
		if(d_len==10)
			minute=0;
		else {
			if(d_len>=12) {
				if( convertString(minute,datum.substr(10,2))==false ) return false;
			} else
				return false;
			if(d_len==12)
				second=0;
			else {
				if(d_len==14) {
					if( convertString(second,datum.substr(12,2))==false ) return false;
				} else
					return false;
			}
		}
		
		t.setDate( year, month, day, hour, minute, time_zone );
	}

	std::string tmp(rest);
	trim(tmp);
	if ((tmp.length() > 0) && tmp[0] != '#' && tmp[0] != ';') {//if line holds more than one value it's invalid
		return false;
	}

	return true;
}

template<> bool IOUtils::convertString<Coords>(Coords& t, const std::string& str, std::ios_base& (*f)(std::ios_base&))
{
	std::string s = str;
	trim(s); //delete trailing and leading whitespaces and tabs

	(void)f;
	double lat, lon;
	try {
		Coords::parseLatLon(s,lat, lon);
	} catch(IOException &e) {
		return false;
	}
	t.setLatLon(lat, lon, nodata);

	return true;
}


void IOUtils::getProjectionParameters(const Config& cfg, std::string& coordin, std::string& coordinparam,
                                      std::string& coordout, std::string& coordoutparam) {
	cfg.getValue("COORDSYS", "Input", coordin);
	cfg.getValue("COORDPARAM", "Input", coordinparam, Config::nothrow);
	cfg.getValue("COORDSYS", "Output", coordout, Config::nothrow);
	cfg.getValue("COORDPARAM", "Output", coordoutparam, Config::nothrow);
}

void IOUtils::getTimeZoneParameters(const Config& cfg, double& tz_in, double& tz_out) {
	cfg.getValue("TIME_ZONE", "Input", tz_in, Config::nothrow);
	cfg.getValue("TIME_ZONE", "Output", tz_out, Config::nothrow);
}

unsigned int IOUtils::seek(const Date& soughtdate, const std::vector<MeteoData>& vecM, const bool& exactmatch){
	//returns index of element, if element does not exist it returns closest index after soughtdate
	//the element needs to be an exact hit or embedded between two measurments

	if (vecM.size() <= 0) {//no elements in buffer
		return IOUtils::npos;
	}

	//if we reach this point: at least one element in buffer
	if (vecM[0].date > soughtdate) {
		return IOUtils::npos;
	}

	if (vecM[vecM.size()-1].date < soughtdate) {//last element is earlier, return npos
		return IOUtils::npos;
	}

	if (vecM[0].date == soughtdate) {//closest element
		return 0;
	}

	//if we reach this point: the date is spanned by the buffer and there are at least two elements
	//HACK: would it be better to create a timeseries object and call vector's binary search on it?
	if (exactmatch){
		unsigned int first = 1, last = vecM.size()-1;

		//perform binary search
		while (first <= last) {
			unsigned int mid = (first + last) / 2;  // compute mid point
			if (soughtdate > vecM[mid].date)
				first = mid + 1;                   // repeat search in top half
			else if (soughtdate < vecM[mid].date)
				last = mid - 1;                    // repeat search in bottom half
			else
				return mid;                        // found it. return position
		}
	} else {
		unsigned int first = 0, last = vecM.size()-1;

		//perform binary search
		while (first <= last) {
			unsigned int mid = (first + last) / 2;  // compute mid point

			if (mid < (vecM.size()-1))
				if ((soughtdate > vecM[mid].date) && (soughtdate < vecM[mid+1].date))
					return mid+1;

			if (soughtdate > vecM[mid].date)
				first = mid + 1;                   // repeat search in top half
			else if (soughtdate < vecM[mid].date)
				last = mid - 1;                    // repeat search in bottom half
			else
				return mid;                        // found it. return position

		}
	}

	return IOUtils::npos;
}

std::string IOUtils::printFractionalDay(const double& fractional) {
	const double hours=floor(fractional*24.);
	const double minutes=floor((fractional*24.-hours)*60.);
	const double seconds=fractional*24.*3600.-hours*3600.-minutes*60.;

	std::stringstream tmp;
	tmp << std::fixed << std::setfill('0') << std::setprecision(0);
	tmp << std::setw(2) << hours << ":";
	tmp << std::setw(2) << minutes << ":";
	tmp << std::setw(2) << seconds;

	return tmp.str();
}

} //namespace
