# 🪞 xyzai vs. real IQABOD — what the agent's own integration gets right, gets wrong, and doesn't know exists

> ✅ **UPDATE 2026-07-30: the headline finding below is FIXED — in two
> passes.** The first fix was verified only via a manual CLI sequence
> and was itself wrong for real runs (project_root is session-scoped in
> the real app, not the top-level project dir); a second pass, found by
> actually building and running the project's own real harness
> (`test-harn-same/scenarios/demo_iqabod_chat.sh`, real key injection,
> not a CLI shortcut), fixed it for real. See `TODO.txt` in this same
> directory for both passes and `!.xyzos-pitfalls+1.txt` PITFALL 60
> (with its own UPDATE section) / `!.xyzos-standards+1.txt` §37 + §37.1
> for the house-wide rules this generalized into. The investigation
> below is left as-is, unedited, as the real evidence trail that led to
> the fix — read it for the "why," not as a description of current
> (still-broken) state.

> "xyzai" here means this house's own AI integration layer:
> `045.muchi-pal-agent🤖️+1/ops/send_message.c` +
> `ops/check_response.c` + `pieces/registry/models/model_list.txt` — the
> multi-provider router (`ollama`/`gemini`/`llamacpp`/`iqabod`/`script`)
> that `gemma_strategy.c` dispatches through. "Real IQABOD" means the
> actual project on disk at
> `#.z.mirror_llm]z5]IQABOD🪞️+4/` (this house's own current copy — user-
> confirmed authoritative location). Everything below was verified by
> direct read AND, where noted, by actually running the real binary —
> not inferred from either side's comments about the other.

## 🔴 The headline finding: xyzai's own registry points at a project that no longer exists

`model_list.txt` — the file that tells `send_message.c` where IQABOD
lives — has this, verbatim:

```
iqabod-test|iqabod|/home/no/Desktop/🤖️🪤️🏠️/🚽️🧻️/🚽️🥡️-00.00/ZEST-10.00/x0.parent-level-dev-env-03.00/#.ref/^.IQABOD-llm-06.00|curriculum/test/test.txt
iqabod-daily-life|iqabod|/home/no/Desktop/🤖️🪤️🏠️/🚽️🧻️/🚽️🥡️-00.00/ZEST-10.00/x0.parent-level-dev-env-03.00/#.ref/^.IQABOD-llm-06.00|curriculum/daily_life/daily_life.txt
```

**Verified: that directory does not exist.** `ls` on it returns "No such
file or directory." This is a leftover from a prior top-level directory
reorganization (the emoji trail gives it away — `🚽️🧻️/🚽️🥡️-00.00/
ZEST-10.00` is an OLDER naming generation than this house's current
`🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/.../44.xyz❤️‍🔥️00.10`; this house has
clearly been renamed/reorganized more than once and this one registry
entry never got updated). The real, current project — the one the user
pointed at — lives at `#.z.mirror_llm]z5]IQABOD🪞️+4/` (the `mirror_llm`
naming and the `z5`/`z6` versioning suggest this is itself a snapshot of
an even more actively-developed copy elsewhere, but it's real, it's
here, it compiles, it runs, and it is closer to current than the dead
`ZEST-10.00` path by a wide margin — real training data, `.weights`
files, and a Dec-16 final report sit here).

**Practical consequence, confirmed by reading the exec path in
`send_message.c` (`if (chdir(api_url) != 0) _exit(127);`):** every single
call to `iqabod-test` or `iqabod-daily-life` from inside muchi-pal-agent
today fails at the very first step — `chdir()` into a nonexistent
directory — and the forked child just exits. This is not "IQABOD gives
bad answers," this is **"IQABOD never runs at all."** The "word-salad"
characterization from the earlier research pass in this handoff was
based on the underlying IQABOD project's own documented state, not on
having actually exercised xyzai's own current wiring — this doc corrects
that: xyzai's wiring is currently 100% broken, a strictly worse and more
immediately-fixable state than "runs but produces weak output."

## 🟡 Second finding: even the curriculum names are stale, not just the path

Even setting the dead path aside, `model_list.txt` references
`curriculum/test/test.txt` and `curriculum/daily_life/daily_life.txt`.
**Neither exists in the real, current IQABOD project.** Its actual
`curriculum/` directory has exactly two entries:

```
curriculum/guess_the_word/guess_the_word.txt            <- has a real
curriculum/guess_the_word/guess_the_word.txt.weights     <- trained checkpoint
curriculum/guess_the_letter/guess_the_letter.txt         <- NO .weights file, untrained
```

So `iqabod-daily-life` doesn't just point at a dead root, it names a
curriculum that never existed there in the first place (or existed in
an even older snapshot that predates BOTH the dead `ZEST-10.00` copy and
the current mirror — either way, gone). `iqabod-test` is closer — `test`
is a plausible name for what's now called `guess_the_word` or
`guess_the_letter` — but doesn't match either literally.

## 🟢 Third finding: everything ELSE about the integration is actually correct — verified by running it for real

This matters because it changes the fix from "rewrite the integration"
to "correct two stale strings." Confirmed by directly executing the real
binary with the real curriculum:

