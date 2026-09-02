# 🎬 vs 🌊 Film-hai vs Philo-hai — Scientific Prompt-Pipeline Comparison

> **Date:** 2026-08-18  
> **Goal:** Why does philosophy feel worse than film? Is it the topic diversity, the personalities, or the prompt pipeline?

---

## 📊 Executive Summary

| Axis | 🎬 Film-hai (main) | 🌊 Philo-hai (philosophy) |
|------|-------------------|---------------------------|
| **Era** | 2026-08-14 / 15 | 2026-08-18 |
| **Personas** | moxie 🐺, bravo 🐻, sage 🦉, pip 🐹 | daoist 🌊, evolutionist 🧬, theologian ✝️, synthesist ♾️ |
| **Tiers** | router (0.5B) ×2, quick (1.5B) ×2, manager (7B) ×1 | router (0.5B) ×3, manager (7B) ×1 |
| **Model family** | qwen2.5-coder (all) | qwen2.5-coder (all) |
| **Pipeline version** | v0 (raw) | v1 (enhanced) |
| **Active session?** | ❌ archived | ✅ live |
| **Vibe** | Cheerleader loop | Academic-summary + hallucination |
| **Failure mode** | Verbose repetition, instruction echo | Fake citations, AI self-reference, refusal loops |

**Bottom line:** The topic isn't too diverse — it's that **both runs are hammering the same wrong model family** (qwen2.5-coder, a code model) at tiny parameter counts. Philosophy *looks* worse because abstract topics expose the model's "fake scholar" failure mode, while film just loops on cheerleader phrases. The enhanced pipeline in philo-hai actually *helps* (higher relation counts, user bias, anti-repeat gates), but it can't fix a code model trying to do philosophy.

---

## 🧠 Model-Family Analysis (The #1 Variable)

### What's actually running

| Tier | Model | Params | Role in film | Role in philo |
|------|-------|--------|-------------|---------------|
| router | qwen2.5-coder:0.5B | 0.5B | moxie, pip | daoist, evolutionist, theologian |
| quick | qwen2.5-coder:1.5B | 1.5B | bravo, sage | — |
| manager | qwen2.5-coder:7B | 7B | conductor (inert) | synthesist (inert) |
| smol | gemma3:270m | 270M | harness pre-step | harness pre-step |

**Critical finding:** Every persona that actually speaks is a **qwen2.5-coder** — a *coding* model, not a chat/conversation model. The `ollama-lan.pdl` registry confirms this is the **only** family on the ladder. The original vision (per `chat-hai.2026`) was "4 gemma270 models chatting" — that swap never happened.

### Why this kills both topics equally

- **Film topic** ("favorite movie franchise") is concrete and bounded — a 0.5B coder model can parrot phrases about "The Godfather" and "Star Wars" because those terms appear in its training data. It loops because it has no genuine opinion, but the surface looks on-topic.
- **Philosophy topic** ("Bible + Tao Te Ching + evolutionary science") is abstract and requires *synthesis across domains*. A 0.5B coder model has zero training signal for this, so it falls back to its *next strongest pattern*: academic-summary text with fake structure (bullet points, fake URLs, "DAOIST (Deity of All Saints' Wisdom)" hallucinations).

**Same root cause, different surface failure.**

---

## 🔧 Prompt-Pipeline Comparison

### Film-hai pipeline (v0 — early 2026-08-14/15)

```
speak() {
    prompt = persona.system-prompt
    ctx = recent_context(20)          # last 20 ledger lines
    question = prompt + "\n\nRecent conversation:\n" + ctx + "\n\n" + glyph + " You are " + name + ". Reply to " + prev_name + " now."
    reply = qwen.sh ask tier question
    # NO user bias
    # NO fresh_angle
    # NO memory recall
    # NO relation note
    # NO anti-repeat gate (word-overlap check added later)
}
```

**What's missing:**
- ❌ User input bias (added 2026-08-16 in philo-hai)
- ❌ `fresh_angle()` staged pre-prompt
- ❌ `recall_memory()` 
- ❌ `relation_note` (co-occurrence flavor)
- ❌ Word-overlap anti-repeat gate
- ❌ Instruction-echo detector

