/***********************************************************************************/
/*  Copyright 2020 ALPsolut.eu                                                     */
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
#include <meteoio/FileUtils.h>
#include <meteoio/meteoStats/libinterpol2D.h>
#include <meteoio/spatialInterpolations/SnowlineAlgorithm.h>   

#include <fstream>
#include <limits>

namespace mio {

SnowlineAlgorithm::SnowlineAlgorithm(const std::vector< std::pair<std::string, std::string> >& vecArgs,
    const std::string& i_algo, const std::string& i_param, TimeSeriesManager& i_tsm,
    GridsManager& i_gdm, Meteo2DInterpolator& i_mi) : 
    InterpolationAlgorithm(vecArgs, i_algo, i_param, i_tsm), gdm(i_gdm), mi(i_mi), base_alg("IDW_LAPSE"),
    snowline(IOUtils::nodata), assim_method(CUTOFF), snowline_file(), where("Interpolations2D::" + algo),
    cutoff_val(0.), band_height(10.), band_no(10),
    quiet(false)
{
	std::string algo_info( "CUTOFF" );
	for (size_t ii = 0; ii < vecArgs.size(); ii++) {
		if (vecArgs[ii].first == "BASE") {
			base_alg = IOUtils::strToUpper(vecArgs[ii].second);
		} else if (vecArgs[ii].first == "SNOWLINE") {
			IOUtils::parseArg(vecArgs[ii], where, snowline);
		} else if (vecArgs[ii].first == "SNOWLINEFILE") {
			snowline_file = vecArgs[ii].second;
		} else if (vecArgs[ii].first == "METHOD") {
			const std::string mode( IOUtils::strToUpper(vecArgs[ii].second) );
			if (mode == "CUTOFF")
				assim_method = CUTOFF;
			else if (mode == "BANDS")
				assim_method = BANDS;
			else
				throw InvalidArgumentException("Snowline assimilation mode \"" + mode +
				    "\" supplied for " + where + " not known.", AT);
			algo_info = mode;
		} else if (vecArgs[ii].first == "QUIET") {
			IOUtils::parseArg(vecArgs[ii], where, quiet);
		/* args of method CUTOFF */
		} else if (vecArgs[ii].first == "SET") {
			IOUtils::parseArg(vecArgs[ii], where, cutoff_val);
		/* args of method BANDS */
		} else if (vecArgs[ii].first == "BAND_HEIGHT") {
			IOUtils::parseArg(vecArgs[ii], where, band_height);
		} else if (vecArgs[ii].first == "BAND_NO") {
			IOUtils::parseArg(vecArgs[ii], where, band_no);
		}
	}
	info << "method: " << algo_info << ", ";
}

double SnowlineAlgorithm::getQualityRating(const Date& i_date)
{
	date = i_date; //this is always called before calculate() and sets the date!
	nrOfMeasurments = getData(date, param, vecData, vecMeta); //more initialization
	if (nrOfMeasurments == 0)
		return 0.0;
	if (snowline != IOUtils::nodata) //TODO: possible to propagate from base alg?
		return 0.8;
	else
		return 0.7;
}

void SnowlineAlgorithm::calculate(const DEMObject& dem, Grid2DObject& grid)
{
	getSnowline();
	baseInterpol(dem, grid);
	
	if (snowline == IOUtils::nodata) //we already gave notice for this
		return; 

	if (assim_method == CUTOFF)
		assimilateCutoff(dem, grid);
	else if (assim_method == BANDS)
		assimilateBands(dem, grid);
}

void SnowlineAlgorithm::baseInterpol(const DEMObject& dem, Grid2DObject& grid)
{
	std::string base_alg_fallback( base_alg ); //fallback algorithm if only 1 station is used
	if (nrOfMeasurments == 1) {
		base_alg_fallback = "AVG";
		msg("[W] Falling back to \"AVG\" for " + where + " (insufficient number of stations on " +
		    date.toString(Date::ISO_DATE) + ").");
	}
	//flexibly read parameters for base algorithm:
	const std::vector< std::pair<std::string, std::string> >
	    vecArgs( mi.getArgumentsForAlgorithm(param, base_alg_fallback, "Interpolations2D") );
	InterpolationAlgorithm* algorithm(
	    AlgorithmFactory::getAlgorithm(base_alg_fallback, mi, vecArgs, tsmanager, gdm, param) );
	//apply base interpolation algorithm to whole grid:
	algorithm->getQualityRating(date); //set date
	algorithm->calculate(dem, grid);
	info << algorithm->getInfo();
	delete algorithm;
}

void SnowlineAlgorithm::assimilateCutoff(const DEMObject& dem, Grid2DObject& grid)
{ //set everything below snowlin elevation to zero
	for (size_t ii = 0; ii < grid.getNx(); ii++) {
		for (size_t jj = 0; jj < grid.getNy(); jj++) {
			if (dem(ii, jj) == IOUtils::nodata)
				continue;
			if (dem(ii, jj) < snowline)
				grid(ii, jj) = cutoff_val;
		}
	}
}

void SnowlineAlgorithm::assimilateBands(const DEMObject& dem, Grid2DObject& grid)
{ //multiply elevation bands above snowline with factors from 0 to 1
	for (size_t ii = 0; ii < grid.getNx(); ii++) {
		for (size_t jj = 0; jj < grid.getNy(); jj++) {
			if (dem(ii, jj) == IOUtils::nodata) {
				continue;
			} else if (dem(ii, jj) > snowline + band_no * band_height) {
				continue;
			} else if (dem(ii, jj) < snowline) {
				grid(ii, jj) = 0;
				continue;
			}
			for (unsigned int bb = 0; bb < band_no; ++bb) { //bin DEM into bands
				if ( (dem(ii, jj) >= snowline + bb * band_height) && (dem(ii, jj) < snowline + (bb + 1.) * band_height) )
					grid(ii, jj) = grid(ii, jj) * bb / band_no;
			}
		}
	}
}

double SnowlineAlgorithm::readSnowlineFile()
{
	std::ifstream fin( snowline_file.c_str() );
	if (fin.fail())
		return IOUtils::nodata;
	
	double snowline_elevation;
	const char eoln = FileUtils::getEoln(fin); //get the end of line character for the file
	try {
		do {
			std::string line;
			getline(fin, line, eoln);
			IOUtils::stripComments(line);
			IOUtils::trim(line);
			if (line.empty()) continue; //skip comments

			std::istringstream iss( line );
			iss.setf(std::ios::fixed);
			iss.precision(std::numeric_limits<double>::digits10);
			iss >> std::skipws >> snowline_elevation;
			if (!iss) 
				return IOUtils::nodata;
		} while (!fin.eof());
		fin.close();
	} catch (const std::exception&){
		if (fin.is_open()) fin.close();
		throw;
	}
	return snowline_elevation;
}

void SnowlineAlgorithm::getSnowline() 
{
	if (!snowline_file.empty()) {
		if (snowline != IOUtils::nodata) {
			msg("[i] Ignoring additional SNOWLINE argument since SNOWFILE is given for " + where + ".");
		}
		snowline = readSnowlineFile();
		if (snowline == IOUtils::nodata) {
			msg("[W] No valid snowline elevation can be read from SNOWFILE for " +
			    where + ". Continuing without...");
		}
	} else {
		if (snowline == IOUtils::nodata) {
			msg("[W] No numeric value found for SNOWLINE, no SNOWFILE provided either for " +
			    where + ". Continuing without...");
		}
	}
}

void SnowlineAlgorithm::msg(const std::string& message)
{
	if (!quiet)
		std::cerr << message << std::endl;
}

} //namespace
