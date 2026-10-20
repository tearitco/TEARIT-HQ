#!/usr/bin/env python3
"""
run_handtune.py — Orchestrate the full hand-tuning cycle for a tomom curriculum.

Flow:
  1. Run tune.py to get LLM-suggested weight edits (edits.txt)
  2. Run concept_bank_bridge.py to apply edits to vocab + emit EDIT records
  3. Run validate.py to measure loss/response quality before vs after
  4. If loss improved (or --force-commit), git commit the changes
     Otherwise git revert (restore from .bak)
  5. Save results to a timestamped .results.txt

Usage:
  python3 run_handtune.py --curriculum Mathematics \
      --prompt "Explain quadratic equations" \
      --behavior "Make 'equations' and 'variables' more likely to appear" \
      [--vocab curriculum/Mathematics/Mathematics.txt] \
      [--target-loss 4.0] [--force-commit] [--skip-ollama]
"""
import argparse
import os
import subprocess
import sys
import shutil
import json
from datetime import datetime
from pathlib import Path

HARNEC_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = HARNEC_DIR.parent


def run_step(name: str, cmd: list, cwd=None, env=None, timeout=120,
             allow_fail=False) -> subprocess.CompletedProcess:
    """Run a subprocess command and log it."""
    print(f"\n{'='*60}")
    print(f"📋 STEP: {name}")
    print(f"📍 CMD: {' '.join(cmd)}")
    print(f"{'='*60}")
    
    result = subprocess.run(
        cmd,
        cwd=cwd or str(PROJECT_ROOT),
        env=env,
        capture_output=True,
        text=True,
        timeout=timeout
    )
    
    if result.stdout:
        print(result.stdout[:2000])
    if result.stderr:
        print(f"STDERR: {result.stderr[:1000]}", file=sys.stderr)
    
    if result.returncode != 0 and not allow_fail:
        print(f"❌ Step failed with exit code {result.returncode}", file=sys.stderr)
        if not allow_fail:
            sys.exit(1)
    
    return result


def find_vocab(curriculum: str) -> Path:
    """Find the vocab.txt file for a given curriculum."""
    # Common locations
    candidates = [
        PROJECT_ROOT / "curriculum" / curriculum / f"{curriculum}.txt",
        PROJECT_ROOT / "curriculum" / curriculum / "vocab.txt",
        PROJECT_ROOT / "curriculum" / curriculum / f"{curriculum}.vocab.txt",
    ]
    
    # Search in curriculum dir
    curriculum_dirs = list((PROJECT_ROOT / "curriculum").glob("*"))
    for d in curriculum_dirs:
        if curriculum.lower() in d.name.lower():
            for ext in ['.txt', 'vocab.txt']:
                p = d / (d.name + '.txt')
                if p.exists():
                    return p
                p = d / 'vocab.txt'
                if p.exists():
                    return p
    
    # Check candidates
    for c in candidates:
        if c.exists():
            return c
    
    return None


def get_loss_from_validate(curriculum_dir: str, vocab_file: str) -> float | None:
    """Read loss from the validate.py output or loss.txt."""
    loss_path = Path(PROJECT_ROOT) / curriculum_dir / 'loss.txt'
    if loss_path.exists():
        with open(loss_path) as f:
            lines = f.readlines()
            if lines:
                try:
                    return float(lines[-1].strip())
                except ValueError:
                    pass
    
    # Check for .results.txt files
    results_files = sorted(Path(PROJECT_ROOT / curriculum_dir).glob("*.results.txt"),
                           key=lambda x: x.stat().st_mtime, reverse=True)
    for rf in results_files[:3]:
        with open(rf) as f:
            for line in f:
                if 'loss' in line.lower() and '=' in line:
                    try:
                        return float(line.split('=')[-1].strip())
                    except ValueError:
                        pass
    return None


