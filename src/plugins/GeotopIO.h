#ifndef __GEOTOPIO_H__
#define __GEOTOPIO_H__

#include "IOInterface.h"
#include "ConfigReader.h"
#include "IOUtils.h"

#include <string>
#include <sstream>
#include <iostream>

#include "IOExceptions.h"
#include "DynamicLibrary.h"

/**
 * @class GeotopIO
 * @brief This class enables the access meteo data in legacy Geotop format
 *
 * @author Thomas Egger
 * @date   2009-07-02
 */
class GeotopIO : public IOInterface {
	public:
		//virtual GeotopIO* clone() const;

		GeotopIO(void (*delObj)(void*), const string& filename);

		GeotopIO(const std::string& configfile);
		GeotopIO(const GeotopIO&);
		GeotopIO(const ConfigReader&);
		~GeotopIO() throw();

		virtual void read2DGrid(Grid2DObject& dem_out, const string& parameter="");

		virtual void readDEM(DEMObject& dem_out);
		virtual void readLanduse(Grid2DObject& landuse_out);

		virtual void readMeteoData(const Date_IO& dateStart, const Date_IO& dateEnd, 
							  std::vector< std::vector<MeteoData> >& vecMeteo, 
							  std::vector< std::vector<StationData> >& vecStation,
							  const unsigned int& stationindex=IOUtils::npos);

		virtual void readAssimilationData(const Date_IO&, Grid2DObject& da_out);
		virtual void readSpecialPoints(CSpecialPTSArray& pts);

		virtual void write2DGrid(const Grid2DObject& grid_in, const string& name);
		void read2DMeteo(const Date_IO&, vector<MeteoData>&, vector<StationData>&); ///<No buffering

	private:
		void readMetaData(std::vector<StationData>& vecStation, std::vector<std::string>& vecColumnNames,
					   const std::string& metafile);
		void makeColumnMap(const std::vector<std::string>& tmpvec, 
					    const std::vector<std::string>& vecColumnNames, 
					    std::map<std::string, unsigned int>& mapHeader);
		void convertUnits(MeteoData& meteo);
		void cleanup() throw();
		ConfigReader cfg;
		ifstream fin; //Input file streams
};

#endif
