# analysis

All analysis macros take a file pattern (wildcards allowed). Scripts that need positions take the detx
as an argument. DU and DOM numbers always come from
`include/DomMap.h`.

Coincidence spectrum and PMT vs multiplicity:

```
root -l -b -q 'analysis/CoincidenceSpectrum.C+("l1/*.root", "D0ENDV012_expected.detx", "plots")'
```

PMT rows are ordered by direction from the detx, upward-facing at the top; the dashed line separates the
upper and lower hemisphere.
Optional: window in ns (default 100, at most the tree's window; shorter windows are an approximation),
time cut in ms (default 0: whole timeslice; e.g. 20 for the first 20 ms only), use the
high-rate veto time as well (default false), multiplicity threshold for the summed rate (default 6).
Rates are per DOM: counts divided by the livetime summed over DOMs.

Muon tagging:

```
root -l -b -q 'analysis/MuonTag.C+("l1/*.root", "D0ENDV012_expected.detx", "plots")'
```

DOMs with the same x, y in the detx form a column. A muon is tagged on a column when its top and bottom
DOMs both have a window with m >= 4 and the bottom one starts within 40 ns of dz/c after the top one.
A column is live in a timeslice only when all its DOMs have a frame, up to the earliest high-rate veto among
them; rates are per column: tags divided by the livetime summed over columns (`livetime_s`, `n_columns`).
Optional: tag multiplicity, half width in ns, time cut in ms (default 0: whole timeslice). Writes `muons.root`
(tree `muons`: `du`, and per DOM of the column, lowest first, `dom_id`, `dom`, the best window's `m`,
its time `dt` relative to a vertical muon, and its `entry` in the L1 chain), the bottom-top time
difference and the multiplicity of the middle DOM(s).

Event dump for the display (see `display/`):

```
root -l -b -q 'analysis/EventDump.C+("plots/muons.root", "l1/*.root", "D0ENDV012_expected.detx", "events.json")'
```

Writes the brightest and some typical tagged muons with their hits, and the geometry of all DOMs of the
DUs in the data.
Use the same L1 files as for `MuonTag.C`.
