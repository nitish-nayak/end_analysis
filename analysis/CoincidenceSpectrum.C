// Coincidence multiplicity spectrum and PMT-vs-multiplicity map from L1 trees.
//
// Only hits in the first time_cut_ms of each frame are used (<= 0: whole frame); with
// hrv_cut the frame also ends at its high-rate veto. Rates are per DOM: counts / summed livetime.
// A window_ns shorter than the tree's re-counts PMTs within window_ns of each window's first hit.
// The detx (optional) orders the PMT rows by direction; without it they are in channel order.

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "TChain.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TParameter.h"
#include "TSystem.h"

#include "../include/Constants.h"
#include "../include/L1Tree.h"
#include "../include/plotting/SpectrumPlots.h"

using namespace END;

typedef std::tuple<int, int, int> FrameKey; // run, frame, dom_id

void CoincidenceSpectrum(std::string pattern, std::string out_dir = ".", double window_ns = 100.,
                         double time_cut_ms = 20., bool hrv_cut = false, std::string detx = "",
                         int sum_from = 6)
{
  const L1Settings settings = ReadL1Settings(pattern);
  if(window_ns > settings.window_ns){
    std::cerr << "Tree was made with " << settings.window_ns << " ns windows, can't use " << window_ns << "! Exiting!" << std::endl;
    std::exit(1);
  }
  const double time_cut_ns = time_cut_ms * 1e6;

  const std::string xtitle = Form("coincidence multiplicity (distinct PMTs within %.0f ns)", window_ns);
  TH1D windows("windows", (";" + xtitle + ";windows").c_str(), kNPMT, 0.5, kNPMT + 0.5);
  TH2D pmt("pmt_vs_multiplicity", (";" + xtitle + ";PMT channel (dz)").c_str(), kNPMT, 0.5, kNPMT + 0.5, kNPMT, 0.5, kNPMT + 0.5);
  windows.SetDirectory(nullptr);
  pmt.SetDirectory(nullptr);
  const std::vector<std::pair<int, std::string>> rows = PmtRows(detx);
  for(int ch = 0; ch < kNPMT; ch++)
    pmt.GetYaxis()->SetBinLabel(rows[ch].first, rows[ch].second.c_str());

  // frames: usable time of each frame, livetime, and m = 1 windows in the usable time
  TChain frames("frames");
  frames.Add(pattern.c_str());
  int run, frame, dom_id, n_single[kNSlices];
  double frame_ns, t_hrv_cut;
  frames.SetBranchStatus("*", 0);
  for(const char* b : {"run", "frame", "dom_id", "n_single", "frame_ns", "t_hrv_cut"}) frames.SetBranchStatus(b, 1);
  frames.SetBranchAddress("run", &run);
  frames.SetBranchAddress("frame", &frame);
  frames.SetBranchAddress("dom_id", &dom_id);
  frames.SetBranchAddress("n_single", n_single);
  frames.SetBranchAddress("frame_ns", &frame_ns);
  frames.SetBranchAddress("t_hrv_cut", &t_hrv_cut);

  std::map<FrameKey, double> limits;
  double livetime_s = 0;
  for(Long64_t i = 0; i < frames.GetEntries(); i++){
    frames.GetEntry(i);
    double limit = (time_cut_ns > 0) ? std::min(time_cut_ns, frame_ns) : frame_ns;
    if(hrv_cut) limit = std::min(limit, t_hrv_cut);
    limits[FrameKey(run, frame, dom_id)] = limit;
    livetime_s += limit * 1e-9;

    // n_single is counted in kNSlices parts of the frame; take the part before the limit
    const double width = frame_ns / kNSlices;
    for(int k = 0; k < kNSlices; k++)
      windows.Fill(1, n_single[k] * std::max(0., std::min(1., (limit - k * width) / width)));
  }
  std::cout << "Frames : " << limits.size() << ", livetime summed over DOMs : " << livetime_s << " s" << std::endl;

  // L1 windows inside the usable time
  TChain l1("l1");
  l1.Add(pattern.c_str());
  int nhit;
  unsigned int pmt_mask;
  double t0;
  unsigned char hit_ch[kMaxHits];
  float hit_t[kMaxHits];
  const bool recount = window_ns < settings.window_ns;
  l1.SetBranchStatus("*", 0);
  for(const char* b : {"run", "frame", "dom_id", "t0", "pmt_mask"}) l1.SetBranchStatus(b, 1);
  if(recount) for(const char* b : {"nhit", "hit_ch", "hit_t"}) l1.SetBranchStatus(b, 1);
  l1.SetBranchAddress("run", &run);
  l1.SetBranchAddress("frame", &frame);
  l1.SetBranchAddress("dom_id", &dom_id);
  l1.SetBranchAddress("t0", &t0);
  l1.SetBranchAddress("pmt_mask", &pmt_mask);
  if(recount){
    l1.SetBranchAddress("nhit", &nhit);
    l1.SetBranchAddress("hit_ch", hit_ch);
    l1.SetBranchAddress("hit_t", hit_t);
  }

  const Long64_t n = l1.GetEntries();
  for(Long64_t i = 0; i < n; i++){
    l1.GetEntry(i);
    if(i % 10000000 == 0) std::cout << "L1 window " << i << " / " << n << std::endl;
    if(t0 >= limits[FrameKey(run, frame, dom_id)]) continue;

    unsigned int mask = pmt_mask;
    if(recount){
      mask = 0;
      for(int h = 0; h < nhit; h++)
        if(hit_t[h] <= window_ns) mask |= (1u << hit_ch[h]);
    }
    const int m = __builtin_popcount(mask);
    windows.Fill(m);
    for(int ch = 0; ch < kNPMT; ch++)
      if(mask & (1u << ch)) pmt.Fill(m, rows[ch].first);
  }

  TH1D rate(windows);
  rate.SetName("rate");
  rate.Scale(1. / livetime_s);
  rate.GetYaxis()->SetTitle("rate per DOM [Hz]");

  // plots and output
  gSystem->mkdir(out_dir.c_str(), true);
  const std::string cut = (time_cut_ms > 0 ? Form("first %.0f ms of each timeslice", time_cut_ms) : "whole timeslice")
                          + std::string(hrv_cut ? ", before high-rate veto" : "");
  const std::string title = Form("%.0f ns windows, %s", window_ns, cut.c_str());
  DrawMultiplicity(rate, title, out_dir + "/multiplicity.png", sum_from);
  DrawPmtVsMultiplicity(pmt, windows, title, out_dir + "/pmt_vs_multiplicity.png");

  TFile fout((out_dir + "/coincidence_spectrum.root").c_str(), "RECREATE");
  windows.Write();
  rate.Write();
  pmt.Write();
  TParameter<double>("livetime_s", livetime_s).Write();
  TParameter<double>("window_ns", window_ns).Write();
  fout.Close();

  std::cout << "Rate >= " << sum_from << " : " << rate.Integral(sum_from, kNPMT) << " Hz per DOM" << std::endl;
  std::cout << "Wrote " << out_dir << "/{multiplicity.png, pmt_vs_multiplicity.png, coincidence_spectrum.root}" << std::endl;
}
