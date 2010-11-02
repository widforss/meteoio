/***********************************************************************************/
/*  Copyright 2010 WSL Institute for Snow and Avalanche Research    SLF-DAVOS      */
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
#include <meteoio/InterpolationAlgorithms.h>

using namespace std;

namespace mio {

std::set<std::string> AlgorithmFactory::setAlgorithms;
const bool AlgorithmFactory::__init = AlgorithmFactory::initStaticData();

bool AlgorithmFactory::initStaticData()
{
	/* 
	 * Keywords for selecting the spatial interpolation algorithm among the 
	 * available methods for single source and multiple sources interpolations. 
	 * More details about some of these algorithms can be found in "A Meteorological 
	 * Distribution System for High-Resolution Terrestrial Modeling (MicroMet)", Liston and Elder, 2006.
	 * Please don't forget to document InterpolationAlgorithms.h for the available algorithms!
	 */

	setAlgorithms.insert("CST");       // constant fill
	setAlgorithms.insert("STD_PRESS"); // standard air pressure interpolation
	setAlgorithms.insert("CST_LAPSE"); // constant fill with an elevation lapse rate
	setAlgorithms.insert("IDW");       // Inverse Distance Weighting fill
	setAlgorithms.insert("IDW_LAPSE"); // Inverse Distance Weighting with an elevation lapse rate fill
	setAlgorithms.insert("LIDW_LAPSE"); // Inverse Distance Weighting with an elevation lapse rate fill, restricted to a local scale
	setAlgorithms.insert("RH");        // relative humidity interpolation
	setAlgorithms.insert("WIND_CURV"); // wind velocity interpolation (using a heuristic terrain effect)
	setAlgorithms.insert("HNW_SNOW"); // precipitation interpolation according to (Magnusson, 2010)
	setAlgorithms.insert("ODKRIG"); // ordinary kriging
	setAlgorithms.insert("USER"); // read user provided grid

	return true;
}

//HACK: do not build a new object at every time step!!
InterpolationAlgorithm* AlgorithmFactory::getAlgorithm(const std::string& _algoname, 
                                                       const Meteo2DInterpolator& _mi,
                                                       const DEMObject& _dem,
                                                       const std::vector<MeteoData>& _vecMeteo,
                                                       const std::vector<StationData>& _vecStation,
                                                       const std::vector<std::string>& _vecArgs)
{
	std::string algoname(_algoname);
	IOUtils::toUpper(algoname);

	//Check whether algorithm theoretically exists
	if (setAlgorithms.find(algoname) == setAlgorithms.end())
		throw UnknownValueException("The interpolation algorithm '"+algoname+"' does not exist" , AT);

	if (algoname == "CST"){
		return new ConstAlgorithm(_mi, _dem, _vecMeteo, _vecStation, _vecArgs, _algoname);
	} else if (algoname == "STD_PRESS"){
		return new StandardPressureAlgorithm(_mi, _dem, _vecMeteo, _vecStation, _vecArgs, _algoname);
	} else if (algoname == "CST_LAPSE"){
		return new ConstLapseRateAlgorithm(_mi, _dem, _vecMeteo, _vecStation, _vecArgs, _algoname);
	} else if (algoname == "IDW"){
		return new IDWAlgorithm(_mi, _dem, _vecMeteo, _vecStation, _vecArgs, _algoname);
	} else if (algoname == "IDW_LAPSE"){
		return new IDWLapseAlgorithm(_mi, _dem, _vecMeteo, _vecStation, _vecArgs, _algoname);
	} else if (algoname == "LIDW_LAPSE"){
		return new LocalIDWLapseAlgorithm(_mi, _dem, _vecMeteo, _vecStation, _vecArgs, _algoname);
	} else if (algoname == "RH"){
		return new RHAlgorithm(_mi, _dem, _vecMeteo, _vecStation, _vecArgs, _algoname);
	} else if (algoname == "WIND_CURV"){
		return new SimpleWindInterpolationAlgorithm(_mi, _dem, _vecMeteo, _vecStation, _vecArgs, _algoname);
	} else if (algoname == "ODKRIG"){
		return new OrdinaryKrigingAlgorithm(_mi, _dem, _vecMeteo, _vecStation, _vecArgs, _algoname);
	} else if (algoname == "USER"){
		return new USERInterpolation(_mi, _dem, _vecMeteo, _vecStation, _vecArgs, _algoname);
	} else if (algoname == "HNW_SNOW"){
		return new SnowHNWInterpolation(_mi, _dem, _vecMeteo, _vecStation, _vecArgs, _algoname);
	} else {
		throw IOException("The interpolation algorithm '"+algoname+"' is not implemented" , AT);
	}

	return NULL;
}

unsigned int InterpolationAlgorithm::getData(const MeteoData::Parameters& param, 
                                             std::vector<double>& vecData) const
{
	vector<StationData> vecMeta;
	return getData(param, vecData, vecMeta);
	
}

unsigned int InterpolationAlgorithm::getData(const MeteoData::Parameters& param, 
                                             std::vector<double>& vecData, std::vector<StationData>& vecMeta) const
{
	if(vecData.size()>0) {
		vecData.clear();
	}
	if(vecMeta.size()>0) {
		vecMeta.clear();
	}
	for (unsigned int ii=0; ii<vecMeteo.size(); ii++){
		const double& val = vecMeteo[ii].param(param);
		if (val != IOUtils::nodata){
			vecData.push_back(val);
			vecMeta.push_back(vecStation[ii]);
		}
	}

	return vecData.size();
}

unsigned int InterpolationAlgorithm::getStationAltitudes(const std::vector<StationData>& vecMeta,
                                                         std::vector<double>& vecData) const
{
	for (unsigned int ii=0; ii<vecMeta.size(); ii++){
		const double& val = vecMeta[ii].position.getAltitude();
		if (val != IOUtils::nodata)
			vecData.push_back(val);
	}

	return vecData.size();
}

/**
 * @brief Return an information string about the interpolation process
 * @return string containing some information (algorithm used, number of stations)
*/
std::string InterpolationAlgorithm::getInfo() const
{
	std::stringstream os;
	os << algo << ", " << nrOfMeasurments;
	if(nrOfMeasurments==1)
		os << " station";
	else
		os << " stations";
	std::string tmp = info.str();
	if(!tmp.empty()) {
		os << ", " << tmp;
	}
	return os.str();
}

/**********************************************************************************/
void StandardPressureAlgorithm::initialize(const MeteoData::Parameters& in_param) {
	param = in_param;
	nrOfMeasurments = getData(param, vecData, vecMeta);
}

double StandardPressureAlgorithm::getQualityRating()
{
	if (param != MeteoData::P)
		return 0.0;

	if (nrOfMeasurments == 0)
		return 1.0;

	return 0.1;
}

void StandardPressureAlgorithm::calculate(Grid2DObject& grid)
{
	//run algorithm
	Interpol2D::stdPressureGrid2DFill(dem, grid);
}


void ConstAlgorithm::initialize(const MeteoData::Parameters& in_param) {
	param = in_param;
	nrOfMeasurments = getData(param, vecData, vecMeta);
}

double ConstAlgorithm::getQualityRating()
{
	if (nrOfMeasurments == 0){
		return 0.0;
	} else if (nrOfMeasurments == 1){
		return 0.8;
	} else if (nrOfMeasurments > 1){
		return 0.2;
	}

	return 0.0;
}

void ConstAlgorithm::calculate(Grid2DObject& grid)
{
	//check how many data points there are (not including nodata)
	if (nrOfMeasurments == 0)
		throw IOException("Interpolation FAILED for parameter " + MeteoData::getParameterName(param), AT);

	//run algorithm
	Interpol2D::constantGrid2DFill(Interpol1D::arithmeticMean(vecData), dem, grid);
}


void ConstLapseRateAlgorithm::initialize(const MeteoData::Parameters& in_param) {
	param = in_param;
	nrOfMeasurments = getData(param, vecData, vecMeta);
}

double ConstLapseRateAlgorithm::getQualityRating()
{
	if (nrOfMeasurments == 0){
		return 0.0;
	} else if (nrOfMeasurments == 1){
		if (vecArgs.size() > 0)
			return 0.9; //the laspe rate is provided
		else
			return 0.0; //no lapse rate is provided and it can not be computed
	} else if (nrOfMeasurments == 2){
		//in any case, we can do at least as good as IDW_LAPSE
		return 0.71;
	} else if (nrOfMeasurments>2){
		return 0.2;
	}

	return 0.2;
}

void ConstLapseRateAlgorithm::calculate(Grid2DObject& grid)
{
	vector<double> vecAltitudes;

	getStationAltitudes(vecMeta, vecAltitudes);
	LapseRateProjectPtr funcptr = &Interpol2D::LinProject;

	if ((vecAltitudes.size() == 0) || (nrOfMeasurments == 0))
		throw IOException("Interpolation FAILED for parameter " + MeteoData::getParameterName(param), AT);

	double avgAltitudes = Interpol1D::arithmeticMean(vecAltitudes);
	double avgData = Interpol1D::arithmeticMean(vecData);

	//Set regression coefficients
	std::vector<double> vecCoefficients;
	vecCoefficients.resize(4, 0.0);

	//Get the optional arguments for the algorithm: lapse rate, lapse rate usage
	if (vecArgs.size() == 0) {
		Interpol2D::LinRegression(vecAltitudes, vecData, vecCoefficients);
	} else if (vecArgs.size() == 1) {
		IOUtils::convertString(vecCoefficients[1], vecArgs[0]);
	} else if (vecArgs.size() == 2) {
		std::string extraArg;
		IOUtils::convertString(extraArg, vecArgs[1]);
		if(extraArg=="soft") { //soft
			if(Interpol2D::LinRegression(vecAltitudes, vecData, vecCoefficients) != EXIT_SUCCESS) {
				vecCoefficients.assign(4, 0.0);
				IOUtils::convertString(vecCoefficients[1], vecArgs[0]);
			}
		} else if(extraArg=="frac") {
			funcptr = &Interpol2D::FracProject;
			IOUtils::convertString(vecCoefficients[1], vecArgs[0]);
		} else {
			std::stringstream os;
			os << "Unknown argument \"" << extraArg << "\" supplied for the CST_LAPSE algorithm";
			throw InvalidArgumentException(os.str(), AT);
		}
	} else { //incorrect arguments, throw an exception
		throw InvalidArgumentException("Wrong number of arguments supplied for the CST_LAPSE algorithm", AT);
	}

	//run algorithm
	info << "r^2=" << IOUtils::pow2( vecCoefficients[3] );
	Interpol2D::constantLapseGrid2DFill(avgData, avgAltitudes, dem, vecCoefficients, funcptr, grid);
}


void IDWAlgorithm::initialize(const MeteoData::Parameters& in_param) {
	param = in_param;
	nrOfMeasurments = getData(param, vecData, vecMeta);
}

double IDWAlgorithm::getQualityRating()
{
	if (nrOfMeasurments == 0){
		return 0.0;
	} else if (nrOfMeasurments == 1){
		return 0.3;
	} else if (nrOfMeasurments > 1){
		return 0.5;
	}

	return 0.2;
}

void IDWAlgorithm::calculate(Grid2DObject& grid)
{
	if (nrOfMeasurments == 0)
		throw IOException("Interpolation FAILED for parameter " + MeteoData::getParameterName(param), AT);

	//run algorithm
	Interpol2D::IDW(vecData, vecMeta, dem, grid);
}


void IDWLapseAlgorithm::initialize(const MeteoData::Parameters& in_param) {
	param = in_param;
	nrOfMeasurments = getData(param, vecData, vecMeta);
}

double IDWLapseAlgorithm::getQualityRating()
{
	if (nrOfMeasurments == 0)
		return 0.0;

	return 0.7;
}

void IDWLapseAlgorithm::calculate(Grid2DObject& grid)
{	const double thresh_r_fixed_rate = 0.6;
	vector<double> vecAltitudes;

	if (nrOfMeasurments == 0)
		throw IOException("Interpolation FAILED for parameter " + MeteoData::getParameterName(param), AT);

	getStationAltitudes(vecMeta, vecAltitudes);
	LapseRateProjectPtr funcptr = &Interpol2D::LinProject;

	//Set regression coefficients
	std::vector<double> vecCoefficients;
	vecCoefficients.resize(4, 0.0);

	//Get the optional arguments for the algorithm: lapse rate, lapse rate usage
	if (vecArgs.size() == 0) { //force compute lapse rate
		Interpol2D::LinRegression(vecAltitudes, vecData, vecCoefficients);
	} else if (vecArgs.size() == 1) { //force lapse rate
		IOUtils::convertString(vecCoefficients[1], vecArgs[0]);
	} else if (vecArgs.size() == 2) {
		std::string extraArg;
		IOUtils::convertString(extraArg, vecArgs[1]);
		if(extraArg=="soft") { //soft
			if((Interpol2D::LinRegression(vecAltitudes, vecData, vecCoefficients) != EXIT_SUCCESS) ||
			   (vecCoefficients[3]<thresh_r_fixed_rate) ) { //because we return |r|
				vecCoefficients.assign(4, 0.0);
				IOUtils::convertString(vecCoefficients[1], vecArgs[0]);
			}
		} else if(extraArg=="frac") {
			funcptr = &Interpol2D::FracProject;
			IOUtils::convertString(vecCoefficients[1], vecArgs[0]);
		} else {
			std::stringstream os;
			os << "Unknown argument \"" << extraArg << "\" supplied for the IDW_LAPSE algorithm";
			throw InvalidArgumentException(os.str(), AT);
		}
	} else { //incorrect arguments, throw an exception
		throw InvalidArgumentException("Wrong number of arguments supplied for the IDW_LAPSE algorithm", AT);
	}

	//run algorithm
	info << "r^2=" << IOUtils::pow2( vecCoefficients[3] );
	Interpol2D::LapseIDW(vecData, vecMeta, dem, vecCoefficients, funcptr, grid);
}


void LocalIDWLapseAlgorithm::initialize(const MeteoData::Parameters& in_param) {
	param = in_param;
	nrOfMeasurments = getData(param, vecData, vecMeta);
}

double LocalIDWLapseAlgorithm::getQualityRating()
{
	if (nrOfMeasurments == 0)
		return 0.0;

	return 0.7;
}

void LocalIDWLapseAlgorithm::calculate(Grid2DObject& grid)
{
	unsigned int nrOfNeighbors=0;

	if (nrOfMeasurments == 0)
		throw IOException("Interpolation FAILED for parameter " + MeteoData::getParameterName(param), AT);

	//Set regression coefficients
	std::vector<double> vecCoefficients;
	vecCoefficients.resize(4, 0.0);

	//Get the optional arguments for the algorithm: lapse rate, lapse rate usage
	if (vecArgs.size() == 1) { //compute lapse rate on a reduced data set
		IOUtils::convertString(nrOfNeighbors, vecArgs[0]);
	} else { //incorrect arguments, throw an exception
		throw InvalidArgumentException("Wrong number of arguments supplied for the IDW_LAPSE algorithm", AT);
	}

	//run algorithm
	double r2=0.;
	Interpol2D::LocalLapseIDW(vecData, vecMeta, dem, nrOfNeighbors, grid, r2);
	info << "r^2=" << IOUtils::pow2( r2 );
}


void RHAlgorithm::initialize(const MeteoData::Parameters& in_param) {
	param = in_param;

	nrOfMeasurments = 0;
	for (unsigned int ii=0; ii<vecMeteo.size(); ii++){
		if ((vecMeteo[ii].rh != IOUtils::nodata) && (vecMeteo[ii].ta != IOUtils::nodata)){
			vecDataTA.push_back(vecMeteo[ii].ta);
			vecDataRH.push_back(vecMeteo[ii].rh);
			vecMeta.push_back(vecStation[ii]);
			nrOfMeasurments++;
		}
	}
}

double RHAlgorithm::getQualityRating()
{
	//This algorithm is only valid for RH
	if (param != MeteoData::RH)
		return 0.0;

	if (vecDataTA.size() == 0)
		return 0.0;
	if( ( nrOfMeasurments<(unsigned int)(0.5*vecDataRH.size()) ) || ( nrOfMeasurments<2 ) )
		return 0.6;

	return 0.9;
}

void RHAlgorithm::calculate(Grid2DObject& grid)
{		
	//This algorithm is only valid for RH
	if (param != MeteoData::RH)
		throw IOException("Interpolation FAILED for parameter " + MeteoData::getParameterName(param), AT);

	vector<double> vecAltitudes;
	getStationAltitudes(vecMeta, vecAltitudes);

	if (vecDataTA.size() == 0) //No matching data
		throw IOException("Interpolation FAILED for parameter " + MeteoData::getParameterName(param), AT);

	Grid2DObject ta;
	mi.interpolate(MeteoData::TA, ta); //get TA interpolation from call back to Meteo2DInterpolator

	//here, RH->Td, interpolations, Td->RH
	std::vector<double> vecTd(vecDataRH.size(), 0.0); // init to 0.0

	//Compute dew point temperatures at stations
	for (unsigned int ii=0; ii<vecDataRH.size(); ii++){
		vecTd[ii] = Interpol2D::RhtoDewPoint(vecDataRH[ii], vecDataTA[ii], 1);
	}
			
	//Krieging on Td
	std::vector<double> vecCoefficients;
	vecCoefficients.resize(4, 0.0);

	//run algorithm
	Interpol2D::LinRegression(vecAltitudes, vecTd, vecCoefficients);
	Interpol2D::LapseIDW(vecTd, vecMeta, dem, vecCoefficients, &Interpol2D::LinProject, grid);

	//Recompute Rh from the interpolated td
	for (unsigned int jj=0; jj<grid.nrows; jj++) {
		for (unsigned int ii=0; ii<grid.ncols; ii++) {
			grid.grid2D(ii,jj) = Interpol2D::DewPointtoRh(grid.grid2D(ii,jj), ta.grid2D(ii,jj), 1);
		}
	}
}


void SimpleWindInterpolationAlgorithm::initialize(const MeteoData::Parameters& in_param) {
	param = in_param;

	nrOfMeasurments = 0;
	for (unsigned int ii=0; ii<vecMeteo.size(); ii++){
		if ((vecMeteo[ii].vw != IOUtils::nodata) && (vecMeteo[ii].dw != IOUtils::nodata)){
			vecDataVW.push_back(vecMeteo[ii].vw);
			vecDataDW.push_back(vecMeteo[ii].dw);
			vecMeta.push_back(vecStation[ii]);
			nrOfMeasurments++;
		}
	}
}

double SimpleWindInterpolationAlgorithm::getQualityRating()
{
	//This algorithm is only valid for VW
	if (param != MeteoData::VW)
		return 0.0;

	//This algorithm requires the curvatures
	unsigned int nx=0, ny=0;
	dem.curvature.size(nx,ny);
	if (nx==0 || ny==0) {
		std::cerr << "[W] WIND_CURV spatial interpolations algorithm selected but no dem curvature available! Skipping algorithm...\n";
		return 0.0;
	}

	if (vecDataVW.size() == 0)
		return 0.0;
	if( ( nrOfMeasurments<(unsigned int)(0.5*vecDataVW.size()) ) || ( nrOfMeasurments<2 ) )
		return 0.6;

	return 0.9;
}

void SimpleWindInterpolationAlgorithm::calculate(Grid2DObject& grid)
{		
	//This algorithm is only valid for VW
	if (param != MeteoData::VW)
		throw IOException("Interpolation FAILED for parameter " + MeteoData::getParameterName(param), AT);

	vector<double> vecAltitudes;

	getStationAltitudes(vecMeta, vecAltitudes);

	if( vecDataDW.size() == 0) 
		throw IOException("Interpolation FAILED for parameter " + MeteoData::getParameterName(param), AT);

	Grid2DObject dw;
	mi.interpolate(MeteoData::DW, dw); //get DW interpolation from call back to Meteo2DInterpolator
	
	//Krieging
	std::vector<double> vecCoefficients;
	vecCoefficients.resize(4, 0.0);
	
	Interpol2D::LinRegression(vecAltitudes, vecDataVW, vecCoefficients);
	Interpol2D::LapseIDW(vecDataVW, vecMeta, dem, vecCoefficients, &Interpol2D::LinProject, grid);
	Interpol2D::SimpleDEMWindInterpolate(dem, grid, dw);
}


void USERInterpolation::initialize(const MeteoData::Parameters& in_param) {
	param = in_param;
}

std::string USERInterpolation::getGridFileName()
{
	const std::string ext=std::string(".asc");
	if (vecArgs.size() != 1){
		throw InvalidArgumentException("Please provide the path to the grids for the USER interpolation algorithm", AT);
	}
	const std::string& grid_path = vecArgs[0];
	std::string gridname = grid_path + std::string("/");

	if(vecMeteo.size()>0) {
		const Date& timestep = vecMeteo.at(0).date;
		gridname = gridname + timestep.toString(Date::NUM) + std::string("_") + MeteoData::getParameterName(param) + ext;
	} else {
		gridname = gridname + std::string("Default") + std::string("_") + MeteoData::getParameterName(param) + ext;
	}
	
	return gridname;
}

double USERInterpolation::getQualityRating()
{
	const std::string filename = getGridFileName();

	if (!IOUtils::validFileName(filename)) {
		std::cout << "[E] Invalid grid filename for USER interpolation algorithm: " << filename << "\n";
		return 0.0;
	}
	if(IOUtils::fileExists(filename)) {
		return 1.0;
	} else {
		return 0.0;
	}
}

void USERInterpolation::calculate(Grid2DObject& /*grid*/)
{
	const std::string filename = getGridFileName();
	nrOfMeasurments = 0;

	//read2DGrid(grid, filename);
	throw IOException("USER interpolation algorithm not yet implemented...", AT);
}


void SnowHNWInterpolation::initialize(const MeteoData::Parameters& in_param) {
	param = in_param;
	nrOfMeasurments = getData(param, vecData, vecMeta);
}

double SnowHNWInterpolation::getQualityRating()
{
	if (nrOfMeasurments == 0)
		return 0.0;

	return 0.9;
}

void SnowHNWInterpolation::calculate(Grid2DObject& grid)
{
	//retrieve optional arguments
	std::string base_algo;
	if (vecArgs.size() == 0){
		base_algo=std::string("IDW_LAPSE");
	} else if (vecArgs.size() == 1){
		IOUtils::convertString(base_algo, vecArgs[0]);
	} else { //incorrect arguments, throw an exception
		throw InvalidArgumentException("Wrong number of arguments supplied for the HNW_SNOW algorithm", AT);
	}

	//initialize precipitation grid with user supplied algorithm (IDW_LAPSE by default)
	IOUtils::toUpper(base_algo);
	vector<string> vecArgs2;
	mi.getArgumentsForAlgorithm(param, base_algo, vecArgs2);
	auto_ptr<InterpolationAlgorithm> algorithm(AlgorithmFactory::getAlgorithm(base_algo, mi, dem, vecMeteo, vecStation, vecArgs2));
	algorithm->initialize(param);
	algorithm->calculate(grid);
	info << algorithm->getInfo();
	const double orig_mean = grid.grid2D.getMean();
	std::cout << grid;

	 //get TA interpolation from call back to Meteo2DInterpolator
	Grid2DObject ta;
	mi.interpolate(MeteoData::TA, ta);

	//slope/curvature correction for solid precipitation
	Interpol2D::PrecipSnow(dem, ta, grid);

	//HACK: correction for precipitation sum over the whole domain
	//this is a cheap/crappy way of compensating for the spatial redistribution of snow on the slopes
	const double new_mean = grid.grid2D.getMean();
	if(new_mean!=0.) grid.grid2D *= orig_mean/new_mean;
}


void OrdinaryKrigingAlgorithm::initialize(const MeteoData::Parameters& in_param) {
	param = in_param;
	nrOfMeasurments = getData(param, vecData, vecMeta);
}

double OrdinaryKrigingAlgorithm::getQualityRating()
{
	throw IOException("ODKRIG interpolation algorithm not yet implemented...", AT);

	if(nrOfMeasurments>=20) return 0.9;
	return 0.;
}


void OrdinaryKrigingAlgorithm::calculate(Grid2DObject& grid)
{
//optimization: getrange (from variogram fit -> exclude stations that are at distances > range (-> smaller matrix)
	throw IOException("ODKRIG interpolation algorithm not yet implemented...", AT);
	Interpol2D::ODKriging(vecData, vecMeta, dem, grid);
}

double OrdinaryKrigingAlgorithm::computeVariogram(const std::vector<StationData>& /*vecStations*/) const
{
	//return variogramm fit of covariance between stations i and j
	//HACK: todo!
	
	return 1.;
}

} //namespace

