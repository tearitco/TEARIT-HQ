#!/usr/bin/env python3
"""
tomom_dashboard.py — Manager process for the tomom hand-tuning dashboard.

Generates the dynamic .chtpm content that the tomom_dashboard.chtpm layout
references via ${variable} placeholders. Reads from:
  - curriculum_bank.txt (list of 27 expert paths)
  - curriculum/*/loss.txt (per-subject loss)
  - debug_chain.txt (routing decisions from latest generation)
  - curriculum/<name>/<name>.txt (vocab files for token inspection)

Output: tomom_dashboard_data.txt — a plain-text projection file that the
chtpm renderer reads as ${variable} replacements.

Usage:
  python3 tomom_dashboard.py --refresh
  python3 tomom_dashboard.py --generate "math" --length 6 --temp 1.0
  python3 tomom_dashboard.py --edit-weight "equations" 0.97 --vocab curriculum/Grade8_math/Grade8_math.txt
  python3 tomom_dashboard.py --watch  (live-updating loop)
"""
import argparse
import os
import re
import sys
import time
import json
import subprocess
from pathlib import Path
from datetime import datetime

# Resolve project root (parent of harnec/)
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent


def load_curriculum_bank():
    """Load the list of curriculum paths from curriculum_bank.txt."""
    bank_path = PROJECT_ROOT / "curriculum_bank.txt"
    if not bank_path.exists():
        return []
    paths = []
    with open(bank_path) as f:
        for line in f:
            line = line.strip()
            if line:
                paths.append(line)
    return paths


def load_loss_data():
    """Load loss data from each curriculum's loss.txt."""
    losses = {}
    bank = load_curriculum_bank()
    for path in bank:
        # path is like "curriculum/Grade1_math/Grade1_math.txt"
        # loss.txt is in the parent dir (curriculum/Grade1_math/loss.txt)
        curriculum_dir = (PROJECT_ROOT / path).parent
        loss_path = curriculum_dir / 'loss.txt'
        if loss_path.exists():
            with open(loss_path) as f:
                lines = f.readlines()
                if lines:
                    try:
                        losses[path] = float(lines[-1].strip())
                    except ValueError:
                        pass
    return losses


def load_results_files():
    """Load .results.txt files from harnec/ for hand-tuning results."""
    results = {}
    harnec = PROJECT_ROOT / "harnec"
    for rf in harnec.glob("*.results.txt"):
        name = rf.stem.replace('.results', '')
        with open(rf) as f:
            content = f.read()
            results[name] = content[:500]
    return results


def load_debug_chain():
    """Parse debug_chain.txt for routing decisions and final response."""
    debug_path = PROJECT_ROOT / "debug_chain.txt"
    if not debug_path.exists():
        return {"response": "", "routing": [], "expert_count": 0}
    
    content = debug_path.read_text()
    
    # Extract final response
    response = ""
    response_match = re.search(r'=== Final Response ===\n(.+?)\n======================', content, re.DOTALL)
    if response_match:
        response = response_match.group(1).strip()
    
    # Extract expert count
    expert_count = 0
    using_match = re.search(r'Using (\d+) training sets', content)
    if using_match:
        expert_count = int(using_match.group(1))
    
    # Extract routing — the last selected expert is typically where the response came from
    # For now, we capture the response and expert count
    routing = []
    expert_matches = re.findall(r'(\d+)\.\s+(curriculum/.+?\.txt)', content)
    for num, path in expert_matches:
        routing.append({"idx": int(num), "path": path})
    
    return {
        "response": response,
        "routing": routing[-5:] if len(routing) > 5 else routing,  # last 5
        "expert_count": expert_count
    }


def format_curriculum_list(losses: dict, bank: list) -> str:
    """Format curriculum list with loss data for chtpm text element."""
    lines = []
    for i, path in enumerate(bank, 1):
        name = Path(path).stem
        loss = losses.get(path, 0.0)
        loss_str = f"{loss:.3f}" if loss else "N/A"
        lines.append(f"  [{i:2d}] {name:<30} loss={loss_str}")
    return '\n'.join(lines)


