#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <stdlib.h>

#include "TF1.h"
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TCanvas.h"
#include "TH2F.h"
#include "TSystem.h"
#include "TStyle.h"
#include "TTreeFormula.h"
#include "TList.h"
#include "TTreePlayer.h"
#include "TROOT.h"

const int framesize = 64;
const int cellsize = 13;

const int data_all = 3848;

struct TreeData{
  int bx;
  int acq;
  int trig_hg[framesize][cellsize];
  int trig_lg[framesize][cellsize];
  int adc_hg[framesize][cellsize];
  int adc_lg[framesize][cellsize];
  int toa_fall[framesize];
  int toa_rise[framesize];
  int tot_fast[framesize];
  int tot_slow[framesize];
  int rollpos;
  int global_ts;
  int chipid;
};

void rawToTree_cms(char *filename,char *Treename){
  TreeData data;
  
  TTree* tree = new TTree("tree" , "tree");
 
  tree->Branch("bx",&data.bx,"bx/I");
  tree->Branch("acq",&data.acq,"acq/I");
  tree->Branch("trig_hg",&data.trig_hg,TString::Format("trig_hg[%d][%d]/I",framesize,cellsize));
  tree->Branch("trig_lg",&data.trig_lg,TString::Format("trig_lg[%d][%d]/I",framesize,cellsize));
  tree->Branch("adc_hg",&data.adc_hg,TString::Format("adc_hg[%d][%d]/I",framesize,cellsize));
  tree->Branch("adc_lg",&data.adc_lg,TString::Format("adc_lg[%d][%d]/I",framesize,cellsize));
  tree->Branch("toa_fall",&data.toa_fall,TString::Format("toa_fall[%d]/I",framesize));
  tree->Branch("toa_rise",&data.toa_rise,TString::Format("toa_rise[%d]/I",framesize));
  tree->Branch("tot_fast",&data.tot_fast,TString::Format("tot_fast[%d]/I",framesize));
  tree->Branch("tot_slow",&data.tot_slow,TString::Format("tot_slow[%d]/I",framesize));
  tree->Branch("rollpos",&data.rollpos,"rollpos/I");
  tree->Branch("global_ts",&data.global_ts,"global_ts/I");
  tree->Branch("chipid",&data.chipid,"chipid/I");
  
  ifstream raw;
  unsigned short int dummy;
  int ndata;
  int counts;
  int n;
  int dummy_box[5]={};
  int break_num;

  counts = 1; 
  break_num = 0;

  raw.open(Form("%s",filename), ios::binary);
  if(raw.fail()){
    cout << "No such the raw file" <<endl;
  }
  while(!raw.eof()){
    n = 0;
    cout << "Counts : " << counts << endl;
    // Comfirmation of header
    raw.read((char*)&dummy,sizeof(short)); 
    if(dummy != 65532){
      cout << "error : header is not appropriate" << endl;
      cout << "Iregular header : " << dummy << endl;
      while(dummy != 65532){
	raw.read((char*)&dummy,sizeof(short));
	cout << "Look for header : " << dummy << endl;	
	dummy_box[n] = dummy;
	n++;
	cout << dummy_box[0] << dummy_box[1] << dummy_box[2] << dummy_box[3] << dummy_box[4] << endl;
	if(dummy_box[0] == 8224 && dummy_box[1] == 8224 && dummy_box[2] == 8224 && dummy_box[3] == 8224 && dummy_box[4] == 8224){
	  break_num = 1; 
	  break;
	}
      }
    }
    if(break_num == 1){
      break;
    }
    if(dummy == 65532){
      // skip
      for(int j = 0 ; j < 2; j++){
	raw.read((char*)&dummy,sizeof(short));
      }
      raw.read((char*)&dummy,sizeof(short));
      if(dummy != 20563){
	cout << "error : header is not appropriate" << endl;
	//exit(1);
      }else{
	for(int j = 0; j < 7; j++){
	  raw.read((char*)&dummy,sizeof(short));
	}
      }
    }
    // Read adc_lg, adc_hg, trig_lg and trig_hg
    for(int cell = 12; cell >= 0; cell--){
      for(int ch = 63; ch >= 0; ch--){
	//raw.read((char*)&dummy,sizeof(short));
	raw.read((char*)&dummy,sizeof(unsigned short int)); 
	if(dummy > 4096){
          data.adc_lg[ch][cell] = dummy - 4096;
          data.trig_lg[ch][cell] = 1;
        }else{
          data.adc_lg[ch][cell] = dummy;
          data.trig_lg[ch][cell] = 0;
	}
      }
      for(int ch = 63; ch >= 0; ch--){
	raw.read((char*)&dummy,sizeof(unsigned short int));
	if(dummy > 4096){
	  data.adc_hg[ch][cell] = dummy - 4096;
	  data.trig_hg[ch][cell] = 1;
	}else{
	  data.adc_hg[ch][cell] = dummy;
          data.trig_hg[ch][cell] = 0;
	}
	//cout << "dummy : " << dummy << ", adc_hg : " << data.adc_hg[ch][cell] << endl;
      }
    }
    
    //toa_fall
    for(int ch = 63; ch >= 0; ch--){
      raw.read((char*)&dummy,sizeof(short));
      data.toa_fall[ch] = dummy;
    }
    
    //toa_rise
    for(int ch = 63; ch >= 0; ch--){
      raw.read((char*)&dummy,sizeof(short));
      data.toa_rise[ch] = dummy;
    }
    
    //tot_fast
    for(int ch = 63; ch >= 0; ch--){
      raw.read((char*)&dummy,sizeof(short));
      data.tot_fast[ch] = dummy;
    }
    
    //tot_slow
    for(int ch = 63; ch >= 0; ch--){
      raw.read((char*)&dummy,sizeof(short));
      data.tot_slow[ch] = dummy;
    }

    //roll position
    raw.read((char*)&dummy,sizeof(short));
    data.rollpos = dummy;
    
    //global TS (skip)
    for(int j = 0; j < 2; j++){
      raw.read((char*)&dummy,sizeof(short));
    }
    
    //chip ID
    raw.read((char*)&dummy,sizeof(short));
    data.chipid = dummy;

    //acq number
    data.acq = counts;

    // Confirmation of trailer
    raw.read((char*)&dummy,sizeof(short));
    cout << "dummy2 : " << dummy << endl; 
    if(dummy != 65534){
      cout << "trailer is not appropriate" << endl;
      exit(1);
    }
    // skip
    for(int j = 0; j < 10; j++){
      raw.read((char*)&dummy,sizeof(short));
    }
    
    //test
    cout << "adc_hg[0][0] : " << data.adc_hg[0][0] << ", tot_fast[32] : " << data.tot_fast[32] << endl;


    if(data.adc_hg[0][0] != 8224 || data.toa_fall[32] != 8224 || data.tot_slow[63] != 8224){
    tree->Fill();
    counts++;
    }
  }

  TFile* file = new TFile(Treename , "RECREATE");
  tree->Write();
  file->Close();    
}
