#pragma once

#include <UGVModule.h>
#include <ControllerInterface.h>

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

ref class Controller : public UGVModule {
public:

    error_state processSharedMemory() override;

    void shutdownModules();

    bool getShutdownFlag() override;

    void threadFunction() override;

private:
    ControllerInterface* CONTROLLER_;
    SM_VehicleControl^ SM_VEHICLE_CONTROL_;
};
