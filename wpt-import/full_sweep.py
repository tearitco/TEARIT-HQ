#!/usr/bin/env python3
"""
Full Top-50 Site Sweep for browser compatibility testing.
"""
import time
import json
import sys
import os

# Top 50 sites from Tranco list (categories: search, social, news, commerce, dev, media)
TOP_50_SITES = [
    # Search & Portals
    ("https://www.google.com/", "google"),
    ("https://www.bing.com/", "bing"),
    ("https://duckduckgo.com/", "duckduckgo"),
    ("https://www.yahoo.com/", "yahoo"),
    
    # Video & Social
    ("https://www.youtube.com/", "youtube"),
    ("https://www.facebook.com/", "facebook"),
    ("https://www.instagram.com/", "instagram"),
    ("https://www.twitter.com/", "twitter"),
    ("https://www.linkedin.com/", "linkedin"),
    ("https://www.reddit.com/", "reddit"),
    
    # News & Content
    ("https://en.wikipedia.org/", "wikipedia"),
    ("https://www.nytimes.com/", "nytimes"),
    ("https://www.bbc.com/", "bbc"),
    ("https://www.theguardian.com/", "guardian"),
    ("https://www.cnn.com/", "cnn"),
    ("https://www.reuters.com/", "reuters"),
    ("https://www.washingtonpost.com/", "washingtonpost"),
    ("https://news.ycombinator.com/", "hackernews"),
    
    # Commerce
    ("https://www.amazon.com/", "amazon"),
    ("https://www.ebay.com/", "ebay"),
    ("https://www.etsy.com/", "etsy"),
    
    # Developer
    ("https://github.com/", "github"),
    ("https://gitlab.com/", "gitlab"),
    ("https://stackoverflow.com/", "stackoverflow"),
    ("https://developer.mozilla.org/", "mdn"),
    
    # Media & Tech
    ("https://www.theverge.com/", "theverge"),
    ("https://www.techcrunch.com/", "techcrunch"),
    ("https://arstechnica.com/", "arstechnica"),
    ("https://www.engadget.com/", "engadget"),
    
    # Reference
    ("https://www.britannica.com/", "britannica"),
    ("https://www.dictionary.com/", "dictionary"),
    
    # Other popular
    ("https://www.amazon.com/", "amazon"),
    ("https://www.netflix.com/", "netflix"),
    ("https://www.spotify.com/", "spotify"),
    ("https://www.twitch.tv/", "twitch"),
    ("https://www.discord.com/", "discord"),
    ("https://www.slack.com/", "slack"),
    ("https://www.notion.so/", "notion"),
    ("https://www.figma.com/", "figma"),
    ("https://www.canva.com/", "canva"),
    ("https://www.shopify.com/", "shopify"),
    ("https://www.stripe.com/", "stripe"),
]

HOUSE = "/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00"
REQ_FILE = f"{HOUSE}/#.desktop/network_browser_request.txt"
STATUS_FILE = f"{HOUSE}/#.desktop/network_browser_status.state.txt"
CONSOLE_FILE = f"{HOUSE}/#.desktop/network_browser_console.txt"

def wait_for_status(expected="ready", timeout=90):
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

def load_page(url, timeout=90):
    with open(f"{HOUSE}/#.desktop/network_browser_request.txt", "w") as f:
        f.write(f"go:{url}\n")
    return wait_for_status("ready", timeout)

def check_console_for_errors():
    try:
        with open(f"{HOUSE}/#.desktop/network_browser_console.txt") as f:
            content = f.read()
        errors = []
        for line in content.split('\n'):
            if ('ERROR|' in line or 'TypeError:' in line or 
                'ReferenceError:' in line or 'SyntaxError:' in line):
                if 'Error parsing client-env' not in line:
                    errors.append(line.strip())
        return errors[-10:] if errors else []
    except:
        return []

def run_sweep():
    results = []
    for i, (url, name) in enumerate(TOP_50_SITES):
        print(f"\n[{i+1}/50] Testing: {name} ({url})")
        try:
            if load_page(url, timeout=120):
                time.sleep(5)
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
        
        time.sleep(2)
    
    return results

if __name__ == "__main__":
    print("Starting Full Top-50 Site Sweep")
    print("=" * 50)
    
    HOUSE = "/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00"
    try:
        with open(f"{HOUSE}/#.desktop/network_browser_status.state.txt") as f:
            status = f.read().strip()
        print(f"Browser status: {status}")
    except:
        print("ERROR: Browser not running!")
        sys.exit(1)
    
    results = run_sweep()
    
    print("\n" + "=" * 50)
    print("SUMMARY")
    print("=" * 50)
    loaded = sum(1 for r in results if r["status"] == "loaded")
    print(f"Loaded: {loaded}/50")
    for r in results:
        status_icon = "✓" if r["status"] == "loaded" else "✗"
        print(f"  {status_icon} {r['name']} - {r['status']}")
        if r["js_errors"]:
            for err in r["js_errors"][:3]:
                print(f"    JS Error: {err}")
    
    with open("top50_results.json", "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nResults saved to top50_results.json")

if __name__ == "__main__":
    print("Starting Full Top-50 Site Sweep")
    print("=" * 50)
    run_sweep()
