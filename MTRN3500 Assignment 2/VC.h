#pragma once

#using <System.dll>
#include "NetworkedModule.h"
#include "SMObjects.h"


using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

ref class VehicleControl : public NetworkedModule
{
public:
	VehicleControl(SM_ThreadManagement^ SM_TM, SM_VehicleControl^ SM_vehicleControl);

	error_state processSharedMemory() override;

	error_state processHeartbeats();

	void shutdownModules();

	bool getShutdownFlag() override;

	void threadFunction() override;

	error_state checkData();

	error_state connect(String^ hostName, int portNumber) override;

	error_state communicate() override;

private:
	SM_VehicleControl^ SM_VEHICLE_CONTROL_;
	Stopwatch^ STOPWATCH_;

	double steering_;
	double speed_;
	bool wdog_;

	error_state sendCommand(double steering, double speed);
};