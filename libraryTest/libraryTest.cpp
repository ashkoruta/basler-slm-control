// libraryTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <chrono>
#include <string>
#include "bareBonesBasler.h"
#include <windows.h>
#include <ctime>

int test_basler()
{
	const char *cfg = "C:/gateway/basler.conf";
	uint8_t buf[32 * 32];
	int ret = 0;
	for (int i = 0; i < 3; ++i) {
		std::cout << "#### " << i << " ####" << std::endl;
		ret = initBasler(cfg);
		printf("init: %d\n", ret);
		if (ret < 0)
			break;
		ret = startBasler();
		printf("start: %d\n", ret);
		if (ret < 0)
			break;
		uint64_t ts;
		ret = retrieveOne(buf,&ts);
		printf("grab: %d\n", ret);
		if (ret < 0)
			break;
		ret = exitBasler();
		printf("exit: %d\n", ret);
		if (ret < 0)
			break;

	}
	return ret;
}

int test_ff()
{
	openLog();
	const char *ctrlCfg = "C:/gateway/ctrl_feedforward.cfg";
	int res = initController_ff(ctrlCfg);
	for (int i = 0; i < 40; i++) {
		std::cout << nextPower_ff() << std::endl;
	}
	return 0;
}

int test_dummy()
{
	openLog();
	std::cout << "dummy init returned " << initController_dummy("") << std::endl;
	std::cout << "dummy nextPower returned " << nextPower_dummy() << std::endl;
	return 0;
}

int test_movmean()
{
	std::vector<float> t = {1,2,3,4,5,6,7,8,9,10,11,12};
	auto res = movmean(t, 2);
	for (auto it = res.begin(); it != res.end(); ++it) {
		std::cout << *it << std::endl;
	}
	return 0;
}

int test_ilc()
{
	openLog();
	initController_ilc("C:/gateway/ctrl_ilc.cfg");
	for (int i = 0; i < 20; ++i) {
		// 20 scans, i.e. 2 full layers and a change
		std::cout << "init controller " << i << std::endl;
		initController_ilc("C:/gateway/ctrl_ilc.cfg");
		for (int i = 0; i < 20; ++i) {
			std::cout << i  << ":" << nextPower_ilc() << std::endl;
		}
	}
	// now current state is: layer = 2, scan = 2
	// reset the controller
	reset_ilc();
	// now if we do init, it should be config etc. all over again
	initController_ilc("C:/gateway/ctrl_ilc.cfg");
	return 0;
}

int test_ilc_emulation()
{
	openLog();
	int count = 0;
	int layer = 0;
	int part = 0;
	while (1) {
		std::cout << "init controller: layer " << layer << " part " << part << std::endl;
		initController_ilc("C:/gateway/ctrl_ilc.cfg");
		// count number of lines in the output to mimic number of nextPowers
		std::string n = "C:/gateway/crash_replica_debug/ilc_y_s" + std::to_string(part) + "_l" + std::to_string(layer) + ".txt";
		std::ifstream f(n);
		size_t lc = 0;
		while (std::getline(f, n)) { ++lc; }
		for (int i = 0; i < lc/3.5; ++i) { // because fps of camera is 3500 while laser update is 1000 Hz
			nextPower_ilc();
		}
		if (++part == 9) {
			++layer;
			part = 0;
		}
	}
	return 0;
}

int test_time_acquisition(int N)
{
	using namespace std::chrono;
	const char *cfg = "basler.conf";
	uint8_t buf[32 * 32];
	int ret = 0;
	//if (!SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS)) {
	//	std::cout << "Failed to switch into realtime class\n";
	//	return -1;
	//}
	ret = initBasler(cfg);
	if (ret < 0)
		return -1;
	ret = startBasler();
	if (ret < 0)
		return -1;
	uint64_t ts;
	double mx = 0, mn = 50000, av = 0;
	std::vector<double> dt1(N), dt2(N);
	int skipped = 0, returns = 0;
	auto start = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < N; ++i) {
		DbgInfo dbg;
		int ret = retrieveOne_dbg(buf, &ts, &dbg);
		returns += ret;
		dt1[i] = dbg.t1;
		dt2[i] = dbg.t2;
		ret = dbg.skipped;
		skipped += ret;
	}
	auto finish = std::chrono::high_resolution_clock::now();
	ret = exitBasler();
	for (int i = 0; i < N; ++i) {
		printf("%.1f\t%.1f\n", dt1[i], dt2[i]);
	}
	std::chrono::duration<double> dt = finish - start;
	std::cout << 1e6*dt.count() << " " << skipped << " " << returns << std::endl;
	return ret;
}

int exact_labview_replica()
{
	const char *cfg = "basler.conf";
	uint8_t buf[32 * 32];
	int ret = 0;
	ret = initBasler(cfg);
	if (ret < 0)
		return -1;
	ret = startBasler();
	if (ret < 0)
		return -1;
	uint64_t ts;
	double mx = 0, mn = 50000, av = 0;
	auto start = std::chrono::high_resolution_clock::now(), prev = start;
	int i = 0;
	while (i < 100) {
		auto t1 = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> dt = t1 - prev;
		if (1e6*dt.count() < 300.0)
			continue;
		prev = t1;
		DbgInfo dbg;
		retrieveOne_dbg(buf, &ts, &dbg);
		i++;
	}
	auto finish = std::chrono::high_resolution_clock::now();
	ret = exitBasler();
	//for (int i = 0; i < N; ++i)
	//	printf("%d:%.1f\t%.1f\t%d\n", returns[i], dt1[i], dt2[i],skipped[i]);
	std::chrono::duration<double> dt = finish - start;
	std::cout << 1e6*dt.count() << std::endl;
	return ret;
}

int test_clocks()
{
	auto c1 = std::chrono::high_resolution_clock::now();
	auto sc1 = std::chrono::steady_clock::now();
	auto cc1 = std::clock();
	int tmp = 0;
	for (int i = 0; i < 100; i++) {
		for (int j = 0; j < 100; j++)
			tmp += i * j;
	}
	auto c2 = std::chrono::high_resolution_clock::now();
	auto sc2 = std::chrono::steady_clock::now();
	auto cc2 = std::clock();
	std::cout
		<< " HR: " << std::chrono::duration<double, std::micro>(c2 - c1).count()
		<< " SC: " << std::chrono::duration<double, std::micro>(sc2 - sc1).count()
		<< " C: " << 1000000.0 * (cc2 - cc1) / CLOCKS_PER_SEC << "ms";
	return 0;
}

int test_fb()
{
	openLog();
	const char *ctrlCfg = "C:/gateway/ctrl_feedback.cfg";
	int res = initController_fb(ctrlCfg);
	for (int i = 0; i < 40; i++) {
		std::cout << nextRef_fb() << std::endl;
	}
	return 0;
}
int main(int argc, char** argv)
{
	return test_basler();
	//return test_ff();
	//return test_fb();
	//return test_dummy();
	//return test_movmean();
	//return test_ilc();
	//return test_clocks();
	//return exact_labview_replica();
	//int N = 100;
	//if (argc > 1)
	//	N = std::stoi(std::string(argv[1]));
	//return test_time_acquisition(N);
}