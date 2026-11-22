# slim

Run from the repository directory. `l0_slimmer` must be on the PATH.

```
slim/count_timeslices.sh run.root
slim/slim_timeslices.sh run.root run_every10.root 10
```

`count_timeslices.sh` prints the number of timeslices, the first and last UTC second, and MB per timeslice.
`slim_timeslices.sh` keeps 1 in every N timeslices (N a multiple of 10): the first timeslice of every
(N/10)-th second.
