#ifndef L1Tree_h
#define L1Tree_h

#pragma once

// Settings stored with the L1 tree, and PMT ordering for plots.

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "TChain.h"
#include "TFile.h"
#include "TParameter.h"

#include "Constants.h"
#include "Detx.h"

namespace END {

  struct L1Settings {
    double window_ns;
    int min_m;
  };

  // Settings of the first file matching the pattern; exits if none match.
  inline L1Settings ReadL1Settings(const std::string& pattern)
  {
    TChain chain("l1");
    if(chain.Add(pattern.c_str()) == 0){
      std::cerr << "Couldn't find files matching " << pattern << "! Exiting!" << std::endl;
      std::exit(1);
    }
    TFile* f = TFile::Open(chain.GetListOfFiles()->At(0)->GetTitle(), "read");
    L1Settings s;
    s.window_ns = ((TParameter<double>*)f->Get("window_ns"))->GetVal();
    s.min_m = ((TParameter<int>*)f->Get("min_m"))->GetVal();
    f->Close();
    return s;
  }

  // Row (1 = bottom of plot) and label of each PMT channel, ordered by PMT direction dz
  // so the most upward-facing PMT is at the top.
  inline std::vector<std::pair<int, std::string>> PmtRows(const DetxDom& dom)
  {
    std::vector<std::pair<double, int>> by_dz;
    for(int ch = 0; ch < (int)dom.pmts.size(); ch++) by_dz.push_back(std::make_pair(dom.pmts[ch].dz, ch));
    std::sort(by_dz.begin(), by_dz.end());

    std::vector<std::pair<int, std::string>> rows(by_dz.size()); // channel -> (row, label)
    for(size_t i = 0; i < by_dz.size(); i++)
      rows[by_dz[i].second] = std::make_pair((int)i + 1, std::string(Form("%2d (%+.2f)", by_dz[i].second, by_dz[i].first)));
    return rows;
  }

}

#endif
