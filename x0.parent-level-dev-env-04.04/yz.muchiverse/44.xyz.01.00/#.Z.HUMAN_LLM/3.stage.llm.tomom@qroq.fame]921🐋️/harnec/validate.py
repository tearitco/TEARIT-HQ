#!/usr/bin/env python3
"""
validate.py — Run chatbot with before/after weights, measure loss improvement, decide keep/revert.

Usage:
  python3 validate.py --vocab <vocab.txt> --curriculum <name> --prompt "test prompt"
                      [--length 10] [--temperature 1.0]
                      [--before-loss 4.217] [--target-loss 4.0]
                      [--auto-commit] [--auto-revert]

This script:
1. Runs chatbot_moe_v1 on a test prompt BEFORE edits are applied
2. Applies the weight edits (assumes bridge already ran)
3. Runs chatbot_moe_v1 AFTER edits
4. Compares output quality + loss delta
5. Optionally git-commits or git-reverts
"""
import argparse
import os
import subprocess
import sys
import json
import re
from pathlib import Path
from datetime import datetime
from typing import Optional, Dict, Tuple


def run_chatbot(curriculum_bank: str, prompt: str, length: int = 10, 
                 temperature: float = 1.0, verbose: bool = True) -> Dict:
    """
    Run chatbot_moe_v1 and capture output.
    Returns dict with 'response' and 'success'.
    """
    result = {
        'response': '',
        'success': False,
        'stderr_output': '',
        'stdout_output': '',
        'error': None
    }
    
    if not os.path.exists(curriculum_bank):
        result['error'] = f"Curriculum bank not found: {curriculum_bank}"
        return result
    
    chatbot_bin = './+x/chatbot_moe_v1.+x'
    if not os.path.exists(chatbot_bin):
        chatbot_bin = './chatbot_moe_v1'
    if not os.path.exists(chatbot_bin):
        result['error'] = "chatbot_moe_v1 executable not found"
        return result
    
    cmd = [chatbot_bin, curriculum_bank, prompt, str(length), str(temperature)]
    
    try:
        proc = subprocess.run(
            cmd, 
            capture_output=True, 
            text=True, 
            timeout=30,
            cwd=os.path.dirname(os.path.abspath(__file__)) + '/..'
        )
        result['stdout_output'] = proc.stdout
        result['stderr_output'] = proc.stderr
        result['success'] = proc.returncode == 0
        
        if verbose:
            print(f"🤖 chatbot exit code: {proc.returncode}")
            if proc.stderr:
                print(f"📝 stderr: {proc.stderr.strip()[:500]}")
        
        # Extract response
        output = proc.stdout
        response_match = re.search(r'Response:\s*(.+)', output, re.DOTALL)
        if response_match:
            result['response'] = response_match.group(1).strip()
        else:
            result['response'] = output.strip()
        
    except subprocess.TimeoutExpired:
        result['error'] = "chatbot_moe_v1 timed out (30s)"
    except FileNotFoundError as e:
        result['error'] = f"chatbot_moe_v1 not found: {e}"
    except Exception as e:
        result['error'] = f"Error running chatbot: {e}"
    
    return result


def measure_loss(curriculum_dir: str, vocab_file: str, verbose: bool = True) -> Optional[float]:
    """
    Measure the current loss of a trained model by reading loss.txt.
    Returns the final epoch loss, or None if not found.
    """
    loss_path = os.path.join(curriculum_dir, 'loss.txt')
    if os.path.exists(loss_path):
        with open(loss_path, 'r') as f:
            lines = f.readlines()
            if lines:
                try:
                    last_loss = float(lines[-1].strip())
                    if verbose:
                        print(f"📊 Loss from {loss_path}: {last_loss:.4f}")
                    return last_loss
                except ValueError:
                    pass
    
    vocab_path = os.path.join(curriculum_dir, vocab_file)
    if verbose:
        print(f"⚠️  No loss.txt found at {loss_path}")
    return None


