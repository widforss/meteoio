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

#include <meteoio/meteoResampling/ARIMAResampling.h>
#include <unistd.h>

#include <sstream>

namespace mio {

    ARIMAResampling::ARIMAResampling(const std::string &i_algoname, const std::string &i_parname, const double &dflt_window_size,
                                     const std::vector<std::pair<std::string, std::string>> &vecArgs)
        : ResamplingAlgorithms(i_algoname, i_parname, dflt_window_size, vecArgs), gap_data(), filled_data(), all_dates(), before_window(),
          after_window(), is_valid_gap_data(), warned_about_gap() {
        const std::string where("Interpolations1D::" + i_parname + "::" + i_algoname);
        if (vecArgs.empty()) // incorrect arguments, throw an exception
            throw InvalidArgumentException("Wrong number of arguments for \"" + where + "\"", AT);

        before_window = after_window = 0.;
        std::string method_string;
        std::string opt_method_string;
        for (size_t ii = 0; ii < vecArgs.size(); ii++) {
            if (vecArgs[ii].first == "BEFORE_WINDOW") {
                IOUtils::parseArg(vecArgs[ii], where, before_window);
                before_window /= 86400.; // user uses seconds, internally julian day is used
            } else if (vecArgs[ii].first == "AFTER_WINDOW") {
                IOUtils::parseArg(vecArgs[ii], where, after_window);
                after_window /= 86400.; // user uses seconds, internally julian day is used
            } else if (vecArgs[ii].first == "MAX_P") {
                IOUtils::parseArg(vecArgs[ii], where, max_p);
            } else if (vecArgs[ii].first == "MAX_D") {
                IOUtils::parseArg(vecArgs[ii], where, max_d);
            } else if (vecArgs[ii].first == "MAX_Q") {
                IOUtils::parseArg(vecArgs[ii], where, max_q);
            } else if (vecArgs[ii].first == "MAX_P_SEASONAL") {
                IOUtils::parseArg(vecArgs[ii], where, max_P);
            } else if (vecArgs[ii].first == "MAX_D_SEASONAL") {
                IOUtils::parseArg(vecArgs[ii], where, max_D);
            } else if (vecArgs[ii].first == "MAX_Q_SEASONAL") {
                IOUtils::parseArg(vecArgs[ii], where, max_Q);
            } else if (vecArgs[ii].first == "SEASONAL_PERIOD") {
                IOUtils::parseArg(vecArgs[ii], where, period);
                period /= 86400.;
            } else if (vecArgs[ii].first == "LIK_METHOD") {
                IOUtils::parseArg(vecArgs[ii], where, method_string);
                if (method_string == "CSS_MLE")
                    method = CSS_MLE;
                else if (method_string == "MLE")
                    method = MLE;
                else if (method_string == "CSS")
                    method = CSS;
                else
                    throw InvalidArgumentException("Unknown argument \"" + vecArgs[ii].first + "\" for \"" + where + "\"", AT);
            } else if (vecArgs[ii].first == "OPTIMIZATION_METHOD") {
                IOUtils::parseArg(vecArgs[ii], where, opt_method_string);
                if (opt_method_string == "Nelder_Mead")
                    opt_method = Nelder_Mead;
                else if (opt_method_string == "Newton_Line_Search")
                    opt_method = Newton_Line_Search;
                else if (opt_method_string == "Newton_Trust_Region_Hook_Step")
                    opt_method = Newton_Trust_Region_Hook_Step;
                else if (opt_method_string == "Newton_Trust_Region_Double_Dog_Leg")
                    opt_method = Newton_Trust_Region_Double_Dog_Leg;
                else if (opt_method_string == "Conjugate_Gradient")
                    opt_method = Conjugate_Gradient;
                else if (opt_method_string == "BFGS")
                    opt_method = BFGS;
                else if (opt_method_string == "Limited_Memory_BFGS")
                    opt_method = Limited_Memory_BFGS;
                else if (opt_method_string == "BFGS_Using_More_Thuente_Method")
                    opt_method = BFGS_Using_More_Thuente_Method;
                else
                    throw InvalidArgumentException("Unknown argument \"" + vecArgs[ii].first + "\" for \"" + where + "\"", AT);
            } else if (vecArgs[ii].first == "STEPWISE") {
                IOUtils::parseArg(vecArgs[ii], where, stepwise);
            } else if (vecArgs[ii].first == "APPROXIMATION") {
                IOUtils::parseArg(vecArgs[ii], where, approximation);
            } else if (vecArgs[ii].first == "NUM_MODELS") {
                IOUtils::parseArg(vecArgs[ii], where, num_models);
            } else if (vecArgs[ii].first == "SEASONAL") {
                IOUtils::parseArg(vecArgs[ii], where, seasonal);
            } else if (vecArgs[ii].first == "STATIONARY") {
                IOUtils::parseArg(vecArgs[ii], where, stationary);
            } else {
                throw InvalidArgumentException("Unknown argument \"" + vecArgs[ii].first + "\" for \"" + where + "\"", AT);
            }
        }

        if (!(before_window != 0 || after_window != 0))
            throw InvalidArgumentException("Please provide a ARIMA window for " + where, AT);
        if (before_window + after_window > window_size)
            throw InvalidArgumentException("The ARIMA window is larger than the resampling window for " + where, AT);
    }



