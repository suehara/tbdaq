// raw_tree.C 
// original Author V. Balagura, balagura@cern.ch (19.11.2012)
// modified from raw_main.C T. Suehara (28.11.2013)

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>

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

#include "raw.C"

const int framesize = 64;
const int chsize = 4;

struct TreeData{
  int dif;
  int bx;
  int acq;
  int isca[chsize][framesize];
  int iscadif;
  int trig_hg[chsize][framesize];
  int trig_lg[chsize][framesize];
  int adc_hg[chsize][framesize];
  int adc_lg[chsize][framesize];
  int status_ped[chsize][framesize];
};

void Pedestal(TTree *tree, int noisycut = -5);
void CountPixels(TTree *tree);


TTree * rawToTree(const char *rawfile, int dif = 0)
{
  TreeData data;
  
  TTree *tree = new TTree("tree","tree");

  tree->Branch("dif"    ,&data.dif    , "dif/I" );
  tree->Branch("bx"     ,&data.bx     , "bx/I" );
  tree->Branch("acq"    ,&data.acq    , "acq/I");
  tree->Branch("isca"   ,&data.isca   , TString::Format("isca[%d][%d]/I",chsize,framesize));
  tree->Branch("iscadif",&data.iscadif, "iscadif/I");
  tree->Branch("trig_hg",&data.trig_hg, TString::Format("trig_hg[%d][%d]/I",chsize,framesize));
  tree->Branch("trig_lg",&data.trig_lg, TString::Format("trig_lg[%d][%d]/I",chsize,framesize));
  tree->Branch("adc_hg" ,&data.adc_hg , TString::Format("adc_hg[%d][%d]/I",chsize,framesize));
  tree->Branch("adc_lg" ,&data.adc_lg , TString::Format("adc_lg[%d][%d]/I",chsize,framesize));

  ReadSpill spill(rawfile);

  int prev_acq = -1; // acquisition_number has 16 bits; when it goes down (eg. from 65535 to 0): add 65536
  int acq = 0;

  while (spill.next()) {
    if      (prev_acq == -1)                         acq  =  spill.acquisition_number();
    else if (spill.acquisition_number() >= prev_acq) acq += (spill.acquisition_number() - prev_acq);
    else                                             acq += (spill.acquisition_number() - prev_acq + 65536);
    prev_acq = spill.acquisition_number();

    if(acq % 1000 == 0){
      cout << acq << endl;
      tree->Write();
    }
    
    const map<unsigned short int, vector<ChipSCA> >& sca = spill.sca();
    for (map<unsigned short int, vector<ChipSCA> >::const_iterator isca=sca.begin(); isca!=sca.end(); ++isca) {
      memset(&data, 0,sizeof(data));
      data.dif = dif;
      data.acq = acq;
      data.bx  = isca->first;
      int nch = (int)isca->second.size();

      //cout << "bx = " << data.bx << ", nch = " << nch << endl;

      if(nch > chsize)nch = chsize;

      data.iscadif=0;
      for (int ich=0; ich<nch; ++ich) {
        const ChipSCA& s = isca->second[ich];
	if(data.iscadif < s.isca)data.iscadif = s.isca;
      }

      for (int ich=0; ich<nch; ++ich) {
        const ChipSCA& s = isca->second[ich];
	//cout << "ich = " << ich << ", s.chip_id = " << s.chip_id << ", s.isca = " << s.isca << endl;

	int size = (int)s.high.size();
	if(size>(int)s.low.size())size = s.low.size();
	if(size>framesize)size = framesize;

	for(int i=0;i<size;i++){
	  ChipADC ah = s.high[i];
	  ChipADC al = s.low[i];
	  data.isca[s.chip_id][i] = s.isca;
	  data.trig_hg[s.chip_id][i] = ah.hit;
	  data.trig_lg[s.chip_id][i] = al.hit;
	  data.adc_hg[s.chip_id][i] = ah.adc;
	  data.adc_lg[s.chip_id][i] = al.adc;
	  //if(ah.hit){cout << "acq = " << data.acq << ", bx = " << data.bx << ", chip = " << s.chip_id << ", ch = " << i << endl;}
	}	
      }
      tree->Fill();
    }
  }
  tree->ResetBranchAddresses();
  return tree;
}

