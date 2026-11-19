#!/usr/bin/env python3
"""
Drive a headless BrebOS VM from the command line.

The kernel writes "READY\\n" to the QEMU debug console (port 0xE9) whenever a process blocks waiting for a key press,
i.e. when the system is idle and waiting for user input. Every command below that sends input blocks on that signal
(select() on a FIFO, no polling) instead of sleeping for a fixed amount of time.

Usage:
    vm.py start [--image stick.img]   boot the VM, return once the shell waits for input
                [--debug-log PATH]    stop on CPU reset (triple fault) and log exceptions to PATH
    vm.py type "ls /bin"              type a command + Enter, return once it completed
    vm.py type --no-enter "abc"       type text without pressing Enter
    vm.py key ctrl-c                  send raw QEMU key(s), wait for READY after each
    vm.py shot [out.png]              screenshot (default: <state dir>/screen.png), prints its path
    vm.py stop                        quit the VM

`type` and `key` take --shot [path] to screenshot once input has been processed.
"""

import argparse
import os
import select
import signal
import socket
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STATE_DIR = os.environ.get("BREBOS_VM_DIR", os.path.join(ROOT, "build", "vm"))
MONITOR = os.path.join(STATE_DIR, "monitor.sock")
DEBUGCON = os.path.join(STATE_DIR, "debugcon")  # QEMU pipe chardev: writes to debugcon.out
DEBUGCON_OUT = DEBUGCON + ".out"
PIDFILE = os.path.join(STATE_DIR, "qemu.pid")
LOG = os.path.join(STATE_DIR, "qemu.log")
DEFAULT_SHOT = os.path.join(STATE_DIR, "screen.png")

READY = b"READY\n"

KEYS = {
    " ": "spc", "\n": "ret", "\t": "tab",
    "/": "slash", "?": "shift-slash", ".": "dot", ">": "shift-dot", ",": "comma", "<": "shift-comma",
    "-": "minus", "_": "shift-minus", "=": "equal", "+": "shift-equal",
    ";": "semicolon", ":": "shift-semicolon", "'": "apostrophe", '"': "shift-apostrophe",
    "\\": "backslash", "|": "shift-backslash", "`": "grave_accent", "~": "shift-grave_accent",
    "[": "bracket_left", "{": "shift-bracket_left", "]": "bracket_right", "}": "shift-bracket_right",
    "!": "shift-1", "@": "shift-2", "#": "shift-3", "$": "shift-4", "%": "shift-5",
    "^": "shift-6", "&": "shift-7", "*": "shift-8", "(": "shift-9", ")": "shift-0",
}


def char_to_key(c):
    if c in KEYS:
        return KEYS[c]
    if c.isascii() and (c.islower() or c.isdigit()):
        return c
    if c.isascii() and c.isupper():
        return "shift-" + c.lower()
    sys.exit(f"vm.py: no key mapping for {c!r}")


class Monitor:
    def __init__(self):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(MONITOR)
        self._read_prompt()

    def _read_prompt(self):
        buf = b""
        while not buf.endswith(b"(qemu) "):
            chunk = self.sock.recv(4096)
            if not chunk:
                break
            buf += chunk
        return buf

    def cmd(self, line):
        """Runs an HMP command synchronously (returns once QEMU printed the next prompt)."""
        self.sock.sendall(line.encode() + b"\n")
        return self._read_prompt()

    def close(self):
        self.sock.close()


class Debugcon:
    def __init__(self):
        self.fd = os.open(DEBUGCON_OUT, os.O_RDONLY | os.O_NONBLOCK)
        self.buf = b""

    def drain(self):
        """Discards stale output, so the next wait only sees signals caused by the next input."""
        try:
            while os.read(self.fd, 4096):
                pass
        except BlockingIOError:
            pass
        self.buf = b""

    def wait_ready(self, timeout):
        deadline = time.monotonic() + timeout
        while READY not in self.buf:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not select.select([self.fd], [], [], remaining)[0]:
                return False
            try:
                self.buf += os.read(self.fd, 4096)
            except BlockingIOError:
                pass
        self.buf = self.buf.split(READY, 1)[1]
        return True


def qemu_running():
    try:
        with open(PIDFILE) as f:
            os.kill(int(f.read()), 0)
        return True
    except (OSError, ValueError):
        return False


def require_running():
    if not qemu_running():
        sys.exit("vm.py: VM is not running (vm.py start)")


def send_keys(keys, timeout):
    """Sends keys one at a time, waiting for READY after each. Returns False on timeout."""
    mon, con = Monitor(), Debugcon()
    try:
        for key in keys:
            con.drain()
            mon.cmd(f"sendkey {key}")
            if not con.wait_ready(timeout):
                print(f"vm.py: timeout ({timeout}s) waiting for READY after key '{key}'", file=sys.stderr)
                return False
        return True
    finally:
        mon.close()


def screenshot(path):
    path = os.path.abspath(path)
    mon = Monitor()
    out = mon.cmd(f"screendump {path} -f png")
    mon.close()
    if not os.path.exists(path):
        sys.exit(f"vm.py: screendump failed: {out.decode(errors='replace')}")
    print(path)


