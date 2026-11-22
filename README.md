# end_analysis

Analysis of L0 timeslice data from END detectors. The L0 data are first reduced to a flat ROOT tree of
L1 coincidences, and the analysis and plotting run on that tree.

## Requirements

- ROOT (6.20 or later)
- `km3net-dataformat` with its ROOT dictionary `libKM3NeTROOT.so`, for the steps that read L0 files
  (`l1tree/`, `diagnostics/L0Monitor.C`, `slim/`). A Jpp environment provides it.
- `l0_slimmer` for `slim/slim_timeslices.sh`
- python3 and openssl for the event display

## Setting up Jpp

On Linux, with ROOT set up, build Jpp once from a Jpp checkout:

```
cd /path/to/Jpp
source setenv.sh /path/to/Jpp
make -j4
```

Then, in every new shell:

```
source /path/to/Jpp/setenv.sh /path/to/Jpp
```

This puts ROOT, the `km3net-dataformat` headers and `libKM3NeTROOT.so` on the paths used by the macros.

For MacOS, run it in Docker with a ROOT 6.20 image (jpp does not natively build on MacOS). A minimal `Dockerfile`:

```
FROM rootproject/root:6.20.08-ubuntu20.04
RUN apt-get update && apt-get install -y --no-install-recommends zsh git && rm -rf /var/lib/apt/lists/*
RUN git config --system --add safe.directory '*'
```

Build the image, then build Jpp inside it:

```
docker build -t jpp-env .
docker run --rm -v /path/to/Jpp:/jpp -w /jpp jpp-env bash -c 'source setenv.sh /jpp && make -j4'
```

Run the commands in this README the same way, with the data and this repository mounted at the same path
inside the container:

```
docker run --rm -v /path/to/Jpp:/jpp -v $HOME/work:$HOME/work -w $PWD jpp-env \
  bash -c 'source /jpp/setenv.sh /jpp && root -l -b -q "l1tree/MakeL1Tree.C+(\"run.root\", \"run_l1.root\", \"det.detx\")"'
```

## Layout

- `l1tree/` L0 files to the L1 tree
- `analysis/` coincidence spectrum, muon tagging, event dump
- `diagnostics/` L0 monitoring histograms, dropped packets
- `slim/` counting and thinning out timeslices in L0 files
- `display/` event display page
- `include/` detx reader, constants, plotting functions

## L1 tree

```
root -l -b -q 'l1tree/MakeL1Tree.C+("run.root", "run_l1.root", "detector.detx")'
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
| `dom_id`, `du`, `dom` | DOM; `du` and `dom` are -1 without a detx |
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

Hit times are corrected with the PMT t0 from the detx. PMT index i in the detx is TDC channel i.

The tree can be used directly, for example:

```
root run_l1.root
l1->Draw("m", "du==2 && dom==3 && t0 < 20e6")
l1->Draw("m", "pmt_mask & (1<<5)")
```

## Analysis

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

Event display:

```
root -l -b -q 'analysis/EventDump.C+("plots/muons.root", "l1/*.root", "events.json")'
python3 display/make_event_display.py events.json event_display.html
```

Use the same L1 files as for `MuonTag.C`. `--user` and `--password` give a password-protected page.

## Diagnostics

```
root -l -b -q 'diagnostics/L0Monitor.C+("run.root", "run_monitor.root", "detector.detx")'
root -l -b -q 'diagnostics/DroppedPackets.C+("l1/*.root", "plots")'
```

`L0Monitor.C` writes per-PMT rate, rate vs time, hit time in the timeslice and ToT histograms, one
directory per DOM. `DroppedPackets.C` plots dropped UDP packets per DOM and vs time, from the `frames` tree.

## Slimming L0 files

```
slim/count_timeslices.sh run.root
slim/slim_timeslices.sh run.root run_every10.root 10
```

`slim_timeslices.sh` keeps 1 in every N timeslices (N = 2, 5, 10 or a multiple of 10) using `l0_slimmer`;
set `L0_SLIMMER` if it is not on the PATH.
