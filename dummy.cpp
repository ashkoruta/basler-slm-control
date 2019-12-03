#include "bareBonesBasler.h"

/***********************************
**	Dummy controller: doesn't do anything. Is used as placeholder
**
*************************************/
extern "C" _declspec(dllexport) int _stdcall initController_dummy(const char *cfg)
{
	// does not do anything
	return 0;
}

extern "C" _declspec(dllexport) int _stdcall nextPower_dummy()
{
	// does not do anything
	return -1;
}

