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
#ifndef __IOINTERFACE_H__
#define __IOINTERFACE_H__

#include <meteoio/Array.h>
#include <meteoio/Array2D.h>
#include <meteoio/Date.h>
#include <meteoio/DEMObject.h>
#include <meteoio/DynamicLibrary.h>
#include <meteoio/Grid2DObject.h>
#include <meteoio/MeteoData.h>
#include <meteoio/StationData.h>

#include <vector>

namespace mio {

/**
 * @page dev_plugins Plugins developement guide
 * The data access is handled by a system of plugins. They all offer the same interface, meaning that a plugin can transparently be replaced by another one. This means that plugins should follow some common rules, which are described in this guide. Templates header and code files are available to get you started, look into the "plugins" subdirectory of the source directory (files "template.cc" and "template.h").
 *
 * @section plugins_implementation Plugins implementation
 * Each plugin must inherit the class IOInterface and implement all or part of its interface (at least for public methods). The plugins are then free to implement the private methods that they see fit. Because the interface described in IOInterface is quite generic, some methods might not be relevant for a given plugin. In such a case, the plugin should throw an exception as illustrated in the example below:
 * @code
 * void MyPlugin::read2DGrid(Grid2DObject&, const std::string&)
 * {
 * 	//Nothing so far
 * 	throw IOException("Nothing implemented here", AT);
 * }
 * @endcode
 * 
 * It is the responsibility of the plugin to properly convert the units toward the SI as used in MeteoIO (see the MeteoData class for a list of parameters and their units)
 *
 * The meteorological data must be returned in a vector of vectors of MeteoData (and similarly, of StationData in order to provide the metadata). This consists of building a vector of MeteoData objects, each containing a set of measurements for a given timestamp, at a given location. This vector that contains the time series at one specific location is then added to a vector (pushed) that will then contain all locations.
 * \image html vector_vector.png "vector of vector structure"
 * \image latex vector_vector.eps "vector of vector structure" width=0.9\textwidth
 * 
 * Various classes from MeteoIO can prove convenient for use by plugins: for example the Coords class should be used for geographic coordinates conversions, while the ConfigReader class should be used for getting configuration information from the user's configuration file. Please do NOT implement your own version of this kind of feature in your plugin but exclusively rely on the matching classes of MeteoIO, extending them if necessary.
 * A template for writing a plugin class is available in the plugin directory under the name template.cc and template.h. Simply copy these two files under a new name and fill the methods that you want to implement. Some easy example implementation can be found in ARCIO or A3DIO.
 * 
 * @section plugins_registration Plugins registration
 * Once a plugin has been written, it must be "registered" so that it is known by the rest of the library. This is done in IOHandler::registerPlugins by adding a plugin key (that will be used by the user in the configuration file when he wants to use the said plugin), the name of the dynamic library that the plugin is bunddled in (knowing that CMake adds "lib" to prefix all library names, which means that you should expect a file name to look like "libmyplugin"), the name of its implementation class, a pointer to the implementation class (use NULL and it will be properly initialized), and a pointer to the dynamicl library (again, set as NULL and the proper initialization will take place). For more information, see the IOPlugin class.
 * 
 * Finally, the build system has to be updated so that it offers the plugin to be build: a local file (<a href="../../meteoio/plugins/CMakeLists.txt">meteoio/plugins/CMakeLists.txt</a>) has to be edited so that the plugin is really built, then the toplevel file has to be modified so the user can choose to build the plugin if he wishes (<a href="../../CMakeLists.txt">CMakeLists.txt</a>). Please keep in mind that all plugins should be optional (ie: they should not prevent the build of MeteoIO without them) and please call your plugin compilation flag similarly as the other plugins (ie: PLUGIN_MYNAME).
 *
 * @section plugins_documentation Plugins documentation
 * It is the responsibility of the plugin developer to properly document his plugin. The documentation should be placed as doxygen comments in the implementation file, following the example of A3DIO.cc: a subpage nammed after the plugin should be created (and referenced in MainPage.h) with at least the following sections:
 * - format, for describing the data format
 * - units, expressing what the input units should be
 * - keywords, listing (with a brief description) the keywords that are recognized by the plugin for its configuration
 * 
 * The internal documentation of the plugin can remain as normal C++ comments (since they are addressed to the maintainer of the plugin).
 */

/**
 * @class IOInterface
 * @brief An abstract class representing the IO Layer of the software Alpine3D. For each type of IO (File, DB, Webservice, etc)
 * a derived class is to be created that holds the specific implementation of the purely virtual member funtions. 
 * The IOHandler class is a wrapper class that is able to deal with all above implementations of the IOInterface abstract base class.
 *
 * @author Thomas Egger
 * @date   2009-01-08
 */
class IOInterface : public PluginObject {
	public:

		IOInterface(void (*delObj)(void*));
		virtual ~IOInterface();

		/**
		* @brief A virtual copy constructor used for cloning objects
		*    
		* Example Usage:
		* @code
		* IOInterface *io1,*io2;
		* io1 = new IOHandler("io.ini");
		* io2 = io1->clone(); 
		* @endcode
		* @return A pointer to the cloned object.
		*/
		//virtual IOInterface* clone() const = 0;

		/**
		* @brief A generic function for parsing 2D grids into a Grid2DObject. The string parameter shall be used for addressing the 
		* specific 2D grid to be parsed into the Grid2DObject.
		* @param grid_out A Grid2DObject instance 
		* @param parameter A std::string representing some information for the function on what grid to retrieve
		*/ 
		virtual void read2DGrid(Grid2DObject& grid_out, const std::string& parameter="") = 0;

