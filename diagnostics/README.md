# diagnostics

```
root -l -b -q 'diagnostics/L0Monitor.C+("run.root", "run_monitor.root", "detector.detx")'
root -l -b -q 'diagnostics/DroppedPackets.C+("l1/*.root", "plots")'
```

`L0Monitor.C` writes per-PMT rate, rate vs time, hit time in the timeslice and ToT histograms, one
directory per DOM. The ToT peak of each PMT is fitted with a Gaussian (peak +- 10 ns); `tot_mean` and
`totcut_mean` in each DOM directory hold the fitted peak per PMT, for all frames and for frames without
dropped packets. `DroppedPackets.C` plots dropped UDP packets per DOM and vs time, from the `frames` tree.
