#ifndef __IOUTILS_H__
#define __IOUTILS_H__

#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <list>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>
#include <cctype>
#include <limits>

#include "IOExceptions.h"
#include "Date_IO.h"

#ifndef MAX
#define MAX(x,y)    (((x) < (y)) ? (y) : (x))
#endif
#ifndef MIN
#define MIN(x,y)    (((x) < (y)) ? (x) : (y))
#endif

#ifndef C_TO_K
#define C_TO_K( T ) ( T + 273.15 )	  // Celsius to Kelvin
#endif
#ifndef K_TO_C
#define K_TO_C( T ) ( T - 273.15 )	  // Kelvin to Celsius
#endif

namespace IOUtils {

	const double nodata = -999.0;	//HACK: we should define the same nodata everywhere...
	const unsigned int unodata = (unsigned int)-1;
	const unsigned int npos    = (unsigned int)-1;

	/**
	* @brief Check whether two values are equal regarding a certain epsilon environment (within certain radius of each other)
	* @param val1
	* @param val2
	* @param epsilon is a radius around val1
	* @return true if val2 is within the radius around val1, false otherwise.
	*/
	bool checkEpsilonEquality(double val1, double val2, double epsilon);

	double pow2(const double val);

	void readDirectory(const std::string& path, std::list<std::string>& dirlist, const std::string& pattern = "");

	bool validFileName(const std::string& filename);

	bool fileExists(const std::string& filename);

	/**
	* @brief Removes trailing and leading whitespaces, tabs and newlines from a string. 
	* @param s The reference of the string to trim (in/out parameter)
	*/
	void trim(std::string &s);

	char getEoln(std::istream& fin);

	void skipLines(std::istream& fin, unsigned int nbLines, char eoln='\n');

	/**
	* @brief read a string line, parse it and save it into a map object, that is passed by reference
	* @param in_line (const string&) string to parse
	* @param delimiter (const string&) delimiter to use for the parsing
	* @param out_map (map\<string,string\>&) map after parsing
	* @param keyprefix this string is prefixed before the key, defaults to no prefix: ""
	* @return (bool) true when line is empty
	*/
	bool readKeyValuePair(const std::string& in_line, 
					  const std::string& delimiter, 
					  std::map<std::string,std::string>& out_map,
					  const std::string& keyprefix="");

	void toUpper(std::string& str);
	unsigned int readLineToVec(const std::string& line_in, std::vector<std::string>& vecString);
	unsigned int readLineToVec(const std::string& line_in, std::vector<std::string>& vecString, const char& delim);
	void readKeyValueHeader(std::map<std::string, std::string>& headermap, 
				    std::istream& bs,
				    const unsigned int& linecount=1, 
				    const std::string& delimiter="=");


	/** 
	* @brief Convert a string to the requested type (template function). 
	* @tparam T   [in] The type wanted for the return value (template type parameter). 
	* @param t   [out] The value converted to the requested type. 
	* @param str   [in] The input string to convert; trailling whitespaces are ignored, 
	*              comment after non-string values are allowed, but multiple values are not allowed. 
	* @param f   [in] The radix for reading numbers, such as std::dec or std::oct; default is std::dec. 
	* @return true if everything went fine, false otherwise
	*/
	template <class T> bool convertString(T& t, const std::string str, std::ios_base& (*f)(std::ios_base&) = std::dec) {
		std::string s = str; 
		trim(s); //delete trailing and leading whitespaces and tabs
		if (s.size() == 0) {
			t = static_cast<T> (nodata);
			return true;
		} else {
			std::istringstream iss(s);
			iss.setf(ios::fixed);
			iss.precision(std::numeric_limits<double>::digits10); //try to read values with maximum precision
			iss >> f >> t; //Convert first part of stream with the formatter (e.g. std::dec, std::oct)
			if (iss.fail()) {
				//Conversion failed
				return false;
			}
			std::string tmp="";
			getline(iss,  tmp); //get rest of line, if any
			trim(tmp);
			if ((tmp.length() > 0) && tmp[0] != '#' && tmp[0] != ';') {
				//if line holds more than one value it's invalid
				return false;
			}
			return true;
		}
	}
	// fully specialized template functions (implementation must not be in header)
	template<> bool convertString<std::string>(std::string& t, const std::string str, std::ios_base& (*f)(std::ios_base&));
	template<> bool convertString<bool>(bool& t, const std::string str, std::ios_base& (*f)(std::ios_base&));
	template<> bool convertString<Date_IO>(Date_IO& t, const std::string str, std::ios_base& (*f)(std::ios_base&));

	/**
	* @brief Returns, with the requested type, the value associated to a key (template function).
	* @tparam T   [in] The type wanted for the return value (template type parameter).
	* @param[in] properties   A map containing all the parameters.
	* @param[in] key   The key of the parameter to retrieve.
	* @param[out] t   The value associated to the key, converted to the requested type
	*/
	template <class T> void getValueForKey(const std::map<std::string,std::string>& properties, const std::string& key, T& t) {
		if (key == "") {
			throw InvalidArgumentException("Empty key", AT);
		}
		const std::string value = (const_cast<std::map<std::string,std::string>&>(properties))[key];

		if (value == "") {
			throw UnknownValueException("No value for key " + key, AT);
		}

		if(!convertString<T>(t, value, std::dec)) {
			throw ConversionFailedException(value, AT);
		}
	}

	/**
	* @brief Returns, with the requested type, the value associated to a key (template function).
	* @tparam T           [in] The type wanted for the return value (template type parameter).
	* @param[in] properties  A map containing all the parameters.
	* @param[in] key         The key of the parameter to retrieve.
	* @param[out] vecT        The vector of values associated to the key, each value is converted to the requested type
	*/
	template <class T> void getValueForKey(const std::map<std::string,std::string>& properties, 
								    const std::string& key, vector<T>& vecT) {
		if (key == "") {
			throw InvalidArgumentException("Empty key", AT);
		}
		const std::string value = (const_cast<std::map<std::string,std::string>&>(properties))[key];

		if (value == "") {
			throw UnknownValueException("No value for key " + key, AT);
		}

		//split value string 
		std::vector<std::string> vecUnconvertedValues;
		unsigned int counter = readLineToVec(value, vecUnconvertedValues);

		for (unsigned int ii=0; ii<counter; ii++){
			T myvar;
			if(!convertString<T>(myvar, vecUnconvertedValues.at(ii), std::dec)){
				throw ConversionFailedException(vecUnconvertedValues.at(ii), AT);
			}
			
			vecT.push_back(myvar);
		}
	}
  
} //end namespace IOUtils

#endif
