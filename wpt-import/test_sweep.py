#!/usr/bin/env python3
"""Quick test of sweep infrastructure with just 3 sites."""
import time

TOP_3_SITES = [
    "https://example.com/",
    "https://httpbin.org/html",
    "https://httpbin.org/links/10",
]

HOUSE = "/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00"
REQ_FILE = f"{HOUSE}/#.desktop/network_browser_request.txt"
STATUS_FILE = f"{HOUSE}/#.desktop/network_browser_status.state.txt"

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
    with open("/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/#.desktop/network_browser_request.txt", "w") as f:
        f.write(f"go:{url}\n")
    return wait_for_status("ready", timeout)

print("Testing sweep infrastructure...")
for url in TOP_3_SITES:
    print(f"Loading: {url}")
    if load_page(url, timeout=20):
        print(f"  ✓ Loaded")
    else:
        print(f"  ✗ Timeout")
    time.sleep(2)

print("Done!")
