#ifndef DiagnosticPlots_h
#define DiagnosticPlots_h

#pragma once

#include <string>

#include "TCanvas.h"
#include "TH1D.h"
#include "TStyle.h"

namespace END {

  // One bar per DOM (bin labels), e.g. the fraction of UDP packets dropped.
  inline void DrawPerDom(const TH1D& h, const std::string& png)
  {
    gStyle->SetOptStat(0);
    TCanvas c("c_dom", "", 1200, 700);
    c.SetBottomMargin(0.18);
    c.SetGridy();
    TH1D d(h);
    d.SetFillColor(kAzure + 2);
    d.SetLineColor(kAzure + 2);
    d.SetBarWidth(0.8);
    d.SetBarOffset(0.1);
    d.SetMinimum(0);
    d.GetXaxis()->LabelsOption("v");
    d.GetXaxis()->SetLabelSize(0.03);
    d.Draw("BAR");
    c.Print(png.c_str());
  }

  // A quantity summed over DOMs vs time in the run.
  inline void DrawVsTime(const TH1D& h, const std::string& png)
  {
    gStyle->SetOptStat(0);
    TCanvas c("c_time", "", 1200, 600);
    c.SetGridy();
    TH1D d(h);
    d.SetLineColor(kAzure + 2);
    d.SetLineWidth(2);
    d.SetMinimum(0);
    d.Draw("HIST");
    c.Print(png.c_str());
  }

}

#endif
