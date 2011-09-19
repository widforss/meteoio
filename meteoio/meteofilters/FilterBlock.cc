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
#include <meteoio/meteofilters/FilterBlock.h>

namespace mio {

FilterBlock::FilterBlock(const std::string& filter_name) : ProcessingBlock(filter_name) {

}

FilterBlock::~FilterBlock() {}

bool FilterBlock::is_soft(std::vector<std::string>& vec_args) {
	if (vec_args.size() > 0){
		if (vec_args[0] == "soft"){
			vec_args.erase(vec_args.begin());
			return true;
		}
	}
	
	return false;
}

void FilterBlock::convert_args(const unsigned int& min_nargs, const unsigned int& max_nargs,
                               const std::vector<std::string>& vec_args, std::vector<double>& dbl_args)
{
	if ((vec_args.size() < min_nargs) || (vec_args.size() > max_nargs))
		throw InvalidArgumentException("Wrong number of arguments for filter " + getName(), AT); 

	for (unsigned int ii=0; ii<vec_args.size(); ii++){
		double tmp = IOUtils::nodata;
		IOUtils::convertString(tmp, vec_args[ii]);
		dbl_args.push_back(tmp);
	}
}

void FilterBlock::extract_dbl_vector(const unsigned int& index, const std::vector<MeteoData>& ivec,
                                     std::vector<double>& ovec)
{
	ovec.clear();
	for(unsigned int ii=0; ii<ivec.size(); ii++) {
		ovec.push_back( ivec[ii](index) );
	}
}

void FilterBlock::extract_dbl_vector(const unsigned int& index, const std::vector<const MeteoData*>& ivec,
                                     std::vector<double>& ovec)
{
	ovec.clear();
	for(unsigned int ii=0; ii<ivec.size(); ii++) {
		const double& value = (*ivec[ii])(index);
		ovec.push_back( value );
	}
}

} //end namespace