    std::string ARIMAResampling::toString() const {
        // this should help when debugging, so output relevant parameters for your algorithm
        std::ostringstream ss;
        ss << std::right << std::setw(10) << parname << "::" << std::left << std::setw(15) << algo << "[ ]" << std::endl;
        ss << "  Amount of found gaps " << gap_data.size() << std::endl;
        ss << "  Amount of filled data " << filled_data.size() << std::endl;
        ss << "  Amount of dates " << all_dates.size() << std::endl;
        ss << "  interpolated data: " << std::endl;
        ss << convertVectorsToString(filled_data);
        return ss.str();
    }

    // ------------------------------ Private functions ------------------------------



    std::vector<double> ARIMAResampling::predictData(std::vector<double> &data, const std::string &direction, size_t startIdx_interpol,
                                                     size_t length_gap_interpol, int sr_period) {
#ifdef DEBUG
        std::cout << "predicting " << direction << std::endl;
#endif
        InterpolARIMA arima(data, startIdx_interpol, length_gap_interpol, direction, sr_period);
        setMetaData(arima);
        std::vector<double> predictions = arima.predict();
        std::copy(predictions.begin(), predictions.end(), data.begin() + startIdx_interpol);
        return predictions;
    }

    void ARIMAResampling::setMetaData(InterpolARIMA &arima) {
        arima.setAutoArimaMetaData(max_p, max_d, max_q, start_p, start_q, max_P, max_D, max_Q, start_P, start_Q, seasonal, stationary);
        arima.setOptMetaData(method, opt_method, stepwise, approximation, num_models);
    }



    // ------------------------------ Resample helper functions ------------------------------



    double ARIMAResampling::interpolVecAt(const std::vector<MeteoData> &vecMet, const size_t &idx, const Date &date,
                                          const size_t &paramindex) {
        if (idx >= vecMet.size())
            throw IOException("The index of the element to be resampled is out of bounds", AT);
        else if (idx == vecMet.size() - 1) {
#ifdef DEBUG
            std::cout << "Extrapolation needed to gather ARIMA data, will use constant value";
#endif
            return vecMet[idx](paramindex);
        }
        MeteoData p1 = vecMet[idx];
        MeteoData p2 = vecMet[idx + 1];
        return linearInterpolation(p1.date.getJulian(true), p1(paramindex), p2.date.getJulian(true), p2(paramindex), date.getJulian(true));
    }



    double ARIMAResampling::interpolVecAt(const std::vector<double> &data, const std::vector<Date> &datesVec, const size_t &pos,
                                          const Date &date) {
        // Check if the position is out of bounds
        if (pos >= data.size()) {
            throw IOException("The index of the element to be resampled is out of bounds, for date: " + date.toString(Date::ISO), AT);
        }

        // Check if the position is the last element in the data
        if (pos == data.size() - 1) {
#ifdef DEBUG
            std::cout << "Extrapolation needed to gather ARIMA data, will use constant value";
#endif
            return data[pos];
        }

        // If the position is valid and not the last element, perform linear interpolation
        double x1 = datesVec[pos].getJulian();
        double y1 = data[pos];
        double x2 = datesVec[pos + 1].getJulian();
        double y2 = data[pos + 1];
        double x = date.getJulian(true);

        return linearInterpolation(x1, y1, x2, y2, x);
    }



