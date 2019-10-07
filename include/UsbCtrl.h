// UsbCtrl.h

#ifndef UsbCtrl_H
#define UsbCtrl_H

#include <string>
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>

extern "C"{
#include "ftd2xx.h"
}
#include "Listener.h"
#include "Daq.h"

class TUsbConfig
{
public:
	std::vector<std::string> serials; // USB serial IDs
	
	// USB configurations
	bool doReset; // do device reset
	
	unsigned int baudRate; // baud rate: max=3M (3,000,000)
	unsigned char wordBits; // FT_BITS_8 or FT_BITS_7
	unsigned char stopBits; // FT_STOP_BITS_1 or FT_STOP_BITS_2
	unsigned char parity; // FT_PARITY_NONE, FT_PARITY_ODD, FT_PARITY_EVEN, FT_PARITY_MARK or FT_PARITY SPACE.
	unsigned short flowControl; // FT_FLOW_NONE, FT_FLOW_RTS_CTS, FT_FLOW_DTR_DSR or FT_FLOW_XON_XOFF.
	unsigned int purgeMode; // FT_PURGE_RX and/or FT_PURGE_TX

	unsigned writeTimeout; // timeout in millisec
	unsigned readTimeout; // timeout in millisec
	unsigned dataWaitTimeout; // in millisec
	std::string outLogFileName;
	
	int dataTakingWait; // wait after stopacq in msec

	TUsbConfig() : 
			doReset(true)
		,	baudRate(3000000)
		,	wordBits(FT_BITS_8)
		,	stopBits(FT_STOP_BITS_1)
		,	parity(FT_PARITY_NONE)
		,	flowControl(FT_FLOW_NONE)
		,	purgeMode(FT_PURGE_RX | FT_PURGE_TX)
		,	writeTimeout(100)
		,	readTimeout(200)
		,	dataWaitTimeout(1000)
		,	outLogFileName("")
		,	dataTakingWait(100)
		{} 
};

class TUsbConn
{
public:
	FT_HANDLE h;
	std::vector<unsigned char> buf;
	unsigned int index;
	TDaq * daq;

	TUsbConn(){daq = 0;h = 0;index = 0; buf.resize(1024000);}
	TUsbConn(FT_HANDLE &ha){daq = 0; h = ha; index = 0; buf.resize(1024000);}

	~TUsbConn(){}	
};

class TUsbCtrl
{
public:
	TUsbCtrl();
	~TUsbCtrl();
	
	void SelectDeviceInteractive(TUsbConfig &cfg);
	void InitializeXML(std::istream &instr);
	
	void Initialize(TUsbConfig &cfg);

	int SendCommand(const std::string &serial, int writeSize, void *writeBuf, int readSize, void *readBuf, int readTimeout = -1);
	int SendCommandNoLock(const std::string &serial, int writeSize, void *writeBuf, int readSize, void *readBuf, int readTimeout = -1);

	void StartAcq();
	void StopAcq();

// 	void AddRawListener(int fd){_rawListeners.push_back(fd);}
// 	void RemoveRawListener(int fd){_rawListeners.erase(std::find(_rawListeners.begin(), _rawListeners.end(), fd));}
	void AddListener(TListener *ln){_listeners.push_back(ln);}
	void RemoveListener(TListener *ln){_listeners.erase(std::find(_listeners.begin(), _listeners.end(), ln));}

	void SetDaq(const std::string &serial, TDaq *daq){_ids[serial].daq = daq;}
	void DaqInitialize();
	void DaqConfigure();

	static void * MainLoop(void * thi); // function for thread startpoint
private:
	void MainLoop();

	TUsbConfig _cfg;
	std::map<std::string, TUsbConn> _ids;
	std::map<std::string, TUsbConn>::iterator _itids;
	
	// cond/mux for USB read-write
	EVENT_HANDLE _evh; // for ftd2xx library
/*
	typedef struct _EVENT_HANDLE{
		pthread_cond_t eCondVar;
		pthread_mutex_t eMutex;
		int iVar;
	} EVENT_HANDLE;
*/

	pthread_mutex_t _muxDev; // command/readout mutex
	pthread_mutex_t _muxRun; // start-stop mutex
	pthread_t _thread;
	std::ostream * _lout;

	std::vector<TListener *> _listeners;

	bool _init;
	bool _runacq;
	bool _debug; // set true to display any communication
	
};

#endif
