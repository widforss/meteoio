#ifndef __IOHANDLER_H__
#define __IOHANDLER_H__

#include "IOInterface.h"
#include "A3DIO.h"
#include "IOExceptions.h"
#include "IOPlugin.h"

#include <map>

#include "marshal_meteoio.h"

typedef map<std::string, IOPlugin::IOPlugin>::iterator PLUGIN_ITERATOR;

parclass IOHandler {
// Note : No heritage here for POPC++ : a parclass cannot herit from a class
		classuid(1003);
	public:
		// virtual IOHandler* clone() const; // lwk : not used yet
		IOHandler(const std::string& configfile)  @{ power=100 ?: 50; };
		//IOHandler(const IOHandler&);
		IOHandler(const ConfigReader&)  @{ power=100 ?: 50; };
		~IOHandler();

		virtual void read2DGrid([out]Grid2DObject& dem_out, const string& parameter="");

		virtual void readDEM([out]DEMObject& dem_out);
		virtual void readLanduse([out]Grid2DObject& landuse_out);

		virtual void readMeteoData([in]const Date_IO& dateStart, [in]const Date_IO& dateEnd,
			     			[proc=marshal_vector_METEO_DATASET] std::vector<METEO_DATASET>& vecMeteo,
						[proc=marshal_vector_STATION_DATASET] std::vector<STATION_DATASET>& vecStation,
						const unsigned& stationindex=IOUtils::npos);

		void readMeteoData([in]const Date_IO& date, [proc=marshal_METEO_DATASET] METEO_DATASET& vecMeteo, [proc=marshal_STATION_DATASET] STATION_DATASET& vecStation);

		virtual void readAssimilationData([in] const Date_IO&,[out] Grid2DObject& da_out);
		virtual void readSpecialPoints([out,proc=marshal_CSpecialPTSArray]CSpecialPTSArray& pts);

		virtual void write2DGrid([in]const Grid2DObject& grid_in, [in]const string& name);

	private:
		string ascii_src;
		string boschung_src;
		string imis_src;
		string geotop_src;

	private:
		void loadDynamicPlugins();
		void loadPlugin(const std::string& libname, const std::string& classname, 
					 DynamicLibrary*& dynLibrary, IOInterface*& io);
		void deletePlugin(DynamicLibrary*& dynLibrary, IOInterface*& io);
		void registerPlugins();
		IOInterface *getPlugin(const std::string&);

		ConfigReader cfg;
		map<std::string, IOPlugin::IOPlugin> mapPlugins;
		PLUGIN_ITERATOR mapit;
		A3DIO fileio;
};

#endif
