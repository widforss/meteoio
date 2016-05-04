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
#ifndef GENERATORALGORITHMS_H
#define GENERATORALGORITHMS_H

#include <meteoio/dataClasses/MeteoData.h>
#include <meteoio/meteoLaws/Sun.h>

#include <vector>
#include <set>
#include <string>

#ifdef _MSC_VER
	#pragma warning(disable:4512) //we don't need any = operator!
#endif

namespace mio {

/**
 * @page generators Data generators
 * Once the data has been read, filtered and resampled, it can be that some data points are still missing.
 * These are either a few isolated periods (a sensor was not functioning) that are too large for performing
 * a statistical temporal interpolation or that a meteorological parameter was not even measured. In such a case,
 * we generate data, generally relying on some parametrization using other meteorological parameters. In a few
 * cases, even fully arbitrary data might be helpful (replacing missing value by a given constant so a model can
 * run over the data gap).
 *
 * Another usage scenario is to create new parameters fully based on parametrizations. In such a case, the "generator" is called
 * a "creator" and behaves the same way as a generator, except that it creates an additional parameter. It is declared as
 * {new_parameter}::%create = {data generators}.
 *
 * @note it is generally not advised to use data generators in combination with spatial interpolations as this would
 * potentially mix measured and generated values in the resulting grid. It is therefore advised to turn the data generators
 * off and let the spatial interpolations algorithms adjust to the amount of measured data.
 * @note it is also possible to make a copy of a given parameter under a different name. This is explained in section 
 * \ref data_manipulations "Raw data editing".
 *
 * @section generators_section Data generators section
 * The data generators are defined per meteorological parameter. They are applied to all stations
 * (if using multiple meteorological stations). If multiple dat generators are specified for each parameter,
 * they would be used in the order of declaration, meaning that only the data points that could not be
 * generated by the first generator would be tentatively generated by the second generator, etc. Please also keep
 * in mind that at this stage, all data <b>must be in SI</b> units!
 * @code
 * [Generators]
 * RH::generators = CST
 * RH::Cst        = .7
 *
 * P::generators  = STD_PRESS
 *
 * ILWR::generators = AllSky_LW ClearSky_LW
 *
 * TAU_CLD::create  = CST
 * TA_CLD::Cst      = 0.5
 * @endcode
 *
 * @section generators_keywords Available generators
 * The keywords defining the algorithms are the following:
 * - STD_PRESS: standard atmospheric pressure as a function of the elevation of each station (see StandardPressureGenerator)
 * - RELHUM: relative humidity from other humidity measurements (see RhGenerator)
 * - TS_OLWR: surface temperature from Outgoing Long Wave Radiation (see TsGenerator)
 * - ISWR_ALBEDO: ISWR from RSWR or RSWR from ISWR with either a snow or a soil albedo, depending on HS (see IswrAlbedoGenerator)
 * - CST: constant value as provided in argument (see ConstGenerator)
 * - SIN: sinusoidal variation (see SinGenerator)
 * - CLEARSKY_LW: use a clear sky model to generate ILWR from TA, RH (see ClearSkyLWGenerator)
 * - ALLSKY_LW: use an all sky model to generate ILWR from TA, RH and cloudiness (see AllSkyLWGenerator)
 * - ALLSKY_SW: generate the incoming short wave radiation from the potential radiation, corrected for cloudiness if possible (see AllSkySWGenerator)
 * - ESOLIP: generate precipitation from snow height changes (see ESOLIPGenerator)
 * - PPHASE: generate precipitation phase with a user-selected method (see PPhaseGenerator)
 *
 * @section generators_biblio Bibliography
 * The data generators have been inspired by the following papers:
 * - Brutsaert -- <i>"On a Derivable Formula for Long-Wave Radiation From Clear Skies"</i>, Journal of Water Resources
 * Research, <b>11</b>, No. 5, October 1975, pp 742-744.
 * - Prata -- <i>"A new long-wave formula for estimating downward clear-sky radiation at the surface"</i>, Q. J. R. Meteorolo. Soc., <b>122</b>, 1996, pp 1127-1151.
 * - Dilley and O'Brien -- <i>"Estimating downward clear sky long-wave irradiance at the surface from screen temperature and precipitable water"</i>, Q. J. R. Meteorolo. Soc., Vol. 124, 1998, doi:10.1002/qj.49712454903
 * - Clark & Allen -- <i>"The estimation of atmospheric radiation for clear and cloudy skies"</i>, Proceedings of the second national passive solar conference, <b>2</b>, 1978, p 676.
 * - Tang et al. -- <i>"Estimates of clear night sky emissivity in the Negev Highlands, Israel"</i>, Energy Conversion and Management, <b>45.11</b>, 2004, pp 1831-1843.
 * - Idso -- <i>"A set of equations for full spectrum and 8 to 14 um and 10.5 to 12.5 um thermal radiation from cloudless skies"</i>, Water Resources Research, <b>17</b>, 1981, pp 295-304.
 * - Kasten and Czeplak -- <i>"Solar and terrestrial radiation dependent on the amount and type of cloud"</i>, 1980, Solar energy, 24.2, pp 177-189
 * - Omstedt -- <i>"A coupled one-dimensional sea ice-ocean model applied to a semi-enclosed basin"</i>, Tellus, <b>42 A</b>, 568-582, 1990, DOI:10.1034/j.1600-0870.1990.t01-3-00007.
 * - Konzelmann et al. -- <i>"Parameterization of global and longwave incoming radiation for the Greenland Ice Sheet."</i> Global and Planetary change <b>9.1</b> (1994): 143-164.
 * - Crawford and Duchon -- <i>"An Improved Parametrization for Estimating Effective Atmospheric Emissivity for Use in Calculating Daytime
 * Downwelling Longwave Radiation"</i>, Journal of Applied Meteorology, <b>38</b>, 1999, pp 474-480
 * - Unsworth and Monteith -- <i>"Long-wave radiation at the ground"</i>, Q. J. R. Meteorolo. Soc., Vol. 101, 1975, pp 13-24
 * - Meeus -- <i>"Astronomical Algorithms"</i>, second edition, 1998, Willmann-Bell, Inc., Richmond, VA, USA
 * - Mair et al. -- <i>" ESOLIP–estimate of solid and liquid precipitation at sub-daily time resolution by combining snow height
 * and rain gauge measurements"</i>, Hydrology and Earth System Sciences Discussions, <b>10(7)</b>, 8683-8714, 2013.
 *
 *
 * @author Mathias Bavay
 * @date   2013-03-20
 */

/**
 * @class GeneratorAlgorithm
 * @brief Interface class for the generator models.
 * These models generate data for a specific parameter when all other options failed (the resampling could not help).
 * Therefore, there is nothing more that could be done with the temporal history of the data, we have to use
 * a totally different approach: either generic data (constant value, etc) or generate the data from other
 * meteorological parameters (relying on a parametrization, like clear sky for ILWR).
 *
 * @ingroup meteoLaws
 * @author Mathias Bavay
 * @date   2013-03-20
*/
class GeneratorAlgorithm {

