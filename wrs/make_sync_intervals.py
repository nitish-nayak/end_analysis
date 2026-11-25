#!/usr/bin/env python3
"""Good White Rabbit sync intervals (UTC) from the WRS monitoring logs.

The poll script writes, about once a minute, a "POLL TIMESTAMP : <unix ms>" line to every log and then
reads each switch in turn over ssh: wr_mon{n}_*.log holds a few wr_mon screens, wr_snap{n}_*.log one
compact snapshot. A poll of a switch is good only if both are there and every screen and the snapshot show
  PLL mode BC, PLL LOCKED, PPS On, grand master 1st hop with clock class 6 (GNSS), servo TRACK_PHASE,
  tracking enabled, wri1 (the uplink to the grand master) frequency locked, |offsetFromMaster| < max offset,
and the switch time agrees with the poll time. A poll only counts if its neighbouring polls on both sides
are good as well. Between two consecutive counting polls (at most max gap apart) the switch is taken as
synchronised. The detector is synchronised where all switches are; anything else, including times without
logs or with unreachable switches, is out.

Times come from the switches (WR TAI minus leap seconds), so they are exact for the good polls that bound
the intervals. Switches that never answered (not installed) are left out unless given with --switches.

Writes one line "utc_start,utc_end" (unix seconds) per interval.
"""

import argparse
import calendar
import datetime
import glob
import os
import re
import sys

DEFAULT_LEAP = 37
MAX_CLOCK_LAG = 300.  # s: a switch is read at most this long after the poll started


def log_files(log_dir, kind, n):
    """Logs of one kind and switch in time order (names end in _MMDDYY.log)."""
    files = glob.glob(os.path.join(log_dir, f'wr_{kind}{n}_*.log'))
    def key(f):
        d = re.search(r'_(\d{2})(\d{2})(\d{2})\.log$', f)
        return (d.group(3), d.group(1), d.group(2)) if d else ('', '', f)
    return sorted(files, key=key)


def utc_of(stamp):
    """'2026-11-23 17:11:39.606220' -> unix seconds."""
    date, frac = (stamp.split('.') + ['0'])[:2]
    return calendar.timegm(tuple(int(x) for x in re.split(r'[- :]', date)) + (0, 0, 0)) + float('0.' + frac)


def read_mon(files):
    """List of polls: {'t': poll unix s, 'screens': [dict]}."""
    polls = []
    for f in files:
        poll = scr = None
        for line in open(f, errors='replace'):
            line = line.rstrip('\n')
            if 'POLL TIMESTAMP' in line:  # may follow a truncated line
                poll = {'t': int(line.split(':')[-1]) / 1000., 'screens': []}
                polls.append(poll)
                scr = None
                continue
            if poll is None:
                continue
            if 'WR Switch Sync Monitor' in line:
                scr = {'ports': {}, 'track_dis': False}
                poll['screens'].append(scr)
                continue
            if scr is None:
                continue
            if line.startswith('WR time (TAI)'):
                m = re.match(r'WR time \(TAI\)\s*:\s*(\S+ \S+)\s+Leap seconds:\s*(\d+)', line)
                if m:
                    scr['leap'] = int(m.group(2))
                    scr['utc'] = utc_of(m.group(1)) - scr['leap']
            elif line.startswith('PLL mode'):
                m = re.match(r'PLL mode: (\S+)\s+PLL locking state: (\S+)\s+PPS: (\S+)', line)
                if m:
                    scr['pll'], scr['lock'], scr['pps'] = m.groups()
            elif '|' in line and 'hop' in line and re.match(r'\s*[0-9a-f]{2}:', line):
                f_ = [x.strip() for x in line.split('|')]
                scr['hop'], scr['cls'] = f_[1], f_[2]
            elif re.match(r'[+-][ *]wri\d+\s*\|', line):
                f_ = [x.strip() for x in line.split('|')]
                scr['ports'][f_[0][1:].strip('* ')] = f_[1]  # HAL frequency lock
            elif line.startswith('Servo state'):
                scr['servo'] = line.split('White-Rabbit:')[-1].strip()
            elif 'Tracking forcibly disabled' in line:
                scr['track_dis'] = True
            elif line.startswith(' | offsetFromMaster'):
                m = re.search(r':\s*(-?[\d.]+)\s*nsec', line)
                if m:
                    scr['offset'] = float(m.group(1))
    return polls


def read_snap(files):
    """List of polls: {'t': poll unix s, 'snap': dict or None}."""
    polls = []
    for f in files:
        poll = None
        for line in open(f, errors='replace'):
            line = line.rstrip('\n')
            if 'POLL TIMESTAMP' in line:
                poll = {'t': int(line.split(':')[-1]) / 1000., 'snap': None}
                polls.append(poll)
                continue
            if poll is None:
                continue
            if line.startswith('TIME sec:'):
                m = re.match(r'TIME sec:(\d+) nsec:(\d+)', line)
                poll['snap'] = {'tai': int(m.group(1)) + int(m.group(2)) * 1e-9, 'ports': {}}
            elif poll['snap'] is not None and line.startswith('wri'):
                p = line.split()
                poll['snap']['ports'][p[0]] = p[1:]
            elif poll['snap'] is not None and line.startswith('SERVO'):
                poll['snap']['servo'] = dict(re.findall(r"(\w+):('[^']*'|\S+)", line[6:]))
    return polls