		/**
		* @brief Parse the DEM (Digital Elevation Model) into the Grid2DObject
		*    
		* Example Usage:
		* @code
		* Grid2DObject dem;
		* IOHandler io1("io.ini");
		* io1.readDEM(dem);
		* @endcode
		* @param dem_out A Grid2DObject that holds the DEM
		*/
		virtual void readDEM(DEMObject& dem_out) = 0;

		/**
		* @brief Parse the landuse model into the Grid2DObject
		*    
		* Example Usage:
		* @code
		* Grid2DObject landuse;
		* IOHandler io1("io.ini");
		* io1.readLanduse(landuse);
		* @endcode
		* @param landuse_out A Grid2DObject that holds the landuse model
		*/
		virtual void readLanduse(Grid2DObject& landuse_out) = 0;

		/**
		* @brief Fill vecStation with StationData objects for a certain date of interest  
		*
		* Example Usage:
		* @code
		* vector<StationData> vecStation;  //empty vector
		* Date d1(2008,06,21,11,00);       //21.6.2008 11:00
		* IOHandler io1("io.ini");
		* io1.readStationData(d1, vecStation);
		* @endcode
		* @param date A Date object representing the date for which the meta data is to be fetched
		* @param vecStation  A vector of StationData objects to be filled with meta data
		*/
		virtual void readStationData(const Date& date, std::vector<StationData>& vecStation) = 0;

		/**
		* @brief Fill vecMeteo and vecStation with a time series of objects  
		* corresponding to the interval indicated by dateStart and dateEnd.
		*
		* Matching rules:
		* - if dateStart and dateEnd are the same: return exact match for date
		* - if dateStart > dateEnd: return first data set with date > dateStart
		* - read in all data starting with dateStart until dateEnd
		* - if there is no data at all then the vectors will be empty, no exception will be thrown
		*    
		* Example Usage:
		* @code
		* vector< vector<MeteoData> > vecMeteo;      //empty vector
		* vector< vector<StationData> > vecStation;  //empty vector
		* Date d1(2008,06,21,11,00);       //21.6.2008 11:00
		* Date d2(2008,07,21,11,00);       //21.7.2008 11:00
		* IOHandler io1("io.ini");
		* io1.readMeteoData(d1, d2, vecMeteo, vecStation);
		* @endcode
		* @param dateStart   A Date object representing the beginning of an interval (inclusive)
		* @param dateEnd     A Date object representing the end of an interval (inclusive)
		* @param vecMeteo    A vector of vector<MeteoData> objects to be filled with data
		* @param vecStation  A vector of vector<StationData> objects to be filled with data
		* @param stationindex (optional) update only the station given by this index
		*/
		virtual void readMeteoData(const Date& dateStart, const Date& dateEnd, 
							  std::vector< std::vector<MeteoData> >& vecMeteo, 
							  std::vector< std::vector<StationData> >& vecStation,
							  const unsigned int& stationindex=IOUtils::npos) = 0;

		/**
		* @brief Write vecMeteo and vecStation time series of objects to a certain destination
		*
		* Example Usage:
		* Configure the io.ini to use a certain plugin for the output:
		* @code
		* METEODEST     = GEOTOP
		* METEODESTPATH = /tmp
		* METEODESTSEQ  = Iprec SWglobal
		* @endcode
		* An example implementation (reading and writing):
		* @code
		* vector< vector<MeteoData> > vecMeteo;      //empty vector
		* vector< vector<StationData> > vecStation;  //empty vector
		* Date d1(2008,06,21,11,00);       //21.6.2008 11:00
		* Date d2(2008,07,21,11,00);       //21.7.2008 11:00
		* IOHandler io1("io.ini");
		* io1.readMeteoData(d1, d2, vecMeteo, vecStation);
		* io1.writeMeteoData(vecMeteo, vecStation)
		* @endcode
		* @param vecMeteo    A vector of vector<MeteoData> objects to be filled with data
		* @param vecStation  A vector of vector<StationData> objects to be filled with data
		* @param name        (optional string) Identifier usefull for the output plugin (it could become part 
		*                    of a file name, a db table, etc) 
		*/
		virtual void writeMeteoData(const std::vector< std::vector<MeteoData> >& vecMeteo, 
							   const std::vector< std::vector<StationData> >& vecStation,
							   const std::string& name="") = 0;

		/**
		* @brief Parse the assimilation data into a Grid2DObject for a certain date represented by the Date object
		*    
		* Example Usage:
		* @code
		* Grid2DObject adata;
		* Date d1(2008,06,21,11,00);       //21.6.2008 11:00
		* IOHandler io1("io.ini");
		* io1.readAssimilationData(d1, adata);
		* @endcode
		* @param date_in A Date object representing the date of the assimilation data
		* @param da_out  A Grid2DObject that holds the assimilation data for every grid point
		*/
		virtual void readAssimilationData(const Date& date_in, Grid2DObject& da_out) = 0;

		/**
		* @brief Read a list of points by their grid coordinates
		* This allows for example to get a list of points where to produce more detailed outputs.
		* @param pts (std::vector<Coords>) A vector of points coordinates
		*/
		virtual void readSpecialPoints(std::vector<Coords>& pts) = 0;

		/**
		* @brief Write a Grid2DObject
		* @param grid_in (Grid2DObject) The grid to write
		* @param name (string) Identifier usefull for the output plugin (it could become part of a file name, a db table, etc)
		*/
		virtual void write2DGrid(const Grid2DObject& grid_in, const std::string& name="") = 0;
};
} //end namespace

#endif