    static std::vector<Date>::iterator findDate(std::vector<Date> &gap_dates, const Date &resampling_date) {
        auto exactTime = [&resampling_date](Date curr_date) { return requal(curr_date, resampling_date); };
        return std::find_if(gap_dates.begin(), gap_dates.end(), exactTime);
    }

    
    
    void ARIMAResampling::checkZeroPossibility(const std::vector<MeteoData> &vecM, size_t paramindex) {
        if (!checked_vecM) {
            // check if there are any zero values in the data, to see if a prediction of zero makes sense
            double sum = 0.0, sumOfSquares = 0.0, mean, standardDeviation = 0.0;

            for (size_t ii = 0; ii < vecM.size(); ++ii) {
                double value = vecM[ii](paramindex);
                sum += value;
                sumOfSquares += value * value;
            }

            double num = static_cast<double>(vecM.size());
            mean = sum / num;
            standardDeviation = sqrt(sumOfSquares / num - mean * mean);

            for (size_t ii = 0; ii < vecM.size(); ii++) {
                if (vecM[ii](paramindex) <= standardDeviation) {
                    is_zero_possible = true;
                    break;
                }
            }

            checked_vecM = true;
        }
    }



    bool ARIMAResampling::processKnownGaps(const Date &resampling_date, const size_t paramindex,
                                           const ResamplingAlgorithms::ResamplingPosition &position, const std::vector<MeteoData> &vecM,
                                           MeteoData &md) {
        // check whether given position is in a known gap, if it is either return the
        // exact value or linearly interpolate, to get the correct value
        for (size_t ii = 0; ii < gap_data.size(); ii++) {
            ARIMA_GAP gap = gap_data[ii];
            std::vector<Date> gap_dates = all_dates[ii];
            std::vector<double> data_in_gap = filled_data[ii];
            bool is_valid_data = is_valid_gap_data[ii];

            // check if the resampling date is in this gap
            if (resampling_date >= gap.startDate && resampling_date <= gap.endDate) {
                if (!is_valid_data) {
                    if (!warned_about_gap[ii]) {
                        std::cerr << "Could not find a useful ARIMA model, try other parameters, or another interpolation algorithm for "
                                     "data in between "
                                  << gap.startDate.toString(Date::ISO) << "---" << gap.endDate.toString(Date::ISO) << "||" << std::endl;
                        warned_about_gap[ii] = true;
                    }
                    return true;
                }

                auto it = findDate(gap_dates, resampling_date);
                // if there is an exact match, return the data
                if (it != gap_dates.end()) {
                    size_t idx = std::distance(gap_dates.begin(), it);
                    double value = data_in_gap[idx];
                    if (value != IOUtils::nodata) {
                        md(paramindex) = value; // propagate value
                        return true;
                    }
                    break;
                } else {
                    // otherwise linearly interpolate the data
                    size_t idx = std::distance(gap_dates.begin(), std::lower_bound(gap_dates.begin(), gap_dates.end(), resampling_date));

                    // if the index is at the end, and thus does not have a next value
                    if (idx == gap_dates.size() - 1)
                        break;
                    else if ((data_in_gap[idx] == IOUtils::nodata || data_in_gap[idx + 1] == IOUtils::nodata))
                        break;
                    md(paramindex) = interpolVecAt(data_in_gap, gap_dates, idx, resampling_date);
                    return true;
                }

            } else if (position == ResamplingAlgorithms::end && resampling_date > gap.endDate && resampling_date >= gap.startDate &&
                       gap.startDate == vecM[vecM.size() - 1].date) {

                if (!is_valid_data) {
                    if (!warned_about_gap[ii]) {
                        std::cerr << "Could not find a useful ARIMA model, try other parameters, or another interpolation algorithm for "
                                     "data in between "
                                  << gap.startDate.toString(Date::ISO) << "---" << gap.endDate.toString(Date::ISO) << "||" << std::endl;
                        warned_about_gap[ii] = true;
                    }
                    return true;
                }
                if (!gave_warning_end) {
                    std::cerr << "Extrapolating more than 25 steps into the future is pointless, last known data point: "
                              << gap.startDate.toString(Date::ISO) << "||" << std::endl;
                    gave_warning_end = true;
                }
                return true;
            }
        }
        return false;
    }



