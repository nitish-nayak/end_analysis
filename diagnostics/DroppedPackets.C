// Dropped UDP packets per DOM and vs time, from the "frames" tree of MakeL1Tree.C.
// A frame's dropped packets = expected - received; frames without a UDP trailer are counted separately.
// Missing frames: timeslices in which a DOM sent no frame at all.

#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <string>

#include "TChain.h"
#include "TFile.h"
#include "TH1D.h"
#include "TSystem.h"

#include "../include/plotting/DiagnosticPlots.h"

using namespace END;

void DroppedPackets(std::string pattern, std::string out_dir = ".")
{
  TChain frames("frames");
  if(frames.Add(pattern.c_str()) == 0){
    std::cerr << "Couldn't find files matching " << pattern << "! Exiting!" << std::endl;
    std::exit(1);
  }
  int run, frame, dom_id, du, dom, udp_recv, udp_expected;
  unsigned int utc_s;
  frames.SetBranchStatus("*", 0);
  for(const char* b : {"run", "frame", "dom_id", "du", "dom", "utc_s", "udp_recv", "udp_expected"}) frames.SetBranchStatus(b, 1);
  frames.SetBranchAddress("run", &run);
  frames.SetBranchAddress("frame", &frame);
  frames.SetBranchAddress("dom_id", &dom_id);
  frames.SetBranchAddress("du", &du);
  frames.SetBranchAddress("dom", &dom);
  frames.SetBranchAddress("utc_s", &utc_s);
  frames.SetBranchAddress("udp_recv", &udp_recv);
  frames.SetBranchAddress("udp_expected", &udp_expected);

  // per DOM sums, and dropped packets per second of the run
  struct Counts { double frames = 0, no_trailer = 0, bad_frames = 0, expected = 0, dropped = 0; std::string label; };
  std::map<int, Counts> per_dom;
  std::map<unsigned int, double> per_second;
  std::set<std::pair<int, int>> timeslices;
  const Long64_t n = frames.GetEntries();
  for(Long64_t i = 0; i < n; i++){
    frames.GetEntry(i);
    timeslices.insert(std::make_pair(run, frame));
    Counts& c = per_dom[dom_id];
    c.label = (du >= 0) ? Form("DU%d DOM%d", du, dom) : Form("%d", dom_id);
    c.frames++;
    if(udp_expected < 0){
      c.no_trailer++;
      continue;
    }
    const int dropped = std::max(0, udp_expected - udp_recv);
    c.expected += udp_expected;
    c.dropped += dropped;
    if(dropped > 0) c.bad_frames++;
    per_second[utc_s] += dropped;
  }

  TH1D h_frac("dropped_fraction", ";;fraction of UDP packets dropped", per_dom.size(), 0, per_dom.size());
  TH1D h_bad("frames_with_drops", ";;fraction of frames with dropped packets", per_dom.size(), 0, per_dom.size());
  int bin = 1;
  for(auto& d : per_dom){
    h_frac.GetXaxis()->SetBinLabel(bin, d.second.label.c_str());
    h_bad.GetXaxis()->SetBinLabel(bin, d.second.label.c_str());
    h_frac.SetBinContent(bin, d.second.expected > 0 ? d.second.dropped / d.second.expected : 0);
    h_bad.SetBinContent(bin, d.second.frames > 0 ? d.second.bad_frames / d.second.frames : 0);
    std::cout << d.second.label << " : " << d.second.dropped << " / " << d.second.expected << " packets dropped, "
              << d.second.bad_frames << " / " << d.second.frames << " frames with drops, "
              << d.second.no_trailer << " frames without UDP trailer, "
              << timeslices.size() - d.second.frames << " missing frames" << std::endl;
    bin++;
  }

  const unsigned int t_first = per_second.empty() ? 0 : per_second.begin()->first;
  const unsigned int t_last = per_second.empty() ? 1 : per_second.rbegin()->first + 1;
  TH1D h_time("dropped_vs_time", Form(";seconds since UTC %u;dropped packets per second (all DOMs)", t_first),
              t_last - t_first, 0, t_last - t_first);
  for(auto& s : per_second) h_time.Fill(s.first - t_first + 0.5, s.second);

  gSystem->mkdir(out_dir.c_str(), true);
  DrawPerDom(h_frac, out_dir + "/dropped_packets_per_dom.png");
  DrawPerDom(h_bad, out_dir + "/frames_with_drops_per_dom.png");
  DrawVsTime(h_time, out_dir + "/dropped_packets_vs_time.png");

  TFile fout((out_dir + "/dropped_packets.root").c_str(), "RECREATE");
  h_frac.Write();
  h_bad.Write();
  h_time.Write();
  fout.Close();
  std::cout << "Wrote " << out_dir << "/dropped_packets*.png, frames_with_drops_per_dom.png, dropped_packets.root" << std::endl;
}
