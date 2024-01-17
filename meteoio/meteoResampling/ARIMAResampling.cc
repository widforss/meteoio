// SPDX-License-Identifier: LGPL-3.0-or-later
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

#include "ARIMAResampling.h" // change include to make it uniform
#include <unistd.h>

#include <sstream>

namespace mio {

ARIMAResampling::ARIMAResampling(const std::string& i_algoname, const std::string& i_parname, const double& dflt_window_size, const std::vector< std::pair<std::string, std::string> >& vecArgs)
             : ResamplingAlgorithms(i_algoname, i_parname, dflt_window_size, vecArgs), gap_data(), filled_data(), all_dates(), before_window(), after_window()
{
	const std::string where( "Interpolations1D::"+i_parname+"::"+i_algoname );
	if (vecArgs.empty()) //incorrect arguments, throw an exception
		throw InvalidArgumentException("Wrong number of arguments for \""+where+"\"", AT);
	
	before_window = after_window = 0.;
	for (size_t ii=0; ii<vecArgs.size(); ii++) {
		if (vecArgs[ii].first=="BEFORE_WINDOW") {
			IOUtils::parseArg(vecArgs[ii], where, before_window);
			before_window /= 86400.; //user uses seconds, internally julian day is used
		} else if (vecArgs[ii].first=="AFTER_WINDOW") {
			IOUtils::parseArg(vecArgs[ii], where, after_window);
			after_window /= 86400.; //user uses seconds, internally julian day is used
		} else if (vecArgs[ii].first=="MAX_P") {
			IOUtils::parseArg(vecArgs[ii], where, max_p);
		} else if (vecArgs[ii].first=="MAX_D") {
			IOUtils::parseArg(vecArgs[ii], where, max_d);
		} else if (vecArgs[ii].first=="MAX_Q") {
			IOUtils::parseArg(vecArgs[ii], where, max_q);
		} else if (vecArgs[ii].first=="MAX_P_SEASONAL") {
			IOUtils::parseArg(vecArgs[ii], where, max_P);
		} else if (vecArgs[ii].first=="MAX_D_SEASONAL") {
			IOUtils::parseArg(vecArgs[ii], where, max_D);
		} else if (vecArgs[ii].first=="MAX_Q_SEASONAL") {
			IOUtils::parseArg(vecArgs[ii], where, max_Q);
		} else if (vecArgs[ii].first=="SEASONAL_PERIOD") {
			IOUtils::parseArg(vecArgs[ii], where, period);
		} else if (vecArgs[ii].first=="LIK_METHOD") {
			IOUtils::parseArg(vecArgs[ii], where, method);
		} else if (vecArgs[ii].first=="OPTIMIZATION_METHOD") {
			IOUtils::parseArg(vecArgs[ii], where, opt_method);
		} else if (vecArgs[ii].first=="STEPWISE") {
			IOUtils::parseArg(vecArgs[ii], where, stepwise);
		} else if (vecArgs[ii].first=="APPROXIMATION") {
			IOUtils::parseArg(vecArgs[ii], where, approximation);
		} else if (vecArgs[ii].first=="NUM_MODELS") {
			IOUtils::parseArg(vecArgs[ii], where, num_models);
		} else if (vecArgs[ii].first=="SEASONAL") {
			IOUtils::parseArg(vecArgs[ii], where, seasonal);
		} else if (vecArgs[ii].first=="STATIONARY") {
			IOUtils::parseArg(vecArgs[ii], where, stationary);
		}
		else {
			throw InvalidArgumentException("Unknown argument \""+vecArgs[ii].first+"\" for \""+where+"\"", AT);
		}
	}

	if (!(before_window!=0 || after_window!=0)) throw InvalidArgumentException("Please provide a ARIMA window for "+where, AT);
	if (before_window+after_window > window_size) throw InvalidArgumentException("The ARIMA window is larger than the resampling window for "+where, AT);
}

std::string ARIMAResampling::toString() const
{
	//this should help when debugging, so output relevant parameters for your algorithm
	std::ostringstream ss;
	ss << std::right << std::setw(10) << parname << "::"  << std::left << std::setw(15) << algo << "[ ]" << std::endl;
	ss << "  Amount of found gaps " << gap_data.size() << std::endl;
	ss << "  Amount of filled data " << filled_data.size() << std::endl;
	ss << "  Amount of dates " << all_dates.size() << std::endl;
	return ss.str();
}


double ARIMAResampling::interpolVecAt(const std::vector<MeteoData> &vecMet,const size_t &idx, const Date &date, const size_t &paramindex) {
	MeteoData p1 = vecMet[idx];
	MeteoData p2 = vecMet[idx+1];
    return linearInterpolation(p1.date.getJulian(true), p1(paramindex), p2.date.getJulian(true), p2(paramindex), date.getJulian(true));
}

double ARIMAResampling::interpolVecAt(const std::vector<double> &data, const std::vector<Date> &datesVec, const size_t &pos, const Date &date) {
	return linearInterpolation(datesVec[pos].getJulian(), data[pos], datesVec[pos+1].getJulian(), data[pos+1], date.getJulian(true));
}

void fillGapWithPrediction(std::vector<double>& data, const std::string& direction, const size_t &startIdx, const int &length, const int &period) {
	InterpolARIMA arima(data, length, direction, period);
    std::vector<double> predictions = arima.predict();
    std::copy(predictions.begin(), predictions.end(), data.begin() + startIdx);
}

std::vector<Date>::iterator findDate(std::vector<Date>& gap_dates, Date& resampling_date) {
    auto exactTime = [&resampling_date](Date curr_date) {return requal(curr_date, resampling_date);};
    return std::find_if(gap_dates.begin(), gap_dates.end(), exactTime);
}



void ARIMAResampling::resample(const std::string& /*stationHash*/, const size_t& index, const ResamplingPosition& position, const size_t& paramindex,
                            const std::vector<MeteoData>& vecM, MeteoData& md)
{
	if (index >= vecM.size()) throw IOException("The index of the element to be resampled is out of bounds", AT);

	if (position == ResamplingAlgorithms::exact_match) {
		std::cout << "Exact match" << std::endl;
		const double value = vecM[index](paramindex);
		if (value != IOUtils::nodata) {
			md(paramindex) = value; //propagate value
			return;
		}
	}

	Date resampling_date = md.date;
	// check wether given position is in a known gap, if it is either return the 
	// exact value or linearly interpolate, to get the correct value
	std::cout << "resampling date : " << resampling_date.toString(Date::ISO) << std::endl;
	std::cout << "gaps : " << gap_data.size() << std::endl;
	for (size_t ii = 0; ii < gap_data.size(); ii++) {
		ARIMA_GAP gap = gap_data[ii];
		std::vector<Date> gap_dates = all_dates[ii];
		std::cout << "gap dates : " << gap_dates.size() << std::endl;
		std::vector<double> data_in_gap = filled_data[ii];
		std::cout << "data in gap : " << data_in_gap.size() << std::endl;
		std::cout << "gap : " << gap.toString() << std::endl;
		if (resampling_date >= gap.startDate && resampling_date <= gap.endDate) {
			std::cout << "In a known gap ARIMA" << std::endl;
			auto it = findDate(gap_dates, resampling_date);
			// if there is an exact match, return the data
			sleep(1);
			if (it != gap_dates.end()) {
				std::cout << "using cached data" << std::endl;
				size_t idx = std::distance(gap_dates.begin(), it);
				md(paramindex) = data_in_gap[idx];
				return;
			} else {
				// otherwise linearly interpolate the data
				std::cout << "linearly interpolating" << std::endl;
				size_t idx = std::distance(gap_dates.begin(), std::lower_bound(gap_dates.begin(), gap_dates.end(), resampling_date));
				md(paramindex) = interpolVecAt(data_in_gap, gap_dates, idx, resampling_date);
				return;
			}
		}
	}

	// if it is not in a known gap, cache the gap, and interpolate it for subsequent calls
	size_t gap_start = IOUtils::npos;
	size_t gap_end = IOUtils::npos;
	ARIMA_GAP new_gap;
	computeARIMAGap(new_gap, index, paramindex, vecM, resampling_date, gap_start, gap_end, before_window, after_window);
	
	std::cout << "in a new gap ARIMA" << std::endl;
	std::cout << "new gap: " << new_gap.toString() << std::endl;
	if (new_gap.isGap()) {
		std::cout << "gap found" << std::endl;
		Date data_start_date = new_gap.startDate - before_window;
		Date data_end_date = new_gap.endDate + after_window;		

		// data vector is of length (data_end_date - data_start_date) * sampling_rate
		int length = static_cast<int>((data_end_date - data_start_date).getJulian(true) * new_gap.sampling_rate);
		std::vector<double> data(length);
		std::vector<Date> dates(length);
		// get a vector of meteodata that contains the data before and after the gap
		std::vector<MeteoData> data_vec_before;
		std::vector<MeteoData> data_vec_after;
		// get the data before and after the gap
		for (size_t ii=0; ii<vecM.size(); ii++) {
			if (vecM[ii].date >= data_start_date && ii <= gap_start) {
				data_vec_before.push_back(vecM[ii]);
			}
			if (ii >= gap_end && vecM[ii].date <= data_end_date) {
				data_vec_after.push_back(vecM[ii]);
			}
		}

		if (data_vec_before.size() < 10 && data_vec_after.size() < 10) {
			std::cerr << "Not enough data to interpolate the gap" << std::endl;
			std::cerr << "Datapoints before the gap: " << data_vec_before.size() << std::endl;
			std::cerr << "Datapoints after the gap: " << data_vec_after.size() << std::endl;
			return;
		}

		// resample to the desired sampling rate
		int length_gap_interpol = 0;
		size_t startIdx_interpol = IOUtils::npos;
		std::cout << "resampling to the desired rate " << std::endl;
		for (int i =0; i < length; i++) {
			Date date = data_start_date + i * new_gap.sampling_rate;
			dates[i] = date;
			if (date >=data_start_date && date < new_gap.startDate) {
				if (requal(date, data_vec_before[i].date)) {
					data[i] = data_vec_before[i](paramindex);
				} else {
					std::cout << "interpolating vec before " << std::endl;
					// linearly interpolate the data
					data[i] = interpolVecAt(data_vec_before, i, date, paramindex);
				}
			} else if (date >= new_gap.startDate && date <= new_gap.endDate) {
				if (startIdx_interpol == IOUtils::npos) {
					std::cout << "should just appear once, hii" << std::endl;
					startIdx_interpol = i;
				}
				// the data is missing, so set it to IOUtils::nodata
				data[i] = IOUtils::nodata;
			} else if (date > new_gap.endDate && date <= data_end_date) {
				if (length_gap_interpol == 0) {
					std::cout << "should just appear once, byeee" << std::endl;
					length_gap_interpol = i - 	static_cast<int>(startIdx_interpol)-1;
				}
				if (requal(date, data_vec_after[i].date)) {
					data[i] = data_vec_after[i](paramindex);
				} else {
					std::cout << "interpolating vec after " << std::endl;
					// linearly interpolate the data
					data[i] = interpolVecAt(data_vec_after, i, date, paramindex);
				}
			}
		}

		// now fill the data with the arima model
		// either by interpolating or predicting forward or backward
		if (data_vec_before.size()<10) {
			std::cout << "predicting backward" << std::endl;
			std::cout << "with data size: " << data_vec_after.size() << std::endl;
			fillGapWithPrediction(data, "backward", startIdx_interpol, length_gap_interpol, period);
		} else if (data_vec_after.size()<10) {
			std::cout << "predicting forward" << std::endl;
			std::cout << "with data size: " << data_vec_before.size() << std::endl;
			fillGapWithPrediction(data, "forward", startIdx_interpol, length_gap_interpol, period);
		} else {
			std::cout << "interpolating" << std::endl;
			InterpolARIMA arima(data, startIdx_interpol, length_gap_interpol, period);
			arima.fillGap();
		}
		
		std::cout << "pushing data to the vectors" << std::endl;
		gap_data.push_back(new_gap);
		std::vector<double> interpolated_data(data.begin() + startIdx_interpol, data.begin() + startIdx_interpol + length_gap_interpol);
		std::vector<MeteoData> interpolated_dates(dates.begin() + startIdx_interpol, dates.begin() + startIdx_interpol + length_gap_interpol);
		filled_data.push_back(interpolated_data);
		all_dates.push_back(dates);

		sleep(2);
		auto it = findDate(dates, resampling_date);
		// if there is an exact match, return the data
		if (it != dates.end()) {
			std::cout << "using cached data in a new gap" << std::endl;
			size_t idx = std::distance(dates.begin(), it);
			md(paramindex) = data[idx];
			return;
		} else {
			// otherwise linearly interpolate the data
			std::cout << "linearly interpolating in new gap" << std::endl;
			size_t idx = std::distance(dates.begin(), std::lower_bound(dates.begin(), dates.end(), resampling_date));
			md(paramindex) = interpolVecAt(data, dates, idx, resampling_date);
			return;
		}

	} else {
		sleep(2);
		std::cout << "no gap found, interpolating" << std::endl;
		// linearly interpolate the point
		double start_value = vecM[gap_start-1](paramindex);
		double end_value = vecM[gap_end+1](paramindex);
		Date start_date = vecM[gap_start-1].date;
		Date end_date = vecM[gap_end+1].date;
		md(paramindex) = linearInterpolation(start_date.getJulian(true), start_value, end_date.getJulian(true), end_value, resampling_date.getJulian(true));
		return;
	}
	std::cerr << "something wen seriously wrong in the arima interpolations" << std::endl;
	return;
}
} //namespace
