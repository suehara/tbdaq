// UsbCtrl.cc

#include "UsbCtrl.h"
#include <iostream>
#include <fstream>
#include <errno.h>

using namespace std;

TUsbCtrl::TUsbCtrl()
{
	pthread_mutex_init(&_muxDev, 0);
	pthread_mutex_init(&_muxRun, 0);
	pthread_mutex_init(&_evh.eMutex, 0);
	pthread_cond_init(&_evh.eCondVar, 0);

	_init = false;
	_runacq = false;
	
	_debug = true;
}

TUsbCtrl::~TUsbCtrl()
{
	pthread_mutex_destroy(&_muxDev);
	pthread_mutex_destroy(&_muxRun);
	pthread_mutex_destroy(&_evh.eMutex);
	pthread_cond_destroy(&_evh.eCondVar);
}

void TUsbCtrl::Initialize(TUsbConfig &cfg){
	_cfg = cfg;
	if(cfg.outLogFileName != "")
		_lout = new ofstream(cfg.outLogFileName.c_str());
	else
		_lout = &cout;

	ostream &lout = *_lout;

	FT_STATUS ret = 0;

	if(_cfg.serials.empty())throw(string("TUsbCtrl::Initialize: no serial no specified! cannot open any device."));

	// for any serial numbers
	for(unsigned int i=0;i<_cfg.serials.size();i++){
		FT_HANDLE h;
		if(FT_OpenEx((PVOID)_cfg.serials[i].c_str(), FT_OPEN_BY_SERIAL_NUMBER, &h) != FT_OK)
			throw(string("TUsbCtrl::Initialize: cannot open USB device ") + _cfg.serials[i]);

		// put to map
		//_ids[_cfg.serials[i]] = h;
		_ids[_cfg.serials[i]] = TUsbConn(h);

		// reset
		if(_cfg.doReset)ret |= FT_ResetDevice(h);
		// set data char
		ret |= FT_SetDataCharacteristics(h, _cfg.wordBits, _cfg.stopBits, _cfg.parity);
		// set baud rate
		ret |= FT_SetBaudRate(h, _cfg.baudRate);
		// set flow control
		ret |= FT_SetFlowControl(h, _cfg.flowControl, 0,0);
		// purge setting
		ret |= FT_Purge(h, _cfg.purgeMode);
		// timeout setting
		ret |= FT_SetTimeouts(h, _cfg.readTimeout, _cfg.writeTimeout);

		if(ret != FT_OK)throw(string("TUsbCtrl::Initialize: USB device configuration failed on ") + _cfg.serials[i]);

		lout << "TUsbCtrl::Initialize: initialization of device " << _cfg.serials[i] << " successful." << endl;
	}

	lout << "TUsbCtrl::Initialize: initialization of USB devices finished." << endl;
	lout << "TUsbCtrl::Initialize: initializing threads..." << endl;

	// locking run mutex : prevent run
	pthread_mutex_lock(&_muxRun);
	// start acq thread
	if(pthread_create(&_thread, 0, MainLoop, (void *)this) != 0)
		throw(string("TUsbCtrl::Initialize: start acquisition thread failed."));

	_init = true;
	lout << "TUsbCtrl::Initialize: all initialization done." << endl;
}

void TUsbCtrl::SelectDeviceInteractive(TUsbConfig &cfg)
{
	cerr << "Welcome to interactive selection of USB devices." << endl;
	
	DWORD ndevs;
	FT_CreateDeviceInfoList(&ndevs);

	if(ndevs == 0)throw(string("TUsbCtrl::SelectDeviceInteractive: no USB device found!"));

	FT_DEVICE_LIST_INFO_NODE *info = new FT_DEVICE_LIST_INFO_NODE[ndevs];
	FT_GetDeviceInfoList(info, &ndevs);
	cerr << ndevs << " USB devices found." << endl;

	for(unsigned int i=0;i<ndevs;i++){
		cerr << "Dev " << i << ": ID = " << info[i].ID << ", serial = " << info[i].SerialNumber << ", desc = " << info[i].Description << endl;
	}

	int nselect = 0;
	if(ndevs > 1){
		for(unsigned int i=0;i<ndevs;i++){
			cin.ignore(100, '\n');
			cerr << "Do you select device " << i << "? (y/n + enter) " << flush;
			char c;
			cin.get(c);
			if(c == 'y' || c == 'Y'){
				cfg.serials.push_back(string(info[i].SerialNumber));
				cerr << "Device " << i << " selected." << endl;
				nselect ++;
			}
		}
	}else{
		cerr << "The only device selected automatically." << endl;
		cfg.serials.push_back(string(info[0].SerialNumber));
		nselect ++;
	}
	

	if(nselect == 0)throw(string("TUsbCtrl::SelectDeviceInteractive: no USB device selected!"));

	cerr << nselect << " USB devices selected." << endl;

//	cerr << "Sorry, default values are used for all the settings of USB devices..." << endl;
//	Initialize(cfg);
}

