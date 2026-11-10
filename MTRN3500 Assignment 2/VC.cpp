#using <System.dll>
#include "VC.h"

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;
using namespace System::Globalization;

constexpr double VC_MAX_STEERING = 40.0;	// degrees
constexpr double VC_MAX_SPEED = 1.0;		// m/s

VehicleControl::VehicleControl(SM_ThreadManagement^ SM_TM, SM_VehicleControl^ SM_vehicleControl)
{
	SM_TM_ = SM_TM;
	SM_VEHICLE_CONTROL_ = SM_vehicleControl;
	STOPWATCH_ = gcnew Stopwatch;
	steering_ = 0.0;
	speed_ = 0.0;
	wdog_ = false;
	connect(WEEDER_ADDRESS, VC_PORT);
}

error_state VehicleControl::processSharedMemory()
{
	Monitor::Enter(SM_VEHICLE_CONTROL_->lockObject);
	try
	{
		steering_ = SM_VEHICLE_CONTROL_->Steering;
		speed_ = SM_VEHICLE_CONTROL_->Speed;
	}
	finally
	{
		Monitor::Exit(SM_VEHICLE_CONTROL_->lockObject);
	}

	return checkData();
}

error_state VehicleControl::processHeartbeats()
{
	if ((SM_TM_->heartbeat & bit_VC) == 0)
	{
		SM_TM_->heartbeat |= bit_VC;
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

void VehicleControl::shutdownModules()
{
	SM_TM_->shutdown = 0xFF;
}

bool VehicleControl::getShutdownFlag()
{
	return SM_TM_->shutdown & bit_VC;
}

void VehicleControl::threadFunction()
{
	Console::WriteLine("Vehicle Control Thread is starting");
	SM_TM_->ThreadBarrier->SignalAndWait();
	STOPWATCH_->Start();

	while (!getShutdownFlag())
	{
		processHeartbeats();

		error_state result = processSharedMemory();
		if (result == SUCCESS)
			result = communicate();

		if (result != SUCCESS)
			printError(result);

		Thread::Sleep(50);
	}

	sendCommand(0.0, 0.0);

	Console::WriteLine("Vehicle Control Thread is terminated");
}

error_state VehicleControl::checkData()
{
	if (Double::IsNaN(steering_) || Double::IsNaN(speed_))
	{
		steering_ = 0.0;
		speed_ = 0.0;
		return error_state::ERR_INVALID_DATA;
	}

	steering_ = Math::Max(-VC_MAX_STEERING, Math::Min(VC_MAX_STEERING, steering_));
	speed_ = Math::Max(-VC_MAX_SPEED, Math::Min(VC_MAX_SPEED, speed_));

	return error_state::SUCCESS;
}

error_state VehicleControl::connect(String^ hostName, int portNumber)
{
	try
	{
		Client = gcnew TcpClient(hostName, portNumber);
		Client->SendBufferSize = 64;
		Client->ReceiveBufferSize = 64;
		Client->SendTimeout = 500;
		Client->ReceiveTimeout = 500;
		Client->NoDelay = true;
		Stream = Client->GetStream();

		array<unsigned char>^ SendData = Text::Encoding::ASCII->GetBytes(Z_ID);
		Stream->Write(SendData, 0, SendData->Length);

		array<unsigned char>^ Response = gcnew array<unsigned char>(64);
		int bytesRead = Stream->Read(Response, 0, Response->Length);
		String^ reply = Text::Encoding::ASCII->GetString(Response, 0, bytesRead)->Trim();

		if (reply != "OK")
		{
			Console::WriteLine("Vehicle Control authentication failed, server replied: " + reply);
			return error_state::ERR_CONNECTION;
		}
	}
	catch (Exception^ e)
	{
		Console::WriteLine("Vehicle Control connect failed: " + e->Message);
		Stream = nullptr;
		return error_state::ERR_CONNECTION;
	}

	Console::WriteLine("Vehicle Control connected and authenticated");
	return error_state::SUCCESS;
}

error_state VehicleControl::communicate()
{
	return sendCommand(steering_, speed_);
}

error_state VehicleControl::sendCommand(double steering, double speed)
{
	if (Stream == nullptr)
		return error_state::ERR_CONNECTION;

	wdog_ = !wdog_;

	String^ command = String::Format(CultureInfo::InvariantCulture,
		"# {0:F3} {1:F3} {2} #", steering, speed, wdog_ ? 1 : 0);

	try
	{
		array<unsigned char>^ SendData = Text::Encoding::ASCII->GetBytes(command);
		Stream->Write(SendData, 0, SendData->Length);
	}
	catch (Exception^)
	{
		return error_state::ERR_CONNECTION;
	}

	return error_state::SUCCESS;
}
