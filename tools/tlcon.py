#!/usr/bin/env python3
"""tinyLINK console: tlcon.py [serial | /dev/port]. Ctrl-D quits.

Finds the "tinyLINK console" port by USB serial (prefix match, so any mode),
via ioreg on macOS or /dev/serial/by-id on Linux. Standard library only.
"""
import glob
import os
import re
import select
import subprocess
import sys
import termios
import tty

CTRL_D = b"\x04"


def find_console(serial):
    if sys.platform == "darwin":
        out = subprocess.run(["ioreg", "-r", "-c", "IOUSBHostDevice", "-l", "-w0"],
                             capture_output=True, text=True).stdout
        match, name = False, ""
        for line in out.splitlines():
            if '"USB Serial Number" = ' in line:
                match = (not serial) or ('"' + serial) in line
            elif match and '"kUSBString"' in line:
                name = line.split("= ", 1)[1].strip('"')
            elif match and '"IOCalloutDevice"' in line and name == "tinyLINK console":
                return line.split("= ", 1)[1].strip('"')
        return None
    # The console is the last CDC port, so the highest interface number.
    ports = sorted(glob.glob("/dev/serial/by-id/*tinyLINK*" + (serial or "") + "*-if*"),
                   key=lambda p: int(re.search(r"-if(\d+)", p).group(1)))
    return ports[-1] if ports else None


def main():
    arg = sys.argv[1] if len(sys.argv) > 1 else ""
    port = arg if arg.startswith("/dev/") else find_console(arg)
    if not port:
        sys.exit(f"tinyLINK console {arg or '(any)'} not found")

    fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
    attrs = termios.tcgetattr(fd)
    tty.setraw(fd)
    attrs_raw = termios.tcgetattr(fd)
    attrs_raw[2] |= termios.CLOCAL | termios.CREAD
    termios.tcsetattr(fd, termios.TCSANOW, attrs_raw)

    stdin = sys.stdin.fileno()
    stdin_attrs = termios.tcgetattr(stdin)
    print(f"{port} (Ctrl-D quits)\r")
    tty.setraw(stdin)
    try:
        os.write(fd, b"\r")   # ask for a fresh prompt
        while True:
            ready, _, _ = select.select([fd, stdin], [], [])
            if fd in ready:
                data = os.read(fd, 4096)
                if not data:
                    break
                os.write(sys.stdout.fileno(), data)
            if stdin in ready:
                data = os.read(stdin, 1024)
                if CTRL_D in data:
                    data = data.split(CTRL_D, 1)[0]
                    if data:
                        os.write(fd, data)
                    break
                os.write(fd, data)
    except OSError as e:
        print(f"\r\n{port}: {e.strerror}\r")   # unplugged, or rebooted for a mode change
    finally:
        termios.tcsetattr(stdin, termios.TCSADRAIN, stdin_attrs)
        try:
            termios.tcsetattr(fd, termios.TCSANOW, attrs)
        except termios.error:
            pass
        os.close(fd)
        print()


if __name__ == "__main__":
    main()
