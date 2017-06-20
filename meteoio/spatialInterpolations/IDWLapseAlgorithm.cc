/***********************************************************************************/
/*  Copyright 2013 WSL Institute for Snow and Avalanche Research    SLF-DAVOS      */
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

#include <meteoio/spatialInterpolations/IDWLapseAlgorithm.h>
#include <meteoio/meteoStats/libinterpol2D.h>

namespace mio {

IDWLapseAlgorithm::IDWLapseAlgorithm(Meteo2DInterpolator& i_mi, const std::vector< std::pair<std::string, std::string> >& vecArgs,
					const std::string& i_algo, TimeSeriesManager& i_tsmanager, GridsManager& i_gridsmanager, const std::string& i_param)
			: InterpolationAlgorithm(i_mi, vecArgs, i_algo, i_tsmanager, i_gridsmanager, i_param), scale(1e3), alpha(1.)
{
	setTrendParams(vecArgs);

	for (size_t ii=0; ii<vecArgs.size(); ii++) {
		if (vecArgs[ii].first=="SCALE") {
			parseArg(vecArgs[ii], scale);
		} else if (vecArgs[ii].first=="ALPHA") {
			parseArg(vecArgs[ii], alpha);
		}
	}
}

double IDWLapseAlgorithm::getQualityRating(const Date& i_date)
{
	date = i_date;
	nrOfMeasurments = getData(date, param, vecData, vecMeta);

	if (nrOfMeasurments == 0) return 0.0;
	if (user_lapse==IOUtils::nodata && nrOfMeasurments<2) return 0.0;

	return 0.7;
}

void IDWLapseAlgorithm::calculate(const DEMObject& dem, Grid2DObject& grid)
{
	info.clear(); info.str("");
	const std::vector<double> vecAltitudes( getStationAltitudes(vecMeta) );
	if (vecAltitudes.empty())
		throw IOException("Not enough data for spatially interpolating parameter " + param, AT);

	const Fit1D trend( getTrend(vecAltitudes, vecData) );
	info << trend.getInfo();
	detrend(trend, vecAltitudes, vecData);
	Interpol2D::IDW(vecData, vecMeta, dem, grid, scale, alpha); //the meta should NOT be used for elevations!
	retrend(dem, trend, grid);
}

} //namespace
