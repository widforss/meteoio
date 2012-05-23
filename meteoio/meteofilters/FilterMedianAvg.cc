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
#include <meteoio/meteofilters/FilterMedianAvg.h>
#include <cmath>

using namespace std;

namespace mio {

FilterMedianAvg::FilterMedianAvg(const std::vector<std::string>& vec_args) : WindowedFilter("MEAN_AVG")
{
	parse_args(vec_args);

	//This is safe, but maybe too imprecise:
	properties.time_before = min_time_span;
	properties.time_after  = min_time_span;
	properties.points_before = min_data_points;
	properties.points_after = min_data_points;
}

void FilterMedianAvg::process(const unsigned int& param, const std::vector<MeteoData>& ivec,
                              std::vector<MeteoData>& ovec)
{
	ovec.clear();
	ovec.reserve(ivec.size());
	size_t start, end;

	for (size_t ii=0; ii<ivec.size(); ii++){ //for every element in ivec, get a window
		ovec.push_back(ivec[ii]);
		double& value = ovec[ii](param);

		if( get_window_specs(ii, ivec, start, end) ) {
			value = calc_median(ivec, param, start, end);
		}
	}
}

double FilterMedianAvg::calc_median(const std::vector<MeteoData>& ivec, const unsigned int& param, const size_t& start, const size_t& end)
{
	vector<double> vecTemp;
	for(size_t ii=start; ii<=end; ii++){ //get rid of nodata elements
		const double& value = ivec[ii](param);
		if (value != IOUtils::nodata)
			vecTemp.push_back(value);
	}

	const size_t size_of_vec = vecTemp.size();
	if (size_of_vec == 0)
		return IOUtils::nodata;

	const int middle = (int)(size_of_vec/2);
	nth_element(vecTemp.begin(), vecTemp.begin()+middle, vecTemp.end());

	if ((size_of_vec % 2) == 1){ //uneven
		return *(vecTemp.begin()+middle);
	} else { //use arithmetic mean of element n/2 and n/2-1
		return Interpol1D::weightedMean( *(vecTemp.begin()+middle), *(vecTemp.begin()+middle-1), 0.5);
	}
}

void FilterMedianAvg::parse_args(std::vector<std::string> vec_args)
{
	vector<double> filter_args;

	if (vec_args.size() > 2){
		is_soft = FilterBlock::is_soft(vec_args);
	}

	if (vec_args.size() > 2)
		centering = (WindowedFilter::Centering)WindowedFilter::get_centering(vec_args);

	convert_args(2, 2, vec_args, filter_args);

	if ((filter_args[0] < 1) || (filter_args[1] < 0)){
		throw InvalidArgumentException("Invalid window size configuration for filter " + getName(), AT);
	}

	min_data_points = (unsigned int)floor(filter_args[0]);
	min_time_span = Duration(filter_args[1] / 86400.0, 0.);
}

} //namespace
