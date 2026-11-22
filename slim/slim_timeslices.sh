#!/bin/bash
# Usage: slim_timeslices.sh <L0 file> <output file> <N>
# Keeps 1 in every N timeslices with l0_slimmer (set L0_SLIMMER if it is not on PATH).
# With 100 ms timeslices there are 10 per second, so N is 2, 5, 10 or a multiple of 10.
set -e
[ $# -eq 3 ] || { echo "Usage: $0 <L0 file> <output file> <N>"; exit 1; }
input=$1; output=$2; n=$3
slimmer=${L0_SLIMMER:-l0_slimmer}
here=$(cd "$(dirname "$0")" && pwd)
schedule=$(mktemp)
trap 'rm -f "$schedule"' EXIT

if [ "$n" -eq 2 ] || [ "$n" -eq 5 ]; then
  echo "default $(seq -s, 0 "$n" 9)" > "$schedule"
elif [ "$n" -ge 10 ] && [ $((n % 10)) -eq 0 ]; then
  # slice 0 of every (N/10)-th second
  info=$("$here/count_timeslices.sh" "$input")
  first=$(echo "$info" | awk '/first_utc_s/ {print $2}')
  last=$(echo "$info" | awk '/last_utc_s/ {print $2}')
  echo "default none" > "$schedule"
  for ((s = first; s <= last; s += n / 10)); do echo "$s 0:0" >> "$schedule"; done
else
  echo "N must be 2, 5, 10 or a multiple of 10"; exit 1
fi

"$slimmer" -1 -F "$schedule" -f "$input" -o "$output"