TTree * rawToTree(const char *rawfile, const char *treefile, int dif = 0){
  TFile *file = TFile::Open(treefile,"update");
  TTree *ret = rawToTree(rawfile,dif);
  file->Write();
  return ret;
}

void rawToTreeAll(const char *inbase, const char *outfile, bool pedestal = true, bool npix = true, int difmin = 1, int difmax = 5)
{
  TFile *file = new TFile(outfile,"recreate");

  TList li;
  for(int dif=difmin; dif<=difmax;dif++){
    cout << "converting " << inbase << dif << "..." << endl;
    TTree *tr2 = rawToTree(TString::Format("%s%d.raw",inbase,dif), dif);
    tr2->SetName(TString::Format("tree%d",dif));
    if(pedestal)Pedestal(tr2);
    //tr2->Write();
    li.Add(tr2);
  }

  // merge
  cout << "Merging trees for all difs..." << endl;
  TTree *tr = TTree::MergeTrees(&li);
  tr->SetName("treeall");

  // npix
  if(npix){
    cout << "Counting pixels..." << endl;
    CountPixels(tr);
  }

  file->Write();
}

void MeanRms(TTree *tree, const char *file)
{
  TH1F h("h","h",4096,0,4096);
  ofstream out(file);

  for(int nchip=0;nchip<4;nchip++){
    for(int nch=0;nch<64;nch++){
      h.Reset();
      tree->Project("h",TString::Format("adc_hg[%d][%d]",nchip,nch), TString::Format("adc_hg[%d][%d]>0",nchip,nch));
      out << nchip << " " << nch << " " << h.GetMean() << " " << h.GetRMS() << endl;
    }
  }

}

struct PixMap{
  int x;
  int y;
  int size;
};
PixMap pixmap[4][64];
bool pixmap_init = false;

void LoadPixMap(const char *file = "pixmap.txt"){
  ifstream in(file);
  int chip,pix,x,y,size;

  while(!in.fail()){
    in >> chip >> pix >> x >> y >> size;
    cout << "chip " << chip << " pix " << pix << " x " << x << " y " << y << " size " << size << endl;
    
    pixmap[chip][pix].x = x;
    pixmap[chip][pix].y = y;
    pixmap[chip][pix].size = size;
  }

  pixmap_init = true;
}

int numofbits(int bits)
{
    bits = (bits & 0x55555555) + (bits >> 1 & 0x55555555);
    bits = (bits & 0x33333333) + (bits >> 2 & 0x33333333);
    bits = (bits & 0x0f0f0f0f) + (bits >> 4 & 0x0f0f0f0f);
    bits = (bits & 0x00ff00ff) + (bits >> 8 & 0x00ff00ff);
    return (bits & 0x0000ffff) + (bits >>16 & 0x0000ffff);
}

void CountPixels(TTree *tree)
{
  int nhit;
  int nhitall;

  TreeData data;

  tree->SetBranchAddress("status_ped" ,&data.status_ped);
  tree->SetBranchAddress("isca"   ,&data.isca);
  tree->SetBranchAddress("trig_hg",&data.trig_hg);
  tree->SetBranchAddress("dif",&data.dif);
  tree->SetBranchAddress("acq",&data.acq);
  tree->SetBranchAddress("bx",&data.bx);

  TBranch *br = tree->Branch("nhit",&nhit,"nhit/I");
  TBranch *br2 = tree->Branch("nhitall",&nhitall,"nhitall/I");

  // counting number of pixels
  multimap<int, int>map_acqbx_dif;

  cout << "counting number of pixels..." << endl;
  for(int i=0;i<tree->GetEntries();i++){
    tree->GetEntry(i);
    if(i%1000==0)cout << "Event # " << i << " processing ..." << endl;
    for(int chip=0;chip<chsize;chip++){
      for(int pix=0;pix<framesize;pix++){
	if(data.isca[chip][pix] == 0 && data.trig_hg[chip][pix] == 1 && data.status_ped[chip][pix] == 0){
	  map_acqbx_dif.insert(pair<int,int>(data.acq*4096+data.bx, data.dif));
	  if(map_acqbx_dif.count(data.acq*4096+data.bx) > 1)cout << data.dif << endl;
	}
      }
    }
  }

  cout << "Filling number of hits..." << endl;
  for(int i=0;i<tree->GetEntries();i++){
    tree->GetEntry(i);
    if(i%1000==0)cout << "Event # " << i << " processing ..." << endl;
    for(int chip=0;chip<chsize;chip++){
      for(int pix=0;pix<framesize;pix++){
	if(data.isca[chip][pix] == 0 && data.trig_hg[chip][pix] == 1 && data.status_ped[chip][pix] == 0){
	  nhitall = (int)map_acqbx_dif.count(data.acq*4096+data.bx);
	  map<int,int>::iterator it = map_acqbx_dif.find(data.acq*4096+data.bx);
	  int hitmask = 0;
	  for(int nh=0;nh<nhitall;nh++){
	    hitmask |= (1 << it->second);
	    it ++;
	  }
	  cout << data.dif << " " << hitmask << endl;
	  nhit = numofbits(hitmask);

	  br->Fill();
	  br2->Fill();
	}
      }
    }
  }
  
  tree->ResetBranchAddresses();
}

