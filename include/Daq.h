// Daq.h

#ifndef DAQ_H
#define DAQ_H

#include <string>

class TUsbCtrl;

class TDaq{
public:
	TDaq(){}
	virtual ~TDaq(){}

// handlers to be called from connection control (TUsbCtrl etc.)
// default is to do nothing

	// configuration before starting acquisition
	virtual void Initialize();
	virtual void Configure();

	// acquisition handlers
	virtual void PreAcquisition();
	virtual void Acquisition();
	virtual void StopAcquisition();
	virtual void StartReadout();
};

class TScCalDaqConfig{
public:
	enum VERSION {VER_USBSINGLE = 1, VER_USBMULTI = 2};

	std::string usbSerial;
	std::string scFile[4];
	int daqVersion;
	
	int triggerDelay;
	int nTriggers;
};

class TScCalDaq : public TDaq {
public:
	TScCalDaq(TUsbCtrl *usb, TScCalDaqConfig &cfg) : _con(usb), _cfg(cfg){}
	virtual ~TScCalDaq(){}

	virtual void Configure();

	virtual void PreAcquisition(); // reset
	virtual void Acquisition(); // start_acq
	virtual void StopAcquisition();
	virtual void StartReadout();

private:
	TUsbCtrl *_con;
	TScCalDaqConfig _cfg;

};
#endif