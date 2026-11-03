#pragma once

#using <System.dll>
#include "NetworkedModule.h"
#include "SMObjects.h"


using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

ref class GPS : public NetworkedModule
{
public:
	GPS(SM_ThreadManagement^ SM_TM, SM_GPS^ SM_gps);

	error_state processSharedMemory() override;

	error_state processHeartbeats();

	void shutdownModules();

	bool getShutdownFlag() override;

	void threadFunction() override;

	error_state checkData();

	error_state connect(String^ hostName, int portNumber) override;

	error_state communicate() override;

private:
	SM_GPS^ SM_GPS_;
	Stopwatch^ STOPWATCH_;
};