void Pedestal(TTree *tree, int noisycut)
{
  if(!pixmap_init)LoadPixMap();

  TreeData data;
  /*
  TCanvas *c[2];
  for(int i=0;i<2;i++){
    c[i] = new TCanvas;
    c[i]->Divide(18,18);
  }
  */
  double pedestal[chsize][framesize];
  double adc_hg_ped[chsize][framesize];
  int status_ped[chsize][framesize];

  TBranch *br1 = tree->Branch("adc_hg_ped" ,&adc_hg_ped , TString::Format("adc_hg_ped[%d][%d]/D",chsize,framesize));
  TBranch *br2 = tree->Branch("status_ped" ,&status_ped , TString::Format("status_ped[%d][%d]/I",chsize,framesize));
  tree->SetBranchAddress("isca"   ,&data.isca);
  tree->SetBranchAddress("trig_hg",&data.trig_hg);
  tree->SetBranchAddress("adc_hg" ,&data.adc_hg);

  // initialize histograms
  cout << "Initializing histograms..." << endl;

  TH1F *h[2][chsize][framesize];
  for(int chip=0;chip<chsize;chip++){
    for(int pix=0;pix<framesize;pix++){
      // pedestal 1st
      TString name = TString::Format("h_ped1_%d_%d",chip,pix);
      h[0][chip][pix] = new TH1F(name,name,200,200,400); // pedestal
    }
  }

  // filling histograms
  cout << "Filling histograms for pedestal substractions..." << endl;

  int nhit[chsize][framesize];
  memset(nhit, 0,sizeof(nhit));
  int nhitall = 0;

  for(int i=0;i<tree->GetEntries();i++){
    tree->GetEntry(i);
    if(i%1000==0)cout << "Event # " << i << " processing ..." << endl;
    for(int chip=0;chip<chsize;chip++){
      for(int pix=0;pix<framesize;pix++){
	if(data.isca[chip][pix] == 0 && data.trig_hg[chip][pix] == 0){
	  h[0][chip][pix]->Fill(data.adc_hg[chip][pix]);
	}else if(data.isca[chip][pix] == 0){
	  nhit[chip][pix]++;
	  nhitall ++;
	}
      }
    }
  }
  
  // setting threshold for cut
  int th = noisycut;
  if(th < 0){// automatic determination
    double ave = double(nhitall) / (chsize * framesize);
    th = (int) ave + (-th) * sqrt(ave);
  }
	 
  // fitting histograms
  cout << "Fitting histograms..." << endl;

  for(int chip=0;chip<chsize;chip++){
    for(int pix=0;pix<framesize;pix++){
      status_ped[chip][pix] = 0; // normal
      if(nhit[chip][pix] > th){
	cout << "noisy channel " << chip << " " << pix << endl;
	status_ped[chip][pix] = -1;
      }
      h[0][chip][pix]->Fit("gaus","Q","",270,330);
      if(h[0][chip][pix]->GetFunction("gaus") == 0){
	cout << "Skipping channel with pedestal fit failed " << chip << " " << pix << endl;
	status_ped[chip][pix] = -2;
	pedestal[chip][pix] = 0;
	continue;
      }

      int ped1_center = (int)h[0][chip][pix]->GetFunction("gaus")->GetParameter(1);
      int ped1_sigma = (int)h[0][chip][pix]->GetFunction("gaus")->GetParameter(2);

      int start = ped1_center - ped1_sigma * 5;
      int end = ped1_center + ped1_sigma * 5;

      TString name = TString::Format("h_ped2_%d_%d",chip,pix);
      h[1][chip][pix] = new TH1F(name,name,ped1_sigma*10+1 ,ped1_center-ped1_sigma*5,ped1_center+ped1_sigma*5+1); // pedestal

      for(int ii = start; ii <= end; ii++){
	h[1][chip][pix]->Fill(ii, h[0][chip][pix]->GetBinContent(h[0][chip][pix]->FindBin(ii)));
      }
      h[1][chip][pix]->Fit("gaus","Q");
      if(h[1][chip][pix]->GetFunction("gaus") == 0){
	cout << "Skipping channel with pedestal fit failed " << chip << " " << pix << endl;
	status_ped[chip][pix] = -3;
	pedestal[chip][pix] = 0;
	continue;
      }
      pedestal[chip][pix] = h[1][chip][pix]->GetFunction("gaus")->GetParameter(1);

      cout << "chip " << chip << ", pix " << pix << " pedestal " << pedestal[chip][pix] << endl;
    }
  }

  /*
  // initialize histograms
  TH1F *h[4][chsize][framesize];
  for(int chip=0;chip<chsize;chip++){
    for(int pix=0;pix<framesize;pix++){
      // pedestal 1st
      TString name = TString::Format("h_ped1_%d_%d",chip,pix);
      c[0]->cd(pixmap[chip][pix].x + pixmap[chip][pix].y*18 +1);
      h[0][chip][pix] = new TH1F(name,name,200,200,400); // pedestal
      tree->Project(name, TString::Format("adc_hg[%d][%d]",chip,pix),TString::Format("trig_hg[%d][%d]==0&&isca[%d][%d]==0",chip,pix,chip,pix));
      h[0][chip][pix]->Fit("gaus","","",270,330);
      int ped1_center = (int)h[0][chip][pix]->GetFunction("gaus")->GetParameter(1);
      int ped1_sigma = (int)h[0][chip][pix]->GetFunction("gaus")->GetParameter(2);
      h[0][chip][pix]->Draw();

      // pedestal 2nd
      name = TString::Format("h_ped2_%d_%d",chip,pix);
      c[1]->cd(pixmap[chip][pix].x + pixmap[chip][pix].y*18 +1);
      h[1][chip][pix] = new TH1F(name,name,ped1_sigma*10+1 ,ped1_center-ped1_sigma*5,ped1_center+ped1_sigma*5+1); // pedestal
      tree->Project(name, TString::Format("adc_hg[%d][%d]",chip,pix),TString::Format("trig_hg[%d][%d]==0&&isca[%d][%d]==0",chip,pix,chip,pix));
      h[1][chip][pix]->Fit("gaus");
      pedestal[chip][pix] = h[1][chip][pix]->GetFunction("gaus")->GetParameter(1);
      h[1][chip][pix]->Draw();

      cout << "chip " << chip << ", pix " << pix << " pedestal" << pedestal[chip][pix] << endl;

      
      // sig 1st
      name = TString::Format("h_sig_%d_%d",chip,pix);
      c[2]->cd(pixmap[chip][pix].x + pixmap[chip][pix].y*18 +1);
      h[2][chip][pix] = new TH1F(name,name,700,0,700); // sig
      tree->Project(name, TString::Format("adc_hg[%d][%d]",chip,pix),TString::Format("trig_hg[%d][%d]==1&&isca[%d][%d]==0",chip,pix,chip,pix));
      h[2][chip][pix]->Draw();

      // sig 2nd
      name = TString::Format("h_sigcor_%d_%d",chip,pix);
      c[3]->cd(pixmap[chip][pix].x + pixmap[chip][pix].y*18 +1);
      h[3][chip][pix] = new TH1F(name,name,210,-10,200); // sig with pedsub
      tree->Project(name, TString::Format("adc_hg[%d][%d]-%g",chip,pix,ped2_center),TString::Format("trig_hg[%d][%d]==1&&isca[%d][%d]==0",chip,pix,chip,pix));
      h[3][chip][pix]->Draw();
      
    }
  }*/

  cout << "processing trees..." << endl;
  for(int i=0;i<tree->GetEntries();i++){
    tree->GetEntry(i);
    for(int chip=0;chip<chsize;chip++){
      for(int pix=0;pix<framesize;pix++){
	adc_hg_ped[chip][pix] = data.adc_hg[chip][pix] - pedestal[chip][pix];
      }
    }
    br1->Fill();
    br2->Fill();
  }

  tree->ResetBranchAddresses();
  //  tree->Write();
}


