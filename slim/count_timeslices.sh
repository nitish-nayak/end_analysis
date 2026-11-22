#!/bin/bash
# Usage: count_timeslices.sh <L0 file>
# Prints the number of L0 timeslices, the first and last UTC second, and MB per timeslice.
set -e
[ $# -eq 1 ] || { echo "Usage: $0 <L0 file>"; exit 1; }
here=$(cd "$(dirname "$0")" && pwd)
root -l -b -q "$here/TimesliceInfo.C+(\"$1\")" 2>/dev/null | grep -E '^(timeslices|first_utc_s|last_utc_s|mb_per_timeslice) '
