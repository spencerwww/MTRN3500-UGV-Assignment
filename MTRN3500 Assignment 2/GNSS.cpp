#using <System.dll>
#include "GNSS.h"

using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

GPS::GPS(SM_ThreadManagement^ SM_TM, SM_GPS^ SM_gps)
{
	SM_TM_ = SM_TM;
	SM_GPS_ = SM_gps;
	STOPWATCH_ = gcnew Stopwatch;
	connect(WEEDER_ADDRESS, GNSS_PORT);
}

error_state GPS::processSharedMemory()
{
	double northing = BitConverter::ToDouble(ReadData, GNSS_NORTHING_OFFSET);
	double easting = BitConverter::ToDouble(ReadData, GNSS_EASTING_OFFSET);
	double height = BitConverter::ToDouble(ReadData, GNSS_HEIGHT_OFFSET);

	Monitor::Enter(SM_GPS_->lockObject);
	try
	{
		SM_GPS_->Northing = northing;
		SM_GPS_->Easting = easting;
		SM_GPS_->Height = height;
	}
	finally
	{
		Monitor::Exit(SM_GPS_->lockObject);
	}

	return error_state::SUCCESS;
}

error_state GPS::processHeartbeats()
{
	if ((SM_TM_->heartbeat & bit_GPS) == 0)
	{
		SM_TM_->heartbeat |= bit_GPS;
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

void GPS::shutdownModules()
{
	SM_TM_->shutdown = 0xFF;
}

bool GPS::getShutdownFlag()
{
	return SM_TM_->shutdown & bit_GPS;
}

void GPS::threadFunction()
{
	Console::WriteLine("GPS Thread is starting");
	SM_TM_->ThreadBarrier->SignalAndWait();
	STOPWATCH_->Start();

	while (!getShutdownFlag())
	{
		Console::WriteLine("GPS Thread is running");
		processHeartbeats();

		error_state result = communicate();
		if (result == SUCCESS)
			result = checkData();

		if (result == SUCCESS)
		{
			processSharedMemory();
			Console::WriteLine("GPS  Northing: {0,12:F3}  Easting: {1,12:F3}  Height: {2,8:F3}",
				SM_GPS_->Northing, SM_GPS_->Easting, SM_GPS_->Height);
		}
		else if (result != ERR_NO_DATA)
		{
			printError(result);
		}

		Thread::Sleep(20);
	}

	Console::WriteLine("GPS Thread is terminated");
}

error_state GPS::checkData()
{
	unsigned int receivedCRC = BitConverter::ToUInt32(ReadData, GNSS_CRC_OFFSET);
	unsigned int calculatedCRC = CalculateBlockCRC32(GNSS_CRC_OFFSET, ReadData);

	if (receivedCRC != calculatedCRC)
		return error_state::ERR_INVALID_DATA;

	return error_state::SUCCESS;
}

error_state GPS::connect(String^ hostName, int portNumber)
{
	try
	{
		Client = gcnew TcpClient(hostName, portNumber);
		Client->ReceiveBufferSize = 1024;
		Client->ReceiveTimeout = 500;
		Client->NoDelay = true;
		Stream = Client->GetStream();
	}
	catch (Exception^ e)
	{
		Console::WriteLine("GPS connect failed: " + e->Message);
		Stream = nullptr;
		return error_state::ERR_CONNECTION;
	}

	ReadData = gcnew array<unsigned char>(GNSS_PACKET_SIZE);

	return error_state::SUCCESS;
}

error_state GPS::communicate()
{
	if (Stream == nullptr)
		return error_state::ERR_CONNECTION;

	if (Client->Available < GNSS_PACKET_SIZE)
		return error_state::ERR_NO_DATA;

	try
	{
		unsigned int header = 0;
		int bytesScanned = 0;
		do
		{
			int data = Stream->ReadByte();
			if (data < 0)
				return error_state::ERR_CONNECTION;

			header = (header << 8) | (unsigned char)data;

			if (++bytesScanned > GNSS_MAX_SYNC_BYTES)
				return error_state::ERR_NO_DATA;
		} while (header != GNSS_HEADER);

		ReadData[0] = 0xAA;
		ReadData[1] = 0x44;
		ReadData[2] = 0x12;
		ReadData[3] = 0x1C;

		int offset = 4;
		while (offset < GNSS_PACKET_SIZE)
		{
			int bytesRead = Stream->Read(ReadData, offset, GNSS_PACKET_SIZE - offset);
			if (bytesRead <= 0)
				return error_state::ERR_CONNECTION;
			offset += bytesRead;
		}
	}
	catch (IO::IOException^)
	{
		return error_state::ERR_NO_DATA;
	}

	return error_state::SUCCESS;
}

unsigned int GPS::CRC32Value(int i)
{
	unsigned int crc = i;
	for (int j = 8; j > 0; j--)
	{
		if (crc & 1)
			crc = (crc >> 1) ^ 0xEDB88320;
		else
			crc >>= 1;
	}
	return crc;
}

unsigned int GPS::CalculateBlockCRC32(int count, array<unsigned char>^ data)
{
	unsigned int crc = 0;
	for (int i = 0; i < count; i++)
	{
		unsigned int temp1 = (crc >> 8) & 0x00FFFFFF;
		unsigned int temp2 = CRC32Value(((int)crc ^ data[i]) & 0xFF);
		crc = temp1 ^ temp2;
	}
	return crc;
}
