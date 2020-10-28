#include "stdafx.h"
#include "bareBonesBasler.h"
#include <vector>
#include <map>
#include <string>

/*****************************************
**	Simple reference tracking: store reference signal values and give them away one by one
**  TODO is virtually same code as with FF
******************************************/

static std::vector<double> fb_reference; // list of reference signal values, stored in memory permanently after init call
static int fb_rCount = 0; // current position in the list, maintained btwn function calls. is reset in init call

extern "C" _declspec(dllexport) int _stdcall initController_fb(const char *cfg)
{
	// dumb feedforward: list of powers, return them sequentially
	std::map<std::string, std::string> cfgParams; // map of parameters
	if (parseCfgFile(std::string(cfg), cfgParams) < 0) {
		writeLog(logOpened, logFile, "Controller config is invalid");
		return -1;
	}
	fb_rCount = 0; // reset count
	return loadVectorFromFile(fb_reference, cfgParams["FileName"]);
}

extern "C" _declspec(dllexport) double _stdcall nextRef_fb()
{
	// we have position inside the vector stored and incremented each function call
	// once we ran out of values, we will use the last one indefinitely
	double ret = fb_reference[fb_rCount];
	writeLog(logOpened, logFile, "Retrieved ref[" + std::to_string(fb_rCount) + "] = " + std::to_string(ret));
	if (fb_rCount < fb_reference.size() - 1) {
		++fb_rCount;
	}
	else {
		writeLog(logOpened, logFile, "Reached the end of reference profile");
	}
	return ret;
}

