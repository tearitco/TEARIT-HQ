# hai phones + ghosts presentation (2026-10-06)

Regenerate from the live system (the evidence slides read real files and run real commands each time):

    cd <this folder>
    python3 make_slides.py <house_root>            # house_root = the 44.xyz.01.00 folder; writes snapshots/*.png + manifest.txt
    nice -n 15 ionice -c3 python3 ../make_presentation_video.py .

Output: `Hai-Phones-Ghosts-20261006.mp4` (~2:35, narrated) + a `-yt-summary.txt` chapter list.
Slides tagged EVIDENCE are real output (dry-run report, live counts, Q003 `verify.sh`); slides tagged DESIGN are plans only.
Narration uses edge_tts (the house default for these videos): caption text is sent to Microsoft's online TTS service; pass `--no-tts` to keep it offline.
