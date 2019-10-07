#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <errno.h>
#include <unistd.h>

#include <sstream>
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




void writeNumber(int subadd,int size,int value,unsigned char sc[]){

  int bit,bin;

  bit = 7-((615-(subadd+size-1))%8);
  bin = (615-(subadd+size-1))/8;

  for(int i=0;i<size;i++){
    if(value & (0x01 <<i)){ 
      sc[bin] |= (0x01 <<bit);
    }else{
      sc[bin] &= ~(0x01 <<bit);
    }
    bit--;
    if(bit<0){
      cout <<  "sc[" << bin << "]= " << (int)sc[bin] <<endl;
      bin++;
      bit = 7;
    }
  }
  cout <<  "sc[" << bin << "]= " << (int)sc[bin] <<endl;
};



int Acquisition(int id,unsigned int event, const char *filename){

  int acqtime = 200000; // acquisition time w/o zedboard; in usec
  int fdout = -1;
  unsigned short *buf = new unsigned short[32768]; // 64k buffer
  unsigned char *tmpbuf = new unsigned char[65536]; // 64k buffer
  
  cout << "Filename: " << filename << endl;
  
  fdout = open(filename,O_CREAT | O_WRONLY | O_TRUNC,0666);
  if(fdout < 0){
    std::cout << filename << " output open failed. exit" << endl;
    return 1;
  }
  
  unsigned int acq = 0;
  bool verbose = false ;
  
  for(acq=0; acq<event ;acq++){
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
    ::usleep(acqtime);
    
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
  
  
  if(fdout > 0)
    close(fdout);
  
  delete[] buf;
  delete[] tmpbuf;
  
  return 0;
  
}




int writeSlowControl(int id, unsigned char sc[], bool probe){

  int scsize;
  int rd,wr;
  
  // Slow Control select
  UsbRd(id, 1, &rd, 1);// word1
  if(probe){
    scsize = 193;
    wr = rd & ~0x20; // bit 5
  }else{
    scsize = 77;
    wr = rd | 0x20; // bit 5
  }
  UsbWrt(id, 1, &wr, 1);
  
  unsigned char *sc2 = new unsigned char[scsize];
  unsigned char k;
  
  for(int i=0; i<scsize; i++){ 
    sc2[i] = 0x00;
    
    for(int j=0; j<4;j++){
      k = sc[probe ? scsize-i-1 : i] & (0x01 << j);
      sc2[i] += k << (7-2*j);
    }
    for(int j=0; j<4;j++){
      k = sc[probe ? scsize-i-1 : i] & (0x10 << j);
      sc2[i] += k >> (1+2*j);
    }
  }
  
  for(int i=0;i<scsize;i++){
    printf("%x\n",sc2[i]);
  }
  
  UsbWrt(id, 10, sc2, scsize);
  
  
  //Start Cycle
  UsbRd(id, 1, &rd, 1); //word1
  wr = rd | 0x02; //bit1
  UsbWrt(id, 1, &wr, 1);
  
  //Wait 1ms
  usleep(1000);
  
  //Stop Cycle
  UsbWrt(id, 1, &rd, 1);
  
  //SR register correlation test query
  UsbRd(id, 0, &rd, 1); //word0
  wr = rd | 0x80; //bit7
  UsbWrt(id, 0, &wr, 1);
  
  //Slow control register
  UsbWrt(id, 10, sc2, scsize);
  
  //Start Cycle
  UsbRd(id, 1, &rd, 1); //word1
  wr = rd | 0x02; //bit1
  UsbWrt(id, 1, &wr, 1);
  
  //wait 10ms
  usleep(10000);
  
  //Stop Cycle
  UsbWrt(id, 1, &rd, 1);
  
  //SR register test result
  UsbRd(id, 4, &rd, 1); //word4
  int ret = rd & 0x80; 
  
  //SR resister correlation test query end
  UsbRd(id, 0, &rd, 1); //word0
  wr = rd & 0x7F; //bit7 reverse
  UsbWrt(id, 0, &wr, 1);
  
  delete[] sc2;
  return ret;
  
};



void delayScan(int id,int start,int stop,int step,int event,unsigned char *sc, const char *filenamebase){
  
  //  int num = (stop-start)/step;
  
  cout<<"delay scan start" << endl;
  for(int delay=start; delay<= stop; delay+=step){
    cout << delay << "delayed" << endl;
    writeNumber(545,8,delay,sc);
    writeSlowControl(id,sc,false);
    
    ostringstream oss;
    oss << filenamebase << "_" << delay << ".raw" ;
    Acquisition(id,event,oss.str().c_str());
    
    cout << oss.str().c_str() << "is created."<< endl;
  }
  cout<<"delay scan end" << endl;
}




// resister initial
int rsin[8]={
  0x18

, //word0
  0x9C, //word1
  0x40, //word2
  0x99, //word3
  0x00, //word4
  0x63, //word5
  0x1A, //word6
  0x60 //word7
};

int rsin30[8]={
  28,
  22,
  20,
  10,
  0,
  25,
  10,
  3
};

int main(int argc,char **argv)
{
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

   
    int wr;
    int i;
    
 //reset      
    /*     
    for(i=0;i<8;i++){
      if(i!= 1 && i!=4){ 
	UsbWrt(id, i, &rsin[i], 1);
      }
    };
 
    for(i=0;i<8;i++){
      UsbWrt(id, 30+i, &rsin30[i], 1);
    };
    
    UsbWrt(id, 1, &rsin[1], 1);
     
    //slow control reset
    wr = 0x20;
    UsbWrt(id, 1, &wr, 1);
   
    //10ms wait
    usleep(10000);
    //probe reset
    wr = 0x00;
    UsbWrt(id, 1, &wr, 1);
    //10ms wait
    usleep(10000);
  */

    //word1 written again
    UsbWrt(id, 1, &rsin[1], 1);
    //200ms wait
    usleep(200000);
   


    
    //slow control 77bins 
    const char *file[] ={
      "../skiroc2a_ADC_4.txt"
    };
    ifstream f;
    f.open(file[0]);
    if(f.good())
      cout <<"fileopen" << endl;
    else
      return 0;
  
    const int scsize=77;
    unsigned char sc[scsize];
    unsigned int k;
      
    for(i =0 ; i<scsize ; i++){
      f >> hex >>k;
      sc[i] = (unsigned char) k ;
    };
    
    for(i =0 ; i<scsize ; i++){
      printf("%d : %x\n",i,sc[i]);
    };

    int ret = writeSlowControl(id, sc, false);
    cout << "SlowControl test :" << ret<<endl;

     
    /*
//probe resister

    unsigned char pb[193];
    memset(pb,0x00,sizeof(pb)); 

    pb[4]= 0x01; //ch10preampseen

    ret = writeSlowControl(id, pb, true);
    cout << "Probe test : " << ret <<endl;
    */

    
    
    //    Acquisition(id,1000,"test.raw");
    delayScan(id,0,255,1,500,sc,"tmp/delayscan");




  }catch(string s){
    cout << "Error: "<<s <<endl;
  }

  
  return 0;
}
