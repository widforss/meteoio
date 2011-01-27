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
#include <meteoio/meteofilters/ProcessingBlock.h>
#include <meteoio/meteofilters/FilterMinMax.h>
#include <meteoio/meteofilters/FilterMeanAvg.h>
#include <meteoio/meteofilters/FilterMedianAvg.h>
#include <meteoio/meteofilters/RateFilter.h>

namespace mio {

std::set<std::string> BlockFactory::availableBlocks;
const bool BlockFactory::__init = BlockFactory::initStaticData();

bool BlockFactory::initStaticData()
{
	availableBlocks.insert("MIN_MAX");
	availableBlocks.insert("MEAN_AVG");
	availableBlocks.insert("MEDIAN_AVG");
	availableBlocks.insert("RATE");

	return true;
}

ProcessingBlock* BlockFactory::getBlock(const std::string& blockname, const std::vector<std::string>& vec_args)
{
	//Check whether algorithm theoretically exists
	if (availableBlocks.find(blockname) == availableBlocks.end())
		throw UnknownValueException("The processing block '"+blockname+"' does not exist" , AT);

	if (blockname == "MIN_MAX"){
		return new FilterMinMax(vec_args);
	} else if (blockname == "MEAN_AVG"){
		return new FilterMeanAvg(vec_args);
	} else if (blockname == "MEDIAN_AVG"){
		return new FilterMedianAvg(vec_args);
	} else if (blockname == "RATE"){
		return new RateFilter(vec_args);
	} else {
		throw IOException("The processing block '"+blockname+"' does not exist" , AT);		
	}

	return NULL;
}

ProcessingBlock::ProcessingBlock(const std::string& name) : block_name(name)
{}

std::string ProcessingBlock::getName() const
{
	return block_name;
}

ProcessingBlock::~ProcessingBlock() {}

const ProcessingProperties& ProcessingBlock::getProperties() const
{
	return properties;
}

} //end namespace
