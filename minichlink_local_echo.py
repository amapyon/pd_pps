#!/usr/bin/env python3
"""Local editable console for a ch32fun/minichlink -T session (Linux).

The firmware stays unchanged. Keystrokes are echoed/editable on the PC;
a complete command is forwarded to minichlink only after Enter.

Run in ~/pj/pps/07_pps_manual_voltage:
    python3 ~/Downloads/minichlink_local_echo.py ../ch32fun/minichlink/minichlink -T

Exit with Ctrl+C or Ctrl+D. This does not flash or change the firmware.
"""

import errno
import os
import pty
import select
import signal
import sys
import termios
import tty

MAX_COMMAND_LENGTH = 31  # Original firmware's 32-byte, NUL-terminated line buffer.


def edit_byte(line: bytearray, key: int) -> tuple[bytes, bytes, bool]:
    """Return (local screen output, data to send to the device, exit flag)."""
    if key in (3, 4):  # Ctrl+C, Ctrl+D when delivered as ordinary bytes.
        return b"", b"", True
    if key in (10, 13):  # Enter: send an entire command plus one CR.
        packet = bytes(line) + b"\r"
        line.clear()
        return b"\n", packet, False
    if key in (8, 127):  # Backspace / Delete
        if line:
            line.pop()
            return b"\b \b", b"", False
        return b"", b"", False
    if 32 <= key <= 126:  # Command parser accepts ASCII.
        if len(line) >= MAX_COMMAND_LENGTH:
            return b"\a", b"", False
        line.append(key)
        return bytes([key]), b"", False
    return b"", b"", False


def run(argv: list[str]) -> int:
    if not os.isatty(sys.stdin.fileno()):
        print("ERROR: run from an interactive terminal.", file=sys.stderr)
        return 2

    cmd = argv or ["../ch32fun/minichlink/minichlink", "-T"]
    if cmd[-1] != "-T":
        print("ERROR: last minichlink argument must be -T.", file=sys.stderr)
        return 2

    # Separate slave terminal: minichlink may set raw mode on *its* stdin,
    # without changing the user's terminal or its local echo behavior.
    pid, master_fd = pty.fork()
    if pid == 0:
        try:
            os.execvp(cmd[0], cmd)
        except OSError as e:
            os.write(2, f"Could not run {cmd[0]}: {e}\n".encode())
            os._exit(127)

    input_fd = sys.stdin.fileno()
    output_fd = sys.stdout.fileno()
    saved_tty = termios.tcgetattr(input_fd)
    line = bytearray()
    status = 0
    try:
        tty.setcbreak(input_fd)  # Immediate keyboard input, no local OS echo.
        os.write(output_fd, b"[PC local echo enabled; Ctrl+C to exit]\n")
        while True:
            readable, _, _ = select.select([input_fd, master_fd], [], [])
            if master_fd in readable:
                try:
                    out = os.read(master_fd, 4096)
                except OSError as e:
                    if e.errno == errno.EIO:  # PTY slave closed.
                        break
                    raise
                if not out:
                    break
                os.write(output_fd, out)
            if input_fd in readable:
                raw = os.read(input_fd, 128)
                if not raw:
                    break
                for key in raw:
                    local_echo, packet, want_exit = edit_byte(line, key)
                    if local_echo:
                        os.write(output_fd, local_echo)
                    if want_exit:
                        return 0
                    if packet:
                        os.write(master_fd, packet)
    except KeyboardInterrupt:
        pass
    finally:
        termios.tcsetattr(input_fd, termios.TCSADRAIN, saved_tty)
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            _, process_status = os.waitpid(pid, 0)
            if os.WIFEXITED(process_status):
                status = os.WEXITSTATUS(process_status)
        except ChildProcessError:
            pass
        os.close(master_fd)
        print()  # Restore a clean prompt after the interactive session.
    return status


if __name__ == "__main__":
    sys.exit(run(sys.argv[1:]))
