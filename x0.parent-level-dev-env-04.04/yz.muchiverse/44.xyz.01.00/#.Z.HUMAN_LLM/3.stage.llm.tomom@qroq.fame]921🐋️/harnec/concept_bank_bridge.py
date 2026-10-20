#!/usr/bin/env python3
"""
concept_bank_bridge.py — Bridge between TEARIT Concept Bank spokes (.pdl) and tomom vocab (.txt) files.

This script enables the hand-tuning harness by:
1. Reading spoke files (.pdl) that contain editable WEIGHT values
2. Finding corresponding tokens in tomom vocab .txt files
3. Applying weight edits and logging them as git-diffable changes
4. Syncing EDIT records from tune.py into the promotion_ledger format

Usage:
  # Sync a single spoke to its corresponding vocab entry:
  concept_bank_bridge.py --spoke <path.pdl> --vocab <path.txt> --apply

  # Sync all spokes in a directory to vocab files:
  concept_bank_bridge.py --spoke-dir <spokes/> --vocab-dir <curriculum/> --sync --dry-run

  # Generate EDIT records from a vocab edit:
  concept_bank_bridge.py --vocab <path.txt> --edit-weight <token> <weight> --emits-edits <file>

  # Import EDIT records from tune.py output:
  concept_bank_bridge.py --import-edits <file> --concept-bank <bank_dir>
"""
import argparse
import os
import re
import sys
import uuid
import shutil
from pathlib import Path
from datetime import datetime


def parse_spoke_file(spoke_path: str) -> dict:
    """
    Parse a .pdl spoke file and extract SLOT/POINTS_TO/WEIGHT triples.
    
    Returns:
        {
            "node": "gravity_constant",
            "kind": "spoke",
            "slots": [
                {"idx": 0, "master": "force", "weight": 1.0},
            ]
        }
    """
    result = {"node": None, "kind": None, "slots": [], "raw_lines": []}
    
    with open(spoke_path, 'r') as f:
        lines = f.readlines()
    
    for line in lines:
        line = line.strip()
        result["raw_lines"].append(line)
        
        if line.startswith("NODE |"):
            result["node"] = line.split("|")[1].strip()
        elif line.startswith("KIND |"):
            result["kind"] = line.split("|")[1].strip()
        elif line.startswith("SLOT |"):
            # Parse: SLOT | 0 | POINTS_TO=force | WEIGHT=1.0000
            parts = [p.strip() for p in line.split("|")]
            if len(parts) >= 4:
                idx = int(parts[1])
                points_to_match = re.search(r'POINTS_TO=(\S+)', parts[2])
                weight_match = re.search(r'WEIGHT=([\d.+-eE]+)', parts[3])
                
                slot = {
                    "idx": idx,
                    "master": points_to_match.group(1) if points_to_match else None,
                    "weight": float(weight_match.group(1)) if weight_match else 0.0,
                }
                result["slots"].append(slot)
    
    return result


def write_spoke_file(spoke_path: str, spoke_data: dict, backup: bool = True):
    """
    Write updated spoke data back to .pdl file.
    Preserves comments and format.
    """
    if backup:
        shutil.copy2(spoke_path, spoke_path + ".bak")
    
    lines = []
    original_lines = spoke_data["raw_lines"]
    
    slot_idx = 0
    for line in original_lines:
        if line.startswith("SLOT |"):
            if slot_idx < len(spoke_data["slots"]):
                slot = spoke_data["slots"][slot_idx]
                new_line = f"SLOT | {slot['idx']} | POINTS_TO={slot['master']} | WEIGHT={slot['weight']:.4f}"
                lines.append(new_line)
                slot_idx += 1
            else:
                lines.append(line)
        else:
            lines.append(line)
    
    with open(spoke_path, 'w') as f:
        f.write('\n'.join(lines) + '\n')


