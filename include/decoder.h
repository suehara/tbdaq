// decoder.h

struct data_t {
  int cycleNumber;
  int bunchXID;
  int chipID;
  int memoryCell;
  int channel;
  int tdc;
  int adc;
  int hitBit;
  int gainBit;
};

extern int unpackRaw(int cycleNumber, const unsigned char * rawData, int rawDataSize, data_t* outData, int * outDataLength);
