#!/bin/bash
# Usage (from the repository directory): wrs/filter_sync.sh <L0 file> <output file> <good_sync.csv>
# Keeps only the timeslices entirely inside the good WR sync intervals (from wrs/make_sync_intervals.py),
# with l0_slimmer, which must be on the PATH. Works on full and slimmed L0 files.

if [ ! -f wrs/sync_schedule.py ]; then
  echo "Run this from the repository directory"
  exit 1
fi

if [ $# -ne 3 ]; then
  echo "Usage: wrs/filter_sync.sh <L0 file> <output file> <good_sync.csv>"
  exit 1
fi

input=$1
output=$2
intervals=$3

# first and last UTC second in the file
info=$(slim/count_timeslices.sh $input)
first=$(echo "$info" | awk '/first_utc_s/ {print $2}')
last=$(echo "$info" | awk '/last_utc_s/ {print $2}')
if [ -z "$first" ] || [ -z "$last" ]; then
  echo "Couldn't read the timeslices of $input"
  exit 1
fi

schedule=$output.schedule.txt
python3 wrs/sync_schedule.py $intervals $first $last $schedule || exit 1
if [ $(grep -cv -E '^(#|default)' $schedule) -eq 0 ]; then
  echo "No timeslice of $input is in good sync; nothing written"
  rm $schedule
  exit 1
fi

l0_slimmer -1 -F $schedule -f $input -o $output
rm $schedule
