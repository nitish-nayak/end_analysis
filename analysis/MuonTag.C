// Tags down-going muons along each column of DOMs (DOMs with the same x, y in the detx):
// the top and bottom DOM both have an L1 window with m >= tag_m, and the bottom one starts
// dz/c +- half_width_ns after the top one. For each tag, the best window (largest m) on every
// DOM of the column near the expected time is stored in tree "muons".
// The middle DOMs take no part in the selection, so their multiplicity is an unbiased muon sample.

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "TChain.h"
#include "TFile.h"
#include "TH1D.h"
#include "TNamed.h"
#include "TParameter.h"
#include "TSystem.h"
#include "TTree.h"

#include "../include/Constants.h"
#include "../include/Detx.h"
#include "../include/DomMap.h"
#include "../include/L1Tree.h"
#include "../include/plotting/MuonPlots.h"

using namespace END;

struct L1Window {
  double t0;
  int m;
  Long64_t entry;
};

const int kMaxColumn = 64; // max DOMs in a column

// Best window (largest m, at least min_m) starting within half_width of t; -1 if none.
int BestWindow(const std::vector<L1Window>& w, double t, double half_width, int min_m)
{
  int best = -1;
  auto it = std::lower_bound(w.begin(), w.end(), t - half_width,
                             [](const L1Window& a, double x){ return a.t0 < x; });
  for(; it != w.end() && it->t0 <= t + half_width; ++it)
    if(it->m >= min_m && (best < 0 || it->m > w[best].m)) best = it - w.begin();
  return best;
}

