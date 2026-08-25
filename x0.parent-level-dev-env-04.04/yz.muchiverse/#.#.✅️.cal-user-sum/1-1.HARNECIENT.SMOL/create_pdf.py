#!/usr/bin/env python3
"""Convert HTML to PDF"""

import os

BASE_DIR = "/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.17/x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.✅️.cal-user-sum/1-1.HARNECIENT.AUBIO"
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
