// L0 timeslices -> flat trees "l1" and "frames".
//
// Per DOM and timeslice, hits are sorted in time and grouped into windows: a window opens at the
// first unused hit and holds every hit up to window_ns later; m = number of distinct PMTs.
// Windows with m >= min_m are saved in "l1"; m = 1 windows are only counted (frames.n_single).

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "TFile.h"
#include "TNamed.h"
#include "TParameter.h"
#include "TSystem.h"
#include "TTree.h"

#include "km3net-dataformat/online/JDAQ.hh"
#include "km3net-dataformat/online/JDAQClock.hh"
#include "km3net-dataformat/online/JDAQSuperFrame.hh"

#include "../include/Constants.h"
#include "../include/Detx.h"
#include "../include/DomMap.h"

R__LOAD_LIBRARY(libKM3NeTROOT.so)

using namespace END;

struct L0Hit {
  double t;
  int ch, tot;
  bool operator<(const L0Hit& h) const { return t < h.t; }
};

void MakeL1Tree(std::string input, std::string output, std::string detx = "",
                double window_ns = 100., int min_m = 2, Long64_t max_slices = -1)
{
  gSystem->Load("libtriggerROOT"); // optional, silences warnings about trigger classes in the input

  if(min_m < 1 || min_m > 2){
    std::cerr << "min_m must be 1 or 2! Exiting!" << std::endl;
    std::exit(1);
  }

  std::map<int, DetxDom> doms;
  if(!detx.empty()){
    doms = ReadDetx(detx);
    if(doms.empty()){
      std::cerr << "Couldn't read " << detx << "! Exiting!" << std::endl;
      std::exit(1);
    }
  }

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

  TFile* fout = new TFile(output.c_str(), "RECREATE");

  // l1 tree
  int run, frame, dom_id, du, dom, m, nhit;
  unsigned int utc_s, utc_ns, pmt_mask;
  double t0;
  float tot_sum, hit_t[kMaxHits];
  unsigned char hit_ch[kMaxHits], hit_tot[kMaxHits];

  TTree* l1 = new TTree("l1", "coincidence windows per DOM");
  l1->Branch("run", &run, "run/I");
  l1->Branch("frame", &frame, "frame/I");
  l1->Branch("utc_s", &utc_s, "utc_s/i");
  l1->Branch("utc_ns", &utc_ns, "utc_ns/i");
  l1->Branch("dom_id", &dom_id, "dom_id/I");
  l1->Branch("du", &du, "du/I");
  l1->Branch("dom", &dom, "dom/I");
  l1->Branch("t0", &t0, "t0/D");
  l1->Branch("m", &m, "m/I");
  l1->Branch("nhit", &nhit, "nhit/I");
  l1->Branch("pmt_mask", &pmt_mask, "pmt_mask/i");
  l1->Branch("tot_sum", &tot_sum, "tot_sum/F");
  l1->Branch("hit_ch", hit_ch, "hit_ch[nhit]/b");
  l1->Branch("hit_t", hit_t, "hit_t[nhit]/F");
  l1->Branch("hit_tot", hit_tot, "hit_tot[nhit]/b");

  // frames tree
  int frame_nhit, n_single[kNSlices], udp_recv, udp_expected;
  unsigned int hrv, fifo;
  double frame_ns = KM3NETDAQ::getFrameTime(), t_hrv_cut;

  TTree* frames = new TTree("frames", "one entry per DOM per timeslice");
  frames->Branch("run", &run, "run/I");
  frames->Branch("frame", &frame, "frame/I");
  frames->Branch("utc_s", &utc_s, "utc_s/i");
  frames->Branch("dom_id", &dom_id, "dom_id/I");
  frames->Branch("du", &du, "du/I");
  frames->Branch("dom", &dom, "dom/I");
  frames->Branch("frame_ns", &frame_ns, "frame_ns/D");
  frames->Branch("nhit", &frame_nhit, "nhit/I");
  frames->Branch("n_single", n_single, Form("n_single[%d]/I", kNSlices));
  frames->Branch("hrv", &hrv, "hrv/i");
  frames->Branch("t_hrv_cut", &t_hrv_cut, "t_hrv_cut/D");
  frames->Branch("fifo", &fifo, "fifo/i");
  frames->Branch("udp_recv", &udp_recv, "udp_recv/I");
  frames->Branch("udp_expected", &udp_expected, "udp_expected/I");

  std::vector<L0Hit> hits;
  const Long64_t nslices = (max_slices < 0) ? l0->GetEntries() : std::min(max_slices, l0->GetEntries());

  for(Long64_t entry = 0; entry < nslices; entry++){
    branch->GetEntry(entry);
    if(entry % 1000 == 0) std::cout << "timeslice " << entry << " / " << nslices << std::endl;

    for(const KM3NETDAQ::JDAQSuperFrame& sf : superframes){
      run = sf.getRunNumber();
      frame = sf.getFrameIndex();
      utc_s = sf.getTimesliceStart().getUTCseconds();
      utc_ns = sf.getTimesliceStart().getUTC16nanosecondcycles() * 16;
      dom_id = sf.getModuleID();

      auto it = doms.find(dom_id);
      const DetxDom* detx_dom = (it != doms.end()) ? &it->second : nullptr;
      du = DomDU(dom_id);
      dom = DomNumber(dom_id);

      // DAQ status
      hrv = fifo = 0;
      for(int ch = 0; ch < kNPMT; ch++){
        if(sf.testHighRateVeto(ch)) hrv |= (1u << ch);
        if(sf.testFIFOStatus(ch)) fifo |= (1u << ch);
      }
      udp_recv = sf.getUDPNumberOfReceivedPackets();
      udp_expected = sf.hasUDPTrailer() ? sf.getUDPMaximalSequenceNumber() + 1 : -1;

      // hits, calibrated with the PMT t0 when a detx is given
      hits.clear();
      std::vector<double> last_hit(kNPMT, 0.);
      for(const KM3NETDAQ::JDAQHit& h : sf){
        const int ch = h.getPMT();
        if(ch < 0 || ch >= kNPMT) continue;
        last_hit[ch] = std::max(last_hit[ch], (double)h.getT());
        double t = h.getT();
        if(detx_dom && ch < (int)detx_dom->pmts.size()) t += detx_dom->pmts[ch].t0;
        hits.push_back({t, ch, (int)h.getToT()});
      }
      std::sort(hits.begin(), hits.end());
      frame_nhit = hits.size();

      // a vetoed PMT loses its hits for the rest of the frame: usable time ends at the first veto
      t_hrv_cut = frame_ns;
      for(int ch = 0; ch < kNPMT; ch++)
        if(hrv & (1u << ch)) t_hrv_cut = std::min(t_hrv_cut, last_hit[ch]);

      // windows
      std::fill(n_single, n_single + kNSlices, 0);
      for(size_t i = 0; i < hits.size(); ){
        t0 = hits[i].t;
        pmt_mask = 0;
        tot_sum = 0;
        nhit = 0;
        size_t j = i;
        for(; j < hits.size() && hits[j].t - t0 <= window_ns; j++){
          pmt_mask |= (1u << hits[j].ch);
          tot_sum += hits[j].tot;
          if(nhit < kMaxHits){
            hit_ch[nhit] = hits[j].ch;
            hit_t[nhit] = hits[j].t - t0;
            hit_tot[nhit] = hits[j].tot;
            nhit++;
          }
        }
        m = __builtin_popcount(pmt_mask);
        if(m == 1){
          const int k = (int)(t0 / frame_ns * kNSlices);
          n_single[std::max(0, std::min(kNSlices - 1, k))]++;
        }
        if(m >= min_m) l1->Fill();
        i = j;
      }
      frames->Fill();
    }
  }

  std::cout << "Wrote " << output << " : " << l1->GetEntries() << " L1 windows, "
            << frames->GetEntries() << " frames from " << nslices << " timeslices" << std::endl;

  fout->cd();
  TParameter<double>("window_ns", window_ns).Write();
  TParameter<int>("min_m", min_m).Write();
  TNamed("detx", detx.c_str()).Write();
  fout->Write();
  fout->Close();
  fin->Close();
}
