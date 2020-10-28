#include "stdafx.h"
#include "bareBonesBasler.h"
#include <vector>
#include <map>
#include <string>

/*****************************************
**	Simple feedforward controller: sets powers at 1 ms intervals, as defined in input file
**
******************************************/

static std::vector<double> ff_powerProfile; // list of power values, stored in memory permanently after init call
static int ff_pCount = 0; // current position in the list, maintained btwn function calls. is reset in init call

int loadVectorFromFile(std::vector<double> &in, const std::string &fileName)
{
	writeLog(logOpened, logFile, "Profile file:" + fileName);
	std::ifstream pp(fileName);
	if (!pp.is_open()) {
		writeLog(logOpened, logFile, "Failed to read power profile file");
		return -1;
	}
	// first number is a file length
	std::string line;
	std::getline(pp, line);
	int N = std::stoi(line); // number of power profile samples
	in = std::vector<double>(N); // vector of zeros to begin with
	writeLog(logOpened, logFile, "Number of values: " + std::to_string(N));
	// start filling up the contents
	int j = 0;
	while (std::getline(pp, line)) {
		double p = std::stod(line); //TODO maybe we need float here, but let's do int for now
		if (j > N) {
			writeLog(logOpened, logFile, "Number of lines in the file is larger than declared originally, ignore the rest");
			break;
		}
		in[j++] = p;
	}
	writeLog(logOpened && j < N, logFile, "Number of lines in the file is smaller than declared originally");
	return 0;
}

extern "C" _declspec(dllexport) int _stdcall initController_ff(const char *cfg)
{
	// dumb feedforward: list of powers, return them sequentially
	std::map<std::string, std::string> cfgParams; // map of parameters
	if (parseCfgFile(std::string(cfg), cfgParams) < 0) {
		writeLog(logOpened, logFile, "Controller config is invalid");
		return -1;
	}
	ff_pCount = 0; // reset the position in the list
	return loadVectorFromFile(ff_powerProfile, cfgParams["FileName"]);
}

extern "C" _declspec(dllexport) int _stdcall nextPower_ff()
{
	// we have position inside the vector stored and incremented each function call
	// once we ran out of values, we will use the last one indefinitely
	int ret = ff_powerProfile[ff_pCount]; // implicit cast double to int
	writeLog(logOpened, logFile, "Retrieved power[" + std::to_string(ff_pCount) + "] = " + std::to_string(ret));
	if (ff_pCount < ff_powerProfile.size() - 1) {
		++ff_pCount;
	}
	else {
		writeLog(logOpened, logFile, "Reached the end of power profile");
	}
	return ret;
}

