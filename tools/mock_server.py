#!/usr/bin/env python3
"""
Mock ingest server for LogIQ Agent.

Features:
- Accepts POST requests on any path (e.g. /ingest)
- Reads request body (supports NDJSON)
- Returns 200 OK by default
- Can simulate failures:
  - Fail every N requests: --fail-every 5
  - Fail randomly by rate: --fail-rate 0.2
"""

from http.server import BaseHTTPRequestHandler, HTTPServer
import argparse
import random
import sys

class Handler(BaseHTTPRequestHandler):
    request_count = 0

    def log_message(self, fmt, *args):
        # Keep default logging quieter; print our own structured logs instead.
        sys.stdout.write("[http] " + (fmt % args) + "\n")

    def do_POST(self):
        Handler.request_count += 1
        req_id = Handler.request_count

        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length) if length > 0 else b""

        # Failure simulation
        should_fail = False
        if self.server.fail_every and (req_id % self.server.fail_every == 0):
            should_fail = True
        if self.server.fail_rate and random.random() < self.server.fail_rate:
            should_fail = True

        # Basic metrics
        lines = body.splitlines()
        num_lines = len(lines)
        num_bytes = len(body)

        print(f"[ingest] id={req_id} path={self.path} bytes={num_bytes} lines={num_lines} fail={should_fail}")

        if self.server.print_body and num_bytes > 0:
            # Print at most first 50 lines to avoid flooding terminal
            max_lines = min(50, num_lines)
            preview = b"\n".join(lines[:max_lines]).decode("utf-8", errors="replace")
            print("[body-preview]\n" + preview + ("\n...[truncated]" if num_lines > max_lines else ""))

        if should_fail:
            self.send_response(503)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(b"simulated failure\n")
            return

        # Success
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(b'{"ok":true}\n')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8070)
    ap.add_argument("--fail-every", type=int, default=0, help="Fail every Nth request (0 disables)")
    ap.add_argument("--fail-rate", type=float, default=0.0, help="Fail randomly with probability in [0,1]")
    ap.add_argument("--print-body", action="store_true", help="Print request body preview")
    args = ap.parse_args()

    httpd = HTTPServer((args.host, args.port), Handler)
    httpd.fail_every = args.fail_every
    httpd.fail_rate = args.fail_rate
    httpd.print_body = args.print_body

    print(f"[mock] listening on http://{args.host}:{args.port}  fail_every={args.fail_every} fail_rate={args.fail_rate}")
    httpd.serve_forever()

if __name__ == "__main__":
    main()