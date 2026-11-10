#using <System.dll>
#include "Display.h"

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

constexpr int DISPLAY_RECONNECT_MS = 1000;

Display::Display(SM_ThreadManagement^ SM_TM, SM_Laser^ SM_laser)
{
	SM_TM_ = SM_TM;
	SM_LASER_ = SM_laser;
	STOPWATCH_ = gcnew Stopwatch;
	RECONNECT_TIMER_ = Stopwatch::StartNew();
	xData_ = gcnew array<double>(STANDARD_LASER_LENGTH);
	yData_ = gcnew array<double>(STANDARD_LASER_LENGTH);
	connect(DISPLAY_ADDRESS, DISPLAY_PORT);
}

error_state Display::processSharedMemory()
{
	Monitor::Enter(SM_LASER_->lockObject);
	try
	{
		Array::Copy(SM_LASER_->x, xData_, STANDARD_LASER_LENGTH);
		Array::Copy(SM_LASER_->y, yData_, STANDARD_LASER_LENGTH);
	}
	finally
	{
		Monitor::Exit(SM_LASER_->lockObject);
	}

	return checkData();
}

error_state Display::processHeartbeats()
{
	if ((SM_TM_->heartbeat & bit_DISPLAY) == 0)
	{
		SM_TM_->heartbeat |= bit_DISPLAY;
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

void Display::shutdownModules()
{
	SM_TM_->shutdown = 0xFF;
}

bool Display::getShutdownFlag()
{
	return SM_TM_->shutdown & bit_DISPLAY;
}

void Display::threadFunction()
{
	Console::WriteLine("Display Thread is starting");
	SM_TM_->ThreadBarrier->SignalAndWait();
	STOPWATCH_->Start();

	while (!getShutdownFlag())
	{
		processHeartbeats();

		error_state result = processSharedMemory();
		if (result == SUCCESS)
			result = communicate();

		if (result != SUCCESS && result != ERR_CONNECTION)
			printError(result);

		Thread::Sleep(50);
	}

	if (Client != nullptr)
		Client->Close();

	Console::WriteLine("Display Thread is terminated");
}

error_state Display::checkData()
{
	if (xData_->Length != STANDARD_LASER_LENGTH || yData_->Length != STANDARD_LASER_LENGTH)
		return error_state::ERR_INVALID_DATA;

	return error_state::SUCCESS;
}

error_state Display::connect(String^ hostName, int portNumber)
{
	RECONNECT_TIMER_->Restart();

	try
	{
		Client = gcnew TcpClient(hostName, portNumber);
		Client->SendBufferSize = 2 * STANDARD_LASER_LENGTH * sizeof(double);
		Client->SendTimeout = 500;
		Client->NoDelay = true;
		Stream = Client->GetStream();
	}
	catch (Exception^ e)
	{
		Console::WriteLine("Display connect failed (is the MATLAB display engine running?): " + e->Message);
		Client = nullptr;
		Stream = nullptr;
		return error_state::ERR_CONNECTION;
	}

	Console::WriteLine("Display connected to MATLAB display engine");
	return error_state::SUCCESS;
}

error_state Display::communicate()
{
	if (Stream == nullptr)
	{
		if (RECONNECT_TIMER_->ElapsedMilliseconds < DISPLAY_RECONNECT_MS)
			return error_state::ERR_CONNECTION;
		if (connect(DISPLAY_ADDRESS, DISPLAY_PORT) != SUCCESS)
			return error_state::ERR_CONNECTION;
	}

	try
	{
		sendDisplayData(xData_, yData_, Stream);
	}
	catch (Exception^)
	{
		Console::WriteLine("Display connection lost");
		Client->Close();
		Client = nullptr;
		Stream = nullptr;
		RECONNECT_TIMER_->Restart();
		return error_state::ERR_CONNECTION;
	}

	return error_state::SUCCESS;
}

void Display::sendDisplayData(array<double>^ xData, array<double>^ yData, NetworkStream^ stream)
{
	array<Byte>^ dataX = gcnew array<Byte>(xData->Length * sizeof(double));
	Buffer::BlockCopy(xData, 0, dataX, 0, dataX->Length);
	array<Byte>^ dataY = gcnew array<Byte>(yData->Length * sizeof(double));
	Buffer::BlockCopy(yData, 0, dataY, 0, dataY->Length);

	stream->Write(dataX, 0, dataX->Length);
	Thread::Sleep(10);
	stream->Write(dataY, 0, dataY->Length);
}