def format_routing_decisions(debug: dict) -> str:
    """Format routing decisions for display."""
    lines = []
    lines.append(f"Experts loaded: {debug['expert_count']}")
    lines.append(f"Merged vocabulary size: 2610")
    if debug['routing']:
        lines.append("Last selected experts:")
        for r in debug['routing']:
            lines.append(f"  Expert {r['idx']}: {r['path']}")
    else:
        lines.append("  (run a generation to see routing)")
    return '\n'.join(lines)


def format_chat_bubbles(debug: dict) -> str:
    """Format chat history as bubbles."""
    lines = []
    if debug['response']:
        lines.append("[USER] math")
        lines.append("[ROUTER] cross-expert merge across 27 experts (dot-product scoring)")
        lines.append("[EXPERT] response generated from top-weighted experts")
        lines.append(f"[MODEL] {debug['response']}")
    else:
        lines.append("[SYSTEM] No recent generation. Run 'Generate' to see tokens.")
    return '\n'.join(lines)


def format_token_inspector(vocab_path: Path, word: str = None) -> str:
    """Format vocab token inspector. If word given, show its details."""
    if not vocab_path or not vocab_path.exists():
        return "No vocab file available for inspection."
    
    lines = []
    if word:
        with open(vocab_path) as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) >= 2 and parts[1] == word:
                    # Parse: number word embedding(32) pe(32) weight bias1-4
                    num = parts[0]
                    weight = float(parts[-5]) if len(parts) >= 7 else 0.0
                    emb_str = ' '.join(parts[2:-37]) if len(parts) > 7 else '...'
                    lines.append(f"  Token: {word}")
                    lines.append(f"  Number: {num}")
                    lines.append(f"  Weight: {weight:.4f}")
                    emb_preview = parts[2:7] if len(parts) > 6 else []
                    lines.append(f"  Embedding (first 5): {' '.join(emb_preview)}")
                    lines.append(f"  Editable: weight (0.00-1.00)")
                    found = True
                    break
        if not found:
            lines.append(f"  Word '{word}' not found in {vocab_path.name}")
    else:
        lines.append(f"  Showing first 10 tokens from {vocab_path.name}:")
        with open(vocab_path) as f:
            next(f)  # skip header
            for i, line in enumerate(f):
                if i >= 10:
                    break
                parts = line.strip().split()
                if len(parts) >= 3:
                    num, word_name = parts[0], parts[1]
                    weight = float(parts[-5]) if len(parts) >= 7 else 0.0
                    lines.append(f"  [{num:>3}] {word_name:<20} weight={weight:.4f}")
    
    return '\n'.join(lines)


