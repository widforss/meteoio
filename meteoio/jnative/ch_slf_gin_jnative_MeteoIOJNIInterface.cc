/*
 * ch_slf_gin_jnative_MeteoIOJNIInterface.cc
 *
 *  Created on: 08.01.2010
 *      Author: perot
 */

#ifdef _METEOIO_JNI

#include <jni.h>
#include "jnative.h"
#include <meteoio/MeteoIO.h>
#include "DEMLoader.h"
#include "ch_slf_gin_jnative_MeteoIOJNIInterface.h"

#include <stdlib.h>
#include <time.h>

using namespace mio;

jdoubleArray jMakeError (JNIEnv *env, float errorCode){
	jdoubleArray out = env->NewDoubleArray(1);
	jboolean isCopyOut;
	jdouble* dest = env->GetDoubleArrayElements(out, &isCopyOut);
	dest[0] = errorCode;
	if (isCopyOut == JNI_TRUE)
		env->ReleaseDoubleArrayElements(out, dest, 0);
	return out;
}

jdoubleArray convert_JNIArray(JNIEnv *env, const Grid2DObject&  p, const std::string& cellOrder){

    jboolean isCopyOut;
	jdoubleArray out = env->NewDoubleArray(p.nrows*p.ncols + 6);//size = dem.nrow*dem.ncols+6
	jdouble* dest = env->GetDoubleArrayElements(out, &isCopyOut);

	fulfillDoubleArray( p, cellOrder, dest);

	if (isCopyOut == JNI_TRUE)
		env->ReleaseDoubleArrayElements(out, dest, 0);

	return out;
}

JNIEXPORT jdoubleArray JNICALL Java_ch_slf_gin_jnative_MeteoIOJNIInterface_executeInterpolationSubDem
			  (JNIEnv *env, jclass theClass,
			  jstring jAlgorithm, jstring jIOinterface,
			  jstring jDemFile,jstring jDemCoordSystem,
			  jdouble demXll, jdouble demYll,
			  jdouble demXrt, jdouble demYrt,
			  jdoubleArray jMetadata, jdoubleArray jData,
			  jstring jMetaCoordSystem, jstring jcellOrder){

	const char * cDemFile = env->GetStringUTFChars(jDemFile,0);
	const char * cDemCoordSystem = env->GetStringUTFChars(jDemCoordSystem,0);
	const char * cIOInterface = env->GetStringUTFChars(jIOinterface,0);
	const char * cAlgorithm = env->GetStringUTFChars(jAlgorithm,0);
	const char * cMetaCoordSystem = env->GetStringUTFChars(jMetaCoordSystem,0);
	const char * cCellOrder = env->GetStringUTFChars(jcellOrder,0);

	clock_t tmpStart;
	clock_t tmpEnd;

	tmpStart = clock(); //start
	//get dem
	const DEMObject& dem = (demXll > -1 && demYrt> -1)?
			DEMLoader::loadSubDEM(cDemFile, cDemCoordSystem, cIOInterface, demXll, demYll, demXrt, demYrt) :
				DEMLoader::loadFullDEM(cDemFile, cDemCoordSystem, cIOInterface);
	if (dem.nrows<1 || dem.ncols<2  ){
		std::cout << "Problem with DEM creation : "  << std::endl;
		//error
		return jMakeError(env, -2.f);
	}
	tmpEnd = clock(); //end
	const double msDemLoading = (tmpEnd - tmpStart)/1000.0;


	//Create MeteoData and StationData vectors
	tmpStart = clock(); //start
	const int nbStation = (int)env->GetArrayLength(jMetadata)/3;
	const int nbDataPerStation = (int)env->GetArrayLength(jData)/nbStation;
	jboolean isCopyMetadata;
	jboolean isCopyData;
	double *cMetadata = env->GetDoubleArrayElements(jMetadata,&isCopyMetadata);
	double *cData = env->GetDoubleArrayElements(jData,&isCopyData);
	std::vector<MeteoData> vecData;
	std::vector<StationData> vecStation;
	enum MeteoData::Parameters interpolation_type;
	//initialize MeteoData and StationData vectors
	loadMeteoAndStationData(cMetadata, cData, nbStation, nbDataPerStation, cAlgorithm,
		cMetaCoordSystem, vecStation, vecData, interpolation_type);
	tmpEnd = clock(); //end
	const double msDataLoading = (tmpEnd - tmpStart)/1000.0;

	//Interpolation
	tmpStart = clock(); //start
	Grid2DObject  p(dem.ncols, dem.nrows, dem.cellsize, dem.llcorner);
	bool success = true;
	try {
		ConfigReader cfg; //This should be given as parameter to executeInterpolationSubDem
		Meteo2DInterpolator mi(cfg, dem, vecData, vecStation);
		mi.interpolate(interpolation_type, p);
	}
	catch(IOException e){
		std::cout << "Interpolation failed : " << e.what() << std::endl;
		success = false;
	}
	catch(...){
		std::cout << "Interpolation failed for some reason ?!? " <<  std::endl;
		success = false;
	}
	//copy the interpolation result into a jdoubleArray
	jdoubleArray out = convert_JNIArray(env, p, cCellOrder);
	tmpEnd = clock(); //end
	const double msInterpolation = (tmpEnd - tmpStart)/1000.0;

	//put the different process in the result
	double* times = (double*) malloc( 3* sizeof(double));
	times[0] = msDemLoading;
	times[1] = msDataLoading;
	times[2] = msInterpolation;
	env->SetDoubleArrayRegion(out, 3, 3, times);
	free(times);
	std::cout << " - time to load DEM : "  << msDemLoading << std::endl;
	std::cout << " - time to load Data : "  << msDataLoading << std::endl;
	std::cout << " - time to interpolate: "  << msInterpolation << std::endl;

	//release cMetadata
	if (isCopyMetadata == JNI_TRUE)
		env->ReleaseDoubleArrayElements(jMetadata, cMetadata, JNI_ABORT);
	//release cData
	if (isCopyData == JNI_TRUE)
		env->ReleaseDoubleArrayElements(jData, cData, JNI_ABORT);
	env->ReleaseStringUTFChars(jDemFile, cDemFile);
	env->ReleaseStringUTFChars(jDemCoordSystem, cDemCoordSystem);
	env->ReleaseStringUTFChars(jIOinterface, cIOInterface);
	env->ReleaseStringUTFChars(jAlgorithm, cAlgorithm);
	env->ReleaseStringUTFChars(jMetaCoordSystem, cMetaCoordSystem);
	env->ReleaseStringUTFChars(jcellOrder, cCellOrder);
	vecData.clear();
	vecStation.clear();
	return out;
}

JNIEXPORT jdoubleArray JNICALL Java_ch_slf_gin_jnative_MeteoIOJNIInterface_executeInterpolation
			(JNIEnv *env, jclass theClass,
			jstring jAlgorithm, jstring jIOinterface, jstring jDemFile, jstring jDemCoordSystem,
			jdoubleArray jMetadata, jdoubleArray jData,
			jstring jMetaCoordSystem, jstring jcellOrder){


	return Java_ch_slf_gin_jnative_MeteoIOJNIInterface_executeInterpolationSubDem(
			env, theClass,jAlgorithm, jIOinterface, jDemFile, jDemCoordSystem,
			-1, -1,-1, -1,jMetadata, jData, jMetaCoordSystem, jcellOrder);
}






#endif
