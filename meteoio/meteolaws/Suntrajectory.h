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
#ifndef __SUNTAJECTORY_H__
#define __SUNTAJECTORY_H__

#include <meteoio/IOUtils.h>

namespace mio {

/**
 * @class SunTrajectory
 * @brief A class to calculate the Sun's position
 * This class is purely virtual.
 * @ingroup meteolaws
 * @author Mathias Bavay
 * @date   2010-06-10
 */
class SunTrajectory {
	public:
		SunTrajectory() {};
		virtual ~SunTrajectory() {};

		/** @brief Set the date and time
		* if no timezone is specified, GMT is assumed
		* @param _julian julian date in the time zone of interest
		* @param TZ time zone
		*/
		virtual void setDate(const double& _julian, const double& TZ=0.)=0;
		virtual void setLatLon(const double& _latitude, const double& _longitude)=0;
		virtual void setAll(const double& _latitude, const double& _longitude, const double& _julian, const double& TZ=0.)=0;
		virtual void reset()=0;

		///(see http://en.wikipedia.org/wiki/Horizontal_coordinate_system)
		///please remember that zenith_angle = 90 - elevation
		virtual void getHorizontalCoordinates(double& azimuth, double& elevation) const=0;
		virtual void getHorizontalCoordinates(double& azimuth, double& elevation, double& eccentricity) const=0;
		virtual void getDaylight(double& sunrise, double& sunset, double& daylight, const double& TZ=0.)=0;
		virtual double getSolarTime(const double& TZ=0.) const;

		///(http://en.wikipedia.org/wiki/Equatorial_coordinate_system)
		virtual void getEquatorialCoordinates(double& right_ascension, double& declination)=0;

		///radiation projection methods
		double getAngleOfIncidence(const double& slope_azi, const double& slope_elev) const;
		double getRadiationOnHorizontal(const double& radiation) const;
		double getRadiationOnSlope(const double& slope_azi, const double& slope_elev, const double& radiation) const;
		double getHorizontalOnSlope(const double& slope_azi, const double& slope_elev, const double& H_radiation, const double& elev_threshold=5.) const;

		///static helper for radiation projection
		static double getAngleOfIncidence(const double& sun_azi, const double& sun_elev,
		                                  const double& slope_azi, const double& slope_elev);
		static double projectHorizontalToSlope(const double& sun_azi, const double& sun_elev,
		                                       const double& slope_azi, const double& slope_elev, const double& H_radiation, const double& elev_threshold=5.);
		static double projectSlopeToHorizontal(const double& sun_azi, const double& sun_elev,
		                                       const double& slope_azi, const double& slope_elev, const double& S_radiation);
		static double projectHorizontalToBeam(const double& sun_elev, const double& H_radiation);

		friend std::ostream& operator<<(std::ostream& os, const SunTrajectory& data);

	protected:
		virtual void update()=0;

	protected:
		double julian_gmt; //TODO: we should keep TZ so sunrise/sunset are given in input TZ
		double latitude, longitude;
		double SolarAzimuthAngle, SolarElevation; ///>this is the TRUE solar elevation, not the apparent one
		double eccentricityEarth;
		double SunRise, SunSet, SunlightDuration, SolarNoon;
		double SunRightAscension, SunDeclination;
		double HourAngle;
		static const double nodata;
};

/**
 * @class SunMeeus
 * @brief Calculate the Sun's position based on the Meeus algorithm.
 * See J. Meeus, <i>"Astronomical Algorithms"</i>, 1998, 2nd ed, ISBN 0-943396-61-1.
 * A useful reference is also NOAA's spreadsheet at http://www.esrl.noaa.gov/gmd/grad/solcalc/calcdetails.html or
 * http://energyworksus.com/solar_installation_position.html for comparing positional data. The technical report
 * I. Reda, A. Andreas, <i>"Solar Position Algorithm for Solar Radiation Applications"</i>, 2008, NREL/TP-560-34302
 * also contains an alternative algorithm and very detailed validation data sets.
 *
 * @ingroup meteolaws
 * @author Mathias Bavay
 * @date   2010-06-10
 */
class SunMeeus : public SunTrajectory {
	public:
		SunMeeus();
		~SunMeeus() {};
		SunMeeus(const double& _latitude, const double& _longitude);
		SunMeeus(const double& _latitude, const double& _longitude, const double& _julian, const double& TZ=0.);

		void setDate(const double& _julian, const double& TZ=0.);
		void setLatLon(const double& _latitude, const double& _longitude);
		void setAll(const double& _latitude, const double& _longitude, const double& _julian, const double& TZ=0.);
		void reset();

		void getHorizontalCoordinates(double& azimuth, double& elevation) const;
		void getHorizontalCoordinates(double& azimuth, double& elevation, double& eccentricity) const;
		void getDaylight(double& sunrise, double& sunset, double& daylight, const double& TZ=0.);
		void getEquatorialSunVector(double& sunx, double& suny, double& sunz);
		void getEquatorialCoordinates(double& right_ascension, double& declination);
	private:
		void init();
		void private_init();
		void update();

	private:
		double SolarElevationAtm;

};

} //end namespace

#endif
