#!/bin/bash
# Usage (from the repository directory): slim/slim_timeslices.sh <L0 file> <output file> <N>
# Keeps 1 in every N timeslices with l0_slimmer, which must be on the PATH.
# There are 10 timeslices per second, so N is 10 or a multiple of 10:
# the first timeslice of every (N/10)-th second is kept.

if [ ! -f slim/TimesliceInfo.C ]; then
  echo "Run this from the repository directory"
  exit 1
fi

if [ $# -ne 3 ] || [ $(($3 % 10)) -ne 0 ]; then
  echo "Usage: slim/slim_timeslices.sh <L0 file> <output file> <N, a multiple of 10>"
  exit 1
fi

input=$1
output=$2
seconds=$(($3 / 10))

# first and last UTC second in the file
info=$(slim/count_timeslices.sh $input)
first=$(echo "$info" | awk '/first_utc_s/ {print $2}')
last=$(echo "$info" | awk '/last_utc_s/ {print $2}')

# l0_slimmer schedule: timeslice 0 of every listed second, nothing from the others
schedule=$output.schedule.txt
echo "default none" > $schedule
for ((s = first; s <= last; s += seconds)); do
  echo "$s 0:0" >> $schedule
done

l0_slimmer -1 -F $schedule -f $input -o $output
rm $schedule