def parse_vocab_file(vocab_path: str) -> tuple:
    """
    Parse a tomom vocab .txt file.
    Format: header line with column names (space-separated), then data rows.
    Each row: number word embedding_floats pe_floats weight bias1 bias2 bias3 bias4
    Embedding and PE are variable-length float arrays, everything else is scalar.
    
    Returns (list of dicts, column_names).
    """
    with open(vocab_path, 'r') as f:
        lines = f.readlines()
    
    # Parse header
    header_line = lines[0].strip()
    columns = header_line.split()
    
    if len(columns) < 6:
        print(f"WARNING: Expected 6+ columns, got {len(columns)}", file=sys.stderr)
    
    vocab_entries = []
    for line in lines[1:]:
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        
        tokens = line.split()
        entry = {}
        
        # Layout: [number] [word] [embedding_floats...] [pe_floats...] [weight] [bias1] [bias2] [bias3] [bias4]
        # number = tokens[0], word = tokens[1]
        # bias4 = tokens[-1], bias3 = tokens[-2], bias2 = tokens[-3], bias1 = tokens[-4], weight = tokens[-5]
        # pe = previous 32 floats (or variable), embedding = float tokens before pe
        
        if len(tokens) < 7:
            continue
        
        entry['number'] = int(tokens[0])
        entry['word'] = tokens[1]
        
        num_scalar_after = 5  # weight, bias1, bias2, bias3, bias4
        total_floats = len(tokens) - 2 - num_scalar_after
        half_floats = total_floats // 2
        
        entry['embedding'] = [float(x) for x in tokens[2 : 2 + half_floats]]
        entry['pe'] = [float(x) for x in tokens[2 + half_floats : 2 + total_floats]]
        entry['weight'] = float(tokens[2 + total_floats])
        entry['bias1'] = float(tokens[2 + total_floats + 1])
        entry['bias2'] = float(tokens[2 + total_floats + 2])
        entry['bias3'] = float(tokens[2 + total_floats + 3])
        entry['bias4'] = float(tokens[2 + total_floats + 4])
        
        vocab_entries.append(entry)
    
    return vocab_entries, columns


def write_vocab_file(vocab_path: str, vocab_entries: list, columns: list, backup: bool = True):
    """
    Write vocab entries back to .txt file.
    Only the 'weight' and 'freq' columns should be modified.
    """
    if backup:
        shutil.copy2(vocab_path, vocab_path + ".bak")
    
    def format_entry(entry):
        parts = []
        for col in columns:
            val = entry.get(col, '')
            if isinstance(val, list):
                parts.append(' '.join(f"{x:.6f}" for x in val))
            elif isinstance(val, float):
                if col == 'number':
                    parts.append(str(int(val)))
                else:
                    parts.append(f"{val:.6f}")
            elif isinstance(val, int):
                parts.append(str(val))
            else:
                parts.append(str(val))
        return ' '.join(parts)
    
    with open(vocab_path, 'w') as f:
        f.write(' '.join(columns) + '\n')
        for entry in vocab_entries:
            f.write(format_entry(entry) + '\n')


def find_vocab_entry(vocab_entries: list, token: str) -> dict:
    """Find a token in the vocab entries."""
    for entry in vocab_entries:
        if entry.get('word') == token:
            return entry
    return None


def emit_tearit_edit_record(target: str, slot: str, delta: float, reason: str, 
                            proposer: str = "llama3", edit_type: str = "spoke_weight_delta") -> str:
    """
    Generate a TEARIT EDIT record for the promotion ledger.
    Format per A-TEARIT-IS-ALL-YOU-NEED.md §2.3:
      EDIT | id=<uuid> | type=spoke_weight_delta | target=<word> | slot=weight
        | delta=<change> | reason="<prompt text>" | proposer=llama3 | status=candidate
    """
    edit_id = str(uuid.uuid4())[:8]
    timestamp = datetime.now().isoformat()
    
    record = (f"EDIT | id={edit_id} | type={edit_type} | target={target} | slot={slot} "
              f"| delta={delta:+.4f} | reason=\"{reason}\" | proposer={proposer} "
              f"| status=candidate | timestamp={timestamp}")
    
    return record


def edit_vocab_weight(vocab_path: str, token: str, new_weight: float, 
                       backup: bool = True, verbose: bool = True) -> dict:
    """
    Edit the weight of a specific token in a vocab file.
    Returns a dict with old_weight, new_weight, and the EDIT record.
    """
    entries, columns = parse_vocab_file(vocab_path)
    entry = find_vocab_entry(entries, token)
    
    if not entry:
        print(f"ERROR: Token '{token}' not found in {vocab_path}", file=sys.stderr)
        return None
    
    old_weight = entry.get('weight', 0.0)
    delta = new_weight - old_weight
    entry['weight'] = new_weight
    
    if verbose:
        print(f"✏️ {vocab_path}")
        print(f"   Token: {token}")
        print(f"   Weight: {old_weight:.4f} → {new_weight:.4f} (delta: {delta:+.4f})")
    
    # Write the change
    write_vocab_file(vocab_path, entries, columns, backup=backup)
    
    # Emit EDIT record
    edit_record = emit_tearit_edit_record(
        target=token,
        slot="weight",
        delta=delta,
        reason=f"Manual weight edit via concept_bank_bridge"
    )
    
    return {
        "token": token,
        "old_weight": old_weight,
        "new_weight": new_weight,
        "delta": delta,
        "edit_record": edit_record,
        "vocab_file": vocab_path
    }