void TUsbCtrl::InitializeXML(std::istream &instr)
{
	throw(string("TUsbCtrl::InitailizeXML: Sorry, not implemented yet."));
}

void TUsbCtrl::DaqInitialize()
{
	for(_itids = _ids.begin(); _itids != _ids.end(); _itids ++){
		if(!_itids->second.daq)throw("TUsbCtrl::DAQInitialize:: DAQ class not set.");
		_itids->second.daq->Initialize();
	}
}

void TUsbCtrl::DaqConfigure()
{
	for(_itids = _ids.begin(); _itids != _ids.end(); _itids ++){
		if(!_itids->second.daq)throw("TUsbCtrl::DAQInitialize:: DAQ class not set.");
		_itids->second.daq->Configure();
	}
}

// thread entry point: just start the true main loop
void *TUsbCtrl::MainLoop(void *thi)
{
	((TUsbCtrl *)thi)->MainLoop();

	return (void *)0;
}

// thread main
void TUsbCtrl::MainLoop()
{
	ostream &lout = *_lout;

	lout << "TUsbCtrl: main loop started." << endl;

	do{
		pthread_mutex_lock(&_muxRun);
		pthread_mutex_lock(&_muxDev);
		pthread_mutex_lock(&_evh.eMutex);

		lout << "TUsbCtrl: sending preaquisition to all DAQs..." << endl;
		// temp: reset/start command to all USBs
		for(_itids = _ids.begin(); _itids != _ids.end(); _itids ++){

			// 3 word for reset and 1 word for start
//			short command[4] = { 0xcc04, 0x0001, 0x0002, 0xe311};
//			short reply[3];
//			SendCommandNoLock(_itids->first, 8, command, 6, reply);
//			short command[3] = { 0xcc04, 0x0001, 0x0002};

			// single layer version
//			unsigned char command[6] = { 0xcc, 0x04, 0x00, 0x01, 0x00, 0x02};
//			unsigned char reply[6];
//			int nread = SendCommandNoLock(_itids->first, 6, command, 6, reply);

//			// to validate
//			lout << "reply for reset: " << std::dec << nread << " bytes: " << std::hex;
//			for(int i=0;i<6;i++)
//				lout << (unsigned int)reply[i] << " ";
//			lout << endl;

			// control by DAQ version
			if(!_itids->second.daq)throw("TUsbCtrl:: DAQ class not set.");
			_itids->second.daq->PreAcquisition();
		}
		
		lout << "TUsbCtrl: sending startacq to all DAQs..." << endl;
		for(_itids = _ids.begin(); _itids != _ids.end(); _itids ++){
			/*
			command[1] = 0x1d;
			command[4] = 0x14;
			command[5] = 0x8a;
			SendCommandNoLock(_itids->first, 6, command, 6, reply);

			lout << "reply for startdaq: " << std::dec << nread << " bytes: " << std::hex;
			for(int i=0;i<6;i++)
				lout << (unsigned int)reply[i] << " ";
			lout << endl;
			*/
			_itids->second.daq->Acquisition();
		}

		// wait fixed time: tempolarily
		::usleep(_cfg.dataTakingWait * 1000);

		for(_itids = _ids.begin(); _itids != _ids.end(); _itids ++){
			_itids->second.daq->StopAcquisition();
		}
		
		for(_itids = _ids.begin(); _itids != _ids.end(); _itids ++){
			_itids->second.index = 0;
			FT_SetEventNotification(_itids->second.h, FT_EVENT_RXCHAR, &_evh);

//			unsigned char command[6] = { 0xcc, 0x0e, 0x00, 0x01, 0x00, 0x03};
//			SendCommandNoLock(_itids->first, 6, command, 0, 0);

			_itids->second.daq->StartReadout();
		}
		
		// determining timeout time
		struct timeval tv;
		::gettimeofday(&tv,0);
		struct timespec ts;
		ts.tv_sec = tv.tv_sec + (_cfg.dataWaitTimeout / 1000);
		ts.tv_nsec = tv.tv_usec * 1000 + (_cfg.dataWaitTimeout % 1000) * 1000000;
		if(ts.tv_nsec > (int)1e+9){
			ts.tv_nsec -= (int)1e+9;
			ts.tv_sec ++;
		}

		for(_itids = _ids.begin(); _itids != _ids.end(); _itids ++){
			FT_SetTimeouts(_itids->second.h, 1, _cfg.writeTimeout);
		}

		while(pthread_cond_timedwait(&_evh.eCondVar, &_evh.eMutex, &ts) != ETIMEDOUT){
			for(_itids = _ids.begin(); _itids != _ids.end(); _itids ++){

				unsigned int nr = 0, nw = 0, nst = 0;
				FT_GetStatus(_itids->second.h, &nr, &nw, &nst);
				if(nr > 0){

					unsigned int bufSize = _itids->second.buf.size();
					unsigned int nr2 = 0;
					
					FT_Read(_itids->second.h, &_itids->second.buf[_itids->second.index], bufSize - _itids->second.index, &nr2 ); // timeout = 0
//					lout << "TUsbCtrl:: " << nr2 << " bytes received." << endl;
					_itids->second.index += nr2;
					if(_itids->second.index == bufSize)throw("TUsbCtrl: receive buffer full!");
				}
			}
		}

		for(_itids = _ids.begin(); _itids != _ids.end(); _itids ++){
			FT_SetTimeouts(_itids->second.h, _cfg.readTimeout, _cfg.writeTimeout);
		}

// readout finished: send data
		for(_itids = _ids.begin(); _itids != _ids.end(); _itids ++){
			lout << "TUsbCtrl: serial " << _itids->first << ", " << std::dec << _itids->second.index << " bytes received." << endl;

			if(_listeners.size() == 0){
				for(unsigned int i=0;i<_itids->second.index;i++){
					lout << std::hex << (unsigned int)_itids->second.buf[i] << " ";
					if(i%20 == 19)lout << endl;
				}
				lout << endl;
			}

			else{
				for(unsigned int i=0;i<_listeners.size();i++){
					bool b = _listeners[i]->Process(_itids->second.buf,_itids->second.index);
					lout << "Listener " << i << " status " << b << endl;
//					int n = write(_rawListeners[i], &_itids->second.buf.front(), _itids->second.index);
//					lout << "TUsbCtrl: write fd " << _rawListeners[i] << " " << n << " bytes." << endl;
				}
			}
		}

		// clear event notification
		for(_itids = _ids.begin(); _itids != _ids.end(); _itids ++){
			FT_SetEventNotification(_itids->second.h, 0, &_evh);
		}

		pthread_mutex_unlock(&_evh.eMutex);
		pthread_mutex_unlock(&_muxDev);
		pthread_mutex_unlock(&_muxRun);
	}while(1);
}