def screen_good(s, poll_t, max_offset):
    return (s.get('pll') == 'BC' and s.get('lock') == 'LOCKED' and s.get('pps') == 'On'
            and s.get('hop') == '1st hop' and str(s.get('cls', '')).startswith('6(')
            and s.get('servo') == 'TRACK_PHASE' and not s['track_dis']
            and s['ports'].get('wri1') == 'Lock'
            and 'offset' in s and abs(s['offset']) < max_offset
            and 'utc' in s and 0 <= s['utc'] - poll_t <= MAX_CLOCK_LAG)


def snap_good(s, poll_t, leap, max_offset):
    if s is None or 'servo' not in s:
        return False
    sv = s['servo']
    w1 = s['ports'].get('wri1', [])
    try:
        cko = abs(float(sv.get('cko', 'nan')))
    except ValueError:
        return False
    return (sv.get('ss') == "'TRACK_PHASE'" and sv.get('lock') == '1'
            and len(w1) >= 3 and w1[0] == 'up' and w1[1] == 'slave' and w1[2] == 'Locked'
            and cko < max_offset
            and 0 <= s['tai'] - leap - poll_t <= MAX_CLOCK_LAG)


def switch_samples(log_dir, n, max_offset):
    """One sample per poll: (good, t_first, t_last) in log order; times only meaningful when good."""
    mon = {p['t']: p['screens'] for p in read_mon(log_files(log_dir, 'mon', n))}
    snap = {p['t']: p['snap'] for p in read_snap(log_files(log_dir, 'snap', n))}
    samples = []
    for t in sorted(set(mon) | set(snap)):  # a poll missing from either log is bad
        scr = mon.get(t, [])
        leap = scr[0].get('leap', DEFAULT_LEAP) if scr else DEFAULT_LEAP
        s = snap.get(t)
        good = (len(scr) > 0 and all(screen_good(x, t, max_offset) for x in scr)
                and snap_good(s, t, leap, max_offset))
        if good:
            times = [x['utc'] for x in scr] + [s['tai'] - leap]
            samples.append((True, min(times), max(times)))
        else:
            samples.append((False, t, t))
    return samples, any(mon.values())


def switch_intervals(samples, max_gap):
    """Intervals between consecutive polls that are good together with both their neighbours."""
    ok = [i > 0 and i < len(samples) - 1 and samples[i - 1][0] and samples[i][0] and samples[i + 1][0]
          for i in range(len(samples))]
    out = []
    for i in range(len(samples) - 1):
        if not (ok[i] and ok[i + 1]):
            continue
        a, b = samples[i][1], samples[i + 1][2]
        if samples[i + 1][1] - samples[i][2] > max_gap:
            continue
        if out and a <= out[-1][1]:
            out[-1][1] = max(out[-1][1], b)
        else:
            out.append([a, b])
    return out


def intersect(x, y):
    out, i, j = [], 0, 0
    while i < len(x) and j < len(y):
        a, b = max(x[i][0], y[j][0]), min(x[i][1], y[j][1])
        if a < b:
            out.append([a, b])
        if x[i][1] < y[j][1]:
            i += 1
        else:
            j += 1
    return out


def fmt(t):
    return datetime.datetime.fromtimestamp(t, datetime.timezone.utc).strftime('%Y-%m-%d %H:%M:%S')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('log_dir', help='directory with wr_mon*/wr_snap* logs')
    ap.add_argument('output', help='output csv of good intervals')
    ap.add_argument('--switches', help='comma-separated switch numbers (default: all that ever answered)')
    ap.add_argument('--max-offset-ns', type=float, default=1., help='max |offsetFromMaster| (default 1 ns)')
    ap.add_argument('--max-gap-s', type=float, default=150., help='max time between two polls (default 150 s)')
    args = ap.parse_args()

    found = sorted({int(m.group(1)) for f in glob.glob(os.path.join(args.log_dir, 'wr_mon*_*.log'))
                    for m in [re.search(r'wr_mon(\d+)_', os.path.basename(f))] if m})
    if not found:
        sys.exit(f'No wr_mon logs in {args.log_dir}!')
    wanted = [int(x) for x in args.switches.split(',')] if args.switches else found

    total, used, span = None, [], [float('inf'), -float('inf')]
    for n in wanted:
        samples, answered = switch_samples(args.log_dir, n, args.max_offset_ns)
        if not answered and not args.switches:
            print(f'WRS{n}: never answered, left out (not installed?)')
            continue
        iv = switch_intervals(samples, args.max_gap_s)
        good_t = sum(b - a for a, b in iv)
        if samples:
            span = [min(span[0], samples[0][1]), max(span[1], samples[-1][2])]
        print(f'WRS{n}: {len(samples)} polls, {sum(s[0] for s in samples)} good, '
              f'{len(iv)} intervals, {good_t / 3600:.2f} h synchronised')
        used.append(n)
        total = iv if total is None else intersect(total, iv)
    if total is None:
        sys.exit('No switches!')

    with open(args.output, 'w') as out:
        out.write(f'# good WR sync, UTC unix seconds; switches {",".join(map(str, used))}; '
                  f'max offset {args.max_offset_ns} ns, max gap {args.max_gap_s} s; logs {os.path.abspath(args.log_dir)}\n')
        out.write('utc_start,utc_end\n')
        for a, b in total:
            out.write(f'{a:.3f},{b:.3f}\n')

    good_t = sum(b - a for a, b in total)
    print(f'Logs {fmt(span[0])} -> {fmt(span[1])} UTC: {good_t / 3600:.2f} of {(span[1] - span[0]) / 3600:.2f} h '
          f'synchronised on all of WRS{",".join(map(str, used))}, in {len(total)} intervals:')
    for a, b in total:
        print(f'  {fmt(a)} -> {fmt(b)}  ({(b - a) / 3600:.2f} h)')
    print(f'Wrote {args.output}')


if __name__ == '__main__':
    main()