void MuonTag(std::string pattern, std::string detx, std::string out_dir = ".", int tag_m = 4,
             double half_width_ns = 40., double time_cut_ms = 20.)
{
  ReadL1Settings(pattern); // exits if no files match
  std::map<int, DetxDom> doms = ReadDetx(detx);
  if(doms.empty()){
    std::cerr << "Couldn't read " << detx << "! Exiting!" << std::endl;
    std::exit(1);
  }
  const double time_cut_ns = time_cut_ms * 1e6;

  // columns: DOMs at the same x, y (to 1 cm), lowest first
  std::map<std::pair<long, long>, std::vector<int>> by_position;
  for(auto& d : doms) by_position[std::make_pair(std::lround(d.second.x * 100), std::lround(d.second.y * 100))].push_back(d.first);
  std::vector<std::vector<int>> columns;
  for(auto& c : by_position){
    std::sort(c.second.begin(), c.second.end(), [&doms](int a, int b){ return doms[a].z < doms[b].z; });
    if(c.second.size() >= 2) columns.push_back(c.second);
  }
  std::map<int, int> top_of; // top DOM id -> column
  for(size_t c = 0; c < columns.size(); c++) top_of[columns[c].back()] = c;

  // livetime of each column's top DOM
  TChain frames("frames");
  frames.Add(pattern.c_str());
  int f_dom_id;
  double frame_ns;
  frames.SetBranchAddress("dom_id", &f_dom_id);
  frames.SetBranchAddress("frame_ns", &frame_ns);
  std::map<int, double> livetime_s;
  for(Long64_t i = 0; i < frames.GetEntries(); i++){
    frames.GetEntry(i);
    if(!top_of.count(f_dom_id)) continue;
    livetime_s[top_of[f_dom_id]] += 1e-9 * (time_cut_ns > 0 ? std::min(time_cut_ns, frame_ns) : frame_ns);
  }

  // input
  TChain l1("l1");
  l1.Add(pattern.c_str());
  int run, frame, dom_id, m;
  unsigned int utc_s;
  double t0;
  l1.SetBranchStatus("*", 0);
  for(const char* b : {"run", "frame", "utc_s", "dom_id", "t0", "m"}) l1.SetBranchStatus(b, 1);
  l1.SetBranchAddress("run", &run);
  l1.SetBranchAddress("frame", &frame);
  l1.SetBranchAddress("utc_s", &utc_s);
  l1.SetBranchAddress("dom_id", &dom_id);
  l1.SetBranchAddress("t0", &t0);
  l1.SetBranchAddress("m", &m);

  // output
  gSystem->mkdir(out_dir.c_str(), true);
  TFile fout((out_dir + "/muons.root").c_str(), "RECREATE");
  int o_run, o_frame, o_du, ndom, o_dom_id[kMaxColumn], o_dom[kMaxColumn], o_m[kMaxColumn];
  unsigned int o_utc_s;
  double t_top, o_dt[kMaxColumn];
  Long64_t o_entry[kMaxColumn];
  TTree* muons = new TTree("muons", "tagged muons, DOM arrays lowest first");
  muons->Branch("run", &o_run, "run/I");
  muons->Branch("frame", &o_frame, "frame/I");
  muons->Branch("utc_s", &o_utc_s, "utc_s/i");
  muons->Branch("du", &o_du, "du/I");
  muons->Branch("t_top", &t_top, "t_top/D");
  muons->Branch("ndom", &ndom, "ndom/I");
  muons->Branch("dom_id", o_dom_id, "dom_id[ndom]/I");
  muons->Branch("dom", o_dom, "dom[ndom]/I");
  muons->Branch("m", o_m, "m[ndom]/I");
  muons->Branch("dt", o_dt, "dt[ndom]/D");
  muons->Branch("entry", o_entry, "entry[ndom]/L");

  const double dz = doms[columns[0].back()].z - doms[columns[0].front()].z;
  const double expected_tb = dz / kLightSpeed;
  TH1D h_dt("dt_top_bottom", Form(";t_{bottom} - t_{top} [ns] (both m #geq %d);window pairs / 10 ns", tag_m), 100, -500, 500);
  TH1D h_probe("probe_multiplicity", ";middle DOM multiplicity (0 = no L1 window);tags", kNPMT + 1, -0.5, kNPMT + 0.5);
  h_dt.SetDirectory(nullptr);
  h_probe.SetDirectory(nullptr);

  // process one timeslice at a time
  std::map<int, std::vector<L1Window>> windows; // dom_id -> windows in time order
  auto processFrame = [&](int this_run, int this_frame, unsigned int this_utc_s){
    for(const std::vector<int>& column : columns){
      const double z_top = doms[column.back()].z;
      const std::vector<L1Window>& top = windows[column.back()];
      const std::vector<L1Window>& bottom = windows[column.front()];

      for(const L1Window& w : top){
        if(w.m < tag_m || (time_cut_ns > 0 && w.t0 >= time_cut_ns)) continue;
        for(const L1Window& b : bottom)
          if(b.m >= tag_m && std::abs(b.t0 - w.t0) < 500) h_dt.Fill(b.t0 - w.t0);
        if(BestWindow(bottom, w.t0 + (z_top - doms[column.front()].z) / kLightSpeed, half_width_ns, tag_m) < 0) continue;

        o_run = this_run; o_frame = this_frame; o_utc_s = this_utc_s; o_du = DomDU(column.back()); t_top = w.t0;
        ndom = std::min((int)column.size(), kMaxColumn);
        for(int f = 0; f < ndom; f++){
          const std::vector<L1Window>& wf = windows[column[f]];
          const double expected = w.t0 + (z_top - doms[column[f]].z) / kLightSpeed;
          const int k = BestWindow(wf, expected, half_width_ns, 1);
          o_dom_id[f] = column[f];
          o_dom[f] = DomNumber(column[f]);
          o_m[f] = (k < 0) ? 0 : wf[k].m;
          o_dt[f] = (k < 0) ? 0 : wf[k].t0 - expected;
          o_entry[f] = (k < 0) ? -1 : wf[k].entry;
          if(f > 0 && f < ndom - 1) h_probe.Fill(o_m[f]);
        }
        muons->Fill();
      }
    }
    windows.clear();
  };

  int last_run = -1, last_frame = -1;
  unsigned int last_utc_s = 0;
  const Long64_t n = l1.GetEntries();
  for(Long64_t i = 0; i < n; i++){
    l1.GetEntry(i);
    if(i % 10000000 == 0) std::cout << "L1 window " << i << " / " << n << std::endl;
    if((run != last_run || frame != last_frame) && last_run >= 0)
      processFrame(last_run, last_frame, last_utc_s);
    last_run = run; last_frame = frame; last_utc_s = utc_s;
    windows[dom_id].push_back({t0, m, i});
  }
  processFrame(last_run, last_frame, last_utc_s);

  // summary
  double total_livetime = 0;
  for(auto& l : livetime_s) total_livetime += l.second;
  std::cout << "Tags : " << muons->GetEntries() << ", " << muons->GetEntries() / total_livetime << " Hz per column" << std::endl;

  DrawTopBottomDt(h_dt, expected_tb, half_width_ns, out_dir + "/dt_top_bottom.png");
  DrawProbeMultiplicity(h_probe, out_dir + "/probe_multiplicity.png");

  h_dt.Write();
  h_probe.Write();
  TParameter<double>("livetime_per_column_s", total_livetime / livetime_s.size()).Write();
  TParameter<int>("tag_m", tag_m).Write();
  TParameter<double>("half_width_ns", half_width_ns).Write();
  TNamed("l1_pattern", pattern.c_str()).Write();
  fout.Write();
  fout.Close();
  std::cout << "Wrote " << out_dir << "/{muons.root, dt_top_bottom.png, probe_multiplicity.png}" << std::endl;
}