```
$ cd "#.z.mirror_llm]z5]IQABOD🪞️+4"
$ ./+x/main_orchestrator.+x generate curriculum/guess_the_word/guess_the_word.txt 0.8 60 "hello how are you"
...
Final generated text: hello how are you i cats he am he it likes he it love they plays is they
  it dogs they is cats likes <END> <PAD> food books <UNK> it we big
```

Compare against what `send_message.c` actually execs (line ~907):

```c
execl(orch_path, orch_path, "generate", model_name, "0.8", "60", prompt_text, NULL);
```

— `"generate" <curriculum> 0.8 60 <prompt>` — **exact match** to
`main_orchestrator.c`'s real `argc>=6` "generate" mode signature
(`mode, curriculum_file, temperature, max_tokens, prompt` at
`main_orchestrator.c` lines ~322-332). Not approximately right — the
literal argv shape send_message.c builds is byte-for-byte what the real
binary expects.

And compare against what `check_response.c` parses for (line ~254):

```c
const char *marker = "Final generated text: ";
```

— **exact match** to the real stdout line captured above, right down to
the trailing space in the marker. `check_response.c`'s own strip-the-
echoed-prompt-off-the-front logic also matches
`generation_module.c`'s real behavior (`output` starts as `strcpy(output,
prompt)` before tokens get appended — confirmed in the report at
`^.finall_report_dec16.txt` and directly observable in the captured
output above, where `"hello how are you"` is echoed verbatim before the
generated continuation begins).

**So: the engineering that reads IQABOD's real output format and builds
IQABOD's real invocation is correct, non-trivial, and was clearly built
by someone who actually read `generation_module.c`.** The only things
wrong are two copy-pasted strings in a registry file that never got
updated after a directory reorg. This is a five-minute fix, not a
redesign — see `TODO.txt` in this same directory.

## 🧠 What xyzai does NOT know about IQABOD that it probably should

- **IQABOD is a genuinely real transformer**, independently verified
  (the project's own `^.finall_report_dec16.txt`, an AI-authored
  architecture review, confirms real attention/RoPE/RMSNorm/SwiGLU/
  backprop/cross-entropy-loss/KV-caching, compared directly against
  Andrej Karpathy's reference `run.c` implementation). xyzai's own
  comments (`send_message.c`'s header) correctly call it "not JSON, no
  HTTP" but never characterize WHAT it is beyond that — worth a comment
  update once the path is fixed, so a future reader doesn't assume
  "iqabod" means "some external service" the way the other three
  provider_kinds do.
- **The curriculum format is deliberately unusual and worth knowing
  about**: not a binary weights blob, but a human-readable text file —
  one row per vocabulary token, with embedding/attention/FFN parameters
  spelled out as plain numbers per token (`^.finall_report_dec16.txt`'s
  own "Data Format Comparison" section has the full row schema). This is
  a real, interesting design choice (interpretability, hand-editability)
  that xyzai's own code never mentions or leans on — e.g. nothing in
  xyzai inspects or displays curriculum content, even though it's
  plain text and trivially readable.
- **Model scale**: `config.txt` in the real project shows `dim=16,
  n_layers=2, n_heads=4` — a genuinely tiny transformer, smaller than
  the smallest gemma model xyzai also talks to (`gemma3:270m`) by
  several orders of magnitude. The 32-token vocabulary in
  `guess_the_word.txt` explains the word-salad output directly — the
  model has almost no vocabulary to work with, this isn't a training bug,
  it's a scale/curriculum-size ceiling. `model_list.txt`'s own comment
  already gets this roughly right ("expect mostly `<UNK>` output... until
  a real conversational curriculum is trained") but was written without
  the author having actually run it against the real project to see the
  ACTUAL failure mode (100% crash, not degraded output) — see the
  headline finding above.
- **`bak/`, `lab/`, `#.qwen.ignore/` subdirectories exist** in the real
  project and were not investigated this pass — worth a look before
  assuming `guess_the_word`/`guess_the_letter` are the only two
  curricula that ever existed; `bak/` in particular might hold an older
  or larger curriculum worth recovering.

## ✅ Net assessment

| | xyzai's model of IQABOD | Reality |
|---|---|---|
| Project location | ❌ dead path (pre-reorg) | Real, current: `#.z.mirror_llm]z5]IQABOD🪞️+4/` |
| Curriculum names | ❌ `test`/`daily_life`, neither exists | Real: `guess_the_word` (trained), `guess_the_letter` (untrained) |
| Invocation argv shape | ✅ correct, matches real CLI exactly | — |
| Output parsing (marker string, prompt-stripping) | ✅ correct, matches real stdout exactly | — |
| Understanding that it's a REAL transformer, not a toy stub | ⚠️ implied, never stated outright | Independently confirmed real (Dec-16 report) |
| Understanding of WHY output is incoherent (scale, not a bug) | ⚠️ partially — comment guesses right, was never verified live | Confirmed: dim=16, 32-token vocab, this session |

The integration code is good. The registry data pointing at it is stale.
Fix the data, not the code — concrete steps in `TODO.txt`.
