#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <errno.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <vector>
#include <string>

#include <bitset>

#include "LALUsb.h"

using namespace std;

const char *getTimeString()
{
  timeval tv;
  ::gettimeofday(&tv, 0);

  struct tm *tms = localtime(&tv.tv_sec);
  static char tmc[256];
  int n = strftime(tmc, 256, "%a, %d %b %Y %T %z", tms);
  sprintf(tmc + n, " +%06d usec", (int)tv.tv_usec);

  return tmc;
}

int grayDecode(bitset<16> gray){
    unsigned int decimal = 0;
    bitset<12> bin;

    bin[0]  = gray[11];
    bin[1]  = gray[10] ^ bin[0];
    bin[2]  = gray[9] ^ bin[1];
    bin[3]  = gray[8] ^ bin[2];
    bin[4]  = gray[7] ^ bin[3];
    bin[5]  = gray[6] ^ bin[4];
    bin[6]  = gray[5] ^ bin[5];
    bin[7]  = gray[4] ^ bin[6];
    bin[8]  = gray[3] ^ bin[7];
    bin[9]  = gray[2] ^ bin[8];
    bin[10] = gray[1] ^ bin[9];
    bin[11] = gray[0] ^ bin[10];

    // Converting binary to decimal
    for(int i = 0 ; i < 12 ; i++){
        decimal = decimal << 1 | bin[i];
    }

    return decimal;
}

int main(int argc,char **argv)
{
  bool zedboard = false;
  int zed_port = 4660;
  const char *zed_addr = "192.168.10.98";
  int acqtime = 200000; // acquisition time w/o zedboard; in usec

  bool verbose = false;
  
  int zed_fd = -1;
  int fdout = -1;

  unsigned short *buf = new unsigned short[32768]; // 64k buffer
  unsigned char *tmpbuf = new unsigned char[65536]; // 64k buffer

  try{
    USB_GetNumberOfDevs(); // needed to initialize internal # devs; otherwise open failed
    
    int id = OpenUsbDevice((char *)"SK2_05");
    if(id == -1)throw(string(GetErrMsg(USB_GetLastError())));

    cout << "USB open OK" << endl;
    
    // init
    if(!USB_Init(id,true))throw(string(GetErrMsg(USB_GetLastError())));

    // timeout setting
    USB_SetTimeouts(id, 200, 100);

    cout << "Initialization of USB device finished." << endl;

    // open ZedBoard connection
    if(zedboard){
      // open socket
      struct sockaddr_in dstAddr;
      memset(&dstAddr, 0, sizeof(dstAddr));
      dstAddr.sin_port = htons(zed_port);
      dstAddr.sin_family = AF_INET;
      dstAddr.sin_addr.s_addr = inet_addr(zed_addr);
      zed_fd = socket(AF_INET, SOCK_STREAM, 0);

      int ret = connect(zed_fd, (struct sockaddr *) &dstAddr, sizeof(dstAddr));
      if(ret != 0){
        std::cout << "ZedBoard socket connection failed. exit." << endl;
        return 1;
      }
      cout << "ZedBoard connected." << endl;
    }

    
    unsigned int acq = 0;
    
    int number; //data acquisition number
    if(argc > 2)
      number = atoi(argv[2]);
    else
      number = 1;
    while(number){
      int rd, wr;
      
      cout << "Starting acq " << acq << " at " << getTimeString() << endl;

      // reset asic
      UsbRd(id, 1, &rd, 1); 
      wr = rd & 0xfb; // bit 2
      UsbWrt(id, 1, &wr, 1);
      wr = rd; // bit 2 release
      UsbWrt(id, 1, &wr, 1);

      // start acq
      UsbRd(id, 2, &rd, 1); 
      wr = rd | 0x1; // bit 0
      UsbWrt(id, 2, &wr, 1);
      
      // wait stop signal
      unsigned int reply = 0;

      if(zedboard){
	do{
	  ::read(zed_fd, &reply, 4);
	  cout << "Message from ZedBoard: " << reply << endl;
	}while(reply != 0xced); // conversion end (cbe: conversion begin)
      }else{
	::usleep(acqtime);
      }

      cout << "Stop detected at " << getTimeString() << endl;

      // stop acq
      wr = rd; // bit 0 release
      UsbWrt(id, 2, &wr, 1);

      ::usleep(100000); // important to get correct bytes to read
      
      // start readout
      wr = rd | 0x4; // bit 2
      UsbWrt(id, 2, &wr, 1);
      wr = rd; // bit 2 release
      UsbWrt(id, 2, &wr, 1);

      cout << "Waiting readout..." << endl;
      do{
	UsbRd(id, 4, &rd, 1);
      }while((rd & 8) == 0);
      cout << "End_ReadOut1 detected at " << getTimeString() << endl;

      int nb1, nb2;
      UsbRd(id, 14, &nb1, 1); 
      UsbRd(id, 13, &nb2, 1); 
      
      int nb = ((nb2 & 15) << 8) + (nb1 & 255);
      cout << nb << " bytes to read." << endl;    
    
      int nbr = 0;
      if(nb > 0){
        nbr = UsbRd(id, 15, tmpbuf, nb); 
        cout << nbr << " bytes read." << endl;    
      }

      for(int i=0;i<nbr;i++){
	if(i%16==0)cout << endl;
	cout << std::hex << (int)tmpbuf[i] << " ";
      }
      cout << endl;
      
      acq ++;
    }
    
  }catch(string s){
    cout << "Error: " << s << endl;
  }

  if(zed_fd > 0)
    close(zed_fd);
    
  delete[] buf;
  delete[] tmpbuf;
    
  return 0;
}
