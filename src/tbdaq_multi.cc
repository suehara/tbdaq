// tbdaq_multi.cc
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <errno.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <sstream>
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
  bool verbose = false;
  
  unsigned short *buf = new unsigned short[32768]; // 64k buffer
  unsigned char *tmpbuf = new unsigned char[65536]; // 64k buffer

  if(argc == 0)
    cout << "tbdaq_multi cfgfile outputfile(_%d.raw is automatically added) nevents" << endl;
  
  // configuration
  const int config_max = 8;
  string serial[config_max];
  bool isCMS[config_max];
  bool isAuto[config_max];
  int id[config_max];
  string filenames[config_max];
  int fdout[config_max];
  memset(fdout, 0, sizeof(fdout));

  int nboards = 0;
  
  string cfgfile;
  if(argc > 2){
    cfgfile = argv[1];
  }else{
    cfgfile = "tbdaq_multi.cfg";
  }
  
  cout << "CFG filename: " << cfgfile << endl;
  
  
  ifstream fin(cfgfile.c_str());
  string isCMSTmp, isAutoTmp;
  if(!fin.fail()){
    while(!fin.eof()){
      fin >> serial[nboards] >> isCMSTmp >> isAutoTmp;
      if(fin.eof())break;
      cout << "Board #" << nboards << ", Serial = " << serial[nboards] << ", " << isCMSTmp << ", " << isAutoTmp << endl;
      
      isCMS[nboards] = (isCMSTmp == "SKIROC2CMS");
      isAuto[nboards] = (isAutoTmp == "auto");
      nboards ++;
    }
  }
  if(nboards == 0){
    cout << "Configuration failed: falling back to default configuration" << endl;
    cout << "Board #0 SK2_06 SKIROC2CMS auto" << endl;
    serial[0] = "SK2_06";
    isCMS[0] = true;
    isAuto[0] = true;
    nboards = 1;
  }

  const char *filename;
  if(argc > 2)
    filename = argv[2];
  else
    filename = "data/data";

  cout << "Output filenames: ";
  for(int i=0;i<nboards;i++){
    stringstream ss;
    ss << filename << "_" << i << ".raw";
    filenames[i] = ss.str();
    
    cout << filenames[i] << " ";
  }
  cout << endl;


  int nevents = -1;
  if(argc > 3)
    nevents = atoi(argv[3]);
  
  cout << "Events to run: " << nevents << endl;
  
  try{
    USB_GetNumberOfDevs(); // needed to initialize internal # devs; otherwise open failed

    // initialization
    for(int i=0;i<nboards;i++){
    
      id[i] = OpenUsbDevice(const_cast<char *>(serial[i].c_str()));
      if(id[i] == -1)throw(string(GetErrMsg(USB_GetLastError())));

      // init
      if(!USB_Init(id[i],true))throw(string(GetErrMsg(USB_GetLastError())));

      // timeout setting
      USB_SetTimeouts(id[i], 200, 100);

      cout << "Initialization of USB device finished. Serial = " << serial[i] << ", ID = " << id[i] << endl;
    }
    
    for(int i=0;i<nboards;i++){
      fdout[i] = open(filenames[i].c_str(),O_CREAT | O_WRONLY | O_TRUNC,0666);
      if(fdout[i] < 0){
        std::cout << filenames[i] << " open failed. exit." << endl;
        return 1;
      }
    }
    cout << "All output files have been open." << endl;
    
    unsigned int acq = 0;
    
    while(nevents < 0 || acq < nevents){
      int rd, wr;
      
      cout << "Starting acq " << acq << " at " << getTimeString() << endl;

      // initialize acq
      for(int i=0;i<nboards;i++){
        if(!isAuto[i]){
          UsbRd(id[i], 23, &rd, 1); 
          wr = rd | 0x80; // bit 2
          UsbWrt(id[i], 23, &wr, 1);
        }
        else{
          // auto -> manual
          UsbRd(id[i], 2, &rd, 1); 
          wr = rd & 0xef; // bit 4
          UsbWrt(id[i], 2, &wr, 1);

	  // check readout flag
          //UsbRd(id[i], 4, &rd, 1);
	  //	  if(rd & 8 > 0){
	    // reset asic
	    UsbRd(id[i], 1, &rd, 1); 
	    wr = rd & 0xfb; // bit 2
	    UsbWrt(id[i], 1, &wr, 1);
	    wr = rd; // bit 2 release
	    UsbWrt(id[i], 1, &wr, 1);
	    //}

          // manual -> auto
          UsbRd(id[i], 2, &rd, 1); 
          wr = rd | 0x10; // bit 4
          UsbWrt(id[i], 2, &wr, 1);
        }

      }
      
//      ::usleep(200000);
      
      cout << "Waiting data to be ready..." << endl;
      
      // timeout val
      int timeout_usec1 = 3000000; // 3 sec for recovery
      int timeout_usec2 = 1700000; // 1.7 sec for throwing data away

      struct timeval tv, tv2;
      ::gettimeofday(&tv, 0);
      bool timeout = false;

      for(int i=0;i<nboards;i++){
        do{
          UsbRd(id[i], 4, &rd, 1);
	  ::gettimeofday(&tv2, 0);
	  if((tv2.tv_sec - tv.tv_sec)*1000000 + (tv2.tv_usec - tv.tv_usec)> timeout_usec1){
	    timeout = true;
	    break;
	  }    
        }while((rd & 8) == 0);
	if(timeout){
	  cout << "Timeout occurred: skip this readout." << endl;
	  break;
	}
        cout << "End_ReadOut1 for ID " << i << " detected at " << getTimeString() << endl;
      }

      bool timeout2 = ((tv2.tv_sec - tv.tv_sec)*1000000 + (tv2.tv_usec - tv.tv_usec) > timeout_usec2);

      if(timeout == true){
	cout << "Timeout detected. Trying recovery..." << endl;

	for(int i=0;i<nboards;i++){
	  if(!isAuto[i])continue;

	  // auto -> manual
	  UsbRd(id[i], 2, &rd, 1); 
	  wr = rd & 0xef; // bit 4
	  UsbWrt(id[i], 2, &wr, 1);

	  // reset asic
	  UsbRd(id[i], 1, &rd, 1); 
	  wr = rd & 0xfb; // bit 2
	  UsbWrt(id[i], 1, &wr, 1);
	  wr = rd; // bit 2 release
	  UsbWrt(id[i], 1, &wr, 1);

	  // start acq
	  UsbRd(id[i], 2, &rd, 1);
	  wr = rd | 0x1; // bit 0
	  UsbWrt(id[i], 2, &wr, 1);

	  // wait 200 msec
	  ::usleep(200000);
	  
	  // stop acq
	  wr = rd; // bit 0 release
	  UsbWrt(id[i], 2, &wr, 1);

	  ::usleep(100000); // important to get correct bytes to read

	  // start readout
	  wr = rd | 0x4; // bit 2
	  UsbWrt(id[i], 2, &wr, 1);
	  wr = rd; // bit 2 release
	  UsbWrt(id[i], 2, &wr, 1);

	  cout << "Waiting readout..." << endl;
	  do{
	    UsbRd(id[i], 4, &rd, 1);
	  }while((rd & 8) == 0);
	  cout << "End_ReadOut1 detected at " << getTimeString() << endl;
	  
	  cout << "Readout board " << i << endl;
      
	  int nb1, nb2;
	  UsbRd(id[i], 14, &nb1, 1); 
	  UsbRd(id[i], 13, &nb2, 1); 
      
	  int nb = ((nb2 & 15) << 8) + (nb1 & 255);
	  cout << nb << " bytes to read." << endl;    
    
	  int nbr = 0;
	  if(nb > 0){
	    nbr = UsbRd(id[i], 15, tmpbuf, nb); 
	    cout << nbr << " bytes read." << endl;    
	  }
	}
	
	continue;
      }

      cout << "Starting readout..." << endl;
      for(int i=0;i<nboards;i++){
        cout << "Readout board " << i << endl;
      
        int nb1, nb2;
        UsbRd(id[i], 14, &nb1, 1); 
        UsbRd(id[i], 13, &nb2, 1); 
      
        int nb = ((nb2 & 15) << 8) + (nb1 & 255);
        cout << nb << " bytes to read." << endl;    
    
        int nbr = 0;
        if(nb > 0){
          nbr = UsbRd(id[i], 15, tmpbuf, nb); 
          cout << nbr << " bytes read." << endl;    
        }

        // manual: DAQ stop
        if(!isAuto[i]){
          UsbRd(id[i], 23, &rd, 1); 
          wr = rd & 0x7f; // bit 2
          UsbWrt(id[i], 23, &wr, 1);
        }
        
	if(timeout2){
	  cout << "Acquisition time exceeds acceptable range: throw the data away." << endl;
	  continue;
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

        // readout
        if(isCMS[i]){
          const int readsize = 3848;
          const int datasize = 1924 - 4;

          if(nb != readsize)
            cout << "Data size invalid! Should be 3848, reported " << nb << endl;
          if(nbr != readsize)
            cout << "Readout size invalid! Should be 3848, reported " << nbr << endl;
          
          for(int j=0;j<datasize;j++){
            bitset<8> firstByte(tmpbuf[2*j+1]);
            bitset<8> secondByte(tmpbuf[2*j]);
            bitset<16> word(secondByte.to_string() + firstByte.to_string());
            int dec = grayDecode(word);
            dec |= int(word[12]) << 12;
  
            buf[n++] = dec;
            if(verbose){
              if(j%16==0){cout << endl << std::dec << j << " ";}
              cout << std::hex << dec << " ";
            }
          }
          if(verbose)
            cout << std::dec << endl;

          for(int j=datasize;j<readsize/2;j++){
            buf[n++] = (int(tmpbuf[2*j]) << 8) + tmpbuf[2*j+1];
          }
          
        }else{ // SKIROC2/A
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

            for(int j=dnbr*2;j<nbr-2;j+=2){

              bitset<8> firstByte(tmpbuf[j+1]);
              bitset<8> secondByte(tmpbuf[j]);
              bitset<16> word(secondByte.to_string() + firstByte.to_string());
              int dec = grayDecode(word);
              dec |= int(word[12]) << 12;
              dec |= int(word[13]) << 13;

              buf[n++] = dec;
              if(verbose){
                if(j%16==0)cout << endl;
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

        write(fdout[i], buf, n*2);
        cout << n*2 << " bytes sent at " << getTimeString() << endl;
      }
      
      acq ++;
    }
    
  }catch(string s){
    cout << "Error: " << s << endl;
  }

  for(int i=0;i<nboards;i++){
    if(fdout[i] > 0)
      close(fdout[i]);
  }
  
  delete[] buf;
  delete[] tmpbuf;
    
  return 0;
}
