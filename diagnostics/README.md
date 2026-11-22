# diagnostics

```
root -l -b -q 'diagnostics/L0Monitor.C+("run.root", "run_monitor.root", "detector.detx")'
root -l -b -q 'diagnostics/DroppedPackets.C+("l1/*.root", "plots")'
```

`L0Monitor.C` writes per-PMT rate, rate vs time, hit time in the timeslice and ToT histograms, one
directory per DOM.
`DroppedPackets.C` plots dropped UDP packets per DOM and vs time, from the `frames` tree.
