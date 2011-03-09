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
#ifdef MSVC
	#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <string>
#include <meteoio/meteolaws/Suntrajectory.h>

namespace mio {
//class SunTrajectory
const double SunTrajectory::to_deg = 180./M_PI;
const double SunTrajectory::to_rad = M_PI/180.;

/**
 * @brief Compute the solar incidence (rad), i.e. the angle between the incident sun beam
 *   and the normal to the slope (simplified by Funk (1984 pp.139-140)).
 * @param slope_azi slope azimuth in degrees, bearing
 * @param slope_elev slope angle in degrees
 * @return double angle of incidence of the sun to the local slope
 */
double SunTrajectory::getAngleOfIncidence(const double& slope_azi, const double& slope_elev) const
{
	const double Z = (90.-SolarElevation)*to_rad;
	const double beta = slope_elev*to_rad;
	const double cos_theta = cos(beta)*cos(Z) + sin(beta)*sin(Z)*cos((SolarAzimuthAngle-slope_azi)*to_rad);

	return acos(cos_theta)*to_deg;
}

double SunTrajectory::getAngleOfIncidence(const double& sun_azi, const double& sun_elev,
                                          const double& slope_azi, const double& slope_elev)
{
	const double Z = (90.-sun_elev)*to_rad;
	const double beta = slope_elev*to_rad;
	const double cos_theta = cos(beta)*cos(Z) + sin(beta)*sin(Z)*cos((sun_azi-slope_azi)*to_rad);

	return acos(cos_theta)*to_deg;
}

double SunTrajectory::getRadiationOnHorizontal(const double& radiation) const
{ // Project a beam radiation (ie: perpendicular to the sun beam) to the horizontal
// Oke, T.R., Boundary Layer Climates. 2nd ed, 1987, Routledge, London, 435pp.
	if(radiation==IOUtils::nodata) return IOUtils::nodata;

	const double Z = (90.-SolarElevation)*to_rad;

	const double on_horizontal = radiation * cos(Z);
	return on_horizontal;
}

double SunTrajectory::getRadiationOnSlope(const double& slope_azi, const double& slope_elev, const double& radiation) const
{ // Project a beam radiation (ie: perpendicular to the sun beam) to a given slope
// Oke, T.R., Boundary Layer Climates. 2nd ed, 1987, Routledge, London, 435pp.
	if(radiation==IOUtils::nodata) return IOUtils::nodata;

	const double Z = (90.-SolarElevation)*to_rad;
	const double beta = slope_elev*to_rad;
	const double cos_theta = cos(beta)*cos(Z) + sin(beta)*sin(Z)*cos((SolarAzimuthAngle-slope_azi)*to_rad);

	const double on_slope = radiation*cos_theta;
	if(on_slope>0.) return on_slope;
	else return 0.;
}

//usefull static methods
double SunTrajectory::projectHorizontalToSlope(const double& sun_azi, const double& sun_elev, const double& slope_azi, const double& slope_elev, const double& H_radiation)
{// Project a horizontal radiation to a given slope
// Oke, T.R., Boundary Layer Climates. 2nd ed, 1987, Routledge, London, 435pp.
	const double Z = (90.-sun_elev)*to_rad;
	const double cosZ = cos(Z);

	if(cosZ==0.) {
		return 1.e12;
	} else {
		const double beta = slope_elev*to_rad;
		const double cos_theta = cos(beta)*cosZ + sin(beta)*sin(Z)*cos((sun_azi-slope_azi)*to_rad);
		const double on_slope = ( H_radiation/cosZ ) * cos_theta;
		if(on_slope>0.) return on_slope;
		return 0.;
	}
}

double SunTrajectory::projectSlopeToHorizontal(const double& sun_azi, const double& sun_elev, const double& slope_azi, const double& slope_elev, const double& S_radiation)
{// Project a slope radiation to horizontal
// Oke, T.R., Boundary Layer Climates. 2nd ed, 1987, Routledge, London, 435pp.
	const double Z = (90.-sun_elev)*to_rad;
	const double beta = slope_elev*to_rad;
	const double cos_theta = cos(beta)*cos(Z) + sin(beta)*sin(Z)*cos((sun_azi-slope_azi)*to_rad);

	if(cos_theta==0.) {
		return 1.e12;
	} else {
		const double on_horizontal = ( S_radiation/cos_theta ) * cos(Z);
		return on_horizontal;
	}
}

double SunTrajectory::projectHorizontalToBeam(const double& sun_elev, const double& H_radiation)
{ // Project a beam radiation (ie: perpendicular to the sun beam) to the horizontal
// Oke, T.R., Boundary Layer Climates. 2nd ed, 1987, Routledge, London, 435pp.
	const double Z = (90.-sun_elev)*to_rad;
	const double cosZ = cos(Z);

	if(cosZ==0.) {
		return 1.e12;
	} else {
		const double radiation = H_radiation / cos(Z);
		return radiation;
	}
}

std::ostream& operator<<(std::ostream &os, const SunTrajectory& data)
{
	os << "<SunTrajectory>\n";
	os << std::fixed << std::setprecision(4);
	os << "Julian (gmt)\t" << data.julian_gmt << "\n";
	os << "Lat/Long\t" << std::setw(7) << data.latitude << "° " << std::setw(7) << data.longitude << "°\n";
	os << "Ecc. corr.\t" << std::setprecision(4) << std::setw(7) << data.eccentricityEarth << "°\n";
	os << "Hour Angle\t" << std::setprecision(4) << std::setw(7) << data.HourAngle << "°\n";
	os << std::setprecision(2);
	os << "Azi./Elev.\t" << std::setw(7)<< data.SolarAzimuthAngle << "° " << std::setw(7) << data.SolarElevation << "°\n";
	os << "RA/decl.\t" << std::setw(7) << data.SunRightAscension << "° " << std::setw(7) << data.SunDeclination << "°\n";
	os << "Sunrise (gmt)\t" << IOUtils::printFractionalDay(data.SunRise) << "\n";
	os << "Sunset (gmt)\t" << IOUtils::printFractionalDay(data.SunSet) << "\n";
	os << "Daylight\t" << IOUtils::printFractionalDay(data.SunlightDuration/(60.*24.)) << "\n";
	os << "</SunTrajectory>\n";
	os << std::setfill(' ');
	return os;
}

} //end namespace

