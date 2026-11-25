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

Run the commands in the folder READMEs the same way, with the data and this repository mounted at the same path
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
- `wrs/` good White Rabbit sync intervals from the WRS monitoring logs, and keeping only those timeslices
- `include/` detx reader, DOM id to DU/DOM table, constants, plotting functions

Each folder has a README describing its scripts. Run all commands from the repository directory.
