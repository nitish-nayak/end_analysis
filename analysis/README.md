# analysis

All analysis macros take a file pattern (wildcards allowed) and an output directory. The detx stored in
the L1 file is used unless another is given.

Coincidence spectrum and PMT vs multiplicity:

```
root -l -b -q 'analysis/CoincidenceSpectrum.C+("l1/*.root", "plots")'
```

Optional: window in ns (default 100, at most the tree's window; shorter windows are an approximation),
time cut in ms (default 20, only the first 20 ms of each timeslice; 0 for the whole timeslice), use the
high-rate veto time as well (default false), detx, multiplicity threshold for the summed rate (default 6).
Rates are per DOM: counts divided by the livetime summed over DOMs.

Muon tagging:

```
root -l -b -q 'analysis/MuonTag.C+("l1/*.root", "plots")'
```

A muon is tagged on a DU when the top and bottom DOMs both have a window with m >= 4 and the bottom one
starts within 40 ns of dz/c after the top one. Optional: tag multiplicity, half width in ns, time cut in
ms (default 20), detx. Writes `muons.root` (tree `muons`, with the best window on every floor), the
bottom-top time difference and the multiplicity of the middle DOM(s).

Event dump for the display (see `display/`):

```
root -l -b -q 'analysis/EventDump.C+("plots/muons.root", "l1/*.root", "events.json")'
```

Writes the brightest and some typical tagged muons with their hits and the detector geometry.
Use the same L1 files as for `MuonTag.C`.
