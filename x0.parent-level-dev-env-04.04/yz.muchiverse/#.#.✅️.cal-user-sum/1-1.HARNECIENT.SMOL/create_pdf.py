#!/usr/bin/env python3
"""Convert HTML to PDF"""

import os

import os as _os_pathfix  # REAL FIX 2026-09-01 (S1_HOUSE_PATH_MIGRATION.md) - was a hardcoded absolute path
BASE_DIR = _os_pathfix.path.join(_os_pathfix.path.dirname(_os_pathfix.path.abspath(__file__)), "..", "1-1.HARNECIENT.AUBIO")
BASE_DIR = _os_pathfix.path.normpath(BASE_DIR)
HTML_FILE = os.path.join(BASE_DIR, "HARNECIENT_USER_GUIDE.html")
PDF_FILE = os.path.join(BASE_DIR, "HARNECIENT_USER_GUIDE.pdf")

try:
    from weasyprint import HTML
    print("✓ Using weasyprint to convert HTML to PDF...")
    HTML(HTML_FILE).write_pdf(PDF_FILE)
    if os.path.exists(PDF_FILE):
        size = os.path.getsize(PDF_FILE) / (1024 * 1024)
        print(f"✓ Created: {PDF_FILE}")
        print(f"  Size: {size:.2f} MB")
    else:
        print("✗ PDF creation failed")
except ImportError:
    print("! weasyprint not available, trying pdfkit...")
    try:
        import pdfkit
        pdfkit.from_file(HTML_FILE, PDF_FILE)
        if os.path.exists(PDF_FILE):
            size = os.path.getsize(PDF_FILE) / (1024 * 1024)
            print(f"✓ Created: {PDF_FILE}")
            print(f"  Size: {size:.2f} MB")
    except Exception as e:
        print(f"✗ PDF creation failed: {e}")
        print("\nAlternative: Open the HTML file in a browser and print to PDF:")
        print(f"  open {HTML_FILE}")
        print("  Then: Ctrl+P → Save as PDF")
