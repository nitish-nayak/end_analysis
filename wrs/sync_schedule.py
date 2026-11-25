#!/usr/bin/env python3
"""l0_slimmer schedule keeping only the timeslices that lie entirely inside good WR sync intervals.

Usage: wrs/sync_schedule.py <good_sync.csv> <first_utc_s> <last_utc_s> <schedule>

Timeslice k of UTC second s covers [s + k/10, s + (k+1)/10) (100 ms timeslices, as in l0_slimmer).
Seconds from first_utc_s to last_utc_s with at least one such timeslice get a line "<s> <ranges>";
everything else is dropped ("default none").
"""

import math
import sys

SLICES = 10
EPS = 1e-6


def read_intervals(path):
    out = []
    for line in open(path):
        line = line.strip()
        if not line or line.startswith('#') or line.startswith('utc_start'):
            continue
        a, b = (float(x) for x in line.split(','))
        out.append((a, b))
    return sorted(out)


def main():
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    intervals = read_intervals(sys.argv[1])
    first, last = int(sys.argv[2]), int(sys.argv[3])

    kept_seconds = kept_slices = 0
    with open(sys.argv[4], 'w') as out:
        out.write(f'# timeslices entirely inside the good WR sync intervals of {sys.argv[1]}\n')
        out.write('default none\n')
        lines = {}
        for a, b in intervals:
            s0, s1 = max(first, math.floor(a)), min(last, math.floor(b))
            for s in range(s0, s1 + 1):
                for k in range(SLICES):
                    if s + k / SLICES >= a - EPS and s + (k + 1) / SLICES <= b + EPS:
                        lines.setdefault(s, set()).add(k)
        for s in sorted(lines):
            ks = sorted(lines[s])
            ranges, start = [], ks[0]
            for prev, k in zip(ks, ks[1:] + [None]):
                if k != prev + 1:
                    ranges.append(f'{start}:{prev}' if prev > start else f'{start}')
                    start = k
            out.write(f'{s} {",".join(ranges)}\n')
            kept_seconds += 1
            kept_slices += len(ks)
    print(f'seconds {kept_seconds} of {last - first + 1}, timeslices {kept_slices} ({kept_slices / SLICES:.0f} s) in good sync')


if __name__ == '__main__':
    main()
