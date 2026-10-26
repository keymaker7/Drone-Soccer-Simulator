"""Frozen localhost runtime: no installed Python, Git or source checkout required."""
import argparse
import json
import os
from pathlib import Path
import sys
import threading
import time
from http.server import ThreadingHTTPServer
from sdacs_bridge import GuidanceBridge, make_handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=0)
    parser.add_argument('--ready-file', type=Path, required=True)
    parser.add_argument('--parent-pid', type=int)
    parser.add_argument('--test-lifetime', type=float, default=0)
    args = parser.parse_args()
    root = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parent)) / 'upstream'
    bridge = GuidanceBridge(root, bundled=True)
    server = ThreadingHTTPServer(('127.0.0.1', args.port), make_handler(bridge))
    server.daemon_threads = True
    port = server.server_address[1]
    args.ready_file.parent.mkdir(parents=True, exist_ok=True)
    ready = dict(bridge.health(), url=f'http://127.0.0.1:{port}', pid=os.getpid())
    temporary = args.ready_file.with_suffix('.tmp')
    temporary.write_text(json.dumps(ready), encoding='utf-8')
    temporary.replace(args.ready_file)
    parent_handle = None
    if args.parent_pid and sys.platform == 'win32':
        import ctypes
        from ctypes import wintypes
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel.OpenProcess.restype = wintypes.HANDLE
        kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        parent_handle = kernel.OpenProcess(0x00100000, False, args.parent_pid)
        if not parent_handle:
            server.server_close()
            return 1
    def watch():
        started = time.monotonic()
        while True:
            time.sleep(.25)
            expired = args.test_lifetime > 0 and time.monotonic() - started >= args.test_lifetime
            ended = parent_handle and kernel.WaitForSingleObject(parent_handle, 0) == 0
            if expired or ended:
                server.shutdown()
                return
    if parent_handle or args.test_lifetime:
        threading.Thread(target=watch, daemon=True).start()
    try:
        server.serve_forever(poll_interval=.2)
    finally:
        server.server_close()
        if parent_handle:
            kernel.CloseHandle(parent_handle)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
