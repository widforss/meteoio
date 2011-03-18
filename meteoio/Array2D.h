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
#ifndef ARRAY2D_H
#define ARRAY2D_H

#include <meteoio/IOExceptions.h>
#include <meteoio/IOUtils.h>

#include <vector>
#include <limits>
#include <iostream>

#define NOSAFECHECKS

namespace mio {

template <class T> class Array2D;

/**
 * @class Array2DProxy
 * @brief The template class Array2DProxy is a helper class for the template class Array2D
 *        with the purpose of adding the [][] operator to Array2D
 *
 * @author Thomas Egger
 */
template <class T> class Array2DProxy {
	public:
		friend class Array2D<T>;
		T& operator[](const unsigned int& j) {
			return array2D(anx, j);
		}

	private:
		Array2DProxy(Array2D<T>& _array2D, const unsigned int& _anx) : array2D(_array2D), anx(_anx){}
		Array2D<T>& array2D;
		const unsigned int anx;
};

/**
 * @class Array2D
 * @brief The template class Array2D is a 2D Array (Matrix) able to hold any type of object as datatype. 
 * It relies on the Array2DProxy class to provide the [][] operator (slower than the (i,j) call).
 *
 * @ingroup data_str
 * @author Thomas Egger
 */
template<class T> class Array2D {
	public:
		Array2D();

		/**
		* A constructor that creates an array of a given size
		* @param anx number of columns of the new array
		* @param any number of rows of the new array
		*/
		Array2D(const unsigned int& anx, const unsigned int& any);

		/**
		* A constructor that creates an array filled with constant values
		* @param anx number of columns of the new array
		* @param any number of rows of the new array
		* @param init initial value to fill the array with
		*/
		Array2D(const unsigned int& anx, const unsigned int& any, const T& init);

		/**
		* A constructor that can be used to create an Array2D object that is contained in the
		* one passed as _array2D argument. The resulting Array2D object is a by value copy of
		* a subplane of the plane spanned by the _array2D
		* @param _array2D array containing to extract the values from
		* @param _nx lower left corner cell X index
		* @param _ny lower left corner cell Y index
		* @param _ncols number of columns of the new array
		* @param _nrows number of rows of the new array
		*/
		Array2D(const Array2D<T>& _array2D, const unsigned int& _nx, const unsigned int& _ny,
			    const unsigned int& _ncols, const unsigned int& _nrows);

		/**
		* @brief A method that can be used to cut out a subplane of an existing Array2D object
		* that is passed as _array2D argument. The resulting Array2D object is a by value copy of
		* a subplane of the plane spanned by the _array2D
		* @param _array2D array containing to extract the values from
		* @param _nx lower left corner cell X index
		* @param _ny lower left corner cell Y index
		* @param _ncols number of columns of the new array
		* @param _nrows number of rows of the new array
		*/
		void subset(const Array2D<T>& _array2D, const unsigned int& _nx, const unsigned int& _ny,
		            const unsigned int& _ncols, const unsigned int& _nrows);

		void resize(const unsigned int& nx, const unsigned int& ny);
		void resize(const unsigned int& nx, const unsigned int& ny, const T& init);
		void size(unsigned int& nx, unsigned int& ny) const;

		/**
		* @brief returns the minimum value contained in the grid
		* @param flag_nodata specify how to process nodata values (see NODATA_HANLDING)
		* @return minimum value
		*/
		T getMin(const IOUtils::nodata_handling flag_nodata=IOUtils::PARSE_NODATA) const;
		/**
		* @brief returns the maximum value contained in the grid
		* @param flag_nodata specify how to process nodata values (see NODATA_HANLDING)
		* @return maximum value
		*/
		T getMax(const IOUtils::nodata_handling flag_nodata=IOUtils::PARSE_NODATA) const;
		/**
		* @brief returns the mean value contained in the grid
		* @param flag_nodata specify how to process nodata values (see NODATA_HANLDING)
		* @return mean value
		*/
		T getMean(const IOUtils::nodata_handling flag_nodata=IOUtils::PARSE_NODATA) const;

		template<class P> friend std::ostream& operator<<(std::ostream& os, const Array2D<P>& array);

		void clear();
		T& operator ()(const unsigned int& x, const unsigned int& y);
		const T operator ()(const unsigned int& x, const unsigned int& y) const;
		T& operator ()(const unsigned int& i);
		const T operator ()(const unsigned int& i) const;
		Array2DProxy<T> operator[](const unsigned int& i);

		Array2D<T>& operator =(const Array2D<T>&);
		Array2D<T>& operator =(const T& value);

		Array2D<T>& operator+=(const T& rhs);
		const Array2D<T> operator+(const T& rhs);
		Array2D<T>& operator+=(const Array2D<T>& rhs);
		const Array2D<T> operator+(const Array2D<T>& rhs);

		Array2D<T>& operator-=(const T& rhs);
		const Array2D<T> operator-(const T& rhs);
		Array2D<T>& operator-=(const Array2D<T>& rhs);
		const Array2D<T> operator-(const Array2D<T>& rhs);

		Array2D<T>& operator*=(const T& rhs);
		const Array2D<T> operator*(const T& rhs);
		Array2D<T>& operator*=(const Array2D<T>& rhs);
		const Array2D<T> operator*(const Array2D<T>& rhs);

		Array2D<T>& operator/=(const T& rhs);
		const Array2D<T> operator/(const T& rhs);
		Array2D<T>& operator/=(const Array2D<T>& rhs);
		const Array2D<T> operator/(const Array2D<T>& rhs);

	protected:
		std::vector<T> vecData;
		unsigned int nx;
		unsigned int ny;
};

template<class T> T& Array2D<T>::operator()(const unsigned int& i) {
	return vecData[i];
}

template<class T> const T Array2D<T>::operator()(const unsigned int& i) const {
	return vecData[i];
}
template<class T> T& Array2D<T>::operator()(const unsigned int& x, const unsigned int& y) {
#ifndef NOSAFECHECKS
	if ((x >= nx) || (y >= ny)) {
		throw IndexOutOfBoundsException("", AT);
	}
#endif
	//ROW-MAJOR alignment of the vector: fully C-compatible memory layout
	return vecData[x + y*nx];
}

template<class T> const T Array2D<T>::operator()(const unsigned int& x, const unsigned int& y) const {
#ifndef NOSAFECHECKS
	if ((x >= nx) || (y >= ny)) {
		throw IndexOutOfBoundsException("", AT);
	}
#endif
	return vecData[x + y*nx];
}

template<class T> Array2DProxy<T> Array2D<T>::operator[](const unsigned int& i) {
	return Array2DProxy<T>(*this, i);
}

template<class T> Array2D<T>::Array2D() {
	nx = ny = 0;
}

template<class T> Array2D<T>::Array2D(const Array2D<T>& _array2D, const unsigned int& _nx, const unsigned int& _ny,
			    const unsigned int& _ncols, const unsigned int& _nrows)
{
	subset(_array2D, _nx, _ny, _ncols, _nrows);
}

template<class T> void Array2D<T>::subset(const Array2D<T>& _array2D, const unsigned int& _nx, const unsigned int& _ny,
                                          const unsigned int& _ncols, const unsigned int& _nrows)
{
	if (((_nx+_ncols) > _array2D.nx) || ((_ny+_nrows) > _array2D.ny))
		throw IndexOutOfBoundsException("Trying to cut an array to a size bigger than its original size!", AT);

	if ((_ncols == 0) || (_nrows == 0)) //the plane to copy has to make sense
		throw IndexOutOfBoundsException("Copying an array into a null sized array!", AT);

	resize(_ncols, _nrows); //create new Array2D object

	//Copy by value subspace
	for (unsigned int jj=0; jj<ny; jj++) {
		for (unsigned int ii=0; ii<nx; ii++) {
			operator()(ii,jj) = _array2D(_nx+ii, _ny+jj);
		}
	}
}

template<class T> Array2D<T>::Array2D(const unsigned int& anx, const unsigned int& any, const T& init) {
	nx = ny = 0;
	resize(anx,any,init);
}

template<class T> Array2D<T>::Array2D(const unsigned int& anx, const unsigned int& any) {
	nx = ny = 0;
	resize(anx,any);
}

template<class T> void Array2D<T>::resize(const unsigned int& anx, const unsigned int& any) {
	clear();

	if ((anx > 0) && (any > 0)) {
		vecData.resize(anx*any);
		nx = anx;
		ny = any;
	} else {
		throw IndexOutOfBoundsException("Can not resize a 2D array to negative sizes!", AT);
	}
}

template<class T> void Array2D<T>::resize(const unsigned int& anx, const unsigned int& any, const T& init) {
	resize(anx, any);
	std::fill(vecData.begin(), vecData.end(), init);
}

template<class T> void Array2D<T>::size(unsigned int& anx, unsigned int& any) const{
	anx=nx;
	any=ny;
}

template<class T> void Array2D<T>::clear() {
	vecData.clear();
	nx=ny=0;
}

template<class T> std::ostream& operator<<(std::ostream& os, const Array2D<T>& array) {
	os << "<array2d>\n";
	for(unsigned int jj=0; jj<array.ny; jj++) {
		unsigned int jnx = jj*array.nx;
		for (unsigned int ii=0; ii<array.nx; ii++) {
			os << array(ii+jnx) << " ";
		}
		os << "\n";
	}
	os << "</array2d>\n";
	return os;
}

template<class T> T Array2D<T>::getMin(const IOUtils::nodata_handling flag_nodata) const {

	T min = std::numeric_limits<T>::max();

	const unsigned int nxy = ny*nx;
	if(flag_nodata==IOUtils::RAW_NODATA) {
		for (unsigned int jj=0; jj<nxy; jj++) {
			const T val = operator()(jj);
			if(val<min) min=val;
		}
		return min;
	} else if(flag_nodata==IOUtils::PARSE_NODATA) {
		for (unsigned int jj=0; jj<nxy; jj++) {
			const T val = operator()(jj);
			if(val!=IOUtils::nodata && val<min) min=val;
		}
		if(min!=std::numeric_limits<T>::max()) return min;
		else return (T)IOUtils::nodata;
	} else {
		throw InvalidArgumentException("Unknown nodata_handling flag",AT);
	}
}

template<class T> T Array2D<T>::getMax(const IOUtils::nodata_handling flag_nodata) const {

	T max = -std::numeric_limits<T>::max();

	const unsigned int nxy = ny*nx;
	if(flag_nodata==IOUtils::RAW_NODATA) {
		for (unsigned int jj=0; jj<nxy; jj++) {
			const T val = operator()(jj);
			if(val>max) max=val;
		}
		return max;
	} else if(flag_nodata==IOUtils::PARSE_NODATA) {
		for (unsigned int jj=0; jj<nxy; jj++) {
			const T val = operator()(jj);
			if(val!=IOUtils::nodata && val>max) max=val;
		}
		if(max!=-std::numeric_limits<T>::max()) return max;
		else return (T)IOUtils::nodata;
	} else {
		throw InvalidArgumentException("Unknown nodata_handling flag",AT);
	}
}

template<class T> T Array2D<T>::getMean(const IOUtils::nodata_handling flag_nodata) const {

	T mean = 0;
	const unsigned int nxy = nx*ny;
	
	if(flag_nodata==IOUtils::RAW_NODATA) {
		for (unsigned int jj=0; jj<nxy; jj++) {
			const T val = operator()(jj);
			mean += val;
		}
		if(nxy>0) return mean/(T)(nxy);
		else return (T)0;
	} else if(flag_nodata==IOUtils::PARSE_NODATA) {
		unsigned int count = 0;
		for (unsigned int jj=0; jj<nxy; jj++) {
			const T val = operator()(jj);
			if(val!=IOUtils::nodata) {
				mean += val;
				count++;
			}
		}
		if(count>0) return mean/(T)(count);
		else return (T)IOUtils::nodata;
	} else {
		throw InvalidArgumentException("Unknown nodata_handling flag",AT);
	}
}

//arithmetic operators
template<class T> Array2D<T>& Array2D<T>::operator=(const Array2D<T>& source) {
	if(this != &source) {
		nx = source.nx;
		ny = source.ny;
		vecData = source.vecData;
	}
	return *this;
}

template<class T> Array2D<T>& Array2D<T>::operator=(const T& value) {
	std::fill(vecData.begin(), vecData.end(), value);
	return *this;
}

template<class T> Array2D<T>& Array2D<T>::operator+=(const Array2D<T>& rhs)
{
	//They have to have equal size
	if ((rhs.nx != nx) || (rhs.ny != ny))
		throw IOException("Trying to add two Array2D objects with different dimensions", AT);

	const unsigned int nxy = nx*ny;
	//Add to every single member of the Array2D<T>
	for (unsigned int jj=0; jj<nxy; jj++)
			operator()(jj) += rhs(jj);

	return *this;
}

template<class T> const Array2D<T> Array2D<T>::operator+(const Array2D<T>& rhs)
{
	Array2D<T> result = *this; //make a copy
	result += rhs; //already implemented

	return result;
}

template<class T> Array2D<T>& Array2D<T>::operator+=(const T& rhs)
{
	//Add to every single member of the Array2D<T>
	const unsigned int nxy = nx*ny;
	for (unsigned int jj=0; jj<nxy; jj++)
		operator()(jj) += rhs;

	return *this;
}

template<class T> const Array2D<T> Array2D<T>::operator+(const T& rhs)
{
	Array2D<T> result = *this;
	result += rhs; //already implemented

	return result;
}

template<class T> Array2D<T>& Array2D<T>::operator-=(const Array2D<T>& rhs)
{
	//They have to have equal size
	if ((rhs.nx != nx) || (rhs.ny != ny))
		throw IOException("Trying to substract two Array2D objects with different dimensions", AT);

	//Substract to every single member of the Array2D<T>
	const unsigned int nxy = nx*ny;
	for (unsigned int jj=0; jj<nxy; jj++)
		operator()(jj) -= rhs(jj);

	return *this;
}

template<class T> const Array2D<T> Array2D<T>::operator-(const Array2D<T>& rhs)
{
	Array2D<T> result = *this; //make a copy
	result -= rhs; //already implemented

	return result;
}

template<class T> Array2D<T>& Array2D<T>::operator-=(const T& rhs)
{
	//Substract to every single member of the Array2D<T>
	const unsigned int nxy = nx*ny;
	for (unsigned int jj=0; jj<nxy; jj++)
		operator()(jj) -= rhs;

	return *this;
}

template<class T> const Array2D<T> Array2D<T>::operator-(const T& rhs)
{
	Array2D<T> result = *this;
	result -= rhs; //already implemented

	return result;
}

template<class T> Array2D<T>& Array2D<T>::operator*=(const Array2D<T>& rhs)
{
	//They have to have equal size
	if ((rhs.nx != nx) || (rhs.ny != ny))
		throw IOException("Trying to multiply two Array2D objects with different dimensions", AT);

	//Add to every single member of the Array2D<T>
	const unsigned int nxy = nx*ny;
	for (unsigned int jj=0; jj<nxy; jj++)
		operator()(jj) *= rhs(jj);

	return *this;
}

template<class T> const Array2D<T> Array2D<T>::operator*(const Array2D<T>& rhs)
{
	Array2D<T> result = *this; //make a copy
	result *= rhs; //already implemented

	return result;
}

template<class T> Array2D<T>& Array2D<T>::operator*=(const T& rhs)
{
	//Add to every single member of the Array2D<T>
	const unsigned int nxy = nx*ny;
	for (unsigned int jj=0; jj<nxy; jj++)
			operator()(jj) *= rhs;

	return *this;
}

template<class T> const Array2D<T> Array2D<T>::operator*(const T& rhs)
{
	Array2D<T> result = *this;
	result *= rhs; //already implemented

	return result;
}

template<class T> Array2D<T>& Array2D<T>::operator/=(const Array2D<T>& rhs)
{
	//They have to have equal size
	if ((rhs.nx != nx) || (rhs.ny != ny))
		throw IOException("Trying to divide two Array2D objects with different dimensions", AT);

	//Divide every single member of the Array2D<T>
	const unsigned int nxy = nx*ny;
	for (unsigned int jj=0; jj<nxy; jj++)
		operator()(jj) /= rhs(jj);

	return *this;
}

template<class T> const Array2D<T> Array2D<T>::operator/(const Array2D<T>& rhs)
{
	Array2D<T> result = *this; //make a copy
	result /= rhs; //already implemented

	return result;
}

template<class T> Array2D<T>& Array2D<T>::operator/=(const T& rhs)
{
	//Divide every single member of the Array2D<T>
	const unsigned int nxy = nx*ny;
	for (unsigned int jj=0; jj<nxy; jj++)
		operator()(jj) /= rhs;

	return *this;
}

template<class T> const Array2D<T> Array2D<T>::operator/(const T& rhs)
{
	Array2D<T> result = *this;
	result /= rhs; //already implemented

	return result;
}

} //end namespace mio

#endif
