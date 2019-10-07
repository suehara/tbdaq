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

  const char *filename;
  if(argc > 1)
    filename = argv[1];
  else
    filename = "data_test_20190212_cms.raw";

  cout << "Filename: " << filename << endl;
  
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

    fdout = open(filename,O_CREAT | O_WRONLY | O_TRUNC,0666);
    if(fdout < 0){
      std::cout << filename << " output open failed. exit" << endl;
      return 1;
    }
    
    unsigned int acq = 0;
    
    while(1){
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

      // write DIF header
      // SPIL header
      int n = 0;
      buf[n++] = 0xfffc;
      buf[n++] = (acq >> 16) % 65536;
      buf[n++] = acq % 65536;
      buf[n++] = 0x5053; // SP
      buf[n++] = 0x4c49; // IL
      buf[n++] = 0x2020; // space
      // CHIP header
      buf[n++] = 0xfffd;
      buf[n++] = 0xff01;
      buf[n++] = 0x4843; // CH
      buf[n++] = 0x5049; // IP
      buf[n++] = 0x2020; // space

      if(nbr > 0){
	// adjust number of bytes
	double dnbx = double(nbr-2) / 258.0;
	int nbx = int(dnbx + 0.5);
	int dnbr = (nbr/2-1) - (nbx * 129);
	
	cout << "nbx = " << dnbx << ", assumed = " << nbx << endl;
	if(dnbr < 0){
	  cout << "Dummy " << -dnbr << "word added." << endl;
	  for(;dnbr < 0; dnbr++)
	    buf[n++] = 0;
	}
	else if(dnbr > 0){
	  cout << dnbr << " words striped." << endl; 
	}

        for(int i=dnbr*2;i<nbr-2;i+=2){

	  bitset<8> firstByte(tmpbuf[i+1]);
	  bitset<8> secondByte(tmpbuf[i]);
	  bitset<16> word(secondByte.to_string() + firstByte.to_string());
	  int dec = grayDecode(word);
	  dec |= int(word[12]) << 12;
	  dec |= int(word[13]) << 13;

          buf[n++] = dec;
	  if(verbose){
	    if(i%16==0)cout << endl;
	    cout << std::hex << dec << " ";
	  }
        }
	if(verbose)
	  cout << std::dec << endl;

	// one dummy
	int chipid = tmpbuf[nbr-2];
	buf[n++] = chipid;
	buf[n++] = chipid;
      }

      // CHIP trailer
      buf[n++] = 0xfffe;
      buf[n++] = 0xff01;
      buf[n++] = 0x2020;
      buf[n++] = 0x2020;
      // SPIL trailer
      buf[n++] = 0xffff;
      buf[n++] = (acq >> 16) % 65536;
      buf[n++] = acq % 65536;
      buf[n++] = 0x0001;
      buf[n++] = (acq >> 16) % 65536;
      buf[n++] = acq % 65536;
      buf[n++] = 0x2020;

      write(fdout, buf, n*2);
      cout << n*2 << " bytes sent at " << getTimeString() << endl;
      
      acq ++;
    }
    
  }catch(string s){
    cout << "Error: " << s << endl;
  }

  if(fdout > 0)
    close(fdout);
  if(zed_fd > 0)
    close(zed_fd);
    
  delete[] buf;
  delete[] tmpbuf;
    
  return 0;
}
