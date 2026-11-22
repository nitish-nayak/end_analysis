// Per-PMT monitoring histograms from L0 timeslices, written to a ROOT file
// (offline version of the web monitor's plots). One directory per DOM with, per PMT channel:
//   rate_vs_time_<ch>  mean rate [kHz] vs time in the run
//   rate_<ch>          distribution of the rate in single frames [kHz]
//   hit_time_<ch>      hit time within the frame [ms]
//   tot_<ch>           ToT [ns], all hits
//   totcut_<ch>        ToT [ns], hits from frames without dropped UDP packets

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "TFile.h"
#include "TH1D.h"
#include "TSystem.h"
#include "TTree.h"

#include "km3net-dataformat/online/JDAQ.hh"
#include "km3net-dataformat/online/JDAQClock.hh"
#include "km3net-dataformat/online/JDAQSuperFrame.hh"

#include "../include/Constants.h"
#include "../include/Detx.h"

R__LOAD_LIBRARY(libKM3NeTROOT.so)

using namespace END;

struct PmtHists {
  TH1D *rate_vs_time, *frames_vs_time, *rate, *hit_time, *tot, *totcut;
};

void L0Monitor(std::string input, std::string output, std::string detx = "", Long64_t max_slices = -1)
{
  gSystem->Load("libtriggerROOT"); // optional, silences warnings about trigger classes in the input

  std::map<int, DetxDom> doms = detx.empty() ? std::map<int, DetxDom>() : ReadDetx(detx);

  TFile* fin = TFile::Open(input.c_str(), "read");
  TTree* l0 = fin ? (TTree*)fin->Get("KM3NET_TIMESLICE_L0") : nullptr;
  if(!l0){
    std::cerr << "No KM3NET_TIMESLICE_L0 tree in " << input << "! Exiting!" << std::endl;
    std::exit(1);
  }
  l0->SetMakeClass(1);
  std::vector<KM3NETDAQ::JDAQSuperFrame> superframes;
  TBranch* branch = l0->GetBranch("vector<KM3NETDAQ::JDAQSuperFrame>");
  branch->SetAddress(&superframes);
  const Long64_t nslices = (max_slices < 0) ? l0->GetEntries() : std::min(max_slices, l0->GetEntries());

  // time axis from the first and last timeslice, at most 2000 bins
  branch->GetEntry(0);
  const double t_first = superframes.at(0).getTimesliceStart().getUTCseconds();
  branch->GetEntry(nslices - 1);
  const double duration = superframes.at(0).getTimesliceStart().getUTCseconds() - t_first + 1;
  const int ntime = std::min(2000, (int)duration);
  const double frame_s = KM3NETDAQ::getFrameTime() * 1e-9;

  // per-frame rate, log bins 0.01 - 100 kHz
  std::vector<double> rate_edges;
  for(int i = 0; i <= 200; i++) rate_edges.push_back(std::pow(10., -2. + 4. * i / 200));

  std::map<int, std::vector<PmtHists>> hists;
  auto book = [&](int dom_id){
    std::vector<PmtHists> v(kNPMT);
    for(int ch = 0; ch < kNPMT; ch++){
      const char* t = Form("%d ch %d", dom_id, ch);
      v[ch].rate_vs_time = new TH1D(Form("rate_vs_time_%d", ch), Form("%s;seconds since UTC %.0f;rate [kHz]", t, t_first), ntime, 0, duration);
      v[ch].frames_vs_time = new TH1D(Form("frames_vs_time_%d", ch), "", ntime, 0, duration);
      v[ch].rate = new TH1D(Form("rate_%d", ch), Form("%s;rate in one frame [kHz];frames", t), 200, &rate_edges[0]);
      v[ch].hit_time = new TH1D(Form("hit_time_%d", ch), Form("%s;hit time in frame [ms];hits", t), 1000, 0, frame_s * 1e3);
      v[ch].tot = new TH1D(Form("tot_%d", ch), Form("%s;ToT [ns];hits", t), 257, -0.5, 256.5);
      v[ch].totcut = new TH1D(Form("totcut_%d", ch), Form("%s (frames without dropped packets);ToT [ns];hits", t), 257, -0.5, 256.5);
      for(TH1D* h : {v[ch].rate_vs_time, v[ch].frames_vs_time, v[ch].rate, v[ch].hit_time, v[ch].tot, v[ch].totcut})
        h->SetDirectory(nullptr);
    }
    return v;
  };

  for(Long64_t entry = 0; entry < nslices; entry++){
    branch->GetEntry(entry);
    if(entry % 1000 == 0) std::cout << "timeslice " << entry << " / " << nslices << std::endl;

    for(const KM3NETDAQ::JDAQSuperFrame& sf : superframes){
      const int dom_id = sf.getModuleID();
      if(!hists.count(dom_id)) hists[dom_id] = book(dom_id);
      std::vector<PmtHists>& h = hists[dom_id];
      const double t = sf.getTimesliceStart().getUTCseconds() - t_first + 0.5;
      const bool good = sf.hasUDPTrailer() && sf.getUDPNumberOfReceivedPackets() == sf.getUDPMaximalSequenceNumber() + 1;

      std::vector<int> count(kNPMT, 0);
      for(const KM3NETDAQ::JDAQHit& hit : sf){
        const int ch = hit.getPMT();
        if(ch < 0 || ch >= kNPMT) continue;
        count[ch]++;
        h[ch].hit_time->Fill(hit.getT() * 1e-6);
        h[ch].tot->Fill(hit.getToT());
        if(good) h[ch].totcut->Fill(hit.getToT());
      }
      for(int ch = 0; ch < kNPMT; ch++){
        const double rate_khz = count[ch] / frame_s * 1e-3;
        h[ch].rate->Fill(rate_khz);
        h[ch].rate_vs_time->Fill(t, rate_khz);
        h[ch].frames_vs_time->Fill(t);
      }
    }
  }

  TFile fout(output.c_str(), "RECREATE");
  for(auto& d : hists){
    auto it = doms.find(d.first);
    const std::string name = (it != doms.end()) ? Form("DU%d_DOM%d", it->second.du, it->second.floor) : Form("DOM_%d", d.first);
    fout.mkdir(name.c_str(), Form("DOM %d", d.first))->cd();
    for(int ch = 0; ch < kNPMT; ch++){
      PmtHists& h = d.second[ch];
      h.rate_vs_time->Divide(h.frames_vs_time); // mean rate per time bin
      for(TH1D* x : {h.rate_vs_time, h.rate, h.hit_time, h.tot, h.totcut}) x->Write();
    }
  }
  fout.Close();
  fin->Close();
  std::cout << "Wrote " << output << " : " << hists.size() << " DOMs from " << nslices << " timeslices" << std::endl;
}