	public:
		GeneratorAlgorithm(const std::vector<std::string>& /*vecArgs*/, const std::string& i_algo) : algo(i_algo) {}
		virtual ~GeneratorAlgorithm() {}
		//fill one MeteoData, for one station
		virtual bool generate(const size_t& param, MeteoData& md) = 0;
		//fill one time series of MeteoData for one station
		virtual bool generate(const size_t& param, std::vector<MeteoData>& vecMeteo) = 0;
		std::string getAlgo() const;
 	protected:
		virtual void parse_args(const std::vector<std::string>& i_vecArgs);
		const std::string algo;
};

class GeneratorAlgorithmFactory {
	public:
		static GeneratorAlgorithm* getAlgorithm(const std::string& i_algoname, const std::vector<std::string>& vecArgs);
};

/**
 * @class ConstGenerator
 * @brief Constant value generator.
 * Generate a constant value for this parameter, as provided in argument (please remember that it must be in SI units).
 * @code
 * RH::generators = Cst
 * RH::Cst = .7
 * @endcode
 */
class ConstGenerator : public GeneratorAlgorithm {
	public:
		ConstGenerator(const std::vector<std::string>& vecArgs, const std::string& i_algo)
			: GeneratorAlgorithm(vecArgs, i_algo), constant(IOUtils::nodata) { parse_args(vecArgs); }
		bool generate(const size_t& param, MeteoData& md);
		bool generate(const size_t& param, std::vector<MeteoData>& vecMeteo);
	private:
		void parse_args(const std::vector<std::string>& vecArgs);
		double constant;
};

/**
 * @class SinGenerator
 * @brief Sinusoid generator.
 * Generate a sinusoidal variation for this parameter, as provided in argument (please remember that it must be in SI units).
 * The arguments that must be provided are the following: the sinusoidal period (either "yearly" or "daily"), the minimum value,
 * the maximum value and the offset specifying when the minimum value should be reached, within one period (expressed as fraction of such period).
 * The example below generates a yearly sinusoidal variation for the air temperature, the minimum being 268.26 K and occuring at 1/12
 * of the period (which practically means, at the end of the first month).
 * @code
 * TA::generators = Sin
 * TA::Sin = yearly 268.26 285.56 0.0833
 * @endcode
 */
class SinGenerator : public GeneratorAlgorithm {
	public:
		SinGenerator(const std::vector<std::string>& vecArgs, const std::string& i_algo)
			: GeneratorAlgorithm(vecArgs, i_algo), amplitude(IOUtils::nodata), offset(IOUtils::nodata), phase(IOUtils::nodata), type(' ') { parse_args(vecArgs); }
		bool generate(const size_t& param, MeteoData& md);
		bool generate(const size_t& param, std::vector<MeteoData>& vecMeteo);
	private:
		void parse_args(const std::vector<std::string>& vecArgs);
		double amplitude, offset, phase;
		char type;
};

/**
 * @class StandardPressureGenerator
 * @brief Standard atmospheric pressure generator.
 * Generate a standard atmosphere's pressure, depending on the local elevation.
 * @code
 * P::generators = STD_PRESS
 * @endcode
 */
class StandardPressureGenerator : public GeneratorAlgorithm {
	public:
		StandardPressureGenerator(const std::vector<std::string>& vecArgs, const std::string& i_algo)
			: GeneratorAlgorithm(vecArgs, i_algo) { parse_args(vecArgs); }
		bool generate(const size_t& param, MeteoData& md);
		bool generate(const size_t& param, std::vector<MeteoData>& vecMeteo);
};

/**
 * @class RhGenerator
 * @brief Relative humidity generator.
 * Generate the relative humidity from either dew point temperature or specific humidity and air temperature.
 * The dew point temperature must be named "TD" and the specific humidity "SH"
 * @code
 * RH::generators = RELHUM
 * @endcode
 */
class RhGenerator : public GeneratorAlgorithm {
	public:
		RhGenerator(const std::vector<std::string>& vecArgs, const std::string& i_algo)
			: GeneratorAlgorithm(vecArgs, i_algo) { parse_args(vecArgs); }
		bool generate(const size_t& param, MeteoData& md);
		bool generate(const size_t& param, std::vector<MeteoData>& vecMeteo);
};

/**
 * @class TauCLDGenerator
 * @brief Atmospheric transmissivity generator.
 * Generate the atmospheric transmissivity (or clearness index) from other parameters.
 * Right now, it only looks for a "CLD" parameter that should be the cloud cover / cloudiness (in okta but brought back between
 * 0 and 1) and converts it into a clearness index. 
 * @code
 * TAU_CLD::generators = TAU_CLD
 * @endcode
 */
class TauCLDGenerator : public GeneratorAlgorithm {
	public:
		TauCLDGenerator(const std::vector<std::string>& vecArgs, const std::string& i_algo)
			: GeneratorAlgorithm(vecArgs, i_algo) { parse_args(vecArgs); }
		bool generate(const size_t& param, MeteoData& md);
		bool generate(const size_t& param, std::vector<MeteoData>& vecMeteo);
};

/**
 * @class TsGenerator
 * @brief Surface temperature generator.
 * Generate the surface temperature from the outgoing long wave (OLWR).
 * @code
 * TSS::generators = TS_OLWR
 * @endcode
 */
class TsGenerator : public GeneratorAlgorithm {
	public:
		TsGenerator(const std::vector<std::string>& vecArgs, const std::string& i_algo)
			: GeneratorAlgorithm(vecArgs, i_algo) { parse_args(vecArgs); }
		bool generate(const size_t& param, MeteoData& md);
		bool generate(const size_t& param, std::vector<MeteoData>& vecMeteo);
	private:
		static const double e_snow, e_soil, snow_thresh;
};

/**
 * @class IswrAlbedoGenerator
 * @brief Incoming or reflected short wave generator.
 * Generate the incoming short wave radiation from the reflected short wave radiation or the opposite. The albedo
 * ie either a grassy soil albedo or a snow albedo depending on the snow height.
 * @code
 * ISWR::generators = ISWR_ALBEDO
 * @endcode
 */
class IswrAlbedoGenerator : public GeneratorAlgorithm {
	public:
		IswrAlbedoGenerator(const std::vector<std::string>& vecArgs, const std::string& i_algo)
			: GeneratorAlgorithm(vecArgs, i_algo) { parse_args(vecArgs); }
		bool generate(const size_t& param, MeteoData& md);
		bool generate(const size_t& param, std::vector<MeteoData>& vecMeteo);
	private:
		static const double soil_albedo, snow_albedo, snow_thresh;
};

/**
 * @class ClearSkyLWGenerator
 * @brief ILWR clear sky parametrization
 * Using air temperature (TA) and relative humidity (RH), this offers the choice of several clear sky parametrizations:
 *  - BRUTSAERT -- from Brutsaert, <i>"On a Derivable Formula for Long-Wave Radiation From Clear Skies"</i>,
 * Journal of Water Resources Research, <b>11</b>, No. 5, October 1975, pp 742-744.
 *  - DILLEY -- from Dilley and O'Brien, <i>"Estimating downward clear sky
 * long-wave irradiance at the surface from screen temperature and precipitable water"</i>, Q. J. R. Meteorolo. Soc., <b>124</b>, 1998, pp 1391-1401.
 *  - PRATA -- from Prata, <i>"A new long-wave formula for estimating downward clear-sky radiation at the surface"</i>, Q. J. R. Meteorolo. Soc., <b>122</b>, 1996, pp 1127-1151.
 *  - CLARK -- from Clark & Allen, <i>"The estimation of atmospheric radiation for clear and
 * cloudy skies"</i>, Proceedings of the second national passive solar conference, <b>2</b>, 1978, p 676.
 *  - TANG -- from Tang et al., <i>"Estimates of clear night sky emissivity in the
 * Negev Highlands, Israel"</i>, Energy Conversion and Management, <b>45.11</b>, 2004, pp 1831-1843.
 *  - IDSO -- from Idso, <i>"A set of equations for full spectrum and 8 to 14 um and
 * 10.5 to 12.5 um thermal radiation from cloudless skies"</i>, Water Resources Research, <b>17</b>, 1981, pp 295-304.
 *
 * Please keep in mind that for energy balance modeling, this significantly underestimate the ILWR input.
 * @code
 * ILWR::generators = clearsky_LW
 * ILWR::clearsky_lw = Dilley
 * @endcode
 *
 */
class ClearSkyLWGenerator : public GeneratorAlgorithm {
	public:
		ClearSkyLWGenerator(const std::vector<std::string>& vecArgs, const std::string& i_algo)
			: GeneratorAlgorithm(vecArgs, i_algo), model(BRUTSAERT) { parse_args(vecArgs); }
		bool generate(const size_t& param, MeteoData& md);
		bool generate(const size_t& param, std::vector<MeteoData>& vecMeteo);
	private:
		void parse_args(const std::vector<std::string>& vecArgs);
		typedef enum PARAMETRIZATION {
			BRUTSAERT,
			DILLEY,
			PRATA,
			CLARK,
			TANG,
			IDSO
		} parametrization;
		parametrization model;
};

/**
 * @class AllSkyLWGenerator
 * @brief ILWR all sky parametrization
 * Using air temperature (TA) and relative humidity (RH) and optionnally cloud transmissivity (TAU_CLD),
 * this offers the choice of several all-sky parametrizations:
 *  - OMSTEDT -- from Omstedt, <i>"A coupled one-dimensional sea ice-ocean model applied to a semi-enclosed basin"</i>,
 * Tellus, <b>42 A</b>, 568-582, 1990, DOI:10.1034/j.1600-0870.1990.t01-3-00007.
 *  - KONZELMANN -- from Konzelmann et al., <i>"Parameterization of global and longwave incoming radiation
 * for the Greenland Ice Sheet."</i> Global and Planetary change <b>9.1</b> (1994): 143-164.
 *  - UNSWORTH -- from Unsworth and Monteith, <i>"Long-wave radiation at the ground"</i>,
 * Q. J. R. Meteorolo. Soc., Vol. 101, 1975, pp 13-24 coupled with a clear sky emissivity following (Dilley, 1998).
 *  - CRAWFORD -- from Crawford and Duchon, <i>"An Improved Parametrization for Estimating Effective Atmospheric Emissivity for Use in Calculating Daytime
 * Downwelling Longwave Radiation"</i>, Journal of Applied Meteorology, <b>38</b>, 1999, pp 474-480
 *
 * If no cloud transmissivity is provided in the data, it is calculated from the solar index (ratio of measured iswr to potential iswr, therefore using
 * the current location (lat, lon, altitude) and ISWR to parametrize the cloud cover). This relies on (Kasten and Czeplak, 1980)
 * except for Crawford that provides its own parametrization.
 * The last evaluation of cloud transmissivity is used all along during the times when no ISWR is available if such ratio
 * is not too old (ie. no more than 1 day old).
 * If only RSWR is measured, the measured snow height is used to determine if there is snow on the ground or not.
 * In case of snow, a snow albedo of 0.85 is used while in the abscence of snow, a grass albedo of 0.23 is used
 * in order to compute ISWR from RSWR.
 * Finally, it is recommended to also use a clear sky generator (declared after this one)
 * for the case of no available short wave measurement (by declaring the ClearSky generator \em after AllSky).
 * @code
 * ILWR::generators = allsky_LW
 * ILWR::allsky_lw = Omstedt
 * @endcode
 *
 */
class AllSkyLWGenerator : public GeneratorAlgorithm {
	public:
		AllSkyLWGenerator(const std::vector<std::string>& vecArgs, const std::string& i_algo)
		               : GeneratorAlgorithm(vecArgs, i_algo), model(OMSTEDT), clf_model(KASTEN),
		                 last_cloudiness() { parse_args(vecArgs); }
		bool generate(const size_t& param, MeteoData& md);
		bool generate(const size_t& param, std::vector<MeteoData>& vecMeteo);
	private:
		void parse_args(const std::vector<std::string>& vecArgs);
		double getCloudiness(const MeteoData& md, SunObject& sun, bool &is_night);