    void ARIMAResampling::setEndGap(ARIMA_GAP &new_gap, Date &data_start_date, Date &data_end_date, const std::vector<MeteoData> &vecM,
                                    const Date &resampling_date) {
        new_gap.startDate = vecM[vecM.size() - 1].date;
        new_gap.start = vecM.size() - 1;
        data_start_date = new_gap.startDate - window_size;
        new_gap.sampling_rate = computeSamplingRate(data_start_date, new_gap.startDate, vecM);
        new_gap.endDate = resampling_date + MAX_ARIMA_EXTRAPOLATION / new_gap.sampling_rate;
        new_gap.end = vecM.size() - 1;
        data_end_date = new_gap.endDate;
        data_start_date = adjustStartDate(vecM, new_gap, data_start_date, data_end_date);
    }



    static bool accumulateData(const Date &data_start_date, const Date &data_end_date, const ARIMA_GAP &new_gap,
                        const std::vector<MeteoData> &vecM, bool &gave_warning_interpol, size_t &length, std::vector<double> &data,
                        std::vector<Date> &dates, std::vector<MeteoData> &data_vec_before, std::vector<MeteoData> &data_vec_after,
                        bool &has_data_before, bool &has_data_after) {
        // data vector is of length (data_end_date - data_start_date) * sampling_rate
        length = static_cast<size_t>((data_end_date - data_start_date).getJulian(true) * new_gap.sampling_rate) +
                 1; // otherwise end date is not included
        data.resize(length);
        dates.resize(length);

        // get a vector of meteodata that contains the data before and after the gap
        for (size_t ii = 0; ii < vecM.size(); ii++) {
            Date date_v = vecM[ii].date;
            if (date_v >= data_start_date && date_v <= new_gap.startDate) {
                data_vec_before.push_back(vecM[ii]);
            }
            if (date_v >= new_gap.endDate && date_v <= data_end_date) {
                data_vec_after.push_back(vecM[ii]);
            }
        }

        has_data_before = data_vec_before.size() > 1;
        has_data_after = data_vec_after.size() > 1;

        if (data_vec_before.size() < MIN_ARIMA_DATA_POINTS && data_vec_after.size() < MIN_ARIMA_DATA_POINTS) {
            if (!gave_warning_interpol) {
                std::cerr << "Not enough data to interpolate the gap" << std::endl;
                std::cerr << new_gap.toString() << std::endl;
                std::cerr << "Datapoints before the gap: " << data_vec_before.size() << std::endl;
                std::cerr << "Datapoints after the gap: " << data_vec_after.size() << "||" << std::endl;
                gave_warning_interpol = true;
            }
            return false;
        }
        return true;
    }



    void ARIMAResampling::resampleInterpolationData(size_t &length_gap_interpol, size_t &endIdx_interpol, size_t &startIdx_interpol,
                                                    const ARIMA_GAP &new_gap, const Date &data_start_date, const Date &data_end_date,
                                                    const std::vector<MeteoData> &data_vec_before,
                                                    const std::vector<MeteoData> &data_vec_after, bool has_data_before, bool has_data_after,
                                                    size_t paramindex, std::vector<double> &data, std::vector<Date> &dates, size_t length) {
        for (size_t i = 0; i < length; i++) {
            Date date = data_start_date + static_cast<double>(i) / new_gap.sampling_rate;
            dates[i] = date;

            bool isBeforeGap = date >= data_start_date && date <= new_gap.startDate;
            bool isAfterGap = date >= new_gap.endDate && date <= data_end_date;
            bool isWithinGap = date > new_gap.startDate && date < new_gap.endDate;

            if (isAfterGap && length_gap_interpol == 0) {
                length_gap_interpol = i - startIdx_interpol;
                endIdx_interpol = i;
            }

            if (isBeforeGap && has_data_before && i < data_vec_before.size()) {
                data[i] = requal(date, data_vec_before[i].date) ? data_vec_before[i](paramindex)
                                                                : interpolVecAt(data_vec_before, i, date, paramindex);
            } else if (isAfterGap && has_data_after && i - endIdx_interpol < data_vec_after.size()) {
                size_t after_id = i - endIdx_interpol;
                data[i] = requal(date, data_vec_after[after_id].date) ? data_vec_after[after_id](paramindex)
                                                                      : interpolVecAt(data_vec_after, after_id, date, paramindex);
            } else if (isWithinGap && startIdx_interpol == IOUtils::npos) {
                startIdx_interpol = i;
                data[i] = IOUtils::nodata;
            } else {
                data[i] = IOUtils::nodata;
            }
        }

        if (data_end_date == new_gap.endDate)
            length_gap_interpol = data.size() - startIdx_interpol;
    }



