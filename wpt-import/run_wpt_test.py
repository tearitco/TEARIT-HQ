#!/usr/bin/env python3
"""
Simple WPT test runner for our browser.
Runs individual WPT test files and reports results.
"""
import http.server
import socketserver
import threading
import time
import sys
import os

HOUSE = "/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00"
REQ_FILE = f"{HOUSE}/#.desktop/network_browser_request.txt"
CONSOLE_FILE = f"{HOUSE}/#.desktop/network_browser_console.txt"
STATUS_FILE = f"{HOUSE}/#.desktop/network_browser_status.state.txt"

WPT_ROOT = "/home/debil/Desktop/github/TEARIT-HQ/wpt-import/wpt"

def wait_for_status(expected="ready", timeout=30):
    start = time.time()
    while time.time() - start < timeout:
        try:
            with open(STATUS_FILE) as f:
                status = f.read().strip()
            if status == expected:
                return True
        except:
            pass
        time.sleep(0.5)
    return False

def load_page(url, timeout=30):
    with open(f"{HOUSE}/#.desktop/network_browser_request.txt", "w") as f:
        f.write(f"go:{url}\n")
    return wait_for_status("ready", timeout)

def check_console_for_result(test_name):
    """Check console for test results."""
    try:
        with open(f"{HOUSE}/#.desktop/network_browser_console.txt") as f:
            content = f.read()
        # Look for testharness results
        lines = content.split('\n')
        for line in reversed(lines[-100:]):
            if any(kw in line for kw in ['PASS', 'FAIL', 'assert_', 'test(', 'test_harness']):
                print(f"  Console: {line.strip()}")
    except:
        pass

class WPTRequestHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WPT_ROOT, **kwargs)

def run_test(test_path, port=8888):
    """Run a single WPT test."""
    # Start HTTP server
    with socketserver.TCPServer(("127.0.0.1", port), WPTRequestHandler) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        
        time.sleep(1)
        
        test_url = f"http://127.0.0.1:{port}/{test_path}"
        print(f"Running test: {test_url}")
        
        if load_page(test_url, timeout=30):
            time.sleep(3)
            check_console_for_result(test_path)
            print("  ✓ Loaded")
            return True
        else:
            print("  ✗ Timeout")
            return False

if __name__ == "__main__":
    import socketserver
    import http.server
    import threading
    
    # Test a simple DOM test
    test_path = "html/dom/access-key-label.html"
    run_test(test_path)