def cmd_start(args):
    if qemu_running():
        sys.exit("vm.py: VM already running (vm.py stop)")
    os.makedirs(STATE_DIR, exist_ok=True)
    for p in (MONITOR, DEBUGCON_OUT, DEBUGCON + ".in"):
        if os.path.exists(p):
            os.remove(p)
    os.mkfifo(DEBUGCON_OUT)
    os.mkfifo(DEBUGCON + ".in")

    # Open our end before QEMU starts so that no READY can be missed
    con = Debugcon()

    kvm = not args.tcg and os.access("/dev/kvm", os.R_OK | os.W_OK)
    accel = ["-enable-kvm", "-cpu", "host"] if kvm else []
    qemu = ["qemu-system-i386", *accel, "-display", "none", "-snapshot",
            "-device", "isa-debug-exit",
            "-chardev", f"pipe,id=dbgcon,path={DEBUGCON}", "-device", "isa-debugcon,chardev=dbgcon",
            "-drive", "file=disk_image.img,format=raw,if=ide,file.locking=off",
            "-netdev", "user,id=net0", "-device", "e1000,netdev=net0",
            "-m", "512M", "-device", "qemu-xhci,id=xhci,p3=0",
            "-drive", f"if=none,id=usbstick,file={args.image},format=raw,file.locking=off",
            "-device", "usb-storage,bus=xhci.0,drive=usbstick,bootindex=1",
            "-monitor", f"unix:{MONITOR},server,nowait"]
    if args.debug_log:
        # Stop at a CPU reset (e.g. triple fault) or shutdown instead of rebooting, and log the CPU state + exceptions leading to it
        # -no-shutdown keeps QEMU alive (paused) afterwards, so the screen can still be inspected
        # isa-debug-exit is dropped so that a kernel shutdown leaves the VM (and its screen) alive as well
        i = qemu.index("isa-debug-exit")
        del qemu[i - 1:i + 1]
        # GDB stub on tcp::1234 to inspect the machine when it stops
        qemu += ["-s", "-no-reboot", "-no-shutdown", "-d", "cpu_reset,int", "-D", os.path.abspath(args.debug_log)]
    with open(LOG, "w") as log:
        proc = subprocess.Popen(qemu, cwd=ROOT, stdin=subprocess.DEVNULL, stdout=log, stderr=log,
                                start_new_session=True)
    with open(PIDFILE, "w") as f:
        f.write(str(proc.pid))

    if not con.wait_ready(args.timeout):
        code = proc.poll()
        reason = f"QEMU exited with {code}, see {LOG}" if code is not None else f"no READY within {args.timeout}s"
        print(f"vm.py: boot failed: {reason}", file=sys.stderr)
        if args.shot is not None and code is None:
            screenshot(args.shot or DEFAULT_SHOT)
        sys.exit(1)
    print("ready")
    if args.shot is not None:
        screenshot(args.shot or DEFAULT_SHOT)


def cmd_type(args):
    require_running()
    keys = [char_to_key(c) for c in args.text]
    if not args.no_enter:
        keys.append("ret")
    ok = send_keys(keys, args.timeout)
    if args.shot is not None:
        screenshot(args.shot or DEFAULT_SHOT)
    sys.exit(0 if ok else 1)


def cmd_key(args):
    require_running()
    ok = send_keys(args.keys, args.timeout)
    if args.shot is not None:
        screenshot(args.shot or DEFAULT_SHOT)
    sys.exit(0 if ok else 1)


def cmd_shot(args):
    require_running()
    screenshot(args.path or DEFAULT_SHOT)


def wait_exit(pid, timeout):
    """Waits for a (non-child) process to exit, without polling. Returns False on timeout."""
    try:
        fd = os.pidfd_open(pid)
    except ProcessLookupError:
        return True
    try:
        return bool(select.select([fd], [], [], timeout)[0])
    finally:
        os.close(fd)


def cmd_stop(_):
    if not qemu_running():
        print("not running")
        return
    with open(PIDFILE) as f:
        pid = int(f.read())
    try:
        mon = Monitor()
        mon.sock.sendall(b"quit\n")
        # Keep the connection open until QEMU closes it: closing right away races with QEMU reading the command
        mon.sock.settimeout(5)
        while mon.sock.recv(4096):
            pass
        mon.close()
    except OSError:
        pass
    # Safety net in case the quit still did not go through. Never forget a VM that is still alive: the next start
    # would replace its monitor socket, leaving it unreachable and running forever.
    if not wait_exit(pid, 5):
        print(f"vm.py: QEMU did not quit within 5s, killing it", file=sys.stderr)
        os.kill(pid, signal.SIGKILL)
        if not wait_exit(pid, 5):
            sys.exit(f"vm.py: QEMU (pid {pid}) survived SIGKILL, keeping {PIDFILE}")
    os.remove(PIDFILE)
    print("stopped")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)

    def add_shot(p):
        p.add_argument("--shot", nargs="?", const="", default=None, metavar="PATH",
                       help=f"screenshot afterwards (default path: {DEFAULT_SHOT})")

    p = sub.add_parser("start")
    p.add_argument("--image", default="stick.img")
    p.add_argument("--timeout", type=float, default=60)
    p.add_argument("--debug-log", metavar="PATH",
                   help="don't reboot on CPU reset; log resets and interrupts/exceptions to PATH")
    p.add_argument("--tcg", action="store_true",
                   help="don't use KVM (needed for --debug-log to log interrupts/exceptions)")
    add_shot(p)
    p.set_defaults(func=cmd_start)

    p = sub.add_parser("type")
    p.add_argument("text")
    p.add_argument("--no-enter", action="store_true")
    p.add_argument("--timeout", type=float, default=30, help="per key")
    add_shot(p)
    p.set_defaults(func=cmd_type)

    p = sub.add_parser("key")
    p.add_argument("keys", nargs="+")
    p.add_argument("--timeout", type=float, default=30, help="per key")
    add_shot(p)
    p.set_defaults(func=cmd_key)

    p = sub.add_parser("shot")
    p.add_argument("path", nargs="?")
    p.set_defaults(func=cmd_shot)

    p = sub.add_parser("stop")
    p.set_defaults(func=cmd_stop)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
