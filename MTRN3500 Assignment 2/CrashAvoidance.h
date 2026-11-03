#pragma once

#include <UGVModule.h>

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

ref class CrashAvoidance : public UGVModule {
public:

    error_state processSharedMemory() override;

    void shutdownModules();

    bool getShutdownFlag() override;

    void threadFunction() override;

private:
    SM_Laser^ SM_LASER_;
    SM_VehicleControl^ SM_VEHICLE_CONTROL_;
};