    std::vector<double> ARIMAResampling::getInterpolatedData(std::vector<double> &data, size_t size_before, size_t size_after,
                                                             size_t startIdx_interpol, size_t length_gap_interpol, int sr_period) {
        std::vector<double> interpolated_data;
        if (size_before < MIN_ARIMA_DATA_POINTS && size_after > MIN_ARIMA_DATA_POINTS) {
            interpolated_data = predictData(data, "backward", startIdx_interpol, length_gap_interpol, sr_period);
        } else if (size_after < MIN_ARIMA_DATA_POINTS && size_before > MIN_ARIMA_DATA_POINTS) {
            interpolated_data = predictData(data, "forward", startIdx_interpol, length_gap_interpol, sr_period);
        } else if (size_before < MIN_ARIMA_DATA_POINTS && size_after < MIN_ARIMA_DATA_POINTS) {
            throw IOException("Could not accumulate enough data for parameter estimation; Increasing window sizes might help");
        } else {
            InterpolARIMA arima(data, startIdx_interpol, length_gap_interpol, sr_period);
            setMetaData(arima);
            arima.interpolate();
            interpolated_data = arima.getInterpolatedData();
        }
        return interpolated_data;
    }



    void ARIMAResampling::cacheGap(const std::vector<double> &interpolated_data, const std::vector<Date> &interpolated_dates,
                                   const ARIMA_GAP &new_gap) {
        bool contains_zeros = std::any_of(interpolated_data.begin(), interpolated_data.end(), [](double value) { return value == 0.0; });
        bool all_zeros = std::all_of(interpolated_data.begin(), interpolated_data.end(), [](double value) { return value == 0.0; });

        gap_data.push_back(new_gap);

        bool is_valid = !(all_zeros || (contains_zeros && !is_zero_possible));
        is_valid_gap_data.push_back(is_valid);

        warned_about_gap.push_back(false);
        filled_data.push_back(interpolated_data);
        all_dates.push_back(interpolated_dates);
    }

