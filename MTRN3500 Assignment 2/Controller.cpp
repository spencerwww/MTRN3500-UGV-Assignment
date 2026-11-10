#using <System.dll>
#include "Controller.h"

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

Controller::Controller(SM_ThreadManagement^ SM_TM, SM_VehicleControl^ SM_VC)
{
	SM_TM_ = SM_TM;
	SM_VEHICLE_CONTROL_ = SM_VC;
	STOPWATCH_ = gcnew Stopwatch;
	CONTROLLER_ = new ControllerInterface(CONTROLLER_PLAYER_NUM, CONTROLLER_INPUT_TYPE);
	wasConnected_ = true;
}

Controller::~Controller()
{
	this->!Controller();
}

Controller::!Controller()
{
	delete CONTROLLER_;
	CONTROLLER_ = nullptr;
}

error_state Controller::processSharedMemory()
{
	controllerState state = CONTROLLER_->GetState();

	if (!state.isConnected)
	{
		writeVehicleControl(0.0, 0.0);

		if (wasConnected_)
			Console::WriteLine("Controller disconnected: speed and steering set to zero");
		wasConnected_ = false;

		return error_state::ERR_NO_DATA;
	}

	if (!wasConnected_)
		Console::WriteLine("Controller connected");
	wasConnected_ = true;

	// Keyboard mode only fills isConnected and the right stick; every other field
	// (buttons, triggers) is left uninitialised, so only read Back with a real controller
	bool keyboardMode = (CONTROLLER_INPUT_TYPE == 1);

	if (!keyboardMode && state.back)
	{
		Console::WriteLine("Controller: Back pressed, shutting down all modules");
		writeVehicleControl(0.0, 0.0);
		shutdownModules();
		return error_state::SUCCESS;
	}

	double speed = keyboardMode
		? state.rightThumbY * MAX_SPEED
		: (state.rightTrigger - state.leftTrigger) * MAX_SPEED;
	// Stick/arrow right is +1, but the UGV treats positive steering as a left turn, so negate
	double steering = -state.rightThumbX * MAX_STEERING;
	writeVehicleControl(speed, steering);

	return error_state::SUCCESS;
}

error_state Controller::processHeartbeats()
{
	if ((SM_TM_->heartbeat & bit_CONTROLLER) == 0)
	{
		SM_TM_->heartbeat |= bit_CONTROLLER;
		STOPWATCH_->Restart();
	}
	else
	{
		if (STOPWATCH_->ElapsedMilliseconds > CRASH_LIMIT)
		{
			shutdownModules();
			return error_state::ERR_TMM_FAILURE;
		}
	}

	return error_state::SUCCESS;
}

void Controller::shutdownModules()
{
	SM_TM_->shutdown = 0xFF;
}

bool Controller::getShutdownFlag()
{
	return SM_TM_->shutdown & bit_CONTROLLER;
}

void Controller::threadFunction()
{
	Console::WriteLine("Controller Thread is starting");
	SM_TM_->ThreadBarrier->SignalAndWait();
	STOPWATCH_->Start();

	while (!getShutdownFlag())
	{
		processHeartbeats();

		if (processSharedMemory() == SUCCESS)
		{
			Console::WriteLine("Controller  Speed: {0,6:F2}  Steering: {1,7:F2}",
				SM_VEHICLE_CONTROL_->Speed, SM_VEHICLE_CONTROL_->Steering);
		}

		Thread::Sleep(20);
	}

	writeVehicleControl(0.0, 0.0);

	Console::WriteLine("Controller Thread is terminated");
}

void Controller::writeVehicleControl(double speed, double steering)
{
	Monitor::Enter(SM_VEHICLE_CONTROL_->lockObject);
	try
	{
		SM_VEHICLE_CONTROL_->Speed = speed;
		SM_VEHICLE_CONTROL_->Steering = steering;
	}
	finally
	{
		Monitor::Exit(SM_VEHICLE_CONTROL_->lockObject);
	}
}