def git_stage_and_commit(paths: list, message: str, branch: str = "opencode") -> bool:
    """Stage specific paths and commit them."""
    try:
        # Check current branch
        result = subprocess.run(
            ['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
            cwd=str(PROJECT_ROOT),
            capture_output=True, text=True
        )
        current_branch = result.stdout.strip()
        
        # Stage only our files
        for p in paths:
            subprocess.run(
                ['git', 'add', str(p)],
                cwd=str(PROJECT_ROOT),
                capture_output=True
            )
        
        # Verify staged files
        staged = subprocess.run(
            ['git', 'diff', '--cached', '--name-only'],
            cwd=str(PROJECT_ROOT),
            capture_output=True, text=True
        ).stdout.strip()
        
        print(f"Staged files:\n{staged}")
        
        if not staged:
            print("No files staged — nothing to commit")
            return False
        
        subprocess.run(
            ['git', 'commit', '-m', message, '--']
            + [str(p) for p in paths],
            cwd=str(PROJECT_ROOT),
            capture_output=True
        )
        return True
    except Exception as e:
        print(f"Git commit failed: {e}", file=sys.stderr)
        return False


def git_revert_file(filepath: Path, backup_path: Path) -> bool:
    """Restore a file from its backup."""
    try:
        if backup_path.exists():
            shutil.copy2(str(backup_path), str(filepath))
            return True
        else:
            # Try git checkout
            subprocess.run(
                ['git', 'checkout', '--', str(filepath)],
                cwd=str(PROJECT_ROOT),
                capture_output=True
            )
            return True
    except Exception as e:
        print(f"Revert failed: {e}", file=sys.stderr)
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Orchestrate hand-tuning cycle: tune → bridge → validate → commit/revert"
    )
    parser.add_argument('--curriculum', required=True,
                        help='Curriculum name (e.g., Grade8_math, Mathematics)')
    parser.add_argument('--prompt', required=True,
                        help='Test prompt for llama3 to generate edits')
    parser.add_argument('--behavior', required=True,
                        help='Desired behavior/goal for the weight edits')
    parser.add_argument('--vocab', help='Vocab file path (auto-detected if omitted)')
    parser.add_argument('--target-loss', type=float, default=None,
                        help='Target loss threshold for auto-commit')
    parser.add_argument('--force-commit', action='store_true',
                        help='Commit even if loss did not improve')
    parser.add_argument('--skip-ollama', action='store_true',
                        help='Skip the tune.py LLM call if edits.txt already exists')
    parser.add_argument('--auto-commit', action='store_true',
                        help='Commit changes if loss improved')
    parser.add_argument('--verbose', '-v', action='store_true', default=True)
    
    args = parser.parse_args()
    
    # Resolve vocab path
    if args.vocab:
        vocab_path = Path(args.vocab)
    else:
        vocab_path = find_vocab(args.curriculum)
    
    if not vocab_path or not vocab_path.exists():
        print(f"❌ Vocab file not found for curriculum: {args.curriculum}", file=sys.stderr)
        if args.vocab:
            print(f"  Specified path: {args.vocab}")
        sys.exit(1)
    
    print(f"📄 Using vocab: {vocab_path}")
    
    # Get curriculum directory (parent of vocab file)
    curriculum_dir = vocab_path.parent
    
    # Edits file path
    edits_file = HARNEC_DIR / f"{args.curriculum}.edits.txt"
    
    # Backup paths
    vocab_bak = vocab_path.with_suffix('.txt.bak')
    
    # Step 1: Run tune.py to get LLM-suggested edits
    if not edits_file.exists() or not args.skip_ollama:
        run_step(
            "tune.py — LLM weight suggestions",
            [
                sys.executable, str(HARNEC_DIR / 'tune.py'),
                str(vocab_path),
                args.prompt,
                args.behavior,
                str(edits_file)
            ],
            cwd=str(PROJECT_ROOT),
            timeout=300
        )
    else:
        print(f"\n📋 STEP: tune.py — skipped (--skip-ollama), using existing {edits_file}")
    
    if not edits_file.exists():
        print(f"❌ No edits file generated: {edits_file}", file=sys.stderr)
        sys.exit(1)
    
    # Step 2: Measure loss before edits
    print(f"\n📊 Measuring loss BEFORE edits...")
    before_loss = get_loss_from_validate(str(curriculum_dir), str(vocab_path.name))
    # Also run validate.py before
    run_step(
        "validate.py (before edits)",
        [
            sys.executable, str(HARNEC_DIR / 'validate.py'),
            '--vocab', str(vocab_path.name),
            '--curriculum', str(curriculum_dir),
            '--prompt', args.prompt,
            '--length', '10',
            '--temperature', '0.8'
        ],
        cwd=str(PROJECT_ROOT),
        timeout=60,
        allow_fail=True
    )
    after_validate = get_loss_from_validate(str(curriculum_dir), str(vocab_path.name))
    if before_loss is None:
        before_loss = after_validate
    print(f"  Before loss: {before_loss}")
    
    # Step 3: Apply edits via concept_bank_bridge
    run_step(
        "concept_bank_bridge.py — apply edits to vocab",
        [
            sys.executable, str(HARNEC_DIR / 'concept_bank_bridge.py'),
            '--import-edits', str(edits_file),
            '--concept-bank', str(PROJECT_ROOT)
        ],
        cwd=str(PROJECT_ROOT),
        timeout=60
    )
    
    # Step 4: Measure loss after edits
    print(f"\n📊 Measuring loss AFTER edits...")
    run_step(
        "validate.py (after edits)",
        [
            sys.executable, str(HARNEC_DIR / 'validate.py'),
            '--vocab', str(vocab_path.name),
            '--curriculum', str(curriculum_dir),
            '--prompt', args.prompt,
            '--length', '10',
            '--temperature', '0.8'
        ],
        cwd=str(PROJECT_ROOT),
        timeout=60,
        allow_fail=True
    )
    after_loss = get_loss_from_validate(str(curriculum_dir), str(vocab_path.name))
    print(f"  After loss: {after_loss}")
    
    # Step 5: Decide keep/revert
    result = {
        'timestamp': datetime.now().isoformat(),
        'curriculum': args.curriculum,
        'vocab_path': str(vocab_path),
        'prompt': args.prompt,
        'before_loss': before_loss,
        'after_loss': after_loss,
        'improved': False,
        'action': 'revert',
        'edits_file': str(edits_file)
    }
    
    if before_loss is not None and after_loss is not None:
        loss_delta = after_loss - before_loss
        result['loss_delta'] = loss_delta
        improved = loss_delta < 0  # lower loss is better
        result['improved'] = improved
        
        print(f"\n📈 Loss delta: {loss_delta:+.6f}")
        print(f"  {'✅ Improved' if improved else '❌ Did not improve'}")
        
        should_commit = improved or args.force_commit or args.auto_commit
        
        if should_commit:
            # Keep changes and commit
            files_to_commit = [vocab_path, vocab_bak]
            
            # Include EDIT records and any data files
            edit_records = PROJECT_ROOT / 'data' / 'pending_edits.txt'
            if edit_records.exists():
                files_to_commit.append(edit_records)
            
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            commit_msg = f"handtune({args.curriculum}): weight edits from llama3\n\nPrompt: {args.prompt}\nBehavior: {args.behavior}"
            
            committed = git_stage_and_commit(files_to_commit, commit_msg)
            result['action'] = 'commit' if committed else 'commit_failed'
            
            if committed:
                print(f"\n✅ Committed changes for {args.curriculum}")
            else:
                result['action'] = 'no_changes'
        else:
            # Revert
            if git_revert_file(vocab_path, vocab_bak):
                result['action'] = 'reverted'
                print(f"\n↩️  Reverted {vocab_path} from backup")
    else:
        print(f"\n⚠️  Could not measure loss delta — skipping auto-commit/revert")
        result['action'] = 'loss_unknown'
    
    # Save results
    results_file = HARNEC_DIR / f"{args.curriculum}.handtune.results.json"
    with open(results_file, 'w') as f:
        json.dump(result, f, indent=2)
    
    print(f"\n💾 Results saved to {results_file}")
    print(f"📋 Status: {result['action']}")
    
    return 0 if result['action'] in ('commit', 'reverted', 'no_changes') else 1


if __name__ == '__main__':
    sys.exit(main())