void DrawEvent(TTree *tree, int nevStart, int nevEnd, bool newhisto, int wait = 1000, const char *cut = "1", int zmin = 0, int zmax = 0, bool newcanvas = true)
{
  if(!pixmap_init)LoadPixMap();

  gStyle->SetOptStat(0);

  TCanvas *c;
  if(newcanvas){
    c = new TCanvas;
    c->Divide(2,2);
  }
  else
    c = (TCanvas *)gROOT->GetSelectedPad();

  TreeData data;

  tree->SetBranchAddress("bx"     ,&data.bx);
  tree->SetBranchAddress("acq"    ,&data.acq);
  tree->SetBranchAddress("trig_hg",&data.trig_hg);
  tree->SetBranchAddress("trig_lg",&data.trig_lg);
  tree->SetBranchAddress("adc_hg" ,&data.adc_hg);
  tree->SetBranchAddress("adc_lg" ,&data.adc_lg);

  TH2F *hthg, *htlg, *hahg, *halg;

  if(newhisto){
    hthg = new TH2F("hthg","Trig high gain",18,0,18,18,0,18);
    htlg = new TH2F("htlg","Trig low gain",18,0,18,18,0,18);
    hahg = new TH2F("hahg","ADC high gain",18,0,18,18,0,18);
    halg = new TH2F("halg","ADC low gain",18,0,18,18,0,18);
  }else{
    hthg = (TH2F *)gDirectory->Get("hthg");
    htlg = (TH2F *)gDirectory->Get("htlg");
    hahg = (TH2F *)gDirectory->Get("hahg");
    halg = (TH2F *)gDirectory->Get("halg");
  }

  TTreeFormula form("form",cut,tree);

  for(int nev = nevStart; nev <= nevEnd; nev++){

    tree->GetEntry(nev);
    cout << "nev = " << nev << ", form = " << form.EvalInstance(nev) << endl;
    if(form.EvalInstance(nev) == 0)continue;

    hthg->Reset();
    htlg->Reset();
    hahg->Reset();
    halg->Reset();

    cout << "nev = " << nev << ", acq = " << data.acq << ", bx = " << data.bx << endl;

    for(int nchip=0;nchip<4;nchip++){
      for(int npix=0;npix<64;npix++){
	hthg->Fill(pixmap[nchip][npix].x, pixmap[nchip][npix].y, data.trig_hg[nchip][npix]);
	htlg->Fill(pixmap[nchip][npix].x, pixmap[nchip][npix].y, data.trig_lg[nchip][npix]);
	hahg->Fill(pixmap[nchip][npix].x, pixmap[nchip][npix].y, data.adc_hg[nchip][npix]);
	halg->Fill(pixmap[nchip][npix].x, pixmap[nchip][npix].y, data.adc_lg[nchip][npix]);
      }
    } 

    c->cd(1);
    hthg->Draw("colz");
    c->cd(2);
    htlg->Draw("colz");
    c->cd(3);
    hahg->Draw("colz text");
    if(zmax != 0)hahg->GetZaxis()->SetRangeUser(zmin,zmax);
    c->cd(4);
    halg->Draw("colz text");
    if(zmax != 0)halg->GetZaxis()->SetRangeUser(zmin,zmax);

    c->Update();
    gSystem->Sleep(wait);
  }
}

