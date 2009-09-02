#ifndef __IOHANDLER_H__
#define __IOHANDLER_H__

#ifdef _POPC_
#error
#endif

#include "IOInterface.h"
#include "A3DIO.h"
#include "IOExceptions.h"
#include "IOPlugin.h"

#include <map>

/**
* @file IOHandler.h
* The is the class implementing the interface as defined by the IOInterface class.
* This class is responsible for loading the necessary plugins and getting the data through them.
*/
class IOHandler : public IOInterface {
	public:
		// virtual IOHandler* clone() const; // lwk : not used yet

		IOHandler(const std::string& configfile);
		IOHandler(const IOHandler&);
		IOHandler(const ConfigReader&);
		~IOHandler() throw();

		virtual void read2DGrid(Grid2DObject& dem_out, const string& parameter="");

		virtual void readDEM(Grid2DObject& dem_out);
		virtual void readLanduse(Grid2DObject& landuse_out);

		virtual void readMeteoData(const Date_IO& dateStart, const Date_IO& dateEnd, 
							  std::vector<METEO_DATASET>& vecMeteo, 
							  std::vector<STATION_DATASET>& vecStation,
							  const unsigned int& stationindex=IOUtils::npos);

		void readMeteoData(const Date_IO& date, METEO_DATASET& vecMeteo, STATION_DATASET& vecStation);

		virtual void readAssimilationData(const Date_IO&, Grid2DObject& da_out);
		virtual void readSpecialPoints(CSpecialPTSArray& pts);

		virtual void write2DGrid(const Grid2DObject& grid_in, const string& name);

	private:
		string ascii_src;
		string boschung_src;
		string imis_src;
		string geotop_src;

	private:
		void loadDynamicPlugins();
		void loadPlugin(const std::string& libname, const std::string& classname, 
					 DynamicLibrary*& dynLibrary, IOInterface*& io);
		void deletePlugin(DynamicLibrary*& dynLibrary, IOInterface*& io) throw();
		void registerPlugins();
		IOInterface *getPlugin(const std::string&);

		ConfigReader cfg;
		map<std::string, IOPlugin> mapPlugins;
		map<std::string, IOPlugin>::iterator mapit;
		A3DIO fileio;
};

#endif
