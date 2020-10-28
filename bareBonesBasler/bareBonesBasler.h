#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <map>

extern std::ofstream logFile;
extern bool logOpened;

struct DbgInfo {
	double t1;
	double t2;
	int skipped;
};

typedef struct DbgInfo DbgInfo;

extern "C" _declspec(dllexport) void _stdcall openLog();
extern "C" void writeLog(bool , std::ofstream &, const std::string &);

extern "C" int parseCfgFile(const std::string &config, std::map<std::string, std::string> &cfgParams);
extern "C" int loadVectorFromFile(std::vector<double> &, const std::string &fileName);

extern _declspec(dllexport) std::vector<float> movmean(const std::vector<float> &, int );

extern "C" _declspec(dllexport) int _stdcall initBasler(const char *cfg);
extern "C" _declspec(dllexport) int _stdcall startBasler();
extern "C" _declspec(dllexport) int _stdcall retrieveOne(uint8_t*, uint64_t*);
extern "C" _declspec(dllexport) int _stdcall retrieveOne_dbg(uint8_t*, uint64_t*, DbgInfo*);
extern "C" _declspec(dllexport) int _stdcall exitBasler();

extern "C" _declspec(dllexport) int _stdcall initController_dummy(const char *fileName);
extern "C" _declspec(dllexport) int _stdcall nextPower_dummy();

extern "C" _declspec(dllexport) int _stdcall initController_ff(const char *fileName);
extern "C" _declspec(dllexport) int _stdcall nextPower_ff();

extern "C" _declspec(dllexport) int _stdcall initController_ilc(const char *fileName);
extern "C" _declspec(dllexport) int _stdcall nextPower_ilc();
extern "C" _declspec(dllexport) int _stdcall reset_ilc();

extern "C" _declspec(dllexport) int _stdcall initController_fb(const char *fileName);
extern "C" _declspec(dllexport) double _stdcall nextRef_fb();