    // ------------------------------ Resample ------------------------------
    void ARIMAResampling::resample(const std::string & /*stationHash*/, const size_t &index, const ResamplingPosition &position,
                                   const size_t &paramindex, const std::vector<MeteoData> &vecM, MeteoData &md) {
        if (index >= vecM.size())
            throw IOException("The index of the element to be resampled is out of bounds", AT);

        if (position == ResamplingAlgorithms::exact_match) {
            const double value = vecM[index](paramindex);
            if (value != IOUtils::nodata) {
                md(paramindex) = value; // propagate value
                return;
            }
        }

        checkZeroPossibility(vecM, paramindex);

        const Date resampling_date = md.date;

        // check wether given position is in a known gap, if it is either return the
        // exact value or linearly interpolate, to get the correct value
        bool found_gap = processKnownGaps(resampling_date, paramindex, position, vecM, md);
        if (found_gap)
            return;

        // if it is not in a known gap, cache the gap, and interpolate it for subsequent calls
        size_t gap_start = IOUtils::npos;
        size_t gap_end = IOUtils::npos;
        ARIMA_GAP new_gap;
        Date data_start_date;
        Date data_end_date;
        if (position == ResamplingAlgorithms::end) {
            setEndGap(new_gap, data_start_date, data_end_date, vecM, resampling_date);
        } else {
            computeARIMAGap(new_gap, index, paramindex, vecM, resampling_date, gap_start, gap_end, before_window, after_window, window_size,
                            data_start_date, data_end_date);
        }

        if (position == ResamplingAlgorithms::begin) {
            size_t gap_length = static_cast<size_t>((new_gap.endDate - new_gap.startDate).getJulian(true) * new_gap.sampling_rate);
            if (gap_length > MAX_ARIMA_EXTRAPOLATION) {
                if (!gave_warning_start) {
                    std::cerr << "Extrapolating more than " << MAX_ARIMA_EXTRAPOLATION
                              << " steps into the past is pointless, last known data point: " << new_gap.endDate.toString(Date::ISO) << "||"
                              << std::endl;
                    gave_warning_start = true;
                }
                return;
            }
        }

        // check if gap ended up being bigger than the window size
        if (new_gap.endDate - new_gap.startDate > window_size) {
            double difference = (new_gap.endDate - new_gap.startDate).getJulian(true) - window_size;
            difference *= 86400;
            throw IOException(
                "The window size is smaller than the data gap to be interpolated, please increase window size, by at least: " +
                    std::to_string(difference) + "s",
                AT);
        }

        if (new_gap.isGap()) {
            // data vector is of length (data_end_date - data_start_date) * sampling_rate
            size_t length;
            std::vector<double> data;
            std::vector<Date> dates;

            // get a vector of meteodata that contains the data before and after the gap
            std::vector<MeteoData> data_vec_before, data_vec_after;
            bool has_data_before, has_data_after;

            bool success = accumulateData(data_start_date, data_end_date, new_gap, vecM, gave_warning_interpol, length, data, dates,
                                          data_vec_before, data_vec_after, has_data_before, has_data_after);
            if (!success)
                return;

            // resample to the desired sampling rate
            size_t length_gap_interpol = 0;
            size_t endIdx_interpol = IOUtils::npos;
            size_t startIdx_interpol = (data_start_date == new_gap.startDate) ? 0 : IOUtils::npos;

            resampleInterpolationData(length_gap_interpol, endIdx_interpol, startIdx_interpol, new_gap, data_start_date, data_end_date,
                                      data_vec_before, data_vec_after, has_data_before, has_data_after, paramindex, data, dates, length);

            // Now fill the data with the arima model
            int sr_period = static_cast<int>(period * new_gap.sampling_rate);
            std::vector<double> interpolated_data = getInterpolatedData(data, data_vec_before.size(), data_vec_after.size(),
                                                                        startIdx_interpol, length_gap_interpol, sr_period);

            std::vector<Date> interpolated_dates(dates.begin() + startIdx_interpol,
                                                 dates.begin() + startIdx_interpol + length_gap_interpol);

            if (data_end_date != new_gap.endDate) {
                interpolated_data.push_back(data[endIdx_interpol]);
                interpolated_dates.push_back(dates[endIdx_interpol]);
            }

            cacheGap(interpolated_data, interpolated_dates, new_gap);

            // check if the data in the gap is valid (arima(0,0,0) arima(0,1,0) model)
            if (!is_valid_gap_data.back()) {
                if (!warned_about_gap.back()) {
                    std::cerr << "Could not find a useful ARIMA model, try other parameters, or another interpolation algorithm for data "
                                 "in between "
                              << new_gap.startDate.toString(Date::ISO) << "---" << new_gap.endDate.toString(Date::ISO) << "||" << std::endl;
                    warned_about_gap.back() = true;
                }
                return;
            }

            // get the value at the resample date
            auto it = findDate(interpolated_dates, resampling_date);
            // if there is an exact match, return the data
            if (it != interpolated_dates.end()) {
                size_t idx = std::distance(interpolated_dates.begin(), it);
                md(paramindex) = interpolated_data[idx];
                return;
            }

            // otherwise linearly interpolate the data
            size_t idx = std::distance(interpolated_dates.begin(),
                                       std::lower_bound(interpolated_dates.begin(), interpolated_dates.end(), resampling_date));
            md(paramindex) = interpolVecAt(interpolated_data, interpolated_dates, idx, resampling_date);
            return;
        }

        // TODO: remove when interpolation stack is implemented
        //  linearly interpolate the point
        //  should i actually do that or just return?
#ifdef DEBUG
        std::cout << "linearly interpolating the point" << std::endl;
#endif
        double start_value = vecM[gap_start - 1](paramindex);
        double end_value = vecM[gap_end + 1](paramindex);
        Date start_date = vecM[gap_start - 1].date;
        Date end_date = vecM[gap_end + 1].date;
        md(paramindex) = linearInterpolation(start_date.getJulian(true), start_value, end_date.getJulian(true), end_value,
                                             resampling_date.getJulian(true));
        return;
    }
} // namespace
