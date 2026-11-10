#pragma once

#using <System.dll>
#include "NetworkedModule.h"
#include "SMObjects.h"


using namespace System;
using namespace System::Threading;
using namespace System::Diagnostics;

constexpr unsigned int GNSS_HEADER = 0xAA44121C;
constexpr int GNSS_PACKET_SIZE = 112;
constexpr int GNSS_CRC_OFFSET = 108;
constexpr int GNSS_NORTHING_OFFSET = 44;
constexpr int GNSS_EASTING_OFFSET = 52;
constexpr int GNSS_HEIGHT_OFFSET = 60;
constexpr int GNSS_MAX_SYNC_BYTES = 4 * GNSS_PACKET_SIZE;

ref class GPS : public NetworkedModule
{
public:
	GPS(SM_ThreadManagement^ SM_TM, SM_GPS^ SM_gps);

	error_state processSharedMemory() override;

	error_state processHeartbeats();

	void shutdownModules();

	bool getShutdownFlag() override;

	void threadFunction() override;

	error_state checkData();

	error_state connect(String^ hostName, int portNumber) override;

	error_state communicate() override;

private:
	SM_GPS^ SM_GPS_;
	Stopwatch^ STOPWATCH_;

	static unsigned int CRC32Value(int i);
	static unsigned int CalculateBlockCRC32(int count, array<unsigned char>^ data);
};