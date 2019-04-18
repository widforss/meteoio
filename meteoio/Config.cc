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
#include <meteoio/Config.h>
#include <meteoio/FileUtils.h>

#include <algorithm>
#include <fstream>
#include <cstdio>

using namespace std;

namespace mio {

const char* Config::defaultSection = "GENERAL";

//Constructors
Config::Config() : properties(), imported(), sections(), sourcename(), configRootDir() {}

Config::Config(const std::string& i_filename) : properties(), imported(), sections(), sourcename(i_filename), configRootDir(FileUtils::getPath(i_filename, true))
{
	addFile(i_filename);
}

const ConfigProxy Config::get(const std::string& key, const std::string& section) const
{
	return ConfigProxy(*this, key, section);
}

template <typename T> T Config::get(const std::string& key, const std::string& section, const T& dflt) const
{
	if (keyExists(key, section)) {
		T tmp;
		getValue(key, section, tmp);
		return tmp;
	} else {
		return dflt;
	}
}

std::string Config::get(const std::string& key, const std::string& section, const std::string& dflt) const
{
	if (keyExists(key, section)) {
		std::string tmp;
		getValue(key, section, tmp);
		return tmp;
	} else {
		return dflt;
	}
}

std::string Config::get(const std::string& key, const std::string& section, const char dflt[]) const
{
	if (keyExists(key, section)) {
		std::string tmp;
		getValue(key, section, tmp);
		return tmp;
	} else {
		return std::string(dflt);
	}
}

bool Config::get(const std::string& key, const std::string& section, const bool& dflt) const
{
	if (keyExists(key, section)) {
		bool tmp;
		getValue(key, section, tmp);
		return tmp;
	} else {
		return dflt;
	}
}

//Populating the property map
void Config::addFile(const std::string& i_filename)
{
	if (configRootDir.empty()) configRootDir = FileUtils::getPath(i_filename, true);
	sourcename = i_filename;
	parseFile(i_filename);
}

void Config::addKey(std::string key, std::string section, const std::string& value)
{
	IOUtils::toUpper(section);
	IOUtils::toUpper(key);
	properties[ section + "::" + key ] = value;
}

void Config::deleteKey(std::string key, std::string section)
{
	IOUtils::toUpper(section);
	IOUtils::toUpper(key);
	properties.erase( section + "::" + key );
}

void Config::deleteKeys(std::string keymatch, std::string section, const bool& anywhere)
{
	IOUtils::toUpper(section);
	IOUtils::toUpper(keymatch);
	const size_t section_len = section.length();

	//Loop through keys, look for match - delete matches
	if (anywhere) {
		std::map<std::string,std::string>::iterator it = properties.begin();
		while (it != properties.end()) {
			const size_t found_section = (it->first).find(section, 0);

			if ( found_section!=string::npos && (it->first).find(keymatch, section_len)!=string::npos )
				properties.erase( it++ ); // advance before iterator become invalid
			else //wrong section or no match
				++it;
		}

	} else {
		keymatch = section + "::" + keymatch;
		std::map<std::string,std::string>::iterator it = properties.begin();
		while (it != properties.end()) {
			if ( (it->first).find(keymatch, 0)==0 ) //match found at start
				properties.erase( it++ ); // advance before iterator become invalid
			else
				++it;
		}
	}
}

bool Config::keyExists(std::string key, std::string section) const
{
	IOUtils::toUpper(section);
	IOUtils::toUpper(key);
	const std::string full_key( section + "::" + key );
	const std::map<string,string>::const_iterator it = properties.find(full_key);
	return (it!=properties.end());
}

bool Config::sectionExists(std::string section) const
{
	IOUtils::toUpper( section );
	std::set<std::string>::const_iterator it = sections.begin();

	for (; it!=sections.end(); ++it) {
		if (*it==section) return true;
	}

	return false;
}

void Config::moveSection(std::string org, std::string dest, const bool& overwrite)
{
	IOUtils::toUpper( org );
	IOUtils::toUpper( dest );
	std::map<string,string>::iterator it = properties.begin();

	//delete all current keys in "dest" if overwrite==true
	if (overwrite) {
		while (it != properties.end()) {
			const std::string::size_type pos = it->first.find("::");
			if (pos!=std::string::npos) {
				const std::string sectionname( IOUtils::strToUpper(it->first.substr(0, pos)) );
				if (sectionname==dest) properties.erase( it++ ); // advance before iterator become invalid
				else ++it;
			} else {
				++it;
			}
		}
	}

	//move the keys from org to dest
	it = properties.begin();
	while (it != properties.end()) {
		const std::string::size_type pos = it->first.find("::");
		if (pos!=std::string::npos) {
			const std::string sectionname( IOUtils::strToUpper(it->first.substr(0, pos)) );
			if (sectionname==org) {
				const std::string key( it->first.substr(pos) );
				properties[ dest+key ] = it->second;
				properties.erase( it++ ); // advance before iterator become invalid
			} else {
				++it;
			}
		} else {
			++it;
		}
	}
}

std::vector< std::pair<std::string, std::string> > Config::getValues(std::string keymatch, std::string section, const bool& anywhere) const
{
	IOUtils::toUpper(section);
	IOUtils::toUpper(keymatch);
	const size_t section_len = section.length();
	std::vector< std::pair<std::string, std::string> > vecResult;

	//Loop through keys, look for match - push it into vecResult
	if (anywhere) {
		for (std::map<string,string>::const_iterator it=properties.begin(); it != properties.end(); ++it) {
			const size_t found_section = (it->first).find(section, 0);
			if (found_section==string::npos) continue; //not in the right section

			const size_t found_pos = (it->first).find(keymatch, section_len);
			if (found_pos!=string::npos) { //found it!
				const std::string key( (it->first).substr(section_len + 2) ); //from pos to the end
				vecResult.push_back( make_pair(key, it->second));
			}
		}
	} else {
		keymatch = section + "::" + keymatch;
		for (std::map<string,string>::const_iterator it=properties.begin(); it != properties.end(); ++it) {
			const size_t found_pos = (it->first).find(keymatch, 0);
			if (found_pos==0) { //found it!
				const std::string key( (it->first).substr(section_len + 2) ); //from pos to the end
				vecResult.push_back( make_pair(key, it->second));
			}
		}
	}

	return vecResult;
}

std::vector<std::string> Config::getKeys(std::string keymatch,
                        std::string section, const bool& anywhere) const
{
	IOUtils::toUpper(section);
	IOUtils::toUpper(keymatch);
	const size_t section_len = section.length();
	std::vector<std::string> vecResult;

	//Loop through keys, look for match - push it into vecResult
	if (anywhere) {
		for (std::map<string,string>::const_iterator it=properties.begin(); it != properties.end(); ++it) {
			const size_t found_section = (it->first).find(section, 0);
			if (found_section==string::npos) continue; //not in the right section

			const size_t found_pos = (it->first).find(keymatch, section_len);
			if (found_pos!=string::npos) { //found it!
				const std::string key( (it->first).substr(section_len + 2) ); //from pos to the end
				vecResult.push_back(key);
			}
		}
	} else {
		keymatch = section + "::" + keymatch;

		for (std::map<string,string>::const_iterator it=properties.begin(); it != properties.end(); ++it) {
			const size_t found_pos = (it->first).find(keymatch, 0);
			if (found_pos==0) { //found it!
				const std::string key( (it->first).substr(section_len + 2) ); //from pos to the end
				vecResult.push_back(key);
			}
		}
	}

	return vecResult;
}

void Config::write(const std::string& filename) const
{
	if (!FileUtils::validFileAndPath(filename)) throw InvalidNameException(filename,AT);
	std::ofstream fout(filename.c_str(), ios::out);
	if (fout.fail()) throw AccessException(filename, AT);

	try {
		std::string current_section;
		unsigned int sectioncount = 0;
		for (std::map<string,string>::const_iterator it=properties.begin(); it != properties.end(); ++it) {
			const std::string key_full( it->first );
			const std::string section( extract_section(key_full) );

			if (current_section != section) {
				current_section = section;
				if (sectioncount != 0)
					fout << endl;
				sectioncount++;
				fout << "[" << section << "]" << endl;
			}

			const size_t key_start = key_full.find_first_of(":");
			const std::string value( it->second );
			if (value.empty()) continue;

			if (key_start!=string::npos) //start after the "::" marking the section prefix
				fout << key_full.substr(key_start+2) << " = " << value << endl;
			else //every key should have a section prefix, but just in case...
				fout << key_full << " = " << value << endl;
		}
	} catch(...) {
		if (fout.is_open()) //close fout if open
			fout.close();

		throw;
	}

	if (fout.is_open()) //close fout if open
		fout.close();
}

const std::string Config::toString() const {
	std::ostringstream os;
	os << "<Config>\n";
	os << "Source: " << sourcename << "\n";
	for (map<string,string>::const_iterator it = properties.begin(); it != properties.end(); ++it){
		os << (*it).first << " -> " << (*it).second << "\n";
	}
	os << "</Config>\n";
	return os.str();
}

std::ostream& operator<<(std::ostream& os, const Config& cfg) {
	const size_t s_source = cfg.sourcename.size();
	os.write(reinterpret_cast<const char*>(&s_source), sizeof(size_t));
	os.write(reinterpret_cast<const char*>(&cfg.sourcename[0]), s_source*sizeof(cfg.sourcename[0]));
	
	const size_t s_rootDir = cfg.configRootDir.size();
	os.write(reinterpret_cast<const char*>(&s_rootDir), sizeof(size_t));
	os.write(reinterpret_cast<const char*>(&cfg.configRootDir[0]), s_rootDir*sizeof(cfg.configRootDir[0]));

	const size_t s_map = cfg.properties.size();
	os.write(reinterpret_cast<const char*>(&s_map), sizeof(size_t));
	for (map<string,string>::const_iterator it = cfg.properties.begin(); it != cfg.properties.end(); ++it){
		const string& key = (*it).first;
		const size_t s_key = key.size();
		os.write(reinterpret_cast<const char*>(&s_key), sizeof(size_t));
		os.write(reinterpret_cast<const char*>(&key[0]), s_key*sizeof(key[0]));

		const string& value = (*it).second;
		const size_t s_value = value.size();
		os.write(reinterpret_cast<const char*>(&s_value), sizeof(size_t));
		os.write(reinterpret_cast<const char*>(&value[0]), s_value*sizeof(value[0]));
	}
	
	const size_t s_imported = cfg.imported.size();
	os.write(reinterpret_cast<const char*>(&s_imported), sizeof(size_t));
	for (set<string>::const_iterator it = cfg.imported.begin(); it != cfg.imported.end(); ++it){
		const string& value = *it;
		const size_t s_value = value.size();
		os.write(reinterpret_cast<const char*>(&s_value), sizeof(size_t));
		os.write(reinterpret_cast<const char*>(&value[0]), s_value*sizeof(value[0]));
	}
	
	const size_t s_set = cfg.sections.size();
	os.write(reinterpret_cast<const char*>(&s_set), sizeof(size_t));
	for (set<string>::const_iterator it = cfg.sections.begin(); it != cfg.sections.end(); ++it){
		const string& value = *it;
		const size_t s_value = value.size();
		os.write(reinterpret_cast<const char*>(&s_value), sizeof(size_t));
		os.write(reinterpret_cast<const char*>(&value[0]), s_value*sizeof(value[0]));
	}

	return os;
}

std::istream& operator>>(std::istream& is, Config& cfg) {
	size_t s_source;
	is.read(reinterpret_cast<char*>(&s_source), sizeof(size_t));
	cfg.sourcename.resize(s_source);
	is.read(reinterpret_cast<char*>(&cfg.sourcename[0]), s_source*sizeof(cfg.sourcename[0]));
	
	size_t s_rootDir;
	is.read(reinterpret_cast<char*>(&s_rootDir), sizeof(size_t));
	cfg.configRootDir.resize(s_rootDir);
	is.read(reinterpret_cast<char*>(&cfg.configRootDir[0]), s_rootDir*sizeof(cfg.configRootDir[0]));

	cfg.properties.clear();
	size_t s_map;
	is.read(reinterpret_cast<char*>(&s_map), sizeof(size_t));
	for (size_t ii=0; ii<s_map; ii++) {
		size_t s_key, s_value;
		is.read(reinterpret_cast<char*>(&s_key), sizeof(size_t));
		string key;
		key.resize(s_key);
		is.read(reinterpret_cast<char*>(&key[0]), s_key*sizeof(key[0]));

		is.read(reinterpret_cast<char*>(&s_value), sizeof(size_t));
		string value;
		value.resize(s_value);
		is.read(reinterpret_cast<char*>(&value[0]), s_value*sizeof(value[0]));

		cfg.properties[key] = value;
	}
	
	cfg.imported.clear();
	size_t s_imported;
	is.read(reinterpret_cast<char*>(&s_imported), sizeof(size_t));
	for (size_t ii=0; ii<s_imported; ii++) {
		size_t s_value;
		is.read(reinterpret_cast<char*>(&s_value), sizeof(size_t));
		string value;
		value.resize(s_value);
		is.read(reinterpret_cast<char*>(&value[0]), s_value*sizeof(value[0]));

		cfg.imported.insert( value );
	}
	
	cfg.sections.clear();
	size_t s_set;
	is.read(reinterpret_cast<char*>(&s_set), sizeof(size_t));
	for (size_t ii=0; ii<s_set; ii++) {
		size_t s_value;
		is.read(reinterpret_cast<char*>(&s_value), sizeof(size_t));
		string value;
		value.resize(s_value);
		is.read(reinterpret_cast<char*>(&value[0]), s_value*sizeof(value[0]));

		cfg.sections.insert( value );
	}
	return is;
}

///////////////////////////////////////////////////// Private members //////////////////////////////////////////

/**
* @brief Parse the whole file, line per line
* @param[in] filename file to parse
*/
void Config::parseFile(const std::string& filename)
{
	if (!FileUtils::validFileAndPath(filename)) throw InvalidNameException(filename,AT);
	if (!FileUtils::fileExists(filename)) throw NotFoundException(filename, AT);

	//Open file
	std::ifstream fin(filename.c_str(), ifstream::in);
	if (fin.fail()) throw AccessException(filename, AT);
	imported.insert( FileUtils::cleanPath(filename, true) ); //keep track of this file being processed to prevent circular IMPORT directives
	
	std::string section( defaultSection );
	const char eoln = FileUtils::getEoln(fin); //get the end of line character for the file
	unsigned int linenr = 1;
	std::vector<std::string> import_after; //files to import after the current one
	bool accept_import_before = true;

	try {
		do {
			std::string line;
			getline(fin, line, eoln); //read complete line
			parseLine(linenr++, import_after, accept_import_before, line, section);
		} while (!fin.eof());
		fin.close();
	} catch(const std::exception&){
		if (fin.is_open()) {//close fin if open
			fin.close();
		}
		throw;
	}

	std::reverse(import_after.begin(), import_after.end());
	while (!import_after.empty()) {
		parseFile( import_after.back());
		import_after.pop_back();
	}
	
	imported.erase( filename );
}

bool Config::processSectionHeader(const std::string& line, std::string &section, const unsigned int& linenr)
{
	if (line[0] == '[') {
		const size_t endpos = line.find_last_of(']');
		if ((endpos == string::npos) || (endpos < 2) || (endpos != (line.length()-1))) {
			throw IOException("Section header corrupt at line " + IOUtils::toString(linenr), AT);
		} else {
			section = line.substr(1, endpos-1);
			IOUtils::toUpper(section);
			sections.insert( section );
			return true;
		}
	}

	return false;
}

//TODO add the following syntax: ${key} to refer to another key
//${env:var} to refer to an env variable
//possibility to evaluate a numeric expression

//expand ${env:xxx} syntax if necessary
void Config::processVars(std::string& value)
{
	size_t pos_start = 0;
	while ((pos_start = value.find("${env:")) != std::string::npos) {
		const size_t pos_end = value.find("}", pos_start);
		if (pos_end==std::string::npos || pos_end<=(pos_start+6+2))
			throw InvalidFormatException("Wrong syntax for environment variable: '"+value+"'", AT);
		const size_t next_start = value.find("${", pos_start+6);
		if (next_start!=std::string::npos && next_start<pos_end)
			throw InvalidFormatException("Wrong syntax for environment variable: '"+value+"'", AT);
		
		const size_t len = pos_end - (pos_start+6); //we have tested above that this is >=1
		const std::string envVar( value.substr(pos_start+6, len ) );
		char *tmp = getenv( envVar.c_str() );
		if (tmp==NULL) 
			throw InvalidNameException("Environment variable '"+envVar+"' declared in ini file could not be resolved", AT);
		
		value.replace(pos_start, pos_end+1, std::string(tmp));
		//std::cout << "envVar=" << envVar << " -> " << tmp << " -> value=" << value << "\n";
	}
	
	
	return;
}

bool Config::processImports(const std::string& key, const std::string& value, std::vector<std::string> &import_after, const bool &accept_import_before)
{
	if (key=="IMPORT_BEFORE") {
		const std::string file_and_path( FileUtils::cleanPath(value, true) );
		if (!accept_import_before)
			throw IOException("Error in \""+sourcename+"\": IMPORT_BEFORE key MUST occur before any other key!", AT);
		if (imported.count( file_and_path ) != 0)
			throw IOException("IMPORT Circular dependency with \"" + value + "\"", AT);
		parseFile( file_and_path );
		return true;
	}
	if (key=="IMPORT_AFTER") {
		const std::string file_and_path( FileUtils::cleanPath(value, true) );
		if (imported.count( file_and_path ) != 0)
			throw IOException("IMPORT Circular dependency with \"" + value + "\"", AT);
		import_after.push_back(file_and_path);
		return true;
	}

	return false;
}

void Config::handleNonKeyValue(const std::string& line_backup, const std::string& section, const unsigned int& linenr, bool &accept_import_before)
{
	std::string key, value;
	if (IOUtils::readKeyValuePair(line_backup, "=", key, value, true)) {
		if (value==";" || value=="#") { //so we can accept the comments char if given only by themselves (for example, to define a CSV delimiter)
			properties[section+"::"+key] = value; //save the key/value pair
			accept_import_before = false; //this is not an import, so no further import_before allowed
			return;
		}
	}

	const std::string key_msg = (key.empty())? "" : "key "+key+" ";
	const std::string key_value_link = (key.empty() && !value.empty())? "value " : "";
	const std::string value_msg = (value.empty())? "" : value+" " ;
	const std::string keyvalue_msg = (key.empty() && value.empty())? "key/value " : key_msg+key_value_link+value_msg;
	const std::string section_msg = (section.empty())? "" : "in section "+section+" ";
	const std::string source_msg = (sourcename.empty())? "" : "from \""+sourcename+"\" at line "+IOUtils::toString(linenr);

	throw InvalidFormatException("Error reading "+keyvalue_msg+section_msg+source_msg, AT);
}

void Config::parseLine(const unsigned int& linenr, std::vector<std::string> &import_after, bool &accept_import_before, std::string &line, std::string &section)
{
	const std::string line_backup( line ); //this might be needed in some rare cases
	//First thing cut away any possible comments (may start with "#" or ";")
	IOUtils::stripComments(line);
	IOUtils::trim(line);    //delete leading and trailing whitespace characters
	if (line.empty()) return;//ignore empty lines

	//if this is a section header, read it and return
	if (processSectionHeader(line, section, linenr)) return;

	//first, we check that we don't have two '=' chars in one line (this indicates a missing newline)
	if (std::count(line.begin(), line.end(), '=') != 1) {
		const std::string source_msg = (sourcename.empty())? "" : " in \""+sourcename+"\"";
		throw InvalidFormatException("Error reading line "+IOUtils::toString(linenr)+source_msg, AT);
	}
	
	//this can only be a key value pair...
	std::string key, value;
	if (IOUtils::readKeyValuePair(line, "=", key, value, true)) {
		//if this is an import, process it and return
		if (processImports(key, value, import_after, accept_import_before)) return;

		processVars(value);
		properties[section+"::"+key] = value; //save the key/value pair
		accept_import_before = false; //this is not an import, so no further import_before allowed
	} else {
		handleNonKeyValue(line_backup, section, linenr, accept_import_before);
	}

}

std::string Config::extract_section(std::string key)
{
	const std::string::size_type pos = key.find("::");

	if (pos != string::npos){
		const std::string sectionname( key.substr(0, pos) );
		key.erase(key.begin(), key.begin() + pos + 2); //delete section name
		return sectionname;
	}
	return std::string( defaultSection );
}

} //end namespace
