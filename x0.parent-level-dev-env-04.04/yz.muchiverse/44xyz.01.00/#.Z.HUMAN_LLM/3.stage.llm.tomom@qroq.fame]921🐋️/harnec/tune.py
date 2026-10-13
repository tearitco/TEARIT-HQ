#!/usr/bin/env python3
import json, urllib.request, sys, re, time

def call_llama3(prompt, timeout=300):
    data = json.dumps({"model":"llama3:latest","prompt":prompt,"stream":False}).encode()
    req = urllib.request.Request("http://127.0.0.1:11434/api/generate",
                                  data=data, headers={"Content-Type":"application/json"})
    for _ in range(3):
        try:
            resp = urllib.request.urlopen(req, timeout=timeout)
            return json.loads(resp.read()).get("response", "")
        except Exception as e:
            print(f"  retry: {e}", file=sys.stderr); time.sleep(5)
    return ""

def main():
    vocab_file, prompt_text, behavior, edits_file = sys.argv[1:5]
    
    with open(vocab_file) as f:
        lines = f.read().split('\n')
    words = [l.strip().split()[1] for l in lines[1:] if l.strip().split() and len(l.strip().split())>=2]
    words_str = ', '.join(words[:40])
    
    # Build prompt with explicit tab character
    TAB = '\t'
    prompt = (
        "Weight engineer for small text model. ONLY edit weight and bias1-4 fields.\n\n"
        "Vocab words: " + words_str + "\n\n"
        "Fields: weight (0-1, generation probability), bias1-4 (0-1).\n"
        "ONLY use words from the vocab list.\n"
        "Prompt: " + prompt_text + "\n"
        "Goal: " + behavior + "\n\n"
        "Output lines: word" + TAB + "field" + TAB + "value\n"
        "Real TAB characters. No headers, no prose. Max 3 lines:\n"
    )
    
    print("Requesting from llama3...", flush=True)
    response = call_llama3(prompt)
    if not response:
        print("ERROR: no LLM response", file=sys.stderr); sys.exit(1)
    
    print("LLM response:")
    print(response[:300])
    
    valid_fields = {'weight','bias1','bias2','bias3','bias4'}
    edits = []
    for line in response.strip().split('\n'):
        parts = line.split('\t')
        if len(parts) != 3:
            parts = [p.strip() for p in re.split(r'<tab>', line)]
        if len(parts) == 3:
            w, f, v = parts[0].strip(), parts[1].strip(), parts[2].strip()
            if f in valid_fields and w in words:
                try:
                    val = float(v)
                    if 0.0 <= val <= 1.0:
                        edits.append((w, f, val))
                except ValueError: pass
    
    with open(edits_file, 'w') as f:
        for w, fld, v in edits:
            f.write(f"{w}\t{fld}\t{v}\n")
    print(f"\nValid edits: {len(edits)}")
    for w, f, v in edits:
        print(f"  {w}.{f} = {v}")

if __name__ == '__main__':
    main()
