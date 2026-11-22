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

  // Row (1 = bottom of plot) of each PMT channel, ordered by PMT direction dz so the most
  // upward-facing PMT is at the top. Uses the first DOM of the detx; channel order without one.
  inline std::vector<std::pair<int, std::string>> PmtRows(const std::string& detx)
  {
    std::vector<std::pair<double, int>> by_dz;
    std::map<int, DetxDom> doms = detx.empty() ? std::map<int, DetxDom>() : ReadDetx(detx);
    for(int ch = 0; ch < kNPMT; ch++){
      const bool known = !doms.empty() && ch < (int)doms.begin()->second.pmts.size();
      by_dz.push_back(std::make_pair(known ? doms.begin()->second.pmts[ch].dz : ch, ch));
    }
    std::sort(by_dz.begin(), by_dz.end());

    std::vector<std::pair<int, std::string>> rows(kNPMT); // channel -> (row, label)
    for(int i = 0; i < kNPMT; i++){
      const int ch = by_dz[i].second;
      rows[ch].first = i + 1;
      rows[ch].second = doms.empty() ? Form("%d", ch) : Form("%2d (%+.2f)", ch, by_dz[i].first);
    }
    return rows;
  }

}

#endif
