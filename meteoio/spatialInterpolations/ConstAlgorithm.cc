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

#include <meteoio/spatialInterpolations/ConstAlgorithm.h>
#include <meteoio/meteoStats/libinterpol2D.h>

namespace mio {

ConstAlgorithm::ConstAlgorithm(Meteo2DInterpolator& i_mi, const std::vector< std::pair<std::string, std::string> >& vecArgs,
                           const std::string& i_algo, TimeSeriesManager& i_tsmanager, GridsManager& i_gridsmanager, const std::string& i_param)
                           : InterpolationAlgorithm(i_mi, vecArgs, i_algo, i_tsmanager, i_gridsmanager, i_param), user_cst(0.)
{
	bool has_cst=false;

	for (size_t ii=0; ii<vecArgs.size(); ii++) {
		if (vecArgs[ii].first=="VALUE") {
			parseArg(vecArgs[ii], user_cst);
			has_cst = true;
		}
	}

	if (!has_cst) throw InvalidArgumentException("Please provide a value for the "+algo+" algorithm", AT);
}

double ConstAlgorithm::getQualityRating(const Date& i_date)
{
	date = i_date;

	return 0.01;
}

void ConstAlgorithm::calculate(const DEMObject& dem, Grid2DObject& grid)
{
	Interpol2D::constant(user_cst, dem, grid);
}

} //namespace
