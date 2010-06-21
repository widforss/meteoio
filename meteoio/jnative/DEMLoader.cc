/*
 * DEMLoader.cc
 *
 *  Created on: 19.01.2010
 *      Author: perot
 */


#include "DEMLoader.h"
#include <meteoio/ConfigReader.h>
#include "plugins/ARCIO.h"
//#include "plugins/BormaIO.h"
#include "plugins/GeotopIO.h"
#include "plugins/GrassIO.h"
#include <meteoio/IOInterface.h>
#include "synchronized.h"

#include <iostream>
#include <sstream>


// Initialisation
DEMLoader *DEMLoader::_singleton = NULL;

// DEMLoader::_singleton->demMap is a critical resource in a multithread context
//Any writting needs to be synchrionized first !
Mutex mutexDemMap;


/**
 *
 */
IOInterface* DEMLoader::generateIOInterface(
		const std::string  cDemFile,
		const std::string  cDemCoordSystem,
		const std::string cInterfaceType){

	IOInterface *io = NULL;
	try {
		ConfigReader cfg;
		cfg.addKey("DEMFILE", cDemFile);
		cfg.addKey("COORDIN", cDemCoordSystem);
		cfg.addKey("COORDPARAM","");
		if(cInterfaceType == "ARCIO")
			io = new ARCIO(cfg);
		//else if(cInterfaceType ==  "BormaIO" ): io = new BormaIO(cfg);
		else if(cInterfaceType == "GeotopIO" )
			io = new GeotopIO(cfg);
		else if(cInterfaceType == "GrassIO" )
			io = new GrassIO(cfg);
		else
			io = new ARCIO(cfg); //default IOinterface
	}catch (IOException& e){
		std::cout << "Problem with IOInterface ganeration in DEMLoader singleton, cause: "
					<< e.what() << std::endl;
		return NULL ;
	}
	return io;
}

/**
 *
 */
const DEMObject& DEMLoader::internal_loadSubDEM(const std::string  cDemFile,
		const std::string  cDemCoordSystem,
		const std::string cInterfaceType,
		const double demXll, const double demYll, const double demXur, const double demYur){

	std::string s;
	{
		std::ostringstream oss;
		oss << cDemFile;
		oss << demXll;
		oss << demYll;
		oss << demXur;
		oss << demYur;
		s = oss.str();
	}
	demMapType::iterator iter = demMap.find(s);
	if( iter != demMap.end() ){
		const DEMObject& res =  iter->second; //found in map, return it
		std::cout << "DEMLoader : "  << s << " already loaded" <<  std::endl;
		return res;
	}

	synchronized(mutexDemMap) //defines and enter a critical section
	{
		iter = demMap.find(s); //may have been loaded by concurent thread
		if( iter == demMap.end() ){
			IOInterface* io = this->generateIOInterface(cDemFile, cDemCoordSystem, cInterfaceType);
			if (io!=NULL){
				std::cout << "DEMLoader : "  << s << " loading ..." <<  std::endl;
				DEMObject dem;
				//read file ...
				io->readDEM(dem);

				//set the two opposing corners
				Coords llcorner(cDemCoordSystem, "");
				llcorner.setXY(demXll, demYll);
				dem.gridify(llcorner);

				Coords urcorner(cDemCoordSystem, "");
				urcorner.setXY(demXur, demYur);
				dem.gridify(urcorner);

				//extract a sub-dem
				DEMObject sub_dem(dem, llcorner.getGridI(), llcorner.getGridJ(),
						urcorner.getGridI(),-llcorner.getGridI()+1,
						urcorner.getGridJ() -llcorner.getGridJ()+1);

				demMap.insert(demPairType(s, sub_dem));// critical operation
				std::cout << "DEMLoader : "  << s << " loaded !!" <<  std::endl;
				delete io;
			}
		}
	}
	return demMap[s];
}

/**
 *
 */
const DEMObject& DEMLoader::internal_loadFullDEM(const std::string  cDemFile,
		const std::string  cDemCoordSystem,
		const std::string cInterfaceType){

	std::string s(cDemFile);
	demMapType::iterator iter = demMap.find(s);
	if( iter != demMap.end() ){
		const DEMObject& res =  iter->second; //found in map, return it
		std::cout << "DEMLoader : "  << s << " already loaded" <<  std::endl;
		return res;
	}

	synchronized(mutexDemMap) //defines and enter a critical section
	{
		iter = demMap.find(s); //may have been loaded by concurent thread
		if( iter == demMap.end() ){
			IOInterface* io = this->generateIOInterface(cDemFile, cDemCoordSystem, cInterfaceType);
			if (io!=NULL){
				std::cout << "DEMLoader : "  << s << " loading ..." <<  std::endl;
				DEMObject dem;
				io->readDEM(dem);
				//read file ...
				demMap.insert(demPairType(s, dem));// critical operation
				std::cout << "DEMLoader : "  << s << " loaded !!" <<  std::endl;
				delete io;
			}
		}
	}
	return demMap[s];
}
