// Coincidence multiplicity spectrum and PMT-vs-multiplicity map from L1 trees.
//
// Only hits in the first time_cut_ms of each frame are used (<= 0: whole frame); with
// hrv_cut the frame also ends at its high-rate veto. Rates are per DOM: counts / summed livetime.
// A window_ns shorter than the tree's re-counts PMTs within window_ns of each window's first hit.

#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include <ROOT/RDataFrame.hxx>
#include "TChain.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TParameter.h"
#include "TSystem.h"

#include "../include/Constants.h"
#include "../include/L1Tree.h"
#include "../include/plotting/SpectrumPlots.h"

using namespace ROOT;
using namespace ROOT::VecOps;
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
  if(detx.empty()) detx = settings.detx;
  const double time_cut_ns = time_cut_ms * 1e6;

  TChain frames_chain("frames"), l1_chain("l1");
  frames_chain.Add(pattern.c_str());
  l1_chain.Add(pattern.c_str());
  RDataFrame frames(frames_chain), l1(l1_chain);

  // usable time of each frame
  auto limit = [time_cut_ns, hrv_cut](double frame_ns, double t_hrv_cut){
    double lim = (time_cut_ns > 0) ? std::min(time_cut_ns, frame_ns) : frame_ns;
    return hrv_cut ? std::min(lim, t_hrv_cut) : lim;
  };
  // m = 1 windows in the usable time; n_single is counted in kNSlices parts of the frame
  auto singles = [](const RVec<int>& n_single, double frame_ns, double lim){
    const double width = frame_ns / kNSlices;
    double n = 0;
    for(int k = 0; k < kNSlices; k++)
      n += n_single[k] * std::max(0., std::min(1., (lim - k * width) / width));
    return n;
  };
  auto f_frames = frames.Define("limit", limit, {"frame_ns", "t_hrv_cut"})
                        .Define("n1", singles, {"n_single", "frame_ns", "limit"});
  auto r_livetime = f_frames.Sum<double>("limit");
  auto r_singles = f_frames.Sum<double>("n1");
  auto r_run = f_frames.Take<int>("run");
  auto r_frame = f_frames.Take<int>("frame");
  auto r_dom = f_frames.Take<int>("dom_id");
  auto r_limit = f_frames.Take<double>("limit");

  std::map<FrameKey, double> limits;
  for(size_t i = 0; i < r_limit->size(); i++)
    limits[FrameKey((*r_run)[i], (*r_frame)[i], (*r_dom)[i])] = (*r_limit)[i];
  const double livetime_s = *r_livetime * 1e-9;
  std::cout << "Frames : " << r_limit->size() << ", livetime summed over DOMs : " << livetime_s << " s" << std::endl;

  // windows inside the usable time, PMTs within window_ns of the first hit
  auto inTime = [&limits](int run, int frame, int dom_id, double t0){
    return t0 < limits[FrameKey(run, frame, dom_id)];
  };
  const double tree_window = settings.window_ns;
  auto pmtMask = [window_ns, tree_window](unsigned int pmt_mask, const RVec<unsigned char>& ch, const RVec<float>& t){
    if(window_ns >= tree_window) return pmt_mask;
    unsigned int mask = 0;
    for(size_t i = 0; i < ch.size(); i++)
      if(t[i] <= window_ns) mask |= (1u << ch[i]);
    return mask;
  };
  const std::vector<std::pair<int, std::string>> rows = PmtRows(detx);
  auto pmtRows = [&rows](unsigned int mask){
    RVec<int> r;
    for(int ch = 0; ch < kNPMT; ch++)
      if(mask & (1u << ch)) r.push_back(rows[ch].first);
    return r;
  };
  auto repeat = [](int mult, const RVec<int>& r){ return RVec<int>(r.size(), mult); };

  auto f_l1 = l1.Filter(inTime, {"run", "frame", "dom_id", "t0"})
                .Define("mask", pmtMask, {"pmt_mask", "hit_ch", "hit_t"})
                .Define("mult", [](unsigned int mask){ return __builtin_popcount(mask); }, {"mask"})
                .Define("rows", pmtRows, {"mask"})
                .Define("mult_rows", repeat, {"mult", "rows"});

  const std::string xtitle = Form("coincidence multiplicity (distinct PMTs within %.0f ns)", window_ns);
  auto r_mult = f_l1.Histo1D({"windows", (";" + xtitle + ";windows").c_str(), kNPMT, 0.5, kNPMT + 0.5}, "mult");
  auto r_pmt = f_l1.Histo2D({"pmt_vs_multiplicity", (";" + xtitle + ";PMT channel (dz)").c_str(),
                             kNPMT, 0.5, kNPMT + 0.5, kNPMT, 0.5, kNPMT + 0.5}, "mult_rows", "rows");

  TH1D windows(*r_mult);
  windows.AddBinContent(1, *r_singles);
  TH2D pmt(*r_pmt);
  for(int ch = 0; ch < kNPMT; ch++)
    pmt.GetYaxis()->SetBinLabel(rows[ch].first, rows[ch].second.c_str());

  TH1D rate(windows);
  rate.SetName("rate");
  rate.Scale(1. / livetime_s);
  rate.GetYaxis()->SetTitle("rate per DOM [Hz]");

  // plots and output
  gSystem->mkdir(out_dir.c_str(), true);
  const std::string cut = (time_cut_ms > 0 ? Form("first %.0f ms of each timeslice", time_cut_ms) : "whole timeslice")
                          + std::string(hrv_cut ? ", before high-rate veto" : "");
  DrawMultiplicity(rate, Form("%.0f ns windows, %s", window_ns, cut.c_str()), out_dir + "/multiplicity.png", sum_from);
  DrawPmtVsMultiplicity(pmt, windows, Form("%.0f ns windows, %s", window_ns, cut.c_str()), out_dir + "/pmt_vs_multiplicity.png");

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
