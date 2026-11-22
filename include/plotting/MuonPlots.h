#ifndef MuonPlots_h
#define MuonPlots_h

#pragma once

#include <string>

#include "TCanvas.h"
#include "TH1D.h"
#include "TLine.h"
#include "TStyle.h"

namespace END {

  // Time difference between bottom and top DOM windows, with the tag window marked.
  inline void DrawTopBottomDt(const TH1D& dt, double expected_ns, double half_width_ns, const std::string& png)
  {
    gStyle->SetOptStat(0);
    TCanvas c("c_dt", "", 1000, 700);
    TH1D h(dt);
    h.SetLineWidth(2);
    h.SetLineColor(kAzure + 2);
    h.SetMinimum(0);
    h.Draw("HIST");
    c.Update();

    for(double x : {expected_ns - half_width_ns, expected_ns + half_width_ns}){
      TLine* line = new TLine(x, 0, x, c.GetUymax());
      line->SetLineColor(kRed);
      line->SetLineStyle(2);
      line->SetLineWidth(2);
      line->Draw();
    }
    c.Print(png.c_str());
  }

  // Multiplicity of the middle DOM(s) for tagged muons, as a fraction of tags.
  inline void DrawProbeMultiplicity(const TH1D& probe, const std::string& png)
  {
    gStyle->SetOptStat(0);
    TCanvas c("c_probe", "", 1000, 700);
    c.SetLogy();
    c.SetGridy();
    TH1D h(probe);
    if(h.Integral() > 0) h.Scale(1. / h.Integral());
    h.GetYaxis()->SetTitle("fraction of tags");
    h.SetLineWidth(2);
    h.SetLineColor(kAzure + 2);
    h.GetXaxis()->SetNdivisions(h.GetNbinsX() + 1);
    h.GetXaxis()->SetLabelSize(0.03);
    h.Draw("HIST");
    c.Print(png.c_str());
  }

}

#endif