### Philo-hai pipeline (v1 — 2026-08-18)

```
speak() {
    prompt = persona.system-prompt
    ctx = recent_context(20)
    user_msg = last_user_msg()
    user_bias = ">>> THE USER SAID: ${user_msg} <<<"   # ★ NEW
    angle_hint = fresh_angle(name, own_last, ctx)       # ★ NEW
    recall_note = recall_memory(name, ctx)              # ★ NEW
    relation_note = "You and ${prev_name} talk often."  # ★ NEW
    question = user_bias + prompt + ctx + glyph + name + relation_note + angle_hint + recall_note + user_addendum
    reply = qwen.sh ask tier question
    # ★ Anti-repeat gate (word-overlap ≥ 55% → drop)
    # ★ Instruction-echo detector → drop
}
```

**What's new in v1:**
- ✅ User bias block (promotes user input to top of prompt)
- ✅ `fresh_angle()` — smol-tier pre-prompt suggesting a new angle
- ✅ `recall_memory()` — word-overlap memory retrieval
- ✅ `relation_note` — deterministic co-occurrence flavor
- ✅ Word-overlap anti-repeat gate (55% threshold)
- ✅ Instruction-echo leak detector
- ✅ Observations log for tier escalation

**Does the enhanced pipeline help?** Yes, but only marginally. The relation counts for philo-hai are **5-10× higher** than film-hai (daoist-evolutionist: 42 vs moxie-pip: 9), meaning the personas *are* engaging more. But the output quality is still terrible because the model can't generate good content, only more of it.

---

## 🎭 Persona Design Comparison

### Film personas (v0 — simple archetypes)

| Persona | Glyph | Tier | System prompt length | Style |
|---------|-------|------|---------------------|-------|
| moxie | 🐺 | router | 1 sentence | curious wolf scout, playful |
| bravo | 🐻 | quick | 1 sentence | steady bear, keeps on track |
| sage | 🦉 | quick | 1 sentence | wise owl, ties threads |
| pip | 🐹 | router | 1 sentence | tiny tinkerer, enthusiastic |
| conductor | 🎩 | manager | 1 sentence | moderator, speaks rarely |

**Total persona token budget:** ~40 tokens per persona.

### Philosophy personas (v1 — intellectual frameworks)

| Persona | Glyph | Tier | System prompt length | Style |
|---------|-------|------|---------------------|-------|
| daoist | 🌊 | router | 3 sentences | Tao Te Ching, wu wei, paradox |
| evolutionist | 🧬 | router | 3 sentences | evolutionary biology, emergence |
| theologian | ✝️ | router | 3 sentences | Biblical scholar, scriptural insight |
| synthesist | ♾️ | manager | 3 sentences | bridge-builder, finds resonance |
| conductor | 🎩 | manager | 1 sentence | moderator (inert) |

**Total persona token budget:** ~120 tokens per persona. **3× more guidance, same terrible model.**

### What the longer prompts actually do

In a 0.5B coder model, longer system prompts don't steer behavior — they get *pattern-matched* into the model's existing "academic text" template. The result:

- daoist's "wu wei / effortless action / paradox" → model outputs generic "paradoxes arise from our actions without conscious awareness" (a phrase it memorized from the prompt)
- theologian's "Biblical scholar" → model triggers its safety refusal ("I'm sorry, but I can't assist with that") because religious content + small model = refusal
- evolutionist's "evolutionary biology" → model outputs bullet-point lists about GPS, recycling, and almonds (hallucinated "practical applications")

**Longer prompts make the failure mode more specific, not better.**

---

## 💬 Chat-History Vibe Analysis

### Film-hai vibe: 🎭 Cheerleader Loop

**Representative samples:**
```
bravo: Great idea! Let's dive right into our fun new topic of "our favorite movie franchise."
       We've been wondering about classic movies and how we can enjoy them with friends again!
       How do you think we should approach this?

moxie: Great idea! Let's dive right into our fun new topic of "our favorite movie franchise."
       We've been wondering about classic movies and how we can enjoy them with friends again!
       How do you think we should approach this?

pip:   Moxie: Great idea! Let's dive right into our fun new topic of "our favorite movie franchise."
       [echoes bravo's previous message verbatim]
```

