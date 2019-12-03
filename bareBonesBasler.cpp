// bareBonesBasler.cpp : Defines the exported functions for the DLL application.
//

#include <chrono>
#include <fstream>
#include <sstream>
#include <ctime>
#include <map>
#include <string>
#include <vector>
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include "bareBonesBasler.h"

// Namespace for using pylon objects.
using namespace Pylon;
using namespace GenApi;

const std::string logFileName = "basler_dll.log";
const std::string configFileName= "basler.conf";

static CBaslerUsbInstantCamera camera;

std::ofstream logFile;
bool logOpened = false;

extern "C" void openLog()
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
	if (cfgParams["LogEnable"] == "0") {
		writeLog(logOpened, logFile, "Stop writing to log, as requested by cfg file");
		logOpened = false;
	}
	// Set the acquisition parameters
	// 1. subwindow size
	int sz = std::stoi(cfgParams["FrameSide"]);
	camera.Width.SetValue(sz);
	camera.Height.SetValue(sz);
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
	writeLog(logOpened, logFile, "Framerate: " + std::to_string(framerate));
	camera.AcquisitionFrameRateEnable.SetValue(true);
	camera.AcquisitionFrameRate.SetValue(framerate);
	// 5. queue size
	camera.MaxNumBuffer = 100;
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
	// TODO others as required
	writeLog(logOpened, logFile, "Camera parameters set");
	return 0;
}

extern "C" int32_t initBasler(const char *cfgName)
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

extern "C" int startBasler()
{
	try {
		camera.StartGrabbing(GrabStrategy_LatestImages);
		writeLog(logOpened, logFile, "Grabbing started");
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
extern "C" int retrieveOne(uint8_t* rawData, uint64_t* ts)
{
	//auto start = std::chrono::high_resolution_clock::now();
	//auto finish = std::chrono::high_resolution_clock::now();
	CGrabResultPtr ptrGrabResult;
	if (!camera.IsGrabbing()) {
		writeLog(logOpened, logFile, "Camera is not grabbing");
		return -1;
	}
	// Wait for an image and then retrieve it
	//writeLog(logOpened, logFile, "Calling RetrieveResult now...");
	bool retrieved = camera.RetrieveResult(1000, ptrGrabResult, TimeoutHandling_Return);
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
	std::stringstream ss;
	//ss << "Image " << width << " X " << height << std::endl;
	//writeLog(logOpened, logFile, ss.str());
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

extern "C" int exitBasler()
{
	writeLog(logOpened,logFile,"Exit was called");
	camera.Close();
	camera.Attach(NULL); // so static Camera struct gets cleared
	PylonTerminate();
	writeLog(logOpened, logFile, "Bye");
	logFile.close();
	return 0;
}