def sync_spoke_to_vocab(spoke_path: str, vocab_path: str, dry_run: bool = True, 
                         verbose: bool = True) -> list:
    """
    Sync a spoke file's WEIGHT values to the corresponding tomom vocab file.
    If the spoke's master name matches a vocab token, update that token's weight.
    """
    spoke_data = parse_spoke_file(spoke_path)
    if not spoke_data["slots"]:
        print(f"INFO: No slots in {spoke_path}", file=sys.stderr)
        return []
    
    entries, columns = parse_vocab_file(vocab_path)
    changes = []
    
    for slot in spoke_data["slots"]:
        master = slot["master"]
        target_weight = slot["weight"]
        
        entry = find_vocab_entry(entries, master)
        if not entry:
            if verbose:
                print(f"  ⚠️  Spoke target '{master}' not found in vocab")
            continue
        
        old_weight = entry.get('weight', 0.0)
        
        if abs(old_weight - target_weight) > 0.0001:
            if verbose:
                print(f"  ✏️ {master}: weight {old_weight:.4f} → {target_weight:.4f}")
            
            entry['weight'] = target_weight
            changes.append({
                "token": master,
                "old_weight": old_weight,
                "new_weight": target_weight,
                "delta": target_weight - old_weight,
                "spoke": spoke_data["node"],
            })
    
    if changes and not dry_run:
        if verbose:
            print(f"  💾 Writing changes to {vocab_path}")
        write_vocab_file(vocab_path, entries, columns, backup=True)
    
    return changes


def sync_all_spokes(spoke_dir: str, vocab_dir: str, dry_run: bool = True, 
                     verbose: bool = True) -> list:
    """
    Sync all spokes in a directory to corresponding vocab files.
    Maps spoke name → vocab file name (e.g., quadratic → Quadratic/Quadratic.txt).
    """
    all_changes = []
    
    spoke_files = [f for f in os.listdir(spoke_dir) if f.endswith('.pdl')]
    
    for spoke_file in sorted(spoke_files):
        spoke_path = os.path.join(spoke_dir, spoke_file)
        spoke_data = parse_spoke_file(spoke_path)
        
        if verbose:
            print(f"\n🧩 Processing spoke: {spoke_file} (node={spoke_data['node']})")
        
        # Match spoke to vocab file: spoke "quadratic" → search all vocab files
        # The spoke's node name should match a token in some vocab file
        for root, dirs, files in os.walk(vocab_dir):
            for fname in files:
                if fname.endswith('.txt') and not fname.startswith('.'):
                    vocab_path = os.path.join(root, fname)
                    try:
                        changes = sync_spoke_to_vocab(spoke_path, vocab_path, 
                                                      dry_run=dry_run, verbose=verbose)
                        if changes:
                            for c in changes:
                                c["vocab_file"] = vocab_path
                            all_changes.extend(changes)
                    except Exception as e:
                        if verbose:
                            print(f"  ⚠️ Error: {e}")
    
    return all_changes


