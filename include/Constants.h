#ifndef Constants_h
#define Constants_h

#pragma once

namespace END {

  const int    kNPMT        = 31;           // PMTs per DOM
  const int    kNSlices     = 10;           // n_single is counted in kNSlices equal parts of a frame
  const int    kMaxHits     = 4096;         // max hits stored per L1 window
  const double kLightSpeed  = 0.299792458;  // m/ns

}

#endif
