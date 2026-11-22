#!/bin/bash
# Usage (from the repository directory): slim/count_timeslices.sh <L0 file>
# Prints the number of timeslices, the UTC seconds of the first and last one, and MB per timeslice.

if [ ! -f slim/TimesliceInfo.C ]; then
  echo "Run this from the repository directory"
  exit 1
fi

if [ $# -ne 1 ]; then
  echo "Usage: slim/count_timeslices.sh <L0 file>"
  exit 1
fi

root -l -b -q "slim/TimesliceInfo.C+(\"$1\")" 2>/dev/null | grep -E '^(timeslices|first_utc_s|last_utc_s|mb_per_timeslice) '