namespace mio {

//Class SunMeeus
SunMeeus::SunMeeus() {
	init();
}

SunMeeus::SunMeeus(const double& _latitude, const double& _longitude) {
	julian_gmt = IOUtils::nodata;
	latitude = _latitude;
	longitude = _longitude;
	private_init();
}

SunMeeus::SunMeeus(const double& _latitude, const double& _longitude, const double& _julian, const double& TZ) {
	setAll(_latitude, _longitude, _julian, TZ);
}

void SunMeeus::init() {
	julian_gmt = IOUtils::nodata;
	latitude = longitude = IOUtils::nodata;
	private_init();
}

void SunMeeus::private_init() {
	SolarAzimuthAngle = SolarElevation = IOUtils::nodata;
	SolarElevationAtm = IOUtils::nodata;
	eccentricityEarth = IOUtils::nodata;
	SunRise = SunSet = SunlightDuration = IOUtils::nodata;
	SunRightAscension = SunDeclination = IOUtils::nodata;
	HourAngle = IOUtils::nodata;
}


void SunMeeus::setDate(const double& _julian, const double& TZ) {
	if(TZ==0)
		julian_gmt = _julian;
	else
		julian_gmt = _julian - TZ/24.;
	private_init();
	if(latitude!=IOUtils::nodata && longitude!=IOUtils::nodata) {
		update();
	}
}

void SunMeeus::setLatLon(const double& _latitude, const double& _longitude) {
	latitude = _latitude;
	longitude = _longitude;
	private_init();
	if(julian_gmt!=IOUtils::nodata) {
		update();
	}
}

void SunMeeus::setAll(const double& _latitude, const double& _longitude, const double& _julian, const double& TZ) {
	latitude = _latitude;
	longitude = _longitude;
	if(TZ==0)
		julian_gmt = _julian;
	else
		julian_gmt = _julian - TZ/24.;
	update();
}

void SunMeeus::reset() {;
	init();
}

void SunMeeus::getHorizontalCoordinates(double& azimuth, double& elevation) const {
	double eccentricity;
	getHorizontalCoordinates(azimuth, elevation, eccentricity);
}

void SunMeeus::getHorizontalCoordinates(double& azimuth, double& elevation, double& eccentricity) const {
	if(julian_gmt!=IOUtils::nodata && latitude!=IOUtils::nodata && longitude!=IOUtils::nodata) {
		azimuth = SolarAzimuthAngle;
		elevation = SolarElevation; //this is the TRUE elevation, not the apparent!
		eccentricity = eccentricityEarth;
	} else {
		throw new std::string("Please set ALL required parameters to get Sun's position!!");
	}
}

void SunMeeus::getDaylight(double& sunrise, double& sunset, double& daylight, const double& TZ) {
	if(julian_gmt!=IOUtils::nodata && latitude!=IOUtils::nodata && longitude!=IOUtils::nodata) {
		sunrise = SunRise - longitude*1./15.*1./24. + TZ*1./24.; //back to TZ, in days
		sunset = SunSet - longitude*1./15.*1./24. + TZ*1./24.; //back to TZ, in days
		daylight = SunlightDuration;
	} else {
		throw new std::string("Please set ALL required parameters to get Sun's position!!");
	}
}

void SunMeeus::getEquatorialCoordinates(double& right_ascension, double& declination) {
	if(julian_gmt!=IOUtils::nodata && latitude!=IOUtils::nodata && longitude!=IOUtils::nodata) {
		right_ascension = SunRightAscension;
		declination = SunDeclination;
	} else {
		throw new std::string("Please set ALL required parameters to get Sun's position!!");
	}
}

void SunMeeus::getEquatorialSunVector(double& sunx, double& suny, double& sunz) {
	const double to_rad = M_PI/180.;
	double azi_Sacw;

	// Convert to angle measured from South, counterclockwise (rad)
	if ( SolarAzimuthAngle <= 90. ) { //HACK: to check!
		azi_Sacw = M_PI - SolarAzimuthAngle*to_rad;
	} else {
		azi_Sacw = 3.*M_PI - SolarAzimuthAngle*to_rad;
	}

	// derived as shown in Funk (1984) p. 107, but for a y-coordinate increasing northwards
	sunx =  sin( azi_Sacw ) * cos( SolarElevation ); // left handed coordinate system
	suny = -cos( azi_Sacw ) * cos( SolarElevation ); // inverse y-coordinate because of reference system
	sunz =  sin( SolarElevation );
}

void SunMeeus::update() {
	//calculating various time representations
	const double julian = julian_gmt;
	const double lst_TZ = longitude*1./15.;
	const double gmt_time = ((julian + 0.5) - floor(julian + 0.5))*24.; //in hours
	const double lst_hours=(gmt_time+longitude*1./15.); //Local Sidereal Time
	const double julian_century = (julian - 2451545.)/36525.;

	//parameters of the orbits of the Earth and Sun
	const double geomMeanLongSun = fmod( 280.46646 + julian_century*(36000.76983 + julian_century*0.0003032) , 360.);
	const double geomMeanAnomSun = 357.52911 + julian_century*(35999.05029 - 0.0001537*julian_century);
	eccentricityEarth = 0.016708634 - julian_century*(0.000042037 + 0.0001537*julian_century);
	const double SunEqOfCtr =   sin(1.*geomMeanAnomSun*to_rad)*( 1.914602-julian_century*(0.004817+0.000014*julian_century))
	             + sin(2.*geomMeanAnomSun*to_rad)*(0.019993 - 0.000101*julian_century)
	             + sin(3.*geomMeanAnomSun*to_rad)*(0.000289);

	const double SunTrueLong = geomMeanLongSun + SunEqOfCtr;
	/*const double SunTrueAnom = geomMeanAnomSun + SunEqOfCtr;*/
	/*const double SunRadVector =   ( 1.000001018 * (1. - eccentricityEarth*eccentricityEarth) )
	               / ( 1. + eccentricityEarth*cos(SunTrueAnom*to_rad) );*/

	const double SunAppLong = SunTrueLong - 0.00569 - 0.00478*sin( (125.04-1934.136*julian_century)*to_rad );
	const double MeanObliqueEcl = 23. + (26.+
	                 (21.448-julian_century*(46.815+julian_century*(0.00059-julian_century*0.001813))) / 60. )
	                 / 60.;

	const double ObliqueCorr = MeanObliqueEcl + 0.00256*cos( (125.04-1934.136*julian_century)*to_rad );

	//Sun's position in the equatorial coordinate system
	SunRightAscension = atan2(
	                    cos(SunAppLong*to_rad) ,
	                    cos(ObliqueCorr*to_rad) * sin(SunAppLong*to_rad)
	                    ) * to_deg;

	SunDeclination = asin( sin(ObliqueCorr*to_rad) * sin(SunAppLong*to_rad) ) * to_deg;

	//time calculations
	const double var_y = tan( 0.5*ObliqueCorr*to_rad ) * tan( 0.5*ObliqueCorr*to_rad );
	const double EquationOfTime = 4. * ( var_y*sin(2.*geomMeanLongSun*to_rad)
	                 - 2.*eccentricityEarth*sin(geomMeanAnomSun*to_rad) +
	                 4.*eccentricityEarth*var_y*sin(geomMeanAnomSun*to_rad) * cos(2.*geomMeanLongSun*to_rad)
	                 - 0.5*var_y*var_y*sin(4.*geomMeanLongSun*to_rad)
	                 - 1.25*eccentricityEarth*eccentricityEarth*sin(2.*geomMeanAnomSun*to_rad)
	                 )*to_deg;

	const double HA_sunrise = acos( cos(90.833*to_rad)/cos(latitude*to_rad) * cos(SunDeclination*to_rad)
	             - tan(latitude*to_rad)*tan(SunDeclination*to_rad)
	             ) * to_deg;

	const double SolarNoon = (720. - 4.*longitude - EquationOfTime + lst_TZ*60.)/1440.; //in days, in LST time
	SunRise = SolarNoon - HA_sunrise*4./1440.; //in days, in LST
	SunSet = .5 + (SolarNoon + HA_sunrise*4.)/1440.; //in days, in LST
	SunlightDuration = 8.*HA_sunrise;

	//Sun's position in the horizontal coordinate system (see http://en.wikipedia.org/wiki/Horizontal_coordinate_system)
	double AtmosphericRefraction;
	const double TrueSolarTime = fmod( lst_hours*60. + EquationOfTime + 4.*longitude - 60.*lst_TZ , 1440. ); //in LST time
	if( TrueSolarTime<0. )
		HourAngle = TrueSolarTime/4.+180.;
	else
		HourAngle = TrueSolarTime/4.-180.;

	const double SolarZenithAngle = acos(
	                   sin(latitude*to_rad) * sin(SunDeclination*to_rad)
	                   + cos(latitude*to_rad) * cos(SunDeclination*to_rad) * cos(HourAngle*to_rad)
	                   )*to_deg;

	SolarElevation = 90. - SolarZenithAngle;

	if( SolarElevation>85. ) {
		AtmosphericRefraction = 0.;
	} else {
		if( SolarElevation>5. ) {
			AtmosphericRefraction = 58.1 / tan(SolarElevation*to_rad) - 0.07 / pow( tan(SolarElevation*to_rad) , 3 ) + 0.000086/pow( tan(SolarElevation*to_rad), 5);
		} else {
			if( SolarElevation>-0.575 ) {
				AtmosphericRefraction = 1735. + SolarElevation*(-518.2 + SolarElevation*(103.4 + SolarElevation*(-12.79 + SolarElevation*0.711)));
			} else {
				AtmosphericRefraction = -20.772 / tan( SolarElevation*to_rad );
			}
		}
	}
	AtmosphericRefraction /= 3600.;

	SolarElevationAtm = SolarElevation + AtmosphericRefraction; //correction for the effects of the atmosphere
	if( HourAngle>0. ) {
		SolarAzimuthAngle = fmod( acos(
		                     (sin(latitude*to_rad)*cos(SolarZenithAngle*to_rad) - sin(SunDeclination*to_rad)) /
		                     (cos(latitude*to_rad)*sin(SolarZenithAngle*to_rad)))*to_deg + 180.
		                    , 360. );
	} else {
		SolarAzimuthAngle = fmod( 540. - acos(
		                     (sin(latitude*to_rad)*cos(SolarZenithAngle*to_rad) - sin(SunDeclination*to_rad)) /
		                     (cos(latitude*to_rad)*sin(SolarZenithAngle*to_rad)))*to_deg
		                    ,  360. );
	}
}

} //namespace