def import_tune_edits(edits_file: str, concept_bank_dir: str, 
                       apply_to_spokes: bool = True, verbose: bool = True):
    """
    Import EDIT records from tune.py output and apply to concept bank spokes.
    Format of edits file: tab-separated word\tfield\tvalue
    """
    spoke_dir = os.path.join(concept_bank_dir, "data", "spokes")
    
    edits = []
    with open(edits_file, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split('\t')
            if len(parts) == 3:
                word, field, value = parts
                edits.append({"word": word, "field": field, "value": float(value)})
    
    if verbose:
        print(f"📥 Imported {len(edits)} edits from {edits_file}")
    
    # Apply to spoke files
    if apply_to_spokes and os.path.isdir(spoke_dir):
        for edit in edits:
            word = edit["word"]
            target_weight = edit["value"]
            spoke_name = f"{word}.pdl"
            spoke_path = os.path.join(spoke_dir, spoke_name)
            
            if os.path.isfile(spoke_path):
                spoke_data = parse_spoke_file(spoke_path)
                for slot in spoke_data["slots"]:
                    if slot["master"] == word or slot["idx"] == 0:
                        old_weight = slot["weight"]
                        slot["weight"] = target_weight
                        
                        if verbose:
                            print(f"✏️ {spoke_name}: slot {slot['idx']} "
                                  f"POINTS_TO={slot['master']} "
                                  f"WEIGHT={old_weight:.4f} → {target_weight:.4f}")
                        
                        write_spoke_file(spoke_path, spoke_data, backup=True)
                        break
    
    # Emit EDIT records for promotion_ledger
    edit_records = []
    for edit in edits:
        record = emit_tearit_edit_record(
            target=edit["word"],
            slot=edit["field"],
            delta=edit["value"],  # absolute value
            reason=f"tune.py suggestion for {edit['word']}"
        )
        edit_records.append(record)
    
    records_file = os.path.join(concept_bank_dir, "data", "pending_edits.txt")
    os.makedirs(os.path.dirname(records_file), exist_ok=True)
    with open(records_file, 'w') as f:
        for record in edit_records:
            f.write(record + '\n')
    
    if verbose:
        print(f"\n📝 {len(edit_records)} EDIT records saved to {records_file}")
        print("   Import into promotion_ledger:")
        print(f"   promotion_ledger.+x {concept_bank_dir} add_candidate {records_file}")
    
    return edit_records


def main():
    parser = argparse.ArgumentParser(
        description="Bridge TEARIT Concept Bank spokes (.pdl) ↔ tomom vocab (.txt) files",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Sync one spoke to a vocab file:
  %(prog)s --spoke spoke.pdl --vocab vocab.txt --apply

  # Dry-run sync all spokes:
  %(prog)s --sync --spoke-dir ./spokes/ --vocab-dir ./curriculum/ --dry-run

  # Edit a token weight directly:
  %(prog)s --edit-weight quadratic 1.23 --vocab curriculum/Grade8_math/Grade8_math.txt

  # Import tune.py edits:
  %(prog)s --import-edits edits.txt --concept-bank .
        """
    )
    
    parser.add_argument('--spoke', help='Single spoke .pdl file path')
    parser.add_argument('--vocab', help='Single vocab .txt file path')
    parser.add_argument('--spoke-dir', help='Directory containing .pdl spoke files')
    parser.add_argument('--vocab-dir', help='Directory containing vocab subdirectories')
    parser.add_argument('--sync', action='store_true', help='Sync all spokes to vocab files')
    parser.add_argument('--apply', action='store_true', default=False, 
                        help='Apply changes (default: dry-run)')
    parser.add_argument('--dry-run', action='store_true', default=False,
                        help='Show what would change without writing')
    parser.add_argument('--edit-weight', nargs=2, metavar=('TOKEN', 'WEIGHT'),
                        help='Edit weight of TOKEN to WEIGHT in the given vocab file')
    parser.add_argument('--import-edits', help='Import tab-separated edits from tune.py output')
    parser.add_argument('--concept-bank', help='Concept bank root directory')
    parser.add_argument('--verbose', '-v', action='store_true', default=True)
    
    args = parser.parse_args()
    
    changes = []
    
    # Single spoke to vocab sync
    if args.spoke and args.vocab:
        dry = args.dry_run and not args.apply
        changes = sync_spoke_to_vocab(args.spoke, args.vocab, dry_run=dry, verbose=args.verbose)
        if dry and changes:
            print("\n⚠️  Dry run — no files written. Use --apply to commit changes.")
    
    # Sync all spokes
    elif args.sync and args.spoke_dir and args.vocab_dir:
        dry = args.dry_run and not args.apply
        changes = sync_all_spokes(args.spoke_dir, args.vocab_dir, dry_run=dry, verbose=args.verbose)
        if dry and changes:
            print(f"\n⚠️  Dry run — {len(changes)} changes would be applied. Use --apply to commit.")
        elif changes and args.verbose:
            print(f"\n✅ Synced {len(changes)} token weights")
    
    # Edit single weight
    elif args.edit_weight and args.vocab:
        token, weight_str = args.edit_weight
        weight = float(weight_str)
        result = edit_vocab_weight(args.vocab, token, weight, backup=True, verbose=args.verbose)
        if result:
            print(f"\n📝 EDIT record:")
            print(f"   {result['edit_record']}")
    
    # Import tune.py edits
    elif args.import_edits and args.concept_bank:
        import_tune_edits(args.import_edits, args.concept_bank, verbose=args.verbose)
    
    else:
        parser.print_help()
        return 1
    
    return 0


if __name__ == '__main__':
    sys.exit(main())