		typedef enum PARAMETRIZATION {
			OMSTEDT,
			KONZELMANN,
			UNSWORTH,
			CRAWFORD
		} parametrization;
		parametrization model;

		typedef enum CLF_PARAMETRIZATION {
			KASTEN,
			CLF_CRAWFORD
		} clf_parametrization;
		clf_parametrization clf_model;

		std::map< std::string, std::pair<double, double> > last_cloudiness; //as < station_hash, <julian_gmt, cloudiness> >

		static const double soil_albedo, snow_albedo, snow_thresh; //to try using rswr if not iswr is given
};

/**
 * @class AllSkySWGenerator
 * @brief ISWR all sky parametrization
 *
 * Using air temperature (TA) and relative humidity (RH) and optionnally cloud transmissivity (TAU_CLD),
 * This computes the potential incoming solar radiation, based on the position of the sun in the sky
 * (as a function of the location and the date). Please note that although this is the radiation as perceived
 * at ground level (on the horizontal). If a cloud transmissivity (TAU_CLD) has been provided, or if an
 * incoming long wave measurement is available, it corrects the generated iswr for cloudiness
 * (basically doing like UnsworthGenerator in reverse), otherwise this assumes clear sky!
 * If no TA or RH is available, average values will be used (in order to get an average value
 * for the precipitable water vapor).
 * @note This relies on SunObject to perform the heavy duty computation.
 * @code
 * ISWR::generators = allsky_SW
 * @endcode
 */
class AllSkySWGenerator : public GeneratorAlgorithm {
	public:
		AllSkySWGenerator(const std::vector<std::string>& vecArgs, const std::string& i_algo)
			: GeneratorAlgorithm(vecArgs, i_algo), sun() { parse_args(vecArgs); }
		bool generate(const size_t& param, MeteoData& md);
		bool generate(const size_t& param, std::vector<MeteoData>& vecMeteo);
	private:
		void parse_args(const std::vector<std::string>& vecArgs);
		double getSolarIndex(const double& ta, const double& rh, const double& ilwr);
		SunObject sun;

