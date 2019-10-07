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
  int acqtime = 20000; // acquisition time w/o zedboard; in usec

  bool verbose = false;
  
  int fdout = -1;

  const int readsize = 3848;
  const int datasize = 1924 - 4;
  unsigned short *buf = new unsigned short[32768]; // 64k buffer
  unsigned char *tmpbuf = new unsigned char[readsize];

  const char *filename; // .cc file + filename
  if(argc > 1)
    filename = argv[1];
  else
    filename = "data_test_cms.raw";

  cout << "Filename: " << filename << endl;
  
  try{
    USB_GetNumberOfDevs(); // needed to initialize internal # devs; otherwise open failed
    
    int id = OpenUsbDevice((char *)"SK2_06");
    if(id == -1)throw(string(GetErrMsg(USB_GetLastError())));

    cout << "USB open OK" << endl;
    
    // init
    if(!USB_Init(id,true))throw(string(GetErrMsg(USB_GetLastError())));

    // timeout setting
    USB_SetTimeouts(id, 200, 100);

    cout << "Initialization of USB device finished." << endl;

    fdout = open(filename,O_CREAT | O_WRONLY | O_TRUNC,0666);
    if(fdout < 0){
      std::cout << filename << " output open failed. exit" << endl;
      return 1;
    }
    
    unsigned int acq = 0;
    
    int number; //data acquisition number
    if(argc > 2)
      number = atoi(argv[2]);
    else
      number = 0;

    while(number == 0 || acq < number){
      int rd, wr;
      
      cout << "Starting automatic acq " << acq << " at " << getTimeString() << endl;

      // automatic DAQ start
      UsbRd(id, 23, &rd, 1); 
      wr = rd | 0x80; // bit 2
      UsbWrt(id, 23, &wr, 1);

      ::usleep(acqtime);

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

      if(nb > 3848){
	cout << "Read size too big! Truncated to 3848." << endl;
	nb = 3848;
      }
      if(nb < 3848){
	cout << "Read size too small." << endl;
      }
    
      int nbr = 0;
      if(nb > 0){
        nbr = UsbRd(id, 15, tmpbuf, nb); 
        cout << nbr << " bytes read." << endl;    
      }
      if(nbr != nb){
	cout << "Readout size differs from specified data size." << endl;
      }

      // automatic DAQ stop
      UsbRd(id, 23, &rd, 1); 
      wr = rd & 0x7f; // bit 2
      UsbWrt(id, 23, &wr, 1);

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

      //cout << "header : " << n << endl;

      for(int i=0;i<datasize;i++){
	bitset<8> firstByte(tmpbuf[2*i+1]);
	bitset<8> secondByte(tmpbuf[2*i]);
	bitset<16> word(secondByte.to_string() + firstByte.to_string());
	int dec = grayDecode(word);
	dec |= int(word[12]) << 12;
	//dec |= int(word[13]) << 13;
	
	buf[n++] = dec;
	if(verbose){
	  if(i%16==0){cout << endl << std::dec << i << " ";}
	  cout << std::hex << dec << " ";
	}
      }
      if(verbose)
	cout << std::dec << endl;

      for(int i=datasize;i<readsize/2;i++){
	buf[n++] = (int(tmpbuf[2*i]) << 8) + tmpbuf[2*i+1];
      }
	
      //cout << "trailer :  " << n <<endl; 

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
    
  delete[] buf;
  delete[] tmpbuf;
    
  return 0;
}
