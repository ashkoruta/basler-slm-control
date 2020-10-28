// bareBonesBasler.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include <ctime>
#include <map>
#include <string>
#include <vector>
#include <pylon/PylonIncludes.h>
#include <pylon/ThreadPriority.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include "bareBonesBasler.h"

// Namespace for using pylon objects.
using namespace Pylon;
using namespace GenApi;

const std::string logFileName = "c:/gateway/basler_dll.log"; // FIXME that prob should go into cfg file

static CBaslerUsbInstantCamera camera;

std::ofstream logFile;
bool logOpened = false;
int timeoutRetrieve = 1;

extern "C" _declspec(dllexport) void _stdcall openLog()
{
	logFile.open(logFileName, std::ios::app);
	logOpened = logFile.is_open();
}

void writeLog(bool enabled, std::ofstream &os, const std::string &str)
{
	if (enabled) {
		//TODO put timestamp in front
		os << str << std::endl;
		os.flush();
	}
}

int parseCfgFile(const std::string &config, std::map<std::string, std::string> &cfgParams)
{
	writeLog(logOpened, logFile, "Configuration file:" + std::string(config));
	std::ifstream cfg(config);
	if (!cfg.is_open()) {
		writeLog(logOpened, logFile, "Failed to read config file");
		return -1;
	}
	std::string line;
	while (std::getline(cfg, line)) {
		std::istringstream ss(line);
		std::string key, val;
		std::getline(ss, key, '=');
		std::getline(ss, val);
		cfgParams[key] = val;
	}
	writeLog(logOpened, logFile, "Config file reading done");
	return 0;
}

static int setParameters(CBaslerUsbInstantCamera &camera, const char *config)
{
	std::map<std::string, std::string> cfgParams; // map of parameters
	if (parseCfgFile(std::string(config), cfgParams) < 0) {
		writeLog(logOpened, logFile, "Parsing failed");
		return -1;
	}
	// Set the acquisition parameters
	// 1. subwindow size
	int w = std::stoi(cfgParams["FrameSideW"]);
	camera.Width.SetValue(w);
	int h = std::stoi(cfgParams["FrameSideH"]);
	camera.Height.SetValue(h);
	// 2. offset x,y
	int offsetX = std::stoi(cfgParams["OffsetX"]);
	int offsetY = std::stoi(cfgParams["OffsetY"]);
	camera.OffsetX.SetValue(offsetX);
	camera.OffsetY.SetValue(offsetY);
	// 3. exposure
	int exp = std::stoi(cfgParams["Exposure"]);
	camera.ExposureAuto.FromString("Off");
	camera.ExposureTime.SetValue(exp);
	// 4. framerate
	int framerate = std::stoi(cfgParams["Framerate"]);
	camera.AcquisitionFrameRateEnable.SetValue(true);
	camera.AcquisitionFrameRate.SetValue(framerate);
	// 5. queue size
	camera.MaxNumBuffer = std::stoi(cfgParams["MaxNumBuffer"]);
	camera.OutputQueueSize = std::stoi(cfgParams["OutputQueue"]);; // keep *some* latest images
	// 6. pixel format. timestamp in chunks not required
	camera.PixelFormat.SetValue(Basler_UsbCameraParams::PixelFormat_Mono8);
	// 7. triggering
	if (cfgParams["Trigger"] == "0") {
		camera.TriggerMode.SetValue(Basler_UsbCameraParams::TriggerMode_Off);
	} else {
		// set hardware trigger params here
		camera.TriggerMode.SetValue(Basler_UsbCameraParams::TriggerMode_On);
		camera.TriggerSource.SetValue(Basler_UsbCameraParams::TriggerSource_Line4); // that's how camera's wired
	}
	// 8. link throughput
	// by default, is limited to some number that is too small
	// seems to be working fine without limit
	if (cfgParams["DeviceLinkLimit"] == "1") {
		camera.DeviceLinkThroughputLimitMode.SetValue(Basler_UsbCameraParams::DeviceLinkThroughputLimitMode_On);
	} else {
		camera.DeviceLinkThroughputLimitMode.SetValue(Basler_UsbCameraParams::DeviceLinkThroughputLimitMode_Off);
	}
	// 9. Timeout for RetrieveResult call
	timeoutRetrieve = std::stoi(cfgParams["Timeout"]);
	// TODO others as required
	std::stringstream ss;
	ss << camera.Width.GetValue() << "x" << camera.Height.GetValue() << " " << camera.ExposureTime.GetValue() << " us "
		<< camera.AcquisitionFrameRate.GetValue() << " fps buf=" << camera.MaxNumBuffer.GetValue() << " queue=" << camera.OutputQueueSize.GetValue()
		<< " " << camera.TriggerMode.GetValue() << " " << camera.DeviceLinkThroughputLimitMode.GetValue() << " timeout=" << timeoutRetrieve;
	writeLog(logOpened, logFile, ss.str());
	writeLog(logOpened, logFile, "Camera parameters set");
	if (cfgParams["LogEnable"] == "0") {
		writeLog(logOpened, logFile, "Stop writing to log, as requested by cfg file");
		logOpened = false;
	}
	return 0;
}

extern "C" _declspec(dllexport) int32_t _stdcall initBasler(const char *cfgName)
{
	// Before using any pylon methods, the pylon runtime must be initialized. 
	PylonInitialize();
	openLog();
	try
	{	
		// Create an instant camera object with the camera device found first.
		camera.Attach(CTlFactory::GetInstance().CreateFirstDevice());
		writeLog(logOpened,	logFile,"Camera attached");
		// Set the acquisition parameters
		camera.Open();
		writeLog(logOpened, logFile, "Camera opened");
		int res = setParameters(camera, cfgName);
		if (res < 0) {
			writeLog(logOpened, logFile, "Camera parameters failed to set");
			return -1;
		}
		return 0;
	} catch (const GenericException &e) {
		std::stringstream ss;
		ss << "An exception occurred." << std::endl << e.GetDescription() << std::endl;
		writeLog(logOpened, logFile, ss.str());
		return -1;
	}
}

