// Daq.cc

#include "Daq.h"

#include <iostream>
#include <fstream>
#include <string.h>
#include "UsbCtrl.h"

using namespace std;

// TDaq skeltons
void TDaq::Initialize(){}
void TDaq::Configure(){}
void TDaq::PreAcquisition(){}
void TDaq::Acquisition(){}
void TDaq::StopAcquisition(){}
void TDaq::StartReadout(){}

// Sending configuration commands to USB
void TScCalDaq::Configure()
{
	const int n_commands_before_config = 4;
	unsigned char commands_before_config[n_commands_before_config][6] = {
		{0xcc, 0x02, 0x00, 0x01, 0x00, 0x02}, // power on
		{0xcc, 0x12, 0x00, 0x01, 0x00, 0x02}, // V3.3 on
		{0xcc, 0x12, 0x00, 0x01, 0x00, 0x04}, // VDDA/VDDD on
		{0xcc, 0x03, 0x00, 0x01, 0x01, 0x01}, // # ASICs, 0/4/0
	};
	
	unsigned char command_scload[] = {
		0xcc, 0x0a, 0x00, 0x3b, 0x00, 0x46 // for asic0: decrease by 2 to each channel
	};
	
	const int n_commands_after_config = 2;
	unsigned char commands_after_config[n_commands_after_config][6] = {
		{0xcc, 0x04, 0x00, 0x01, 0x00, 0x08}, // SC reset
		{0xcc, 0x0c, 0x00, 0x01, 0x00, 0x40}, // SC write
	};
	
	unsigned char command_enable[] = {
		0xcc, 0x72, 0x00, 0x02, 0x31, 0x00, 0x00, 0x00
	};
	const int n_enables = 13;
	bool enables[n_enables] = {
		true,  // write trig_ext synchr.(1) ON
		false, // write trig_ext async.(2) OFF
		false, // write hold_ext(3) OFF
		false, // write tcalib2(4) OFF
		false, // write tcalib1(5) OFF
		false, // write tcalib_ext(6) OFF
		true, // write pwr_led(7) ON
		true, // write pwr_charge(8) ON
		true, // write slab_power ON
		true, // write sipm_bias 2 ON
		false, // write pre_bias 2 OFF
		true, // write tcalib_direct(9) ON
		false, // write tcalib_hold(10) OFF
	};

	cout << "Configuring " << _cfg.usbSerial << endl;

	int timeout = 5000;
	unsigned char rcvbuf[9];
	
	//int SendCommand(const std::string &serial, int writeSize, void *writeBuf, int readSize, void *readBuf, int readTimeout = -1);

	for(int i=0;i < n_commands_before_config; i++){
		int comsize = 6;
		int nrcv = _con->SendCommand(_cfg.usbSerial, comsize, commands_before_config[i], comsize, rcvbuf, timeout);
		if(nrcv != comsize || memcmp(commands_before_config[i], rcvbuf, comsize))
			throw(string("TScCalDaq::Configure: Reply from DIF invalid! DIF or connection may be corrupted."));
	}

	cout << "Sending SC data." << endl;
	for(int i=0;i<4;i++){
		ifstream scfile;
		scfile.open(_cfg.scFile[i].c_str());
		if(!scfile.is_open())throw(string("TScCalDaq::Configure: SC file cannot be open!"));
		
		vector<unsigned char> v;
		for(int j=0;j<6;j++){
			v.push_back(command_scload[j]);
		}
		v[5] -= i * 2; // set channel

		// skip header
		string shead;
		while(scfile.peek() !='\r' && scfile.peek() != '\n'){
			char ch;
			scfile.get(ch);
			shead.push_back(ch);
		}
		cout << "SC header: " << shead << endl;

		while(!scfile.eof())
		{
			int ch;
			scfile >> ch;
			if(scfile.eof())break;
			if(ch<0 || ch>=256)throw(string("TScCalDaq::Configure: SC file invalid! Number out of range."));

			v.push_back((unsigned char)ch);
		}
		if(v.size() != 117 + 6){throw(string("TScCalDaq::Configure: SC file invalid! Number of lines invalid."));}
		
		int nrcv = _con->SendCommand(_cfg.usbSerial, v.size(), &v.front(), 6, rcvbuf, timeout);
		if(nrcv != 6 || memcmp(&v.front(), rcvbuf, 6))
			throw(string("TScCalDaq::Configure: Reply from DIF invalid! DIF or connection may be corrupted."));
	}
	
	cout << "SC data sent." << endl;

	for(int i=0;i < n_commands_after_config; i++){
		int comsize = 6;
		int nrcv = _con->SendCommand(_cfg.usbSerial, comsize, commands_after_config[i], comsize, rcvbuf, timeout);
		if(nrcv != comsize || memcmp(commands_after_config[i], rcvbuf, comsize))
			throw(string("TScCalDaq::Configure: Reply from DIF invalid! DIF or connection may be corrupted."));
	}
	
	cout << "Setting enables." << endl;
	for(int i=0;i < n_enables; i++){
		int comsize = 8;
		int rcvsize = 9;
		command_enable[4] = 0x31 + i;
		command_enable[7] = enables[i];
		
		int nrcv = _con->SendCommand(_cfg.usbSerial, comsize, command_enable, rcvsize, rcvbuf, timeout);
		if(nrcv != rcvsize || memcmp(command_enable, rcvbuf+3, 6) || rcvbuf[0] != (int)enables[i])
			throw(string("TScCalDaq::Configure: Reply from DIF invalid! DIF or connection may be corrupted."));
	}
	cout << "All configuration of " << _cfg.usbSerial << " done." << endl;
}