int TUsbCtrl::SendCommand(const string &serial, int writeSize, void *writeBuf, int readSize, void *readBuf, int readTimeout){
	pthread_mutex_lock(&_muxDev);
	int ret = SendCommandNoLock(serial, writeSize, writeBuf, readSize, readBuf, readTimeout);
	pthread_mutex_unlock(&_muxDev);
	return ret;
}

int TUsbCtrl::SendCommandNoLock(const string &serial, int writeSize, void *writeBuf, int readSize, void *readBuf, int readTimeout){
	FT_HANDLE h = _ids[serial].h;
	FT_STATUS ret = 0;
	ostream &lout = *_lout;

	if(h == 0){
		lout << "TUsbCtrl::SendCommand: Serial " << serial << " invalid, command ignored." << endl;
		return 0;
	}


	if(_debug){
		lout << "TUsbCtrl::SendCommand: Sending ";
			for(int n=0;n<writeSize;n++)
				lout << std::hex << (int)((unsigned char *)writeBuf)[n] << " ";
			lout << endl;
	}

	// write
	unsigned int nwrite = 0;
	if(writeSize > 0){
		ret = FT_Write(h, writeBuf, writeSize, &nwrite);
		if(ret || nwrite < (unsigned int)writeSize)throw(string("TUsbCtrl::SendCommand: FT_Write failed."));
	}
	
	if(readSize == 0)return 0;

	if(readTimeout >= 0){
		FT_SetTimeouts(h, readTimeout, _cfg.writeTimeout);
	}

	// read
	unsigned int nread = 0;
	ret = FT_Read(h, readBuf, readSize, &nread);
	if(ret)throw(string("TUsbCtrl::SendCommand: FT_Read failed."));

	if(_debug){
		lout << "TUsbCtrl::SendCommand: Received ";
			for(unsigned int n=0;n<nread;n++)
				lout << std::hex << (int)((unsigned char *)readBuf)[n] << " ";
			lout << endl;
	}

	if(nread < (unsigned int)readSize)
		lout << "TUsbCtrl::SendCommand: read size " << nread << " is shorter than buffer size " << readSize << endl;

	if(readTimeout >= 0){
		FT_SetTimeouts(h, _cfg.readTimeout, _cfg.writeTimeout);
	}

	return nread;
}

void TUsbCtrl::StartAcq()
{
	if(_init && !_runacq)
		pthread_mutex_unlock(&_muxRun);
}

void TUsbCtrl::StopAcq()
{
	if(_init && _runacq)
		pthread_mutex_lock(&_muxRun);
}

