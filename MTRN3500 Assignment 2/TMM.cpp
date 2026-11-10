#using <System.dll>

#include "Controller.h"
#include "TMM.h"
#include "LiDAR.h"
#include "GNSS.h"
#include "VC.h"

error_state ThreadManagement::setupSharedMemory()
{
	SM_TM_ = gcnew SM_ThreadManagement;
	SM_LASER_ = gcnew SM_Laser;
	SM_GPS_ = gcnew SM_GPS;
	SM_VEHICLE_CONTROL_ = gcnew SM_VehicleControl;

	return error_state::SUCCESS;
}

error_state ThreadManagement::processSharedMemory() {
	for (int i = 0; i < ThreadList->Length; i++)
	{
		if (SM_TM_->heartbeat & ThreadPropertiesList[i]->BitID)
		{
			SM_TM_->heartbeat &= ~ThreadPropertiesList[i]->BitID;
			SM_TM_->WatchList[i]->Restart();
		}
		else
		{
			if (SM_TM_->WatchList[i]->ElapsedMilliseconds > CRASH_LIMIT)
			{
				if (ThreadPropertiesList[i]->Critical)
				{
					Console::WriteLine(ThreadPropertiesList[i]->ThreadName + " (critical) failure. Shutting down all threads");
					shutdownModules();
					return error_state::ERR_CRITICAL_THREAD_FAILURE;
				}
				else
				{
					Console::WriteLine(ThreadPropertiesList[i]->ThreadName + " failed... Restarting");
					ThreadList[i]->Abort();
					ThreadList[i] = gcnew Thread(ThreadPropertiesList[i]->ThreadStart_);
					SM_TM_->ThreadBarrier = gcnew Barrier(1);
					ThreadList[i]->Start();
				}
			}
		}
	}
	return error_state::SUCCESS;
}

void ThreadManagement::shutdownModules()
{
	SM_TM_->shutdown = 0xFF;
}

bool ThreadManagement::getShutdownFlag()
{
	return (SM_TM_->shutdown & bit_TM);
}

void ThreadManagement::threadFunction()
{
	Console::WriteLine("TMM Thread is starting");

	ThreadPropertiesList = gcnew array<ThreadProperties^>
	{
		gcnew ThreadProperties(gcnew ThreadStart(gcnew Laser(SM_TM_, SM_LASER_), &Laser::threadFunction), bit_LASER, "Laser Thread"),
		gcnew ThreadProperties(gcnew ThreadStart(gcnew GPS(SM_TM_, SM_GPS_), &GPS::threadFunction), bit_GPS, "GPS Thread"),
		gcnew ThreadProperties(gcnew ThreadStart(gcnew Controller(SM_TM_, SM_VEHICLE_CONTROL_), &Controller::threadFunction), bit_CONTROLLER, "Controller Thread"),
		gcnew ThreadProperties(gcnew ThreadStart(gcnew VehicleControl(SM_TM_, SM_VEHICLE_CONTROL_), &VehicleControl::threadFunction), bit_VC, "Vehicle Control Thread")
	};
	ThreadList = gcnew array<Thread^>(ThreadPropertiesList->Length);
	SM_TM_->WatchList = gcnew array<Stopwatch^>(ThreadPropertiesList->Length);
	SM_TM_->ThreadBarrier = gcnew Barrier(ThreadPropertiesList->Length + 1);

	for (int i = 0; i < ThreadPropertiesList->Length; i++)
	{
		SM_TM_->WatchList[i] = gcnew Stopwatch;
		ThreadList[i] = gcnew Thread(ThreadPropertiesList[i]->ThreadStart_);
		ThreadList[i]->Start();
	}

	SM_TM_->ThreadBarrier->SignalAndWait();

	for (int i = 0; i < ThreadPropertiesList->Length; i++)
	{
		SM_TM_->WatchList[i]->Start();
	}

	while (!getShutdownFlag())
	{
		if (Console::KeyAvailable && Console::ReadKey(true).Key == System::ConsoleKey::Q)
		{
			Console::WriteLine("Routine shutdown of TMM thread");
			break;
		}
		Console::WriteLine("TMM Thread is running");
		processSharedMemory();
		Thread::Sleep(50);
	}

	shutdownModules();

	for (int i = 0; i < ThreadPropertiesList->Length; i++)
	{
		ThreadList[i]->Join();
	}

	Console::WriteLine("TMM Thread is terminated");
}