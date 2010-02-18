#include "MeteoIO.h"

//This is a basic example of using as dem: the dem is read, the grid coordinates of a point given by its (lat,long) are retrieved
//and a sub-dem is extracted starting at these coordinates and extending dist_x and dist_y and written out.
int main(void) {
	DEMObject dem;
	IOHandler *io = NULL;
	unsigned int i,j;
	
	try {
		ConfigReader cfg("io.ini");
		io = new IOHandler(cfg);
	} catch (IOException& e){
		std::cout << "Problem with IOHandler creation, cause: " << e.what() << std::endl;
	}
	//reading dem
	io->readDEM(dem);

	//writing some statistics about this dem
	//dem.grid2D.getMin() scans the DEM grid to get the min, while dem.min_altitude is cached and therefore very cheap
	//The raw content of the 2D grids can also be accessed, for example dem.grid2D.getMin(IOUtils::RAW_NODATA). In this case, there would be no interpretation of some values as nodata.
	std::cout << "DEM information: \n";
	std::cout << "\tmin=" << dem.grid2D.getMin() << " max=" << dem.grid2D.getMax() << " mean=" << dem.grid2D.getMean() << "\n";
	std::cout << "\tmin slope=" << dem.min_slope << " max slope=" << dem.max_slope << std::endl;

	//retrieving grid coordinates of a real world point
	Coords point;
	point.copyProj(dem.llcorner); //we use the same projection parameters as the DEM
	point.setLatLon(46.1592, 8.12993);
	dem.WGS84_to_grid(point, i,j);

	//computing grid distances from real world distances
	const double dist_x=700., dist_y=1200.;
	const unsigned int ncols = (unsigned int)ceil(dist_x/dem.cellsize);
	const unsigned int nrows = (unsigned int)ceil(dist_y/dem.cellsize);

	//extracting a sub-dem starting at the given coordinates and extending a given distance along x and along y
	DEMObject sub_dem(dem, i, j, ncols, nrows);

	//writing the sub-dem out
	io->write2DGrid(sub_dem,"sub_dem.dem");

	return 0;
}
