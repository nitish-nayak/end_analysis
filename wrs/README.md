# wrs

Good White Rabbit sync intervals from the WRS monitoring logs (`wr_mon{n}_MMDDYY.log`,
`wr_snap{n}_MMDDYY.log`; the date in the name is local, UTC-6):

```
python3 wrs/make_sync_intervals.py ~/disk_end/wrs_logs_20261123/logs good_sync.csv
```

Every ~56 s the poll script writes `POLL TIMESTAMP` (unix ms, the start of the poll, same in all logs) and
then reads the switches one after the other over ssh (about 5 s each, longer after a timeout). A poll of a
switch is good only if its wr_mon screens and its snapshot are all there and all show PLL mode BC, LOCKED,
PPS On, grand master 1st hop with clock class 6, servo TRACK_PHASE, tracking not disabled, wri1 locked and
|offsetFromMaster| < 1 ns. A poll counts only if the polls before and after it are good too, and polls more
than 150 s apart are not neighbours (the time between is unknown); the switch is synchronised between two
consecutive counting polls.
The detector is synchronised only where every switch is: one bad or unreachable switch removes all data,
and so do times without logs.

Times are the switch's WR time (TAI) minus the leap seconds (37, so UTC = TAI - 37 s), not `POLL TIMESTAMP`.
Switches that never answered (WRS7, not installed yet) are left out; `--switches 1,2,...` forces a list.
Optional: `--max-offset-ns`, `--max-gap-s`.

Writes `utc_start,utc_end` (unix seconds, UTC) per interval and prints the intervals.

Then keep only the timeslices that lie entirely inside these intervals, in a full or slimmed L0 file
(`l0_slimmer` on the PATH, as for `slim/`):

```
wrs/filter_sync.sh run_every10.root run_every10_sync.root good_sync.csv
```

Timeslice k of UTC second s is taken to cover [s + k/10, s + (k+1)/10) (100 ms, the slice index l0_slimmer
uses). `wrs/sync_schedule.py` turns the intervals into the l0_slimmer schedule for the file's seconds;
everything else is dropped. Everything made from the output (L1 tree, livetime) then only sees synchronised
timeslices.

Or, on an L1 file (`l1tree/MakeL1Tree.C`), keeping both trees and the settings so the analysis macros run
on the output unchanged:

```
root -l -b -q 'wrs/FilterL1Sync.C+("run_l1.root", "run_sync_l1.root", "good_sync.csv")'
```

A timeslice is kept if [utc_s + utc_ns, + frame_ns] lies inside one interval; the frames tree has no
`utc_ns`, so it is taken from the l1 windows of the same timeslice (a timeslice without any needs its
whole second inside).
