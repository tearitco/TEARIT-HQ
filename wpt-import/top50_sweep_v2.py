#!/usr/bin/env python3
"""
Top-50 site sweep for browser compatibility testing.
Tests a subset of real sites with generous timeouts.
"""
import subprocess
import time
import json
import sys
import os

# Top 20 sites for initial sweep (mix of content, apps, commerce)
TEST_SITES = [
    ("https://example.com/", "example"),
    ("https://httpbin.org/html", "httpbin"),
    ("https://en.wikipedia.org/", "wikipedia"),
    ("https://github.com/", "github"),
    ("https://news.ycombinator.com/", "hackernews"),
    ("https://www.reddit.com/", "reddit"),
    ("https://www.google.com/", "google"),
    ("https://www.youtube.com/", "youtube"),
    ("https://www.twitter.com/", "twitter"),
    ("https://www.github.com/", "github"),
]

HOUSE = "/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00"
REQ_FILE = f"{HOUSE}/#.desktop/network_browser_request.txt"
STATUS_FILE = f"{HOUSE}/#.desktop/network_browser_status.state.txt"
CONSOLE_FILE = f"{HOUSE}/#.desktop/network_browser_console.txt"

def wait_for_status(expected="ready", timeout=60):
    """Wait for browser status to reach expected state."""
    start = time.time()
    while time.time() - start < timeout:
        try:
            with open(STATUS_FILE) as f:
                status = f.read().strip()
            if status == expected:
                return True
        except:
            pass
        time.sleep(1)
    return False

def load_page(url, timeout=60):
    """Load a page in the browser."""
    with open(f"{os.environ.get('HOUSE', '/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00')}/#.desktop/network_browser_request.txt", "w") as f:
        f.write(f"go:{url}\n")
    return wait_for_status("ready", timeout)

def check_console_for_errors():
    """Check console for JavaScript errors."""
    try:
        with open(f"{os.environ.get('HOUSE', '/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00')}/#.desktop/network_browser_console.txt") as f:
            content = f.read()
        errors = []
        for line in content.split('\n'):
            if 'ERROR|' in line or 'TypeError:' in line or 'ReferenceError:' in line or 'SyntaxError:' in line:
                if 'Error parsing client-env' not in line:  # Filter known noise
                    errors.append(line.strip())
        return errors[-10:] if errors else []
    except:
        return []

def run_sweep():
    results = []
    for i, (url, name) in enumerate(TEST_SITES):
        print(f"\n[{i+1}/{len(TEST_SITES)}] Testing: {name} ({url})")
        try:
            if load_page(url, timeout=60):
                time.sleep(5)  # Let JS execute
                errors = check_console_for_errors()
                results.append({
                    "name": name,
                    "url": url,
                    "status": "loaded",
                    "js_errors": errors[:5]
                })
                print(f"  ✓ Loaded (JS errors: {len(errors)})")
            else:
                results.append({
                    "name": name,
                    "url": url,
                    "status": "timeout",
                    "js_errors": []
                })
                print(f"  ✗ Timeout")
        except Exception as e:
            results.append({
                "name": name,
                "url": url,
                "status": f"error: {str(e)}",
                "js_errors": []
            })
            print(f"  ✗ Error: {e}")
        
        # Small delay between requests
        time.sleep(2)
    
    return results

if __name__ == "__main__":
    print("Starting Top-10 Site Sweep (test run)")
    print("=" * 50)
    
    # Check browser is running
    HOUSE = "/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00"
    try:
        with open(f"{HOUSE}/#.desktop/network_browser_status.state.txt") as f:
            status = f.read().strip()
        print(f"Browser status: {status}")
    except:
        print("ERROR: Browser not running!")
        sys.exit(1)
    
    os.environ["HOUSE"] = "/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00"
    results = run_sweep()
    
    # Summary
    print("\n" + "=" * 50)
    print("SUMMARY")
    print("=" * 50)
    loaded = sum(1 for r in results if r["status"] == "loaded")
    print(f"Loaded: {loaded}/{len(TEST_SITES)}")
    for r in results:
        status_icon = "✓" if r["status"] == "loaded" else "✗"
        print(f"  {status_icon} {r['name']} - {r['status']}")
        if r["js_errors"]:
            for err in r["js_errors"][:3]:
                print(f"    JS Error: {err}")
    
    # Save results
    with open("top10_results.json", "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nResults saved to top10_results.json")