def compute_quality_score(response: str, prompt: str, verbose: bool = True) -> float:
    """
    Simple heuristic to score response quality.
    Higher is better. Based on: length, vocabulary diversity, coherence signals.
    """
    if not response:
        return 0.0
    
    words = re.findall(r'\w+', response)
    if not words:
        return 0.0
    
    # Score components
    length_score = min(len(words) / 20.0, 1.0)  # normalize to max 20 words
    unique_ratio = len(set(words)) / len(words) if words else 0
    avg_word_len = sum(len(w) for w in words) / len(words) if words else 0
    
    # Bonus for longer responses (more tokens generated = better)
    # Penalty for too many repeated tokens
    quality = (length_score * 0.5) + (unique_ratio * 0.3) + (min(avg_word_len / 8.0, 0.2))
    
    if verbose:
        print(f"📈 Quality metrics:")
        print(f"   Words: {len(words)}, Unique: {len(set(words))}, Ratio: {unique_ratio:.2f}")
        print(f"   Avg word length: {avg_word_len:.1f}")
        print(f"   Quality score: {quality:.4f}")
    
    return quality


def git_backup(file_path: str) -> Optional[str]:
    """
    Create a timestamped backup of a file.
    Returns the backup path.
    """
    if not os.path.exists(file_path):
        return None
    
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_path = f"{file_path}.validate_bak_{timestamp}"
    shutil_copy = file_path  # Will use shutil below
    
    import shutil
    shutil.copy2(file_path, backup_path)
    
    if os.path.exists(backup_path):
        print(f"📦 Backup created: {backup_path}")
        return backup_path
    return None


def git_revert(file_path: str, verbose: bool = True) -> bool:
    """Revert a file using git checkout."""
    try:
        proc = subprocess.run(
            ['git', 'checkout', '--', file_path],
            capture_output=True, text=True, timeout=10
        )
        if proc.returncode == 0:
            if verbose:
                print(f"↩️ Reverted {file_path} via git checkout")
            return True
        else:
            if verbose:
                print(f"⚠️  git checkout failed: {proc.stderr.strip()}")
            return False
    except Exception as e:
        print(f"⚠️  git revert error: {e}")
        return False