extern "C" _declspec(dllexport) int _stdcall startBasler()
{
	try {
		//camera.InternalGrabEngineThreadPriorityOverride = true;
		//camera.InternalGrabEngineThreadPriority = 31; //will only work if run as admin

		//Pylon::SetRTThreadPriority(Pylon::GetCurrentThreadHandle(), 31);
		//camera.StartGrabbing();
		camera.StartGrabbing(GrabStrategy_LatestImages);
		//writeLog(logOpened, logFile, "Grabbing started");
		return 0;
	} catch (const GenericException &e) {
		std::stringstream ss;
		ss << "Could not grab an image: " << e.GetDescription();
		writeLog(logOpened, logFile, ss.str());
		return -1;
	}
}

// external caller has to allocate 1D array of width x height bytes.
// width x height must match camera configuration
// labview can't do 2D arrays that's why 1D is used
extern "C" _declspec(dllexport) int _stdcall retrieveOne_dbg(uint8_t* rawData, uint64_t* ts, DbgInfo *dbg)
{
	CGrabResultPtr ptrGrabResult;
	if (!camera.IsGrabbing()) {
		writeLog(logOpened, logFile, "Camera is not grabbing");
		return -1;
	}
	auto start = std::chrono::high_resolution_clock::now();
	// Wait for an image and then retrieve it
	//writeLog(logOpened, logFile, "Calling RetrieveResult now...");
	bool retrieved = camera.RetrieveResult(timeoutRetrieve, ptrGrabResult, TimeoutHandling_Return);
	auto finish = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> dt = finish - start;
	dbg->t1 = 1000000.0*dt.count();
	//writeLog(logOpened, logFile, "Retrieve result returned " + std::string(retrieved ? "1" : "0"));
	// Image grabbed successfully?
	if (!retrieved) {
		//FIXME these line crash. I guess ptrGrabResult is empty if we return by timeout?
		//!ptrGrabResult->GrabSucceeded())
		//ss << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription();
		std::stringstream ss;
		ss << "Error: no image retrieved";
		writeLog(logOpened, logFile, ss.str());
		return -1;
	}
	//writeLog(logOpened,logFile,"Grab succeeded");
	const char *pImageBuffer = (char *)ptrGrabResult->GetBuffer(); //we use mono8, so char == one pixel == uint8
	uint32_t width = ptrGrabResult->GetWidth();
	uint32_t height = ptrGrabResult->GetHeight();
	// copy results to external buffer
	// be very cautious here, external buffer must be of right size
	for (int i = 0; i < height; ++i) { // rows
		for (int j = 0; j < width; ++j) { // columns
			int ind = i * width + j;
			rawData[ind] = pImageBuffer[ind];
		}
	}
	*ts = ptrGrabResult->GetTimeStamp();
	dbg->skipped = ptrGrabResult->GetNumberOfSkippedImages();
	start = std::chrono::high_resolution_clock::now();
	ptrGrabResult.Release();
	finish = std::chrono::high_resolution_clock::now();
	dt = finish - start;
	dbg->t2 = 1000000.0*dt.count();
	return 0;
}

// FIXME need better way of having dbg & release versions of the function
// LabVIEW needs function call with only pointers etc.
// TODO maybe I can do void* and not fill it, if "release" mode?

// external caller has to allocate 1D array of width x height bytes.
// width x height must match camera configuration
// labview can't do 2D arrays that's why 1D is used
extern "C" _declspec(dllexport) int _stdcall retrieveOne(uint8_t* rawData, uint64_t* ts)
{
	CGrabResultPtr ptrGrabResult;
	if (!camera.IsGrabbing()) {
		writeLog(logOpened, logFile, "Camera is not grabbing");
		return -1;
	}
	//bool retrieved = camera.RetrieveResult(timeoutRetrieve, ptrGrabResult, TimeoutHandling_Return);
	bool retrieved = camera.RetrieveResult(timeoutRetrieve, ptrGrabResult, TimeoutHandling_Return);
	// Image grabbed successfully?
	if (!retrieved) {
		//FIXME these line crash. I guess ptrGrabResult is empty if we return by timeout?
		//!ptrGrabResult->GrabSucceeded())
		//ss << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription();
		std::stringstream ss;
		ss << "Error: no image retrieved";
		writeLog(logOpened, logFile, ss.str());
		return -1;
	}
	const char *pImageBuffer = (char *)ptrGrabResult->GetBuffer(); //we use mono8, so char == one pixel == uint8
	uint32_t width = ptrGrabResult->GetWidth();
	uint32_t height = ptrGrabResult->GetHeight();
	// copy results to external buffer
	// be very cautious here, external buffer must be of right size
	for (int i = 0; i < height; ++i) { // rows
		for (int j = 0; j < width; ++j) { // columns
			int ind = i * width + j;
			rawData[ind] = pImageBuffer[ind];
		}
	}
	*ts = ptrGrabResult->GetTimeStamp();
	ptrGrabResult.Release();
	return 0;
}

extern "C" _declspec(dllexport) int _stdcall exitBasler()
{
	writeLog(logOpened,logFile,"Exit was called");
	camera.Close();
	camera.Attach(NULL); // so static Camera struct gets cleared
	PylonTerminate();
	writeLog(logOpened, logFile, "Bye");
	logFile.close();
	return 0;
}