def format_stats(losses: dict, debug: dict) -> str:
    """Format dashboard stats summary."""
    num_subjects = len(losses)
    if losses:
        avg_loss = sum(losses.values()) / len(losses)
        min_loss = min(losses.values())
        max_loss = max(losses.values())
    else:
        avg_loss = min_loss = max_loss = 0.0
    
    lines = []
    lines.append(f"Subjects: {num_subjects}/27 trained")
    lines.append(f"Avg loss: {avg_loss:.4f} | Min: {min_loss:.4f} | Max: {max_loss:.4f}")
    lines.append(f"Experts in MoE: {debug['expert_count']}")
    lines.append(f"Last response: {debug['response'][:0]}..." if len(debug['response']) > 50 else f"Last response: {debug['response']}")
    lines.append(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M')}")
    return '\n'.join(lines)


def generate_projection(output_file: str = None):
    """Generate the data projection file for chtpm rendering."""
    if output_file is None:
        output_file = PROJECT_ROOT / "pieces" / "chtpm" / "layouts" / "tomom_dashboard_data.txt"
    else:
        output_file = Path(output_file)
    
    output_file.parent.mkdir(parents=True, exist_ok=True)
    
    bank = load_curriculum_bank()
    losses = load_loss_data()
    results = load_results_files()
    debug = load_debug_chain()
    
    # Find the first vocab with tokens for inspection
    vocab_path = None
    if bank:
        vocab_path = PROJECT_ROOT / bank[0]
    
    content = []
    content.append(f"# Tomom Dashboard Data — generated {datetime.now().isoformat()}")
    content.append(f"# This file is read by tomom_dashboard.chtpm")
    content.append("")
    content.append(f"[${{dashboard_stats}}]")
    content.append(format_stats(losses, debug))
    content.append("")
    content.append(f"[${{curriculum_list}}]")
    content.append(format_curriculum_list(losses, bank))
    content.append("")
    content.append(f"[${{routing_decisions}}]")
    content.append(format_routing_decisions(debug))
    content.append("")
    content.append(f"[${{chat_bubbles}}]")
    content.append(format_chat_bubbles(debug))
    content.append("")
    content.append(f"[${{token_inspector}}]")
    content.append(format_token_inspector(vocab_path))
    content.append("")
    content.append(f"[${{piece_methods}}]")
    content.append("[1] Generate  [2] Apply Weight Edit  [3] Hand-Tune Cycle  [4] Refresh")
    
    with open(output_file, 'w') as f:
        f.write('\n'.join(content) + '\n')
    
    print(f"Projection written to {output_file}")
    return output_file


def run_dashboard():
    """Run the dashboard in live-updating mode."""
    print("=== Tomom Hand-Tuning Dashboard (live mode) ===")
    print("Press Ctrl+C to exit\n")
    
    while True:
        try:
            generate_projection()
            bank = load_curriculum_bank()
            losses = load_loss_data()
            debug = load_debug_chain()
            
            print(f"\r[{datetime.now().strftime('%H:%M:%S')}] Subjects: {len(losses)}/{len(bank)} | "
                  f"Experts: {debug['expert_count']} | Response: '{debug['response'][:50]}'", end='')
            sys.stdout.flush()
            
            time.sleep(5)
        except KeyboardInterrupt:
            print("\n\nDashboard stopped.")
            break


def main():
    parser = argparse.ArgumentParser(
        description="Tomom hand-tuning dashboard manager"
    )
    parser.add_argument('--refresh', action='store_true',
                        help='Generate/update the dashboard projection file')
    parser.add_argument('--watch', action='store_true',
                        help='Live-updating dashboard mode (refreshes every 5s)')
    parser.add_argument('--generate', metavar='PROMPT',
                        help='Run chatbot_moe_v1 with a prompt (writes debug_chain.txt)')
    parser.add_argument('--length', type=int, default=6,
                        help='Generation length (default: 6)')
    parser.add_argument('--temp', type=float, default=1.0,
                        help='Temperature (default: 1.0)')
    parser.add_argument('--edit-weight', nargs=2, metavar=('WORD', 'WEIGHT'),
                        help='Edit weight of WORD to WEIGHT')
    parser.add_argument('--vocab', help='Vocab file for --edit-weight')
    parser.add_argument('--output', help='Output projection file path')
    parser.add_argument('--verbose', '-v', action='store_true', default=True)
    
    args = parser.parse_args()
    
    if args.watch:
        run_dashboard()
        return
    
    if args.generate:
        # Run the chatbot
        bank = str(PROJECT_ROOT / "curriculum_bank.txt")
        chatbot_bin = PROJECT_ROOT / "+x" / "chatbot_moe_v1.+x"
        if not chatbot_bin.exists():
            chatbot_bin = PROJECT_ROOT / "chatbot_moe_v1"
        if not chatbot_bin.exists():
            print("❌ chatbot_moe_v1 executable not found", file=sys.stderr)
            sys.exit(1)
        
        cmd = [str(chatbot_bin), bank, args.generate, str(args.length), str(args.temp)]
        if args.verbose:
            print(f"Running: {' '.join(cmd)}")
        
        result = subprocess.run(
            cmd,
            cwd=str(PROJECT_ROOT),
            capture_output=True,
            text=True,
            timeout=60
        )
        if args.verbose:
            print(f"stdout: {result.stdout[:500]}")
            if result.stderr:
                print(f"stderr: {result.stderr[:500]}")
    
    if args.edit_weight:
        word, weight = args.edit_weight
        vocab = args.vocab or str(PROJECT_ROOT / "curriculum_bank.txt")
        bridge = Path(__file__).parent / "concept_bank_bridge.py"
        cmd = [
            sys.executable, str(bridge),
            '--edit-weight', word, weight,
            '--vocab', vocab
        ]
        if args.verbose:
            print(f"\n📋 Applying weight edit...")
        subprocess.run(cmd, cwd=str(PROJECT_ROOT), capture_output=not args.verbose, text=True, timeout=60)
    
    # Always refresh the projection
    generate_projection(args.output)


if __name__ == '__main__':
    main()
