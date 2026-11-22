#!/usr/bin/env python3
"""Build the event display page from analysis/EventDump.C output.

    python3 make_event_display.py events.json event_display.html [--user NAME --password PASS]

With --user and --password the event data is encrypted (PBKDF2-SHA256 key from
"user\\npassword", AES-256-CBC, HMAC-SHA256) and the page asks for both before
showing anything. The password itself is not stored in the page. Needs `openssl`.
Without them, the page opens directly.
"""
import argparse
import base64
import hashlib
import hmac
import json
import os
import pathlib
import subprocess
import sys

ITERATIONS = 250_000

parser = argparse.ArgumentParser(description="Build the muon event display page.")
parser.add_argument("events_json")
parser.add_argument("output_html")
parser.add_argument("--user")
parser.add_argument("--password")
args = parser.parse_args()

if (args.user is None) != (args.password is None):
    sys.exit("give both --user and --password, or neither")

template = pathlib.Path(__file__).with_name("event_display_template.html").read_text()
data = json.loads(pathlib.Path(args.events_json).read_text())
plain = json.dumps(data, separators=(",", ":")).encode()

if args.user is None:
    payload = {"plain": data}
else:
    salt = os.urandom(16)
    iv = os.urandom(16)
    keys = hashlib.pbkdf2_hmac("sha256", f"{args.user}\n{args.password}".encode(), salt, ITERATIONS, dklen=64)
    aes_key, mac_key = keys[:32], keys[32:]
    ct = subprocess.run(["openssl", "enc", "-aes-256-cbc", "-K", aes_key.hex(), "-iv", iv.hex()],
                        input=plain, capture_output=True, check=True).stdout
    tag = hmac.new(mac_key, iv + ct, hashlib.sha256).digest()
    b64 = lambda b: base64.b64encode(b).decode()
    payload = {"iterations": ITERATIONS, "salt": b64(salt), "iv": b64(iv), "ct": b64(ct), "tag": b64(tag)}

placeholder = "/*__PAYLOAD__*/null"
if placeholder not in template:
    sys.exit("template has no payload placeholder")

page = template.replace(placeholder, json.dumps(payload, separators=(",", ":")))
pathlib.Path(args.output_html).write_text(page)
print(f"wrote {args.output_html} ({len(data['events'])} events, {len(page) // 1024} kB, "
      f"{'password protected' if args.user else 'open'})")
