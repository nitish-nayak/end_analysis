# slim

```
slim/count_timeslices.sh run.root
slim/slim_timeslices.sh run.root run_every10.root 10
```

`slim_timeslices.sh` keeps 1 in every N timeslices (N = 2, 5, 10 or a multiple of 10) using `l0_slimmer`;
set `L0_SLIMMER` if it is not on the PATH.
