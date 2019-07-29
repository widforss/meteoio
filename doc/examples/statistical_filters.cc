/***********************************************************************************/
/*  Copyright 2019 Avalanche Warning Service Tyrol                  LWD-TIROL      */
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

/*
 * Showcase program for MeteoIO's statistical filters.
 * Michael Reisecker, 2019-07.
 */

#include <meteoio/MeteoIO.h>

#include <cmath>
#include <fstream>
#include <iostream>

int main(/* int argc, char** argv */)
{
	/*
	 * In the first part we simulate an observation according to a model function superimposed with noise.
	 * This will then be streamed into MeteoIO as measured data. In short, we fake two measurements.
	 */

	mio::RandomNumberGenerator RNGU; //noise generator for particle filter model
	RNGU.setDistribution(mio::RandomNumberGenerator::RNG_GAUSS);
	RNGU.setDistributionParameter("mean", 0);
	RNGU.setDistributionParameter("sigma", 0.1);
	mio::RandomNumberGenerator RNGV; //noise generator for particle filter observation
	RNGV.setDistribution(mio::RandomNumberGenerator::RNG_GAUSS);
	RNGV.setDistributionParameter("mean", 0);
	RNGV.setDistributionParameter("sigma", 0.1);
	mio::RandomNumberGenerator RNGK; //noise generator for Kalman filter observation
	RNGK.setDistribution(mio::RandomNumberGenerator::RNG_GAUSS);
	RNGK.setDistributionParameter("mean", 0);
	RNGK.setDistributionParameter("sigma", 3.3);

	//start date, end date, and sample rate of the real dataset:
	mio::Date sdate, edate;
	mio::IOUtils::convertString(sdate, "2018-12-01T00:50", 1.);
	mio::IOUtils::convertString(edate, "2018-12-31T23:50", 1.);
	static const double time_step = 1./24./6.;

	std::ofstream oss("./input/meteo/model.dat", std::ofstream::out);
	if (oss.fail())
		throw "File open operation failed.";
	oss << "# date model_no_noise observation control_signal speed\n"; //print header

	static const double aa = 0.999; //mockup model parameters
    static const double bb = 0.0005;
    static const double x_0 = 2.5; //initial value

	size_t kk = 0;
	double xx = x_0;
	static const double v_real = 10.; //scatter fake measurements around this "true" velocity
	mio::Date dt(sdate);
	while (dt <= edate) { //loop through timestamps present in the input data
		//simulate some measurements for the particle filter:
		const double uu = kk/500.; //control input
		xx = xx * aa + bb * uu; //system model
		oss << dt.toString(mio::Date::ISO, false) << "  " << xx << "  ";
		const double rr = RNGU.doub();
		//observation model plus system noise plus observation noise
		const double obs = (xx + rr) + RNGU.doub(); //here: obs(x) = x
		oss << obs << "  ";
		oss << uu << "  ";

		//simulate measurements for the Kalman filter:
		oss << v_real + RNGK.doub() << std::endl; //noisy velocity measurement

		kk++;
		dt += time_step;
	}
	oss.close();

	/*
	 * Now the mockup model data is in a csv file and will be read together with the real data.
	 * We can call MeteoIO as usual at this point.
	 */

	mio::Config cfg("io_statfilter.ini");
	mio::IOManager io(cfg);
	std::vector< std::vector<mio::MeteoData> > mvec;
	std::cout << "- Statistical filters example program -" << std::endl;
	std::cout << "Filtering..." << std::endl;
	mio::Timer TM;
	TM.start();
	io.getMeteoData(sdate, edate, mvec);
	TM.stop();
	io.writeMeteoData(mvec);
	std::cout << "The filtering took " << TM.getElapsed() << "s" <<  std::endl;
	std::cout << "Done." << std::endl;
	return 0;
}


/*
%Plot the kernel density estimation of particles generated by
%MeteoIO's particle filter in MATLAB.
%(Limits set for 40 time steps of the "literature model function" ex.)
particles = dlmread('./particles.dat', ' ');
figure
hold on;
xi = 1:40;
yi = -25:0.25:25;
kde = zeros(length(yi), length(xi));
for i = xi
   %KDE for  each time step
   kde(:, i) = ksdensity(particles(:, i), yi, 'kernel', 'epanechnikov');
   plot3(repmat(xi(i), length(yi), 1), yi', kde(:, i));
end
view(3)
 */