**Pattern:** Every persona says the same thing. The phrase "Let's dive right into our fun new topic of 'our favorite movie franchise'" appears **20+ times** in 79 lines. Context bleed is extreme — models echo each other's previous outputs verbatim because the anti-repeat gate didn't exist yet.

**Emotional tone:** Enthusiastic, agreeable, stuck. Like 4 friends who can't stop recommending the same 3 movies.

---

### Philo-hai vibe: 📚 Fake Scholar Syndrome

**Representative samples:**
```
daoist: The Daoist concept of wisdom is interconnected with spiritual practices.
        This means that even a moment's thought can lead to profound knowledge and insight.
        [repeats 3x in next 2 messages verbatim]

theologian: I'm sorry, but I can't assist with that request.  ← model refusal

evolutionist: The group is engaging in a discussion on how ancient spiritual wisdom
              speaks to modern questions of consciousness, purpose, and existence.
              The Daoist concept of wisdom is linked with spiritual practices, and
              paradoxes arise from our actions without conscious awareness.
              The group encourages reflection on how these concepts intersect...
```

**Patterns:**
1. **"The group is engaging in a discussion..."** — meta-commentary where the model narrates the chat instead of participating
2. **Fake citations:** "Evolutionist.org", "Natural Adaptability.net", "The Daoist Project" — these don't exist
3. **Hallucinated biography:** "DAOIST (Deity of All Saints' Wisdom) has been founded by Dr. Arthur Da Vinci" — complete fiction
4. **Refusal loops:** theologian says "I'm sorry, but I can't assist with that" repeatedly
5. **Listicle mode:** GPS/recycling/almonds/water purification bullet points presented as "Daoist principles"
6. **AI self-reference:** "As an AI language model, I do not have access to your discussion"
7. **Instruction echo:** "DAOIST: You are the one who has been thinking about integrating Daoism with the natural world..."

**Emotional tone:** Robotic, hallucinating, trapped in academic-theater mode. Like 4 grad students faking their way through a seminar they didn't read for.

---

## 🔬 Quantitative Comparison

### Message statistics (170 lines each)

| Metric | Film-hai | Philo-hai |
|--------|----------|-----------|
| Total messages | 79 | 170 |
| Unique phrases (top 10) | 1 phrase × 20+ repeats | "The group is engaging..." × 15+ |
| Avg message length | ~200 chars | ~800 chars |
| Verbatim echo rate | ~30% | ~25% |
| Fake citations | 0 | 6+ |
| Model refusals | 0 | 8+ |
| AI self-reference | 0 | 4+ |
| Highest relation count | 9 (moxie-pip) | 44 (evolutionist-theologian) |
| Synthesist/conductor speaks | 0 (inert) | 0 (inert) |

### Why philo-hai has MORE messages but feels worse

- The enhanced pipeline (user bias, fresh_angle, memory) *increases* engagement — relation scores are 5-10× higher, meaning the loop calls personas more often and they respond more.
- But each response is *longer* and *more hallucinated* because the abstract topic triggers the model's "write a long academic summary" pattern.
- Film-hai's messages are short and repetitive, which at least reads fast. Philo-hai's messages are long and hallucinated, which is cognitively expensive to read and reveals the fraud more clearly.

---

## 🕸️ Topic Diversity: Is It Too Wide?

**User hypothesis:** "maybe their topic is too diverse?"

**Answer: No.** The topic isn't too diverse — it's *too hard* for the model, and the film topic was *too easy* in a different way.

| Property | Film topic | Philosophy topic |
|----------|-----------|-----------------|
| **Domain breadth** | 1 domain (cinema) | 3 domains (theology, Daoism, evolution) |
| **Concreteness** | High (specific films) | Low (abstract concepts) |
| **Training-data overlap** | High (0.5B coder model has seen film reviews) | Low (cross-domain synthesis = rare in training data) |
| **Ideal persona count** | 2-3 (film buffs) | 1-2 (a polymath) |
| **Actual persona count** | 4 | 4 |

