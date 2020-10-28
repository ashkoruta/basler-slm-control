#include "stdafx.h"
#include "bareBonesBasler.h"
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <numeric>

/**********************************
** ILC contoller. Idea is based on feedforward, but creates input profiles for itself after each layer
**
**********************************/
std::vector<float> movmean(const std::vector<float> &v, const int hw) // tested
{
	std::stringstream ss;
	//ss << __FUNCTION__ << ":" << v[0] << "," << v[1] << "," << v[2] << "," << v[3];
	//writeLog(logOpened, logFile, ss.str());
	if (v.empty())
		return v;
	std::vector<float> res(v.size());
	res[0] = v[0];
	res[res.size() - 1] = v.back();
	int i = 1;
	for (; i < hw && i < v.size(); ++i) {
		auto it = v.begin() + i + 1; // accumulate doesn't include element pointed on by last iterator
		res[i] = std::accumulate(v.begin(), it, 0.0) / (i + 1);
	}
	for (; i < int(v.size()) - hw; ++i) {
		auto it1 = v.begin() + i - hw, it2 = v.begin() + i + hw + 1;
		res[i] = std::accumulate(it1, it2, 0.0) / (2 * hw + 1);
	}
	for (; i < v.size() - 1; ++i) {
		auto it = v.begin() + i;
		res[i] = std::accumulate(it, v.end(), 0.0) / (v.size() - i);
	}
	return res;
}

std::vector<float> downsampleMean(const std::vector<float> &in)
{
	const float CoaxFramesPerPowerCommand = 3.4; // 1 ms period for power, and 290 us for camera
	// so window is set to 4, and only first/last image requires special treatment (bad practice?)
	// FIXME should prob put it into config
	// TODO it's very similar to the movmean but iteration count is different
	// should probably refactor
	std::vector<float> ret;
	size_t i = 0;
	float t = 0.0;
	while (i < in.size()) {
		// just a value creates problems in beginning and end of the scan (where values are low)
		// so need to average more of those
		// usually we will do -1 to the left and 2 to the right
		// in the beginning and end, we'll do 4 one-side
		if (i == 0) {
			auto it = in.size() >= 4 ? in.begin() + 4 : in.end();
			ret.push_back(std::accumulate(in.begin(), it, 0.0) / (it - in.begin()));
		} else if (i < in.size() - 2) {
			// best case: lots of things to average
			ret.push_back(std::accumulate(in.begin() + i - 1, in.begin() + i + 3, 0.0) / 4);
		} else {
			auto it = in.size() >= 4 ? in.end() - 4 : in.begin();
			ret.push_back(std::accumulate(in.begin() + i, in.end(), 0.0) / (in.size() - i));
		}
		t = t + CoaxFramesPerPowerCommand; 
		i = floor(t); // that's the index of the next frame that falls right before next laser cycle
	}
	return ret;
}

