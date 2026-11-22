#ifndef Detx_h
#define Detx_h

#pragma once

// Minimal DETX reader (V1-V5): DOM and PMT positions, PMT t0. PMT i of a DOM is TDC channel i.
// DU and DOM numbers come from DomMap.h.

#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace END {

  struct DetxPmt {
    double x, y, z, dx, dy, dz, t0;
    int status;
  };

  struct DetxDom {
    double x, y, z;
    std::vector<DetxPmt> pmts;
  };

  // DOM id -> DOM. Empty if the file cannot be read.
  inline std::map<int, DetxDom> ReadDetx(const std::string& path)
  {
    std::map<int, DetxDom> doms;
    std::ifstream in(path.c_str());
    std::vector<std::vector<std::string>> lines;
    std::string line;

    while(std::getline(in, line)){
      line = line.substr(0, line.find('#'));
      std::istringstream is(line);
      std::vector<std::string> fields;
      std::string s;
      while(is >> s) fields.push_back(s);
      if(!fields.empty()) lines.push_back(fields);
    }
    if(lines.empty()) return doms;

    // header: "<id> V<n>", validity, UTM, ndoms (V2+); or "<id> <ndoms>" (V1)
    size_t i = (lines[0].size() >= 2 && lines[0][1][0] == 'V') ? 4 : 1;

    while(i < lines.size()){
      const std::vector<std::string>& f = lines[i++];
      DetxDom dom;
      dom.x = dom.y = dom.z = 0.;
      int npmt = 0;
      if(f.size() == 4)
        npmt = std::atoi(f[3].c_str());
      else if(f.size() == 13){
        dom.x = std::atof(f[3].c_str());
        dom.y = std::atof(f[4].c_str());
        dom.z = std::atof(f[5].c_str());
        npmt = std::atoi(f[12].c_str());
      }
      else
        continue;

      for(int p = 0; p < npmt && i < lines.size(); p++, i++){
        const std::vector<std::string>& q = lines[i];
        DetxPmt pmt;
        pmt.x = std::atof(q[1].c_str());  pmt.y = std::atof(q[2].c_str());  pmt.z = std::atof(q[3].c_str());
        pmt.dx = std::atof(q[4].c_str()); pmt.dy = std::atof(q[5].c_str()); pmt.dz = std::atof(q[6].c_str());
        pmt.t0 = std::atof(q[7].c_str());
        pmt.status = (q.size() > 8 ? std::atoi(q[8].c_str()) : 0);
        dom.pmts.push_back(pmt);
      }
      doms[std::atoi(f[0].c_str())] = dom;
    }
    return doms;
  }

}

#endif
