/***********************************************************************************/
/*                   Copyright GridGroup, EIA-FR 2010                              */
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
//AUTHORS: Tuan Anh Nguyen (original implementation in popc)
//         Mathias Bavay (port and rewrite for Alpine3D, then MeteoIO)

#include <stdio.h>
#ifdef _WIN32
	#include <windows.h>
	#undef max
	#undef min
#else
	#include <sys/time.h>
#endif

#include "Timer.h"

namespace mio {

/**
* @brief Default constructor. 
* Initialize internal variables. It does NOT start timing.
*/
Timer::Timer() {
	isRunning=false;
	start_point=0;
	elapsed=0;
}

/**
* @brief Start the timer. 
*/
void Timer::start() {
	if (!isRunning) {
		isRunning=true;
		start_point=getCurrentTime();
	}
}

/**
* @brief Stop the timer. 
* It can be restarted afterward, adding time to what was already timed.
*/
void Timer::stop() {
	if (isRunning) {
		elapsed+=getCurrentTime()-start_point;
		isRunning=false;
	}
}

/**
* @brief Reset the timer to zero.
*/
void Timer::reset() {
	start_point=getCurrentTime();
	elapsed=0;
}

/**
* @brief Get total elapsed time. 
* It returns the sum of all the elapsed time between all the start/stop sessions since
* the timer was created or the last call to reset. Time is in seconds with microsecond resolution.
*/
double Timer::getElapsed() {
	if (isRunning) {
		return elapsed+getCurrentTime()-start_point;
	}
	return elapsed;
}

#ifdef _WIN32
double Timer::getCurrentTime() {
	SYSTEMTIME systemTime;
	GetSystemTime( &systemTime );

	FILETIME fileTime;
	SystemTimeToFileTime( &systemTime, &fileTime );

	ULARGE_INTEGER uli;
	uli.LowPart = fileTime.dwLowDateTime;
	uli.HighPart = fileTime.dwHighDateTime;

	ULONGLONG units_convert = 10000*1000; //it gives the time since 1 January 1601 (UTC) in units of 100ns
	return (double)(uli.QuadPart/units_convert - 11644473600L); //offset in seconds to Unix epoch
	//this is 134774 days * 24*3600
}
#else
double Timer::getCurrentTime() {
	timeval tp;
	gettimeofday(&tp,NULL);
	const double t=tp.tv_sec+double(tp.tv_usec)*1.0e-6;
	return t;
}
#endif

} //namespace

