#ifndef SpectrumPlots_h
#define SpectrumPlots_h

#pragma once

#include <cmath>
#include <string>

#include "TCanvas.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TLine.h"
#include "TPaveText.h"
#include "TStyle.h"

namespace END {

  // Rate per DOM vs multiplicity, with the summed rate from sum_from upwards.
  inline void DrawMultiplicity(const TH1D& rate, const std::string& title, const std::string& png, int sum_from = 6)
  {
    gStyle->SetOptStat(0);
    TCanvas c("c_mult", "", 1000, 700);
    c.SetLogy();
    c.SetGridy();

    TH1D h(rate);
    h.SetTitle(title.c_str());
    h.SetLineWidth(2);
    h.SetLineColor(kAzure + 2);
    h.GetXaxis()->SetNdivisions(h.GetNbinsX() + 1);
    h.GetXaxis()->SetLabelSize(0.03);
    h.Draw("HIST");

    const int b = h.FindBin(sum_from);
    const double sum = h.Integral(b, h.GetNbinsX());
    c.Update();
    const double x = h.GetXaxis()->GetBinLowEdge(b);
    TLine line(x, std::pow(10, c.GetUymin()), x, std::pow(10, c.GetUymax()));
    line.SetLineColor(kRed);
    line.SetLineStyle(2);
    line.SetLineWidth(2);
    line.Draw();

    TPaveText label(0.55, 0.80, 0.88, 0.87, "NDC");
    label.SetFillColor(kWhite);
    label.SetBorderSize(1);
    label.SetTextSize(0.035);
    label.AddText(Form("#geq %d coincidences: %.2f Hz", sum_from, sum));
    label.Draw();

    c.Print(png.c_str());
  }

  // PMT (rows, most upward-facing at the top) vs multiplicity. With probability = true each
  // column is divided by the number of windows of that multiplicity: the chance a PMT fired.
  inline void DrawPmtVsMultiplicity(const TH2D& counts, const TH1D& windows, const std::string& title,
                                    const std::string& png, bool probability = true)
  {
    gStyle->SetOptStat(0);
    gStyle->SetPalette(kViridis);
    TCanvas c("c_pmt", "", 1000, 900);
    c.SetLeftMargin(0.14);
    c.SetRightMargin(0.17);

    TH2D h(counts);
    h.SetTitle(title.c_str());
    if(probability){
      for(int bx = 1; bx <= h.GetNbinsX(); bx++){
        const double n = windows.GetBinContent(bx);
        for(int by = 1; by <= h.GetNbinsY(); by++)
          h.SetBinContent(bx, by, n > 0 ? h.GetBinContent(bx, by) / n : 0.);
      }
      h.GetZaxis()->SetTitle("probability PMT fired");
    }
    else
      c.SetLogz();

    // start at the lowest multiplicity with PMT information
    int first = 1;
    while(first < h.GetNbinsX() && h.Integral(first, first, 1, h.GetNbinsY()) == 0) first++;
    h.GetXaxis()->SetRange(first, h.GetNbinsX());
    h.GetXaxis()->SetNdivisions(h.GetNbinsX() - first + 2);
    h.GetXaxis()->SetLabelSize(0.025);
    h.GetYaxis()->SetLabelSize(0.022);
    h.GetYaxis()->SetTitleOffset(1.9);
    h.GetZaxis()->SetTitleOffset(1.4);
    h.Draw("COLZ");

    c.Print(png.c_str());
  }

}

#endif