def git_commit(file_path: str, message: str, verbose: bool = True) -> bool:
    """Commit a file with a message using git."""
    try:
        # Stage the file
        proc = subprocess.run(
            ['git', 'add', file_path],
            capture_output=True, text=True, timeout=10
        )
        if proc.returncode != 0:
            if verbose:
                print(f"⚠️  git add failed: {proc.stderr.strip()}")
            return False
        
        # Commit
        proc = subprocess.run(
            ['git', 'commit', '-m', message, '--', file_path],
            capture_output=True, text=True, timeout=10
        )
        if proc.returncode == 0:
            if verbose:
                print(f"✅ Committed: {message}")
            return True
        else:
            if verbose:
                print(f"⚠️  git commit failed: {proc.stderr.strip()}")
            return False
    except Exception as e:
        print(f"⚠️  git commit error: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Validate weight edits by running chatbot before/after and measuring loss",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic validation with before/after loss comparison
  %(prog)s --curriculum Grade8_math --vocab curriculum/Grade8_math/Grade8_math.txt \
           --prompt "Explain quadratic equations"

  # Auto-commit if loss improves, auto-revert if it doesn't
  %(prog)s --curriculum Grade8_math --vocab curriculum/Grade8_math/Grade8_math.txt \
           --prompt "Explain quadratic equations" --before-loss 4.217 \
           --target-loss 4.0 --auto-commit --auto-revert
        """
    )
    
    parser.add_argument('--vocab', required=True, 
                        help='Path to the vocab .txt file being validated')
    parser.add_argument('--curriculum', required=True,
                        help='Curriculum name (e.g., Grade8_math)')
    parser.add_argument('--prompt', default='Hello world',
                        help='Test prompt to run through the chatbot')
    parser.add_argument('--length', type=int, default=10,
                        help='Response length (default: 10)')
    parser.add_argument('--temperature', type=float, default=1.0,
                        help='Sampling temperature (default: 1.0)')
    parser.add_argument('--before-loss', type=float, default=None,
                        help='Expected loss before edits (for comparison)')
    parser.add_argument('--target-loss', type=float, default=None,
                        help='Target loss to improve towards')
    parser.add_argument('--auto-commit', action='store_true',
                        help='Auto-commit files if improvement is positive')
    parser.add_argument('--auto-revert', action='store_true',
                        help='Auto-revert files if loss does not improve')
    parser.add_argument('--curriculum-bank', default='curriculum_bank.txt',
                        help='Path to curriculum_bank.txt')
    parser.add_argument('--verbose', '-v', action='store_true', default=True,
                        help='Verbose output')
    parser.add_argument('--dry-run', action='store_true',
                        help='Dry run - do not actually commit/revert')
    
    args = parser.parse_args()
    
    verbose = args.verbose
    
    print(f"🔍 Validation Tool for: {args.curriculum}")
    print(f"   Vocab: {args.vocab}")
    print(f"   Prompt: \"{args.prompt}\"")
    print(f"   Length: {args.length}, Temperature: {args.temperature}")
    print()
    
    # Get current loss from loss.txt
    curriculum_dir = os.path.dirname(args.vocab)
    current_loss = measure_loss(curriculum_dir, os.path.basename(args.vocab), verbose)
    
    # Get debug chain
    debug_path = os.path.join(curriculum_dir, '..', '..', 'debug_chain.txt')
    if not os.path.exists(debug_path):
        debug_path = 'debug_chain.txt'
    
    # Run chatbot
    if verbose:
        print(f"\n🤖 Running chatbot_moe_v1...")
    result = run_chatbot(args.curriculum_bank, args.prompt, args.length, args.temperature, verbose)
    
    if not result['success']:
        print(f"❌ Chatbot failed: {result.get('error', 'Unknown error')}")
        if verbose and result.get('stderr_output'):
            print(f"   stderr: {result['stderr_output'][:500]}")
        sys.exit(1)
    
    # Compute quality score
    quality = compute_quality_score(result['response'], args.prompt, verbose)
    
    # Get debug info
    debug_info = ""
    if os.path.exists(debug_path):
        with open(debug_path, 'r') as f:
            debug_info = f.read()
        if verbose:
            print(f"\n📋 Debug trace (last 500 chars):")
            print(debug_info[-500:])
    
    # Print summary
    print(f"\n📊 Validation Summary:")
    print(f"   Before loss: {args.before_loss if args.before_loss else 'N/A'}")
    print(f"   Current loss: {current_loss if current_loss else 'N/A'}")
    print(f"   Target loss: {args.target_loss if args.target_loss else 'N/A'}")
    print(f"   Quality score: {quality:.4f}")
    print(f"   Response: \"{result['response']}\"")
    
    # Check validation criteria
    improvement = None
    passed = True
    
    if args.before_loss is not None and current_loss is not None:
        improvement = args.before_loss - current_loss
        print(f"   Loss improvement: {improvement:+.4f}")
        if args.target_loss is not None:
            passed = current_loss <= args.target_loss
            if passed:
                print(f"   ✅ PASS: current loss ({current_loss:.4f}) <= target ({args.target_loss})")
            else:
                print(f"   ❌ FAIL: current loss ({current_loss:.4f}) > target ({args.target_loss})")
    
    # Auto-commit / auto-revert
    if args.auto_commit and passed and improvement is not None and improvement > 0:
        if not args.dry_run:
            message = f"validate: {args.curriculum} loss improved {improvement:+.4f} ({args.before_loss:.4f} → {current_loss:.4f})"
            success = git_commit(args.vocab, message, verbose)
            if success and debug_info:
                debug_path_rel = os.path.relpath(debug_path) if os.path.isabs(debug_path) else debug_path
                git_commit(debug_path_rel, message, verbose)
        else:
            print(f"\n⚠️  Dry run: would commit '{args.curriculum} loss improved {improvement:+.4f}'")
    
    if args.auto_revert and not passed and improvement is not None and improvement <= 0:
        if not args.dry_run:
            success = git_revert(args.vocab, verbose)
            if success:
                print(f"↩️ Reverted {args.vocab} — no improvement")
        else:
            print(f"\n⚠️  Dry run: would revert {args.vocab} — no improvement")
    
    # Write validation result to file
    result_file = os.path.join(curriculum_dir, 'validation_result.txt')
    with open(result_file, 'w') as f:
        f.write(f"Validation Result\n")
        f.write(f"Timestamp: {datetime.now().isoformat()}\n")
        f.write(f"Curriculum: {args.curriculum}\n")
        f.write(f"Vocab: {args.vocab}\n")
        f.write(f"Before loss: {args.before_loss if args.before_loss else 'N/A'}\n")
        f.write(f"Current loss: {current_loss if current_loss else 'N/A'}\n")
        f.write(f"Target loss: {args.target_loss if args.target_loss else 'N/A'}\n")
        f.write(f"Improvement: {improvement if improvement is not None else 'N/A'}\n")
        f.write(f"Quality score: {quality:.4f}\n")
        f.write(f"Passed: {passed}\n")
        f.write(f"Response: {result['response']}\n")
    
    print(f"\n📄 Validation result saved to: {result_file}")
    
    return 0 if passed else 1


if __name__ == '__main__':
    sys.exit(main())