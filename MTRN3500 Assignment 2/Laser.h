#pragma once

#using <Systems.dll>
#include "NetworkedModule.h"
#include "SMObjects.h"


using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

ref class Laser : public NetworkedModule
{
public:
	Laser();

	error_state setupSharedMemory();

	error_state processSharedMemory() override;
	
	error_state processHeartbeats();

	void shutdownThreads();

	bool getShutdownFlag() override;

	void threadFunction() override;

	error_state checkData();

	error_state connect() override;

	error_state communicate() override;

private:
	SM_ThreadManagement^ SM_TM_;
	SM_Laser^ SM_LASER_;
	Stopwatch^ Stopwatch;
};