		static const double soil_albedo, snow_albedo, snow_thresh; //to try using rswr if not iswr is given
};

/**
 * @class ESOLIPGenerator
 * @brief Generate precipitation from changes in snow height.
 * This implements the approach laid out in
 * Mair et al., <i>" ESOLIP–estimate of solid and liquid precipitation at sub-daily time resolution by combining snow height
 * and rain gauge measurements"</i>, Hydrology and Earth System Sciences Discussions, <b>10(7)</b>, 8683-8714, 2013. or
 * Mair E., Leitinger G., Della Chiesa S., Niedrist G., Tappeiner U., Bertoldi G., <i>"A simple method to combine snow height and
 * meteorological observations to estimate winter precipitation at sub-daily resolution"</i>, Journal of Hydrological Sciences,
 * in revision, 2015.
 * The snow density relies on Zwart, <i>"Significance of new-snow properties for snowcover development"</i>,master's thesis,
 * Institute for Marine and Atmospheric Research, University of Utrecht, 78 pp, 2007.
 *
 * @note only identified precipitation events are written out, this means that it is recommended to run through a Cst=0 data generator afterward
 *
 * @code
 * PSUM::generators = ESOLIP
 * @endcode
 */
class ESOLIPGenerator : public GeneratorAlgorithm {
	public:
		ESOLIPGenerator(const std::vector<std::string>& vecArgs, const std::string& i_algo)
			: GeneratorAlgorithm(vecArgs, i_algo) { parse_args(vecArgs); }
		bool generate(const size_t& param, MeteoData& md);
		bool generate(const size_t& param, std::vector<MeteoData>& vecMeteo);

	private:
		void parse_args(const std::vector<std::string>& vecArgs);
		double newSnowDensity(const MeteoData& md) const;
};

/**
 * @class PPhaseGenerator
 * @brief Generate precipitation splitting according to the selected method
 * The methods that are offered are currently the following:
 * - THRESH: a provided fixed air temperature threshold splits precipitation as either fully solid or fully liquid
 * - RANGE: two air temperature thresholds provide the lower and upper range for fully solid / fully liquid precipitation.
 *                 Within the provided range, a linear transition is assumed.
 *
 * @code
 * PSUM_PH::generators = PPHASE
 * PSUM_PH::PPHASE = THRESH 274.35
 * @endcode
 */
class PPhaseGenerator : public GeneratorAlgorithm {
	public:
		PPhaseGenerator(const std::vector<std::string>& vecArgs, const std::string& i_algo)
			: GeneratorAlgorithm(vecArgs, i_algo), model(THRESH), fixed_thresh(IOUtils::nodata),
			range_start(IOUtils::nodata), range_norm(IOUtils::nodata) { parse_args(vecArgs); }

		bool generate(const size_t& param, MeteoData& md);
		bool generate(const size_t& param, std::vector<MeteoData>& vecMeteo);

	private:
		void parse_args(const std::vector<std::string>& vecArgs);

		typedef enum PARAMETRIZATION {
			THRESH,
			RANGE
		} parametrization;
		parametrization model;
		double fixed_thresh, range_start, range_norm;
};

} //end namespace mio

#endif
