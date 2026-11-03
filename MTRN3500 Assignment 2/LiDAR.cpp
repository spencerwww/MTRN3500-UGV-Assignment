#using <System.dll>
#include "LiDAR.h"

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

Laser::Laser(SM_ThreadManagement^ SM_TM, SM_Laser^ SM_laser)
{
	SM_TM_ = SM_TM;
	SM_LASER_ = SM_laser;
	STOPWATCH_ = gcnew Stopwatch;
	connect(WEEDER_ADDRESS, LASER_PORT);
}

error_state Laser::processSharedMemory()
{
	String^ Response = Text::Encoding::ASCII->GetString(ReadData);
	array<String^>^ StringArray = Response->Split(' ');

	double StartAngle = Convert::ToInt32(StringArray[23], 16) / 10000.0;
	double Resolution = Convert::ToInt32(StringArray[24], 16) / 10000.0;
	int NumberData = Convert::ToInt32(StringArray[25], 16);

	for (int i = 0; i < NumberData; i++)
	{
		double range = Convert::ToInt32(StringArray[26 + i], 16);
		double angle = (StartAngle + i * Resolution) * Math::PI / 180.0;
		SM_LASER_->x[i] = range * sin(angle);
		SM_LASER_->y[i] = -range * cos(angle);
	}

	return error_state::SUCCESS;
}

error_state Laser::processHeartbeats()
{
	if ((SM_TM_->heartbeat & bit_LASER) == 0)
	{
		SM_TM_->heartbeat |= bit_LASER;
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

void Laser::shutdownModules()
{
	SM_TM_->shutdown = 0xFF;
}

bool Laser::getShutdownFlag()
{
	return SM_TM_->shutdown & bit_LASER;
}

void Laser::threadFunction()
{
	Console::WriteLine("Laser Thread is starting");
	SM_TM_->ThreadBarrier->SignalAndWait();
	STOPWATCH_->Start();

	while (!getShutdownFlag())
	{
		Console::WriteLine("Laser Thread is running");
		processHeartbeats();
		if (communicate() == SUCCESS && checkData() == SUCCESS)
		{
			processSharedMemory();
		}
		Console::WriteLine("--------------Laser Data------------------");
		for (int i = 0; i < STANDARD_LASER_LENGTH; i++) {
			Console::Write("[" + SM_LASER_->x[i] + ", " + SM_LASER_->y[i]  + "] ");
		}
		Console::Write("\n");
		Console::WriteLine("------------------------------------------");
		Thread::Sleep(20);
	}

	Console::WriteLine("Laser Thread is terminated");
}

error_state Laser::checkData()
{
	String^ Response = Text::Encoding::ASCII->GetString(ReadData);
	array<String^>^ StringArray = Response->Split(' ');

	if (StringArray->Length < 27 || StringArray[1] != "LMDscandata")
		return error_state::ERR_INVALID_DATA;

	int NumberData = Convert::ToInt32(StringArray[25], 16);
	if (StringArray->Length < 26 + NumberData)
		return error_state::ERR_INVALID_DATA;

	return error_state::SUCCESS;
}

error_state Laser::connect(String^ hostName, int portNumber)
{
	Client = gcnew TcpClient(hostName, portNumber);
	Client->SendBufferSize = 64;
	Client->ReceiveBufferSize = 2048;
	Client->SendTimeout = 500;
	Client->ReceiveTimeout = 500;
	Client->NoDelay = true;
	Stream = Client->GetStream();

	ReadData = gcnew array<unsigned char>(Client->ReceiveBufferSize);
	array<unsigned char>^ SendData = gcnew array<unsigned char>(Client->SendBufferSize);

	String^ z_id = Z_ID;
	SendData = System::Text::Encoding::ASCII->GetBytes(z_id);

	Stream->Write(SendData, 0, SendData->Length);
	Thread::Sleep(10);

	Stream->Read(ReadData, 0, ReadData->Length);
	Thread::Sleep(25);

	return error_state::SUCCESS;
}

error_state Laser::communicate()
{
	String^ command = "\x02sRN LMDscandata\x03";
	array<unsigned char>^ SendData = System::Text::Encoding::ASCII->GetBytes(command);

	try
	{
		Stream->Write(SendData, 0, SendData->Length);
		Thread::Sleep(10);

		Array::Clear(ReadData, 0, ReadData->Length);
		Stream->Read(ReadData, 0, ReadData->Length);
	}
	catch (System::IO::IOException^ e)
	{
		Console::WriteLine("Laser: no response from LiDAR ({0})", e->Message);
		return error_state::ERR_NO_DATA;
	}

	return error_state::SUCCESS;
}