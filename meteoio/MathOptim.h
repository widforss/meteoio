/***********************************************************************************/
/*  Copyright 2012 WSL Institute for Snow and Avalanche Research    SLF-DAVOS      */
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
#ifndef __MATHOPTIM_H__
#define __MATHOPTIM_H__

#include <stdint.h>
#include <cmath>

//Quake3 fast 1/x² approximation
// For Magic Derivation see: Chris Lomont http://www.lomont.org/Math/Papers/2003/InvSqrt.pdf
// Credited to Greg Walsh.
// 32  Bit float magic number - for 64 bits doubles: 0x5fe6ec85e7de30da
#define SQRT_MAGIC_D 0x5f3759df
#define SQRT_MAGIC_F 0x5f375a86

namespace mio {

namespace Optim {

	/**
	* @brief Optimized version of c++ round()
	* This version works with positive and negative numbers but does not
	* comply with IEEE handling of border cases (like infty, Nan, etc).
	* Please benchmark your code before deciding to use this!!
	* @param x number to round
	* @return rounded number cast as int
	*/
	inline long int round(const double& x) {
		if(x>=0.) return static_cast<long int>( x+.5 );
		else return static_cast<long int>( x-.5 );
	}

	/**
	* @brief Optimized version of c++ floor()
	* This version works with positive and negative numbers but does not
	* comply with IEEE handling of border cases (like infty, Nan, etc).
	* Please benchmark your code before deciding to use this!!
	* @param x number to floor
	* @return floored number cast as int
	*/
	inline long int floor(const double& x) {
		const long int xi = static_cast<long int>(x);
		if (x >= 0 || static_cast<double>(xi) == x) return xi ;
		else return xi - 1 ;
	}

	/**
	* @brief Optimized version of c++ ceil()
	* This version works with positive and negative numbers but does not
	* comply with IEEE handling of border cases (like infty, Nan, etc).
	* Please benchmark your code before deciding to use this!!
	* @param x number to ceil
	* @return ceiled number cast as int
	*/
	inline long int ceil(const double& x) {
		const long int xi = static_cast<long int>(x);
		if (x <= 0 || static_cast<double>(xi) == x) return xi ;
		else return xi + 1 ;
	}

	inline double intPart(const double &x) {
		double intpart;
		modf(x, &intpart);
		return intpart;
	}

	inline double fracPart(const double &x) {
		double intpart;
		return modf(x, &intpart);
	}

	#ifdef _MSC_VER
	#pragma warning( push ) //for Visual C++
	#pragma warning(disable:4244) //Visual C++ righhtfully complains... but this behavior is what we want!
	#endif
	//maximum relative error is <1.7% while computation time for sqrt is <1/4. At 0, returns a large number
	//on a large scale interpolation test on TA, max relative error is 1e-6
	inline float invSqrt(const float x) {
		const float xhalf = 0.5f*x;

		union { // get bits for floating value
			float x;
			int i;
		} u;
		u.x = x;
		u.i = SQRT_MAGIC_F - (u.i >> 1);  // gives initial guess y0
		return u.x*(1.5f - xhalf*u.x*u.x);// Newton step, repeating increases accuracy
	}

	inline double invSqrt(const double x) {
		const double xhalf = 0.5f*x;

		union { // get bits for floating value
			float x;
			int i;
		} u;
		u.x = static_cast<float>(x);
		u.i = SQRT_MAGIC_D - (u.i >> 1);  // gives initial guess y0
		return u.x*(1.5f - xhalf*u.x*u.x);// Newton step, repeating increases accuracy
	}
	#ifdef _MSC_VER
	#pragma warning( pop ) //for Visual C++, restore previous warnings behavior
	#endif

	inline float fastSqrt_Q3(const float x) {
		return x * invSqrt(x);
	}

	inline double fastSqrt_Q3(const double x) {
		return x * invSqrt(x);
	}

	inline double pow2(const double& val) {return (val*val);}
	inline double pow3(const double& val) {return (val*val*val);}
	inline double pow4(const double& val) {return (val*val*val*val);}

	//please do not use this method directly, call fastPow() instead!
	inline double fastPowInternal(double a, double b) {
	//see http://martin.ankerl.com/2012/01/25/optimized-approximative-pow-in-c-and-cpp/
		// calculate approximation with fraction of the exponent
		int e = (int) b;
		union {
			double d;
			int x[2];
		} u = { a };
		u.x[1] = (int)((b - e) * (u.x[1] - 1072632447) + 1072632447);
		u.x[0] = 0;

		// exponentiation by squaring with the exponent's integer part
		// double r = u.d makes everything much slower, not sure why
		double r = 1.0;
		while (e) {
			if (e & 1) {
				r *= a;
			}
			a *= a;
			e >>= 1;
		}

		return r * u.d;
	}

	/**
	* @brief Optimized version of c++ pow()
	* This version works with positive and negative exponents and handles exponents bigger than 1.
	* The relative error remains less than 6% for the benachmarks that we ran (argument between 0 and 500
	* and exponent between -10 and +10). It is ~3.3 times faster than cmath's pow().
	* Source: http://martin.ankerl.com/2012/01/25/optimized-approximative-pow-in-c-and-cpp/
	*
	* Please benchmark your code before deciding to use this!!
	* @param a argument
	* @param b exponent
	* @return a^b
	*/
	inline double fastPow(double a, double b) {
		if(b>0.) {
			return fastPowInternal(a,b);
		} else {
			const double tmp = fastPowInternal(a,-b);
			return 1./tmp;
		}
	}

	//see http://metamerist.com/cbrt/cbrt.htm
	template <int n> inline float nth_rootf(float x) {
		const int ebits = 8;
		const int fbits = 23;

		const int bias = (1 << (ebits-1))-1;
		int& i = reinterpret_cast<int&>(x);
		i = (i - (bias << fbits)) / n + (bias << fbits);

		return x;
	}

	template <int n> inline double nth_rootd(double x) {
		const int ebits = 11;
		const int fbits = 52;

		const int64_t bias = (1 << (ebits-1))-1;
		int64_t& i = reinterpret_cast<int64_t&>(x);
		i = (i - (bias << fbits)) / n + (bias << fbits);

		return x;
	}

	/**
	* @brief Optimized version of cubic root
	* This version is based on a single iteration Halley's method (see https://en.wikipedia.org/wiki/Halley%27s_method)
	* with a seed provided by a bit hack approximation. It should offer 15-16 bits precision and be three times
	* faster than pow(x, 1/3). In some test, between -500 and +500, the largest relative error was 1.2e-4.
	* Source:  Otis E. Lancaster, Machine Method for the Extraction of Cube Root Journal of the American Statistical Association, Vol. 37, No. 217. (Mar., 1942), pp. 112-115.
	* and http://metamerist.com/cbrt/cbrt.htm
	*
	* Please benchmark your code before deciding to use this!!
	* @param x argument
	* @return x^(1/3)
	*/
	inline double cbrt(double x) {
		const double a = nth_rootd<3>(x);
		const double a3 = a*a*a;
		const double b = a * ( (a3 + x) + x) / ( a3 + (a3 + x) );
		return b; //single iteration, otherwise set a=b and do it again
	}

}

} //end namespace

#endif
