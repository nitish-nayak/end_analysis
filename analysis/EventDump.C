// Writes tagged muons (from MuonTag.C) with the detector geometry to JSON for the event display:
// the n_events/2 brightest (summed multiplicity) and n_events/2 closest to the median.
// Hits are taken from all L1 windows in the same timeslice, from pre_ns before to post_ns
// after the top DOM's window. The L1 pattern must be the one MuonTag.C ran on.

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "TChain.h"
#include "TFile.h"
#include "TTree.h"

#include "../include/Constants.h"
#include "../include/Detx.h"
#include "../include/DomMap.h"
#include "../include/L1Tree.h"

using namespace END;

void EventDump(std::string muons_file, std::string pattern, std::string json, int n_events = 8,
               double pre_ns = 100., double post_ns = 250., std::string detx = "")
{
  const L1Settings settings = ReadL1Settings(pattern);
  if(detx.empty()) detx = settings.detx;
  std::map<int, DetxDom> doms = ReadDetx(detx);
  if(doms.empty()){
    std::cerr << "Event dump needs a detx! Exiting!" << std::endl;
    std::exit(1);
  }

  // tagged muons, ranked by summed multiplicity
  TFile* fmu = TFile::Open(muons_file.c_str(), "read");
  TTree* muons = (TTree*)fmu->Get("muons");
  int run, frame, du, ndom, dom_id[64], m[64];
  double t_top;
  Long64_t entry[64];
  muons->SetBranchAddress("run", &run);
  muons->SetBranchAddress("frame", &frame);
  muons->SetBranchAddress("du", &du);
  muons->SetBranchAddress("t_top", &t_top);
  muons->SetBranchAddress("ndom", &ndom);
  muons->SetBranchAddress("dom_id", dom_id);
  muons->SetBranchAddress("m", m);
  muons->SetBranchAddress("entry", entry);

  std::vector<std::pair<int, Long64_t>> ranked; // (summed m, muon entry)
  std::vector<std::tuple<int, int, double>> when; // (run, frame, t_top) per muon entry
  for(Long64_t i = 0; i < muons->GetEntries(); i++){
    muons->GetEntry(i);
    int sum = 0;
    for(int f = 0; f < ndom; f++) sum += m[f];
    ranked.push_back(std::make_pair(sum, i));
    when.push_back(std::make_tuple(run, frame, t_top));
  }
  std::sort(ranked.rbegin(), ranked.rend());

  // brightest, then from the median down; a muon seen on several columns is taken once
  std::vector<Long64_t> chosen;
  auto choose = [&](size_t from, size_t n){
    for(size_t i = from; i < ranked.size() && n > 0; i++){
      const auto& w = when[ranked[i].second];
      bool seen = false;
      for(Long64_t c : chosen)
        seen |= std::get<0>(when[c]) == std::get<0>(w) && std::get<1>(when[c]) == std::get<1>(w)
                && std::abs(std::get<2>(when[c]) - std::get<2>(w)) < 1000;
      if(!seen){ chosen.push_back(ranked[i].second); n--; }
    }
  };
  choose(0, n_events / 2);
  choose(ranked.size() / 2, n_events - n_events / 2);

  // geometry
  std::vector<int> dom_ids;
  std::map<int, int> dom_index;
  for(auto& d : doms){
    dom_index[d.first] = dom_ids.size();
    dom_ids.push_back(d.first);
  }
  FILE* out = fopen(json.c_str(), "w");
  fprintf(out, "{\n\"doms\": [\n");
  for(size_t i = 0; i < dom_ids.size(); i++){
    const DetxDom& d = doms[dom_ids[i]];
    fprintf(out, " {\"id\": %d, \"du\": %d, \"dom\": %d, \"pos\": [%.3f, %.3f, %.3f], \"pmts\": [",
            dom_ids[i], DomDU(dom_ids[i]), DomNumber(dom_ids[i]), d.x, d.y, d.z);
    for(size_t p = 0; p < d.pmts.size(); p++){
      const DetxPmt& q = d.pmts[p];
      fprintf(out, "%s[%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d]", p ? "," : "", q.x, q.y, q.z, q.dx, q.dy, q.dz, q.status);
    }
    fprintf(out, "]}%s\n", i + 1 < dom_ids.size() ? "," : "");
  }
  fprintf(out, "],\n\"events\": [\n");

  // hits around each muon: windows of one timeslice are consecutive entries in the L1 tree
  TChain l1("l1");
  l1.Add(pattern.c_str());
  int l_run, l_frame, l_dom_id, nhit;
  double t0;
  unsigned char hit_ch[kMaxHits], hit_tot[kMaxHits];
  float hit_t[kMaxHits];
  l1.SetBranchAddress("run", &l_run);
  l1.SetBranchAddress("frame", &l_frame);
  l1.SetBranchAddress("dom_id", &l_dom_id);
  l1.SetBranchAddress("t0", &t0);
  l1.SetBranchAddress("nhit", &nhit);
  l1.SetBranchAddress("hit_ch", hit_ch);
  l1.SetBranchAddress("hit_t", hit_t);
  l1.SetBranchAddress("hit_tot", hit_tot);

  for(size_t e = 0; e < chosen.size(); e++){
    muons->GetEntry(chosen[e]);
    Long64_t first = entry[ndom - 1];
    l1.GetEntry(first);
    if(l_run != run || l_frame != frame){
      std::cerr << "Muon file doesn't match the L1 files! Exiting!" << std::endl;
      std::exit(1);
    }
    while(first > 0){
      l1.GetEntry(first - 1);
      if(l_run != run || l_frame != frame) break;
      first--;
    }

    // DOMs of the tagged column, top first
    fprintf(out, " {\"run\": %d, \"timeslice\": %d, \"du\": %d, \"doms\": [", run, frame, du);
    for(int f = ndom - 1; f >= 0; f--) fprintf(out, "%s%d", f < ndom - 1 ? ", " : "", dom_index[dom_id[f]]);
    fprintf(out, "], \"hits\": [");

    int written = 0;
    for(Long64_t i = first; i < l1.GetEntries(); i++){
      l1.GetEntry(i);
      if(l_run != run || l_frame != frame) break;
      if(!dom_index.count(l_dom_id) || t0 + settings.window_ns < t_top - pre_ns || t0 > t_top + post_ns) continue;
      for(int h = 0; h < nhit; h++){
        const double t = t0 + hit_t[h] - t_top;
        if(t < -pre_ns || t > post_ns) continue;
        fprintf(out, "%s[%d,%d,%.1f,%d]", written ? "," : "", dom_index[l_dom_id], hit_ch[h], t, hit_tot[h]);
        written++;
      }
    }
    fprintf(out, "]}%s\n", e + 1 < chosen.size() ? "," : "");
    std::cout << "Event " << e + 1 << " : run " << run << " timeslice " << frame << " DU " << du << " DOM " << DomNumber(dom_id[ndom - 1]) << ", " << written << " hits" << std::endl;
  }
  fprintf(out, "]\n}\n");
  fclose(out);
  fmu->Close();
  std::cout << "Wrote " << json << std::endl;
}
