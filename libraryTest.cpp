// libraryTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <chrono>
#include <sched.h>
#include "bareBonesBasler.h"

int test1()
{
	const char *cfg = "basler.conf";
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

int test_time_acquisition()
{
	using namespace std::chrono;
	const char *cfg = "basler.conf";
	uint8_t buf[32 * 32];
	int ret = 0;
	ret = initBasler(cfg);
	if (ret < 0)
		return -1;
	const struct sched_param prio = {1};
	//sched_setscheduler(0,SCHED_FIFO,&prio);
	ret = startBasler();
	if (ret < 0)
		return -1;
	uint64_t ts;
	double mx = 0, mn = 50000, av = 0;
	for (int i = 0; i < 100; ++i) {
		high_resolution_clock::time_point t1 = high_resolution_clock::now();
		ret = retrieveOne(buf,&ts);
		high_resolution_clock::time_point t2 = high_resolution_clock::now();
		duration<double> time_span = duration_cast<duration<double>>(t2 - t1); 
		double dt = 1000000.0*time_span.count();
		//std::cout << "dt = " << dt << std::endl;
		mx = dt > mx ? dt : mx;
		mn = dt < mn ? dt : mn;
		av = (i*av + dt)/(i+1);
	}
	std::cout << "dt = " << av << " us, min = " << mn << " us, max = " << mx << std::endl;
	ret = exitBasler();
	return ret;
}

/*
int test2()
{
	openLog();
	const char *ctrlCfg = "C:/gateway/ctrl_cfg.txt";
	//const char *powerFile = "C:/gateway/test_power.txt";
	int res = initController_ff(ctrlCfg);
	for (int i = 0; i < 40; i++) {
		std::cout << nextPower_ff() << std::endl;
	}
	return 0;
}

int test3()
{
	openLog();
	std::cout << "dummy init returned " << initController_dummy("") << std::endl;
	std::cout << "dummy nextPower returned " << nextPower_dummy() << std::endl;
	return 0;
}

int test4()
{
	std::vector<float> t = {1,2,3,4,5,6,7,8,9,10,11,12};
	auto res = movmean(t, 2);
	for (auto it = res.begin(); it != res.end(); ++it) {
		std::cout << *it << std::endl;
	}
	return 0;
}

int test5()
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

int test6()
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
*/
int main()
{
	//return test1();
	return test_time_acquisition();
	//return test2();
	//return test3();
	//return test4();
	//return test5();
	//return test6();
}
