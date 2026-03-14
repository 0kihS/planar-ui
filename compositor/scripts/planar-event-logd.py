#!/usr/bin/env python3
"""
planar-event-logd — Planar event stream logger daemon

Connects to the compositor event socket and logs all events to a JSONL file.
Multiple instances can run simultaneously (compositor broadcasts to all subscribers).

Usage:
    planar-event-logd.py [--log-dir DIR] [--max-events N] [--foreground]
    
Log format (JSONL, one event per line):
    {"id":1,"ts":"2026-03-14T01:30:00.123Z","event":"window_focus","data":{"id":"firefox:1"}}
"""

import argparse
import json
import os
import signal
import socket
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

DEFAULT_MAX_EVENTS = 500
DEFAULT_LOG_DIR = os.environ.get(
    "XDG_DATA_HOME",
    os.path.expanduser("~/.local/share")
) + "/planar"

EVENT_SOCKET_ENV = "PLANAR_EVENT_SOCKET"
EVENT_SOCKET_PATTERN = "{runtime}/planar.{wayland}.event.sock"


def get_event_socket_path():
    """Resolve the compositor event socket path."""
    env_path = os.environ.get(EVENT_SOCKET_ENV)
    if env_path:
        return env_path

    runtime = os.environ.get("XDG_RUNTIME_DIR", "/run/user/1000")
    wayland = os.environ.get("WAYLAND_DISPLAY", "wayland-0")
    return os.path.join(runtime, f"planar.{wayland}.event.sock")


def connect_event_socket(path, retries=30, retry_delay=1.0):
    """Connect to the event socket with retries."""
    for attempt in range(retries):
        if not os.path.exists(path):
            if attempt == 0:
                print(f"Waiting for event socket: {path}")
            time.sleep(retry_delay)
            continue
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(path)
            return s
        except (ConnectionRefusedError, FileNotFoundError) as e:
            if attempt == 0:
                print(f"Socket exists but can't connect: {e}")
            time.sleep(retry_delay)
    return None


def ensure_log_dir(log_dir):
    """Create log directory if needed."""
    Path(log_dir).mkdir(parents=True, exist_ok=True)


def get_log_path(log_dir):
    return os.path.join(log_dir, "events.jsonl")


def get_counter_path(log_dir):
    return os.path.join(log_dir, ".event-counter")


def read_counter(log_dir):
    """Read the current event ID counter."""
    path = get_counter_path(log_dir)
    try:
        return int(Path(path).read_text().strip())
    except (FileNotFoundError, ValueError):
        return 0


def write_counter(log_dir, counter):
    """Persist the event ID counter."""
    Path(get_counter_path(log_dir)).write_text(str(counter))


def parse_event(line):
    """Parse a compositor event line.
    
    Format: "event_type json_data" or "event_type value"
    Returns (event_type, data_dict)
    """
    line = line.strip()
    if not line:
        return None, None

    # Try space-separated
    space = line.find(" ")
    if space < 0:
        return line, {}

    event_type = line[:space]
    data_str = line[space + 1:]

    # Try JSON
    try:
        data = json.loads(data_str)
    except json.JSONDecodeError:
        data = {"value": data_str}

    return event_type, data


def rotate_log(log_path, max_events):
    """Trim the log file to keep only the last max_events entries."""
    try:
        lines = Path(log_path).read_text().splitlines()
    except FileNotFoundError:
        return

    if len(lines) <= max_events:
        return

    # Keep last N events
    keep = lines[-max_events:]
    Path(log_path).write_text("\n".join(keep) + "\n")
    print(f"Rotated log: trimmed {len(lines)} → {len(keep)} events")


def append_event(log_path, event_id, event_type, data):
    """Append an event to the JSONL log."""
    entry = {
        "id": event_id,
        "ts": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
        "event": event_type,
        "data": data,
    }
    with open(log_path, "a") as f:
        f.write(json.dumps(entry, separators=(",", ":")) + "\n")


def daemonize():
    """Fork into background (double-fork)."""
    if os.fork() > 0:
        os._exit(0)
    os.setsid()
    if os.fork() > 0:
        os._exit(0)

    # Redirect stdout/stderr to /dev/null
    devnull = os.open(os.devnull, os.O_WRONLY)
    os.dup2(devnull, sys.stdout.fileno())
    os.dup2(devnull, sys.stderr.fileno())
    os.close(devnull)


def main():
    parser = argparse.ArgumentParser(description="Planar event stream logger")
    parser.add_argument("--log-dir", default=DEFAULT_LOG_DIR,
                        help="Directory for event log file")
    parser.add_argument("--max-events", type=int, default=DEFAULT_MAX_EVENTS,
                        help=f"Max events to keep (default: {DEFAULT_MAX_EVENTS})")
    parser.add_argument("--foreground", action="store_true",
                        help="Run in foreground (don't daemonize)")
    parser.add_argument("--socket", default=None,
                        help="Override event socket path")
    args = parser.parse_args()

    socket_path = args.socket or get_event_socket_path()
    log_dir = args.log_dir
    log_path = get_log_path(log_dir)
    max_events = args.max_events

    ensure_log_dir(log_dir)

    # Read resume counter
    event_id = read_counter(log_dir)
    if event_id > 0:
        print(f"Resuming from event ID {event_id}")
    else:
        # Fresh start - rotate any existing log
        if os.path.exists(log_path):
            os.remove(log_path)

    # Daemonize
    if not args.foreground:
        daemonize()
        # Re-open log path after daemonize
        log_path = get_log_path(log_dir)

    # Write PID file
    pid_path = os.path.join(log_dir, "event-logd.pid")
    Path(pid_path).write_text(str(os.getpid()))

    # Signal handlers
    def shutdown(signum, frame):
        write_counter(log_dir, event_id)
        try:
            os.remove(pid_path)
        except FileNotFoundError:
            pass
        sys.exit(0)

    signal.signal(signal.SIGTERM, shutdown)
    signal.signal(signal.SIGINT, shutdown)

    # Main loop
    print(f"planar-event-logd starting")
    print(f"  socket: {socket_path}")
    print(f"  log:    {log_path}")
    print(f"  max:    {max_events} events")

    while True:
        sock = connect_event_socket(socket_path)
        if not sock:
            print(f"Failed to connect to {socket_path}, retrying in 5s...")
            time.sleep(5)
            continue

        print("Connected to event stream")

        try:
            buf = ""
            while True:
                data = sock.recv(4096).decode("utf-8", errors="replace")
                if not data:
                    print("Event stream disconnected")
                    break

                buf += data
                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    line = line.strip()
                    if not line:
                        continue

                    event_type, event_data = parse_event(line)
                    if not event_type:
                        continue

                    event_id += 1
                    append_event(log_path, event_id, event_type, event_data)

                    # Periodic rotation and counter persistence
                    if event_id % 50 == 0:
                        rotate_log(log_path, max_events)
                        write_counter(log_dir, event_id)

        except (ConnectionError, OSError) as e:
            print(f"Connection error: {e}")

        finally:
            try:
                sock.close()
            except OSError:
                pass

            # Always save counter on disconnect
            write_counter(log_dir, event_id)
            rotate_log(log_path, max_events)

        # Reconnect
        print("Reconnecting in 2s...")
        time.sleep(2)


if __name__ == "__main__":
    main()
