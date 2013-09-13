/***********************************************************************************/
/*  Copyright 2011 WSL Institute for Snow and Avalanche Research    SLF-DAVOS      */
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
#include <meteoio/IOUtils.h>
#include <meteoio/MathOptim.h>
#include <meteoio/ResamplingAlgorithms2D.h>
#include <cmath>

using namespace std;

namespace mio {

/**
 * @brief Bilinear spatial data resampling
 */
const Grid2DObject ResamplingAlgorithms2D::BilinearResampling(const Grid2DObject &i_grid, const double &factor)
{
	const double cellsize = i_grid.cellsize/factor;
	const size_t ncols = static_cast<size_t>(Optim::round( static_cast<double>(i_grid.ncols)*factor ));
	const size_t nrows = static_cast<size_t>(Optim::round( static_cast<double>(i_grid.nrows)*factor ));
	Grid2DObject o_grid(ncols, nrows, cellsize, i_grid.llcorner);

	Bilinear(o_grid, i_grid); //GridObjects always keep nodata
	return o_grid;
}

const Grid2DObject ResamplingAlgorithms2D::cubicBSplineResampling(const Grid2DObject &i_grid, const double &factor)
{
	const double cellsize = i_grid.cellsize/factor;
	const size_t ncols = static_cast<size_t>(Optim::round( static_cast<double>(i_grid.ncols)*factor ));
	const size_t nrows = static_cast<size_t>(Optim::round( static_cast<double>(i_grid.nrows)*factor ));
	Grid2DObject o_grid(ncols, nrows, cellsize, i_grid.llcorner);

	cubicBSpline(o_grid, i_grid); //GridObjects always keep nodata
	return o_grid;
}

const Grid2DObject ResamplingAlgorithms2D::NearestNeighbour(const Grid2DObject &i_grid, const double &factor)
{
	const double cellsize = i_grid.cellsize/factor;
	const size_t ncols = static_cast<size_t>(Optim::round( static_cast<double>(i_grid.ncols)*factor ));
	const size_t nrows = static_cast<size_t>(Optim::round( static_cast<double>(i_grid.nrows)*factor ));
	Grid2DObject o_grid(ncols, nrows, cellsize, i_grid.llcorner);

	NearestNeighbour(o_grid, i_grid); //GridObjects always keep nodata
	return o_grid;
}

///////////////////////////////////////////////////////////////////////
//Private Methods
///////////////////////////////////////////////////////////////////////
void ResamplingAlgorithms2D::NearestNeighbour(Grid2DObject &o_grid, const Grid2DObject &i_grid)
{
	const size_t org_ncols = i_grid.ncols;
	const size_t org_nrows = i_grid.nrows;
	const double scale_x = (double)o_grid.ncols / (double)org_ncols;
	const double scale_y = (double)o_grid.nrows / (double)org_nrows;

	for (size_t jj=0; jj<o_grid.nrows; jj++) {
		size_t org_jj = (size_t) Optim::round( (double)jj/scale_y );
		if(org_jj>=org_nrows) org_jj=org_nrows-1;

		for (size_t ii=0; ii<o_grid.ncols; ii++) {
			size_t org_ii = (size_t) Optim::round( (double)ii/scale_x );
			if(org_ii>=org_ncols) org_ii=org_ncols-1;
			o_grid(ii,jj) = i_grid(org_ii, org_jj);
		}
	}
}

double ResamplingAlgorithms2D::bilinear_pixel(const Grid2DObject &i_grid, const size_t &org_ii, const size_t &org_jj, const size_t &org_ncols, const size_t &org_nrows, const double &x, const double &y)
{
	if(org_jj>=(org_nrows-1) || org_ii>=(org_ncols-1)) return i_grid(org_ii, org_jj);

	const double f_0_0 = i_grid(org_ii, org_jj);
	const double f_1_0 = i_grid(org_ii+1, org_jj);
	const double f_0_1 = i_grid(org_ii, org_jj+1);
	const double f_1_1 = i_grid(org_ii+1, org_jj+1);

	double avg_value = 0.;
	unsigned int avg_count = 0;
	if(f_0_0!=IOUtils::nodata) {
		avg_value += f_0_0;
		avg_count++;
	}
	if(f_1_0!=IOUtils::nodata) {
		avg_value += f_1_0;
		avg_count++;
	}
	if(f_0_1!=IOUtils::nodata) {
		avg_value += f_0_1;
		avg_count++;
	}
	if(f_1_1!=IOUtils::nodata) {
		avg_value += f_1_1;
		avg_count++;
	}

	if(avg_count==4) return f_0_0 * (1.-x)*(1.-y) + f_1_0 * x*(1.-y) + f_0_1 * (1.-x)*y + f_1_1 *x*y;

	//special cases: less than two neighbours or three neighbours
	if(avg_count<=2) return IOUtils::nodata;

	double value = 0.;
	const double avg = avg_value/(double)avg_count;
	if(f_0_0!=IOUtils::nodata) value += f_0_0 * (1.-x)*(1.-y);
	else value += avg * (1.-x)*(1.-y);
	if(f_1_0!=IOUtils::nodata) value += f_1_0 * x*(1.-y);
	else value += avg * x*(1.-y);
	if(f_0_1!=IOUtils::nodata) value += f_0_1 * (1.-x)*y;
	else value += avg * (1.-x)*y;
	if(f_1_1!=IOUtils::nodata) value += f_1_1 *x*y;
	else value += avg *x*y;

	return value;
}

void ResamplingAlgorithms2D::Bilinear(Grid2DObject &o_grid, const Grid2DObject &i_grid)
{
	const size_t org_ncols = i_grid.ncols;
	const size_t org_nrows = i_grid.nrows;
	const double scale_x = (double)o_grid.ncols / (double)org_ncols;
	const double scale_y = (double)o_grid.nrows / (double)org_nrows;

	for (size_t jj=0; jj<o_grid.nrows; jj++) {
		const double org_y = (double)jj/scale_y;
		const size_t org_jj = static_cast<size_t>( org_y );
		const double y = org_y - (double)org_jj; //normalized y, between 0 and 1

		for (size_t ii=0; ii<o_grid.ncols; ii++) {
			const double org_x = (double)ii/scale_x;
			const size_t org_ii = static_cast<size_t>( org_x );
			const double x = org_x - (double)org_ii; //normalized x, between 0 and 1

			o_grid(ii,jj) = bilinear_pixel(i_grid, org_ii, org_jj, org_ncols, org_nrows, x, y);
		}
	}
}

double ResamplingAlgorithms2D::BSpline_weight(const double &x) {
	double R = 0.;
	if((x+2.)>0.) R += Optim::pow3(x+2.);
	if((x+1.)>0.) R += -4.*Optim::pow3(x+1.);
	if((x)>0.) R += 6.*Optim::pow3(x);
	if((x-1.)>0.) R += -4.*Optim::pow3(x-1.);

	return 1./6.*R;
}

void ResamplingAlgorithms2D::cubicBSpline(Grid2DObject &o_grid, const Grid2DObject &i_grid)
{//see http://paulbourke.net/texture_colour/imageprocess/
	const size_t org_ncols = i_grid.ncols;
	const size_t org_nrows = i_grid.nrows;
	const double scale_x = (double)o_grid.ncols / (double)org_ncols;
	const double scale_y = (double)o_grid.nrows / (double)org_nrows;

	for (size_t jj=0; jj<o_grid.nrows; jj++) {
		const double org_y = (double)jj/scale_y;
		const size_t org_jj = static_cast<size_t>( org_y );
		const double dy = org_y - (double)org_jj; //normalized y, between 0 and 1

		for (size_t ii=0; ii<o_grid.ncols; ii++) {
			const double org_x = (double)ii/scale_x;
			const size_t org_ii = static_cast<size_t>( org_x );
			const double dx = org_x - (double)org_ii; //normalized x, between 0 and 1

			double F = 0., max=-std::numeric_limits<double>::max(), min=std::numeric_limits<double>::max();
			unsigned int avg_count = 0;
			for(int n=-1; n<=2; n++) {
				for(int m=-1; m<=2; m++) {
					if(((signed)org_ii+m)<0 || ((signed)org_ii+m)>=(signed)org_ncols || ((signed)org_jj+n)<0 || ((signed)org_jj+n)>=(signed)org_nrows) continue;
					const double pixel = i_grid((signed)org_ii+m, (signed)org_jj+n);
					if(pixel!=IOUtils::nodata) {
						F += pixel * BSpline_weight(m-dx) * BSpline_weight(dy-n);
						avg_count++;
						if(pixel>max) max=pixel;
						if(pixel<min) min=pixel;
					}
				}
			}

			if(avg_count==16) { //normal bicubic
				o_grid(ii,jj) = F*(16/avg_count);
				if(o_grid(ii,jj)>max) o_grid(ii,jj)=max; //try to limit overshoot
				else if(o_grid(ii,jj)<min) o_grid(ii,jj)=min; //try to limit overshoot
			} else if(avg_count==0) o_grid(ii,jj) = IOUtils::nodata; //nodata-> nodata
			else //not enought data points -> bilinear for this pixel
				o_grid(ii,jj) = bilinear_pixel(i_grid, org_ii, org_jj, org_ncols, org_nrows, dx, dy);

		}
	}

}

} //namespace