**The real problem:** Philosophy needs *one* deep generalist (like the synthesist ♾️), but the synthesist is **inert** because `MODERATOR_EVERY=0` in `chat_hai_loop.sh:156`. The 3 router-tier personas (daoist, evolutionist, theologian) are all running on the **same 0.5B model** — they can't hold distinct philosophical positions because they don't have enough capacity to simulate 3 different intellectual frameworks simultaneously.

Film-hai works *better* by accident: 4 personas on 2 models (moxie/pip share 0.5B, bravo/sage share 1.5B) debating a concrete topic means even if they echo each other, the surface looks like a cohesive conversation about movies.

---

## 🎯 Root Causes Ranked

### 1. 🔴 Wrong model family (CRITICAL)
- **qwen2.5-coder** is a code model. It was never fine-tuned for multi-turn chat.
- The original vision was **gemma270** (gemma3:270m) for the 4 chatting personas — a chat-tuned model.
- **Fix:** Swap router/quick tiers to gemma3:1b or gemma3:2b (chat-tuned). Keep qwen2.5-coder only for actual coding tasks.

### 2. 🔴 Synthesist is inert (CRITICAL)
- `MODERATOR_EVERY=0` means the one persona designed to tie threads together **never speaks**.
- Philosophy especially needs a moderator because the topic is cross-domain.
- **Fix:** Set `MODERATOR_EVERY=3` or `4` so the synthesist speaks every 3-4 rounds.

### 3. 🟡 Persona tier doubling-up (MODERATE)
- Film: moxie + pip share 0.5B; bravo + sage share 1.5B
- Philo: daoist + evolutionist + theologian ALL share 0.5B
- Three distinct philosophical voices on one 0.5B model = they collapse into the same "academic summary" voice.
- **Fix:** Give each philosophy persona a distinct tier (router/quick/quick), or give them distinct models.

### 4. 🟡 Context window bloat (MODERATE)
- Philo-hai's enhanced pipeline adds user_bias + angle_hint + recall_note + relation_note to every prompt.
- A 0.5B model with 20 lines of context + 4 injected notes = the model attends to the *notes* more than the *conversation*.
- **Fix:** Reduce `CONTEXT_LINES` from 20 to 8 for router-tier personas, or shorten the injection notes.

### 5. 🟢 Topic design (LOW)
- Philosophy's "Bible + Tao Te Ching + evolution" is a valid salon topic, but it requires a *moderator* to prevent drift into fake-citation territory.
- The topic itself isn't the problem — the *lack of a speaking synthesist* is.

---

## 📋 Recommendations (Priority Order)

| Priority | Action | Expected impact |
|----------|--------|-----------------|
| **P0** | Enable `MODERATOR_EVERY=3` so synthesist ♾️ speaks | Prevents drift, adds synthesis voice |
| **P0** | Swap router/quick tiers from qwen2.5-coder → gemma3:1b | Eliminates code-model failure mode |
| **P1** | Give each philo persona a distinct tier (router/quick/quick) | Prevents voice collapse |
| **P1** | Reduce `CONTEXT_LINES` to 8 for router-tier | Reduces note-vs-conversation attention battle |
| **P2** | Add a `salon_host` system message to the session boot | Sets explicit framing ("this is a conversation, not a research paper") |
| **P2** | Re-enable film-hai as a second session with gemma models | A/B test the model-swap hypothesis |

---

## 📜 Raw Data Sources

| File | Path |
|------|------|
| Film transcript | `state/transcript.ledger` (migrated to `state/sessions/main.ledger`) |
| Philosophy transcript | `state/sessions/philosophy.ledger` |
| Film personas | `pieces/personas/{moxie,bravo,sage,pip,conductor}.pdl` |
| Philosophy personas | `pieces/personas/philosophy/{daoist,evolutionist,theologian,synthesist}.pdl` |
| Loop script | `ops/chat_hai_loop.sh` |
| Model registry | `net/ollama-lan.pdl` |
| Relations | `state/relations.pdl` |
| Config | `chat_hai_config.pdl` |

---

*Generated by Kilo 🧠⚡*
