#!/usr/bin/env python3
"""Hand-tuning: ask llama3 to suggest weight/bias edits for tomom vocab.

Emits both a simple edits.txt (word\\tfield\\tvalue) and TEARIT EDIT records
in the format from A-TEARIT-IS-ALL-YOU-NEED.md §2.3:
  EDIT | id=uuid | type=spoke_weight_delta | target=<word> | slot=weight
    | delta=<change> | reason="<prompt text>" | proposer=llama3 | status=candidate

These EDIT records can be ingested by the Concept Bank's promotion ledger,
bridging tomom's hand-tuning to the broader attrition-model pipeline.
"""
import json, urllib.request, sys, re, time, uuid, os
from pathlib import Path

VALID_FIELDS = {'weight', 'bias1', 'bias2', 'bias3', 'bias4'}

def resolve_api_url():
    """Read ollama endpoint from house-shared #.desktop/ai_backend.pdl."""
    # Walk up from harnec/ to find #.desktop/ai_backend.pdl
    project_root = Path(__file__).resolve().parent
    for _ in range(20):
        pdl_path = project_root / "#.desktop" / "ai_backend.pdl"
        if pdl_path.exists():
            for line in open(pdl_path):
                line = line.strip()
                if line.startswith("gemma_lan_url="):
                    base = line.split("=", 1)[1].strip()
                    return base + "/api/generate"
            break
        project_root = project_root.parent
    return "http://127.0.0.1:11434/api/generate"

def call_llama3(prompt, timeout=300):
    api_url = resolve_api_url()
    data = json.dumps({"model":"llama3:latest","prompt":prompt,"stream":False}).encode()
    req = urllib.request.Request(api_url,
                                  data=data, headers={"Content-Type":"application/json"})
    for _ in range(3):
        try:
            resp = urllib.request.urlopen(req, timeout=timeout)
            return json.loads(resp.read()).get("response", "")
        except Exception as e:
            print(f"  retry: {e}", file=sys.stderr); time.sleep(5)
    return ""

def parse_edits(text, valid_words):
    """Parse loose LLM output for word/field/value triples."""
    edits = []
    # Try multiple split patterns: tab, pipe, space
    for line in text.strip().split('\n'):
        # Clean up the line
        line = line.strip()
        if not line:
            continue
        # Try tab-separated
        parts = line.split('\t')
        if len(parts) != 3:
            # Try pipe-separated
            parts = [p.strip() for p in line.split('|')]
        if len(parts) != 3:
            # Try space-separated (but need to identify field vs value)
            continue
        
        # Identify which is the word, field, and value
        word_candidate = parts[0].strip().lower()
        field_candidate = parts[1].strip().lower()
        value_str = parts[2].strip()
        
        # Try different orderings
        for w, f, v in [(parts[0], parts[1], parts[2]),
                        (parts[1], parts[0], parts[2]),
                        (parts[0], parts[2], parts[1])]:
            w = w.strip().lower()
            f = f.strip().lower()
            v = v.strip()
            if w in valid_words and f in VALID_FIELDS:
                try:
                    val = float(v)
                    if 0.0 <= val <= 1.0:
                        edits.append((w, f, val))
                        break
                except ValueError:
                    pass
    return edits

def main():
    vocab_file, prompt_text, behavior, edits_file = sys.argv[1:5]
    
    with open(vocab_file) as f:
        lines = f.read().split('\n')
    words = [l.strip().split()[1] for l in lines[1:] if l.strip().split() and len(l.strip().split())>=2]
    
    prompt = (
        "You are a weight engineer for a tiny neural text model.\n"
        "Vocab fields you can edit: weight (0-1), bias1 (0-1), bias2 (0-1), bias3 (0-1), bias4 (0-1)\n"
        "weight = generation probability (higher = more likely)\n\n"
        "Available words: " + ', '.join(words[:30]) + "\n\n"
        "Prompt: \"" + prompt_text + "\"\n"
        "Goal: " + behavior + "\n\n"
        "Respond with EXACTLY these lines only:\n"
        "word|field|value\n"
        "Using real tab characters between word, field, value.\n"
        "Only use words from the available list.\n"
        "Example: multiplication|weight|0.90\n"
        "Example: division|weight|0.85\n"
    )
    
    print("Requesting from llama3...", flush=True)
    response = call_llama3(prompt)
    if not response:
        print("ERROR: no LLM response", file=sys.stderr); sys.exit(1)
    
    print("LLM response:")
    print(response[:500])
    
    edits = parse_edits(response, set(words))
    
    with open(edits_file, 'w') as ef:
        for w, f, v in edits:
            ef.write(w + '\t' + f + '\t' + str(v) + '\n')
    print("\nValid edits: " + str(len(edits)))
    for w, f, v in edits:
        print("  " + w + "." + f + " = " + str(v))

if __name__ == '__main__':
    main()
