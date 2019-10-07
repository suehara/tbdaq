#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <bitset>
#include <ctime>
#include "decoder.h"


using namespace std;

ifstream::pos_type fileSize;
unsigned char * fileContent;

int dataLength;                             // in 16-bit words
vector<unsigned char> newChip;              // Used to push_back a new row to concatenatedData vector

bitset<8> firstByte;
bitset<8> secondByte;
bitset<16> word;

int dataSize;           // Number of bytes from each chip for the current cycle
int triggerCount;       // Number of triggers in the current cycle
int chipID;

vector<vector<unsigned char> >  concatenatedData;

unsigned char headerByte1 = '\x43';
unsigned char headerByte2 = '\x41';
unsigned char headerByte4 = '\x41';
unsigned char firstPacket = '\xb0';
unsigned char lastPacket  = '\xe0';

struct data_t data[16*36*8];                    // 36 channels per trigger, maximum 16 triggers


// Gray code to binary convertor =============================================================================+
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

int unpackRaw(int cycleNumber, const unsigned char * rawData, int rawDataSize, data_t* outData, int * outDataLength){
    clock_t t1, t2;


    int counter=0;
    // Counting number of packets and find their begining position in the array ===============================+
    for (int i = 0; i < rawDataSize - 6; i++){// last 6 must be an ack
			
        if ( (rawData[i] == headerByte1) && (rawData[i+1] == headerByte2) &&
             (rawData[i+4] == headerByte4)) {

            if(rawData[i+10] == firstPacket){                 // push_back a row if firstPacket detected
              concatenatedData.push_back(newChip);
              //cout << "A new row pushed back..." << endl;
            }
/*                dataLength = (unsigned int)rawData[ i + 7 ] * 2 -4;
                int currentRow = concatenatedData.size() - 1;
                for(int j = 0; j < dataLength; j++){
                  concatenatedData[currentRow].push_back(rawData[i+j+12]);
                }
*/
						i += 12; // skip header
          }else{// no header detected
						int currentRow = concatenatedData.size() - 1;
						concatenatedData[currentRow].push_back(rawData[i]);
					}
     }
     cout << "Number of chips found: " << concatenatedData.size() << endl;


     // Reading, decoding and filing data struct ==============================================================+
     ofstream output ("output.txt");
     for(unsigned int k = 0; k < concatenatedData.size(); k++){
         t1 = t2 = clock();
          dataSize = concatenatedData[k].size();
					cout << "datasize " << dataSize << endl;
          triggerCount = ((concatenatedData[k].size()-1)/2) / 72;
          chipID = concatenatedData[k][dataSize-1];
          cout << "Number of triggers: " << dec << triggerCount << endl;
          cout << "Chip ID: " << chipID << endl;


          for(int i = 0; i<triggerCount; i++){
              firstByte = bitset<8> (concatenatedData[k][dataSize-((i*2)+3)]);
              secondByte = bitset<8> (concatenatedData[k][dataSize-((i*2)+4)]);
              word = bitset<16> (secondByte.to_string() + firstByte.to_string());
              int bunchXID = grayDecode(word);



              for(int j = 0; j < 36; j++){

                  outData[counter].cycleNumber = cycleNumber;
                  outData[counter].chipID = chipID;
                  outData[counter].bunchXID = bunchXID;
                  outData[counter].channel = j;
                  outData[counter].memoryCell = i;


                  int fb = (j*2) + (i*144) + (triggerCount+1)*2 + 1;
                  firstByte = bitset<8> (concatenatedData[k][dataSize - fb ]);
                  secondByte = bitset<8> (concatenatedData[k][dataSize- (fb+1) ]);
                  word = bitset<16> (secondByte.to_string() + firstByte.to_string());
                  outData[counter].adc = grayDecode(word);
                  outData[counter].hitBit = word[12];
                  outData[counter].gainBit = word[13];

                  int fba = fb + 72;
                  //cout << i<< " " << j << " " << fb << " " << fba << endl;
                  firstByte = bitset<8> (concatenatedData[k][dataSize - fba]);
                  secondByte = bitset<8> (concatenatedData[k][dataSize- (fba+1) ]);
                  word = bitset<16> (secondByte.to_string() + firstByte.to_string());
                  outData[counter].tdc = grayDecode(word);

/*                cout << "\t" << outDataç.bunchXID;
                  cout << "\t" << outData[k*triggerCount*36+i*36+j].chipID;
                  cout << "\t" << outData[k*triggerCount*36+i*36+j].memoryCell;
                  cout << "\t" << outData[k*triggerCount*36+i*36+j].channel;
                  cout << "\t" << outData[k*triggerCount*36+i*36+j].tdc;
                  cout << "\t" << outData[k*triggerCount*36+i*36+j].adc;
                  cout << "\t" << outData[k*triggerCount*36+i*36+j].hitBit;
                  cout << "\t" << outData[k*triggerCount*36+i*36+j].gainBit << endl;

                  output << "\t" << outData[counter].bunchXID;
                  output << "\t" << outData[counter].chipID;
                  output << "\t" << outData[counter].memoryCell;
                  output << "\t" << outData[counter].channel;
                  output << "\t" << outData[counter].tdc;
                  output << "\t" << outData[counter].adc;
                  output << "\t" << outData[counter].hitBit;
                  output << "\t" << outData[counter].gainBit << endl;
*/
                  counter ++;


              }

          }
          t2 = clock();
          cout << (double)(t2 - t1) / CLOCKS_PER_SEC * 1000 << " ms.\n";

      }
     *outDataLength = counter;
     output.close();

		 concatenatedData.clear();
		 
     return 0;

}

// ==========================================================================================================+

/*

using namespace std;

int main()
{
    //clock_t t1, t2;
    //t1 = t2 = clock();

    // Reading data file to an array ==========================================================================+
    ifstream file ("/Users/Majid/Downloads/ali_conversion/test__raw_cyc0_usb0.bin", ios::in | ios::binary | ios::ate);
    if (file.is_open()) {
      fileSize = file.tellg();
      fileContent = new unsigned char [fileSize];
      file.seekg(0, ios::beg);
      file.read ((char*)fileContent, fileSize);
      file.close();
      cout << "The file successfuly loaded to memory." << endl;

       // Print out file content in hex
       for (int i = 0; i<fileSize; i++){
          if (i % 16 == 0) {cout << endl << dec <<i/16 << "\t";}
          cout << setw(2) << setfill('0') << hex << (int) fileContent[i] << " ";
        }
        cout << endl << "*******************************************************" << endl;

    } else {
        cout << "Unable to open the file!" << endl;
    }

    int ctr = 0;
    int* outDL = &ctr;

    unpackRaw(0, fileContent,fileSize,data,outDL);

    //t2 = clock();
    // print resolution of clock()
    //cout << (double)(t2 - t1) / CLOCKS_PER_SEC * 1000 << " ms.\n";

    return 0;
}

*/