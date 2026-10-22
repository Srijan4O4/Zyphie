"""Actual CLI signal integration. Only disposable generated data is used."""
import ctypes
import hashlib
import os
from pathlib import Path
import random
import re
import signal
import subprocess
import sys
import tempfile
import threading

exe = str(Path(sys.argv[1]).resolve())
parent = Path(sys.argv[2]).resolve()
parent.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="signal-", dir=parent) as temp:
    root = Path(temp)
    source = root / "source.bin"
    block = random.Random(20261022).randbytes(1024 * 1024)
    with source.open("xb") as out:
        for _ in range(64):
            out.write(block)
    before = hashlib.file_digest(source.open("rb"), "sha256").hexdigest()
    kwargs = {}
    if os.name == "nt":
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE
        kwargs.update(creationflags=subprocess.CREATE_NEW_CONSOLE, startupinfo=startup)
    output = root / "cancelled.zyph"
    process = subprocess.Popen([exe, "pack", str(source), "-o", str(output), "--preset", "maximum", "--threads", "2", "--progress"],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, **kwargs)
    timeout = threading.Timer(90, process.kill)
    timeout.start()
    sent = False
    try:
        for line in process.stderr:
            match = re.match(r"pack: ([1-9][0-9]*)/", line)
            if match and not sent:
                if os.name == "nt":
                    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
                    kernel.FreeConsole()
                    if not kernel.AttachConsole(process.pid):
                        raise ctypes.WinError(ctypes.get_last_error())
                    handler_type = ctypes.WINFUNCTYPE(ctypes.c_int, ctypes.c_ulong)
                    handler = handler_type(lambda event: 1)
                    if not kernel.SetConsoleCtrlHandler(handler, True):
                        raise ctypes.WinError(ctypes.get_last_error())
                    if not kernel.GenerateConsoleCtrlEvent(0, 0):
                        raise ctypes.WinError(ctypes.get_last_error())
                else:
                    process.send_signal(signal.SIGINT)
                sent = True
        code = process.wait(timeout=30)
        assert sent, "No in-flight signal was sent"
        assert code == 7, f"Cancellation returned {code}, expected 7"
        assert not output.exists(), "Cancelled output was published"
        assert not list(root.glob(".zyph-part-*")), "Cancelled staging was retained"
        with source.open("rb") as stream:
            assert hashlib.file_digest(stream, "sha256").hexdigest() == before
        print("Actual CLI Ctrl+C/SIGINT cancellation passed; source preserved, no final or staging output.")
    finally:
        timeout.cancel()
        if process.poll() is None:
            process.kill()
            process.wait()
        if os.name == "nt" and sent:
            kernel.FreeConsole()