void TScCalDaq::PreAcquisition()
{
	int comsize = 6;
	unsigned char rcvbuf[6];
	unsigned char cmd_reset[6] = {0xcc, 0x04, 0x00, 0x01, 0x00, 0x02};

	int nrcv = _con->SendCommandNoLock(_cfg.usbSerial, comsize, cmd_reset, comsize, rcvbuf, 500);
	if(nrcv != comsize || memcmp(cmd_reset, rcvbuf, comsize))
		throw(string("TScCalDaq::PreAcquisition: Reply from DIF invalid! DIF or connection may be corrupted."));
}

void TScCalDaq::Acquisition()
{
	if(_cfg.daqVersion == TScCalDaqConfig::VER_USBSINGLE){
		int comsize = 6;
		unsigned char rcvbuf[6];
		unsigned char cmd_acq[6] = {0xcc, 0x1d, 0x00, 0x01, 0x00, 0x80};

		cmd_acq[4] += _cfg.triggerDelay;
		cmd_acq[5] += _cfg.nTriggers;

		int nrcv = _con->SendCommandNoLock(_cfg.usbSerial, comsize, cmd_acq, comsize, rcvbuf, 500);
		if(nrcv != comsize || memcmp(cmd_acq, rcvbuf, comsize))
			throw(string("TScCalDaq::Acquisition: Reply from DIF invalid! DIF or connection may be corrupted."));
	}
	else if(_cfg.daqVersion == TScCalDaqConfig::VER_USBMULTI){
		int comsize = 2;
		unsigned char rcvbuf[2];
		unsigned char cmd_acq[2] = {0xe3, 0x11};

		int nrcv = _con->SendCommandNoLock(_cfg.usbSerial, comsize, cmd_acq, comsize, rcvbuf, 500);
		if(nrcv != comsize || memcmp(cmd_acq, rcvbuf, comsize))
			throw(string("TScCalDaq::Acquisition: Reply from DIF invalid! DIF or connection may be corrupted."));
	}
	else
		throw(string("TScCalDaq::Acquisition: DAQ version invalid!"));
}


void TScCalDaq::StopAcquisition()
{
	if(_cfg.daqVersion == TScCalDaqConfig::VER_USBSINGLE){
		// no-op.
	}
	else if(_cfg.daqVersion == TScCalDaqConfig::VER_USBMULTI){
		int comsize = 2;
		unsigned char rcvbuf[2];
		unsigned char cmd_acq[2] = {0xe3, 0x13};

		int nrcv = _con->SendCommandNoLock(_cfg.usbSerial, comsize, cmd_acq, comsize, rcvbuf, 500);
		if(nrcv != comsize || memcmp(cmd_acq, rcvbuf, comsize))
			throw(string("TScCalDaq::StopAcquisition: Reply from DIF invalid! DIF or connection may be corrupted."));
	}
	else
		throw(string("TScCalDaq::StopAcquisition: DAQ version invalid!"));
}

void TScCalDaq::StartReadout()
{
	unsigned char cmd_startReadout[6] = {0xcc, 0x0e, 0x00, 0x01, 0x00, 0x03};
	_con->SendCommandNoLock(_cfg.usbSerial, 6, cmd_startReadout, 0, 0, 0); // no recv check
}
