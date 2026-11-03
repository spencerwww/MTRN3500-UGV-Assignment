#pragma once

#include <UGVModule.h>

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

ref struct ThreadProperties
{
    ThreadStart^ ThreadStart_;
    bool Critical;
    String^ ThreadName;
    uint8_t BitID;

    ThreadProperties(ThreadStart^ start, bool critical, uint8_t bit_id, String^ threadname)
    {
        ThreadStart_ = start;
        Critical = critical;
        BitID = bit_id;
        ThreadName = threadname;
    }
};

ref class ThreadManagement : public UGVModule {
public:
    // Create shared memory objects
    error_state setupSharedMemory();

    // Send/Recieve data from shared memory structures
    error_state processSharedMemory() override;

    // Shutdown all modules in the software
    void shutdownModules();

    // Get Shutdown signal for module, from Thread Management SM
    bool getShutdownFlag() override;

    // Thread function for TMM
    void threadFunction() override;

private:
    // Add any additional data members or helper functions here
    SM_Laser^ SM_LASER_;
    SM_GPS^ SM_GPS_;
    SM_VehicleControl^ SM_VEHICLE_CONTROL_;
    array<Thread^>^ ThreadList;
    array<ThreadProperties^>^ ThreadPropertiesList;
};
