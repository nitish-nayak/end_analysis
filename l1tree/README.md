# l1tree

```
root -l -b -q 'l1tree/MakeL1Tree.C+("run.root", "run_l1.root", "D0ENDV012_expected.detx")'
```

Optional arguments after the detx: window length in ns (default 100), minimum multiplicity saved (1 or 2,
default 2), maximum number of timeslices (default all).

For every DOM and timeslice, hits are sorted in time and grouped into windows. A window starts at the first
unused hit and contains all hits up to the window length later. The multiplicity `m` is the number of
distinct PMTs in it. This is an L1 coincidence with a longer window than the standard L1 (10-25 ns).

The output has two trees.

`l1`, one entry per window with `m` at or above the minimum:

| branch | |
|---|---|
| `run`, `frame` | run number, timeslice index |
| `utc_s`, `utc_ns` | timeslice start |
| `dom_id`, `du`, `dom` | DOM; `du` and `dom` from `include/DomMap.h` (-1 if not listed) |
| `t0` | time of the first hit, ns from the timeslice start |
| `m`, `nhit` | multiplicity, number of hits |
| `pmt_mask` | bit `ch` set if PMT channel `ch` has a hit |
| `tot_sum` | summed ToT |
| `hit_ch[nhit]`, `hit_t[nhit]`, `hit_tot[nhit]` | hits; `hit_t` relative to `t0` |

`frames`, one entry per DOM per timeslice:

| branch | |
|---|---|
| `run`, `frame`, `utc_s`, `dom_id`, `du`, `dom` | as above |
| `frame_ns` | timeslice length |
| `nhit` | hits in the frame |
| `n_single[10]` | windows with m = 1, in 10 equal parts of the frame |
| `hrv`, `fifo` | PMTs with high-rate veto / FIFO almost full (bit per channel) |
| `t_hrv_cut` | time of the first high-rate veto, `frame_ns` if none |
| `udp_recv`, `udp_expected` | UDP packets received and expected (-1 without trailer) |

The detx is optional and only used for the PMT t0 that corrects the hit times; it is not stored in the output. PMT index i in the detx is TDC channel i.

The tree can be used directly, for example:

```
root run_l1.root
l1->Draw("m", "du==2 && dom==3 && t0 < 20e6")
l1->Draw("m", "pmt_mask & (1<<5)")
```
