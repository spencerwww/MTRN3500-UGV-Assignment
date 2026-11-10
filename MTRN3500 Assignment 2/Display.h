#pragma once

#using <System.dll>
#include "NetworkedModule.h"
#include "SMObjects.h"


using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

ref class Display : public NetworkedModule
{
public:
	Display(SM_ThreadManagement^ SM_TM, SM_Laser^ SM_laser);

	error_state processSharedMemory() override;

	error_state processHeartbeats();

	void shutdownModules();

	bool getShutdownFlag() override;

	void threadFunction() override;

	error_state checkData();

	error_state connect(String^ hostName, int portNumber) override;

	error_state communicate() override;

private:
	SM_Laser^ SM_LASER_;
	Stopwatch^ STOPWATCH_;
	Stopwatch^ RECONNECT_TIMER_;

	array<double>^ xData_;
	array<double>^ yData_;

	void sendDisplayData(array<double>^ xData, array<double>^ yData, NetworkStream^ stream);
};