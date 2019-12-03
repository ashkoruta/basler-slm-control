#include <fstream>
#include <string>
#include <vector>
#include <map>

extern std::ofstream logFile;
extern bool logOpened;

extern "C" void openLog();
extern "C" void writeLog(bool , std::ofstream &, const std::string &);

extern "C" int parseCfgFile(const std::string &config, std::map<std::string, std::string> &cfgParams);

extern std::vector<float> movmean(const std::vector<float> &, int );

extern "C" int initBasler(const char *cfg);
extern "C" int startBasler();
extern "C" int retrieveOne(uint8_t*, uint64_t*);
extern "C" int exitBasler();

extern "C" int initController_dummy(const char *fileName);
extern "C" int nextPower_dummy();

extern "C" int initController_ff(const char *fileName);
extern "C" int nextPower_ff();

extern "C" int initController_ilc(const char *fileName);
extern "C" int nextPower_ilc();
extern "C" int reset_ilc();
