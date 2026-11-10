#pragma once

#include <ControllerInterface.h>
#include <UGVModule.h>

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

// 0 = X-Box controller, 1 = keyboard
constexpr int CONTROLLER_INPUT_TYPE = 0;
constexpr DWORD CONTROLLER_PLAYER_NUM = 1;

constexpr double MAX_SPEED = 1.0;
constexpr double MAX_STEERING = 40.0;

ref class Controller : public UGVModule {
public:
    Controller(SM_ThreadManagement^ SM_TM, SM_VehicleControl^ SM_VC);

    ~Controller();
    !Controller();

    error_state processSharedMemory() override;

    error_state processHeartbeats();

    void shutdownModules();

    bool getShutdownFlag() override;

    void threadFunction() override;

private:
    ControllerInterface* CONTROLLER_;
    SM_VehicleControl^ SM_VEHICLE_CONTROL_;
    Stopwatch^ STOPWATCH_;
    bool wasConnected_;

    void writeVehicleControl(double speed, double steering);
};