class IlcManager
{
	bool _init;
	const int _preallocNumber = 3340; // FIXME there got to be better way but let it be for now
	// the longest one is the sin17, there are 11689 camera frames. So we'll need 11689/3.5 = 3340 power values
	float _initialPower;
	unsigned int _totalPartCount;
	int _currentPartNo;
	unsigned int _currentLayerNo;
	typedef std::vector<float> PowerProfile_t;
	std::vector<PowerProfile_t> _curPartProfiles;
	std::string _fileNameMask;
	unsigned int _powerSendCount;
	// ILC update parameters
	float _L_gain;
	int _movmeanHalfWindow;
	int _P_min;
	int _P_max;
	float _noiseFloor;
	float _reference;
	std::vector<float> _measurements;
	std::vector<float> _downsampledError;
	//**** the core: power profile update  ****//
	void loadMeasurements() {
		writeLog(logOpened, logFile, __FUNCTION__);
		_measurements.erase(_measurements.begin(), _measurements.end());
		std::string name = _fileNameMask + "_y_s" + std::to_string(_currentPartNo) + "_l" + std::to_string(_currentLayerNo - 1) + ".txt";
		writeLog(logOpened, logFile, "Opening " + name);
		std::ifstream f(name);
		std::string line;
		std::vector<float> ret;
		while (std::getline(f, line)) {
			// file should have lines of MDL,mean
			std::istringstream ss(line);
			std::string f, s;
			std::getline(ss, f, ',');
			std::getline(ss, s);
			int mdl = std::stoi(f);
			float val = std::stof(s);
			if (mdl < _noiseFloor) {
				//writeLog(logOpened, logFile, "Skipped value @" + line);
				continue;
			}
			_measurements.push_back(val);
		}
		writeLog(logOpened, logFile, "Loading is done, " + std::to_string(_measurements.size()) + " frames");
	}
	void calculateErrors() {
		writeLog(logOpened, logFile, __FUNCTION__);
		loadMeasurements();
		// calculate the error 
		writeLog(logOpened, logFile, "Subtract the target to get an error");
		for (auto it = _measurements.begin(); it != _measurements.end(); ++it) {
			*it = _reference - *it;
		}
		writeLog(logOpened, logFile, "Downsampling");
		// (downsample by taking the closest number)
		// downsample by averaging +- 2 left and right
		_downsampledError.erase(_downsampledError.begin(), _downsampledError.end());
		const int w = 5;
		_downsampledError = downsampleMean(_measurements); 
		//TODO maybe median filter would be better?
		writeLog(logOpened, logFile, "downsampled to " + std::to_string(_downsampledError.size()) + " values");
	}
	void capPowers(PowerProfile_t &v) {
		for (auto it = v.begin(); it != v.end(); ++it) {
			if (*it > _P_max)
				*it = _P_max;
			if (*it < _P_min)
				*it = _P_min;
		}
		return;
	}
	void calculateNewPowerProfile() {
		writeLog(logOpened, logFile, __FUNCTION__);
		if (_currentLayerNo == 0) {
			writeLog(logOpened, logFile, "Initial layer, nothing to do");
			return; // nothing to do, we use initial guess
		}
		// ILC law p_new = sat[ movmean(p_old + L*e) ]
		calculateErrors();
		// update power profile
		PowerProfile_t &pp = _curPartProfiles[_currentPartNo];
		PowerProfile_t old(pp);
		auto pit = pp.begin();
		auto eit = _downsampledError.begin();
		float P_last = _initialPower;
		for (; pit != pp.end() && eit != _downsampledError.end(); ++pit, ++eit) {
			P_last = *pit; // see below
 			*pit = *pit + (*eit) * _L_gain;
		}
		writeLog(logOpened, logFile, "Done with main loop, pit == end:" + std::to_string(pit == pp.end())
			+ " eit == end:" + std::to_string(eit == _downsampledError.end()));
		pp.erase(pit, pp.end()); // if too many values, will remove unnecessary tail
		// if there are not enough power values, i.e. some errors left, we should
		// pad the power profile with either default value of power, or last available value of power
		// I automatically pull last value from getNextPower, if LabVIEW request more than there is
		// but increase in vector size might be useful so do last value padding anyway
		for (; eit != _downsampledError.end(); ++eit) {
			writeLog(logOpened, logFile, "Adding last power value:" + std::to_string(P_last));
			pp.push_back(P_last); // this way if we ran out of P, but there are still errors
			// we still have non-modified last laser power value from previous iteration
		}
		// moving average
		pp = movmean(pp, _movmeanHalfWindow);
		// enforce safety limits
		capPowers(pp);
		writeLog(logOpened, logFile, "Done, new profile length:" + std::to_string(pp.size()));
	}
	//**** save power profile to hard drive  ****//
	void serializePowerProfile() {
		writeLog(logOpened, logFile, __FUNCTION__);
		std::string name = _fileNameMask + "_u_s" + std::to_string(_currentPartNo) + "_l" + std::to_string(_currentLayerNo) + ".txt";
		std::ofstream ff(name);
		const auto &pp = _curPartProfiles[_currentPartNo];
		writeLog(logOpened, logFile, "Serializing powers for part" + std::to_string(_currentPartNo) + " into " + name);
		for (size_t i = 0; i < pp.size(); ++i) {
			ff << pp[i] << std::endl;
		}
		ff.close();
	}
public:
	IlcManager() {
		_init = false;
	}
	bool isInitialized() const { return _init; }
	// all the heavy lifting for IlcManager initialization
	int configure(const std::string &cfg) {
		writeLog(logOpened, logFile, __FUNCTION__);
		std::map<std::string, std::string> cfgParams; // map of parameters
		if (parseCfgFile(cfg, cfgParams) < 0) {
			writeLog(logOpened, logFile, "Controller config is invalid");
			return -1;
		}
		_fileNameMask = cfgParams["FileNameTemplate"]; // base name
		_totalPartCount = std::stoi(cfgParams["Parts"]); // number of different profiles in one layer
		//const int L = std::stoi(cfgParams["MaxLayers"]); // number of layers we will continue with
		_initialPower = 1.0*std::stoi(cfgParams["InitPower"]); // number of layers we will continue with
		_powerSendCount = 0;
		// ILC parameters
		_L_gain = std::stof(cfgParams["Gain"]); // number of layers we will continue with
		_movmeanHalfWindow = std::stoi(cfgParams["HalfWindow"]);
		_P_min = std::stoi(cfgParams["P_min"]);
		_P_max = std::stoi(cfgParams["P_max"]);
		_noiseFloor = std::stof(cfgParams["Threshold"]);
		_reference = std::stof(cfgParams["Target"]);
				
		_curPartProfiles = std::vector<PowerProfile_t>(_totalPartCount);
		_currentLayerNo = 0;
		_currentPartNo = -1; // first ever InitNewScan should start with part #0
		for (unsigned int j = 0; j < _totalPartCount; j++) {
			_curPartProfiles[j] = PowerProfile_t(_preallocNumber, _initialPower);
		}
		_init = true;
		std::stringstream ss;
		ss << "Filename:[" << _fileNameMask << "] parts=" << _totalPartCount << " P_ini=" << _initialPower
			<< " L=" << _L_gain << " halfWindow=" << _movmeanHalfWindow << " P_lim=[" << _P_min << "," << _P_max
			<< "] thr=" << _noiseFloor << " target=" << _reference;
		writeLog(logOpened, logFile, ss.str());
		return 0;
	}
	void initNewScan() {
		writeLog(logOpened, logFile, __FUNCTION__);
		// next part and/or layer
		// in the beginning, PartNo = -1. Once we enter here, it becomes 0,1,2,....8
		// if we enter here with PartNo = 8, it means we already did 9 pieces and need to 
		// change to the next layer
		std::stringstream ss;
		ss << "Previous run l#" << _currentLayerNo << "s#" << _currentPartNo << " sent " << _powerSendCount << " power values";
		writeLog(logOpened, logFile, ss.str());
		if (_currentPartNo == _totalPartCount - 1) { // oops, all done in this layer
			_currentPartNo = -1; // resets the part count initial condition
			_currentLayerNo++;
		}
		_currentPartNo = _currentPartNo + 1; // first ever init will increment -1 to 0, initial layer or not
		// calculate the power profile to be used
		calculateNewPowerProfile();
		// serialize it to disk
		serializePowerProfile();
		_powerSendCount = 0; // actual amount of values sent is only known in getNextPower, here it's for the *PREV* scan
	}
	int getNextPower() {
		float val;
		const PowerProfile_t &pp = _curPartProfiles[_currentPartNo];
		if (pp.empty()) {
			writeLog(logOpened, logFile, "ERROR: why power profile is empty??");
			return _initialPower;
		}
		if (_powerSendCount < pp.size() - 1)
			val = _curPartProfiles[_currentPartNo][_powerSendCount++];
		else // if we are out of values, use the last one
			val = pp.back();
		return val;
	}
	int getCurPart() const { return _currentPartNo; }
	int getCurLayer() const { return _currentLayerNo; }
	void reset() {
		// DLL, and IlcManager, persists in memory. So if I do ILC, then some line scans, then ILC again,
		// there is no way of understanding that we actually should start from layer 0 part 0, and not from 
		// current state. This thing resets the state of the class
		writeLog(logOpened, logFile, "Resetting the state of controller");
		_init = false;
	}
};

static IlcManager C; // so we can use it in both functions, and it's persistent

extern "C" _declspec(dllexport) int _stdcall initController_ilc(const char *cfg)
{
	if (!C.isInitialized()) {
		int ret = C.configure(std::string(cfg));
		if (ret < 0) {
			writeLog(logOpened, logFile, "Controller manager is invalid");
			return -1;
		}
	}
	std::stringstream ss;
	ss << "Init new scan, current state layer#" << C.getCurLayer() << " part#" << C.getCurPart();
	writeLog(logOpened, logFile, ss.str());
	C.initNewScan();
	return 0;
}

extern "C" _declspec(dllexport) int _stdcall nextPower_ilc()
{
	return C.getNextPower();
}

extern "C" _declspec(dllexport) int _stdcall reset_ilc()
{
	C.reset();
	return 0;
}
