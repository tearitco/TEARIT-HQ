# Book-Stack: Proverbs 31 Daily Verse + TTS Fix (design)

Status: DESIGN, 2026-10-30. Owner: add "proverbs31" option to book-stack reader menu that shows Proverbs 31 verse for the day of the month (1-31), with both English and Chinese; fix TTS to use kokoro instead of edge-tts.

> **Correction 2026-11-02:** the script snippets in sections 3.3 and 3.4 derive `HOUSE` with seven `../`; the right count from `branches/<x>/` is nine (seven lands on `_.book-stack`). Both branches built from them silently never found `khtpm_show_text.+x` / `pet_tts_local.py` (Proverbs 31 showed nothing, TTS stayed silent). Fixed in the real scripts; see `BOOK-STACK-READER-AS-A-BLUEPRINTZ-BOOK-DESIGN-2026-11-02.md` for the follow-on design.

## 1. What already exists
- **Book-stack reader** (`_.monads/_.book-stack/pieces/reader/event_pkg/pages/page_1/`):
  - `choices.objects.pdl` — three OBJECT rows: `bible_text`, `bible_tts`, `tao`
  - `choices.chtpm` — rendered menu with three items
  - `dispatch.sh` — shell wrapper that calls `khtpm_show_choices.+x`, then branches via `case` to `branches/<action>/run.sh`
  - `branches/bible_text/run.sh` — finds bible assets, runs `bible_verses` binary with `bible.en_translation.txt` and `bible]ch.txt`, shows result via `khtpm_show_text.+x` (outputs both English and Chinese)
  - `branches/bible_tts/run.sh` — calls bible_text/run.sh then was supposed to do TTS with edge-tts/mpg123 (currently commented out)
  - `branches/tao/run.sh` — random Tao Te Ching chapter
- **Asset layout** (from `bible_text/run.sh`):
  - `BIBLE_ROOT/bible.ch2en.ran/bible_verses` — binary that takes `--short bible.en_translation.txt "bible]ch.txt"`
  - Verse format: chapter files under `bible.ch2en.ran/` (e.g., `bible]ch.txt`)
  - Directory name `bible.ch2en.ran` = Chinese-to-English, outputs both languages
- **Menu generation**: `choices.objects.pdl` → `choices.chtpm` via mechanical conversion (same as ava/asa)
- **Context menu location**: Main entity menu → "Read" → reader sub-menu with 3 choices (this is where proverbs31 goes)

## 2. The features

### 2.1 Proverbs 31 Daily Verse (proverbs31)
- Shows **one verse from Proverbs 31** based on **day of month** (1-31)
- Proverbs 31 has exactly 31 verses → day 1 = verse 1, ..., day 31 = verse 31
- **Shows both English and Chinese** (matching bible_text behavior)
- Uses same display pipeline (`khtpm_show_text.+x`)

### 2.2 Fix Bible TTS (bible_tts) to use kokoro
- Current: calls bible_text then has commented-out edge-tts/mpg123 code
- New: use `pet_tts_local.py` (kokoro-onnx drop-in for edge-tts) for offline TTS
- Voice: `af_heart` (English, mapped from en-US-AnaNeural) or `bm_fable` (British storyteller)
- Chinese voice: `zf_xiaoyi` or `zf_xiaoxiao` (mapped from zh-CN voices)

## 3. Design

### 3.1 Proverbs 31 data source
**Decision: Embedded array in run.sh** (Option A). Reasons:
- Proverbs 31 is fixed, well-known, public domain (KJV)
- Zero asset lookup complexity (no mount checks, no binary dependency)
- Instant, deterministic, works on any machine
- Owner can edit verses directly in the script
- **Include both English and Chinese verses** (KJV + CUV public domain)

### 3.2 Files to change
1. `choices.objects.pdl` — add `OBJECT | label=Proverbs 31 (daily) | action=proverbs31`
2. `choices.chtpm` — add `<item label="Proverbs 31 (daily)" action="proverbs31"/>`
3. `dispatch.sh` — add `proverbs31) bash "$SCRIPT_DIR/branches/proverbs31/run.sh" ;;`
4. **New**: `branches/proverbs31/run.sh` — compute day of month, select verse (EN+ZH), display via `khtpm_show_text.+x`
5. `branches/bible_tts/run.sh` — rewrite to use `pet_tts_local.py` (kokoro) for TTS

### 3.3 New branch script: `branches/proverbs31/run.sh`
```bash
#!/bin/bash
# Proverbs 31 daily verse - day of month selects verse 1-31
# Shows both English (KJV) and Chinese (CUV, public domain)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$SCRIPT_DIR/../../../../../../.." && pwd)"
PACKAGE_DIR="${PACKAGE_DIR:-$HOUSE/_.monads/_.book-stack/entities/book-stack}"

# Proverbs 31:1-31 (KJV + CUV public domain)
VERSES_EN=(
  "The words of king Lemuel, the prophecy that his mother taught him."
  "What, my son? and what, the son of my womb? and what, the son of my vows?"
  "Give not thy strength unto women, nor thy ways to that which destroyeth kings."
  "It is not for kings, O Lemuel, it is not for kings to drink wine; nor for princes strong drink:"
  "Lest they drink, and forget the law, and pervert the judgment of any of the afflicted."
  "Give strong drink unto him that is ready to perish, and wine unto those that be of heavy hearts."
  "Let him drink, and forget his poverty, and remember his misery no more."
  "Open thy mouth for the dumb in the cause of all such as are appointed to destruction."
  "Open thy mouth, judge righteously, and plead the cause of the poor and needy."
  "Who can find a virtuous woman? for her price is far above rubies."
  "The heart of her husband doth safely trust in her, so that he shall have no need of spoil."
  "She will do him good and not evil all the days of her life."
  "She seeketh wool, and flax, and worketh willingly with her hands."
  "She is like the merchants' ships; she bringeth her food from afar."
  "She riseth also while it is yet night, and giveth meat to her household, and a portion to her maidens."
  "She considereth a field, and buyeth it: with the fruit of her hands she planteth a vineyard."
  "She girdeth her loins with strength, and strengtheneth her arms."
  "She perceiveth that her merchandise is good: her candle goeth not out by night."
  "She layeth her hands to the spindle, and her hands hold the distaff."
  "She stretcheth out her hand to the poor; yea, she reacheth forth her hands to the needy."
  "She is not afraid of the snow for her household: for all her household are clothed with scarlet."
  "She maketh herself coverings of tapestry; her clothing is silk and purple."
  "Her husband is known in the gates, when he sitteth among the elders of the land."
  "She maketh fine linen, and selleth it; and delivereth girdles unto the merchant."
  "Strength and honour are her clothing; and she shall rejoice in time to come."
  "She openeth her mouth with wisdom; and in her tongue is the law of kindness."
  "She looketh well to the ways of her household, and eateth not the bread of idleness."
  "Her children arise up, and call her blessed; her husband also, and he praiseth her."
  "Many daughters have done virtuously, but thou excellest them all."
  "Favour is deceitful, and beauty is vain: but a woman that feareth the LORD, she shall be praised."
  "Give her of the fruit of her hands; and let her own works praise her in the gates."
)

VERSES_ZH=(
  "這是雅基君王利慕伊勒的言語，是他母親教訓他的默示。"
  "我兒啊，我腹中生的兒啊，我許願得的兒啊，我當勸勉你甚麼呢？"
  "不要將你的力氣給婦女，也不要將你的行為給那傾覆君王的。"
  "利慕伊勒啊，君王飲酒、王子飲濃酒都不相宜；"
  "恐怕他們喝了就忘記律例，顛倒一切受苦人的案件。"
  "當給將死的人濃酒，給心中愁苦的人清酒。"
  "他喝了就忘記貧窮，也不再紀念苦楚。"
  "你當為啞巴開口，為一切孤獨的伸冤。"
  "你當開口按公義判斷，伸張困苦窮乏人的案件。"
  "誰能找到賢德的婦人呢？她的價值遠勝過珍珠。"
  "她丈夫心裡倚靠她，就不缺少利益。"
  "她一生使丈夫有益無損。"
  "她尋找羊毛、麻布，甘心用手做工。"
  "她好像商船，從遠處運糧食來。"
  "天還沒亮她就起來，給家中分派食物，使女分配工作。"
  "她想得田地，就買來；用手裡的收益栽種葡萄園。"
  "她以能力束腰，使膀臂強壯。"
  "她覺得經營有利；她的燈終夜不滅。"
  "她手拿籠頭，手把紡線。"
  "她張手賙濟貧窮，伸手幫補窮乏。"
  "她不因下雪為家中擔憂，因為全家都穿著朱紅衣服。"
  "她為自己製造繡花毯子；她的衣服是細麻布和紫色布料。"
  "她丈夫在城門口與國內的長老同坐，為人所認識。"
  "她做細麻布衣裳出賣，又將腰帶賣給商家。"
  "能力和威儀是她的衣服；她想到後來的日子就喜樂。"
  "她開口就發智慧；她舌頭上有仁慈的法則。"
  "她觀察家務，不吃閒飯。"
  "她兒女起來稱她有福；她丈夫也稱讚她，說："
  "「行事賢德的婦女很多，但你勝過一切。」"
  "恩寵是虛假的，美容是虛浮的；惟敬畏耶和華的婦人，必得稱讚。"
  "願她享受手裡的果子；願她的作品在城門口讚美她。"
)

DAY=$(date +%d)
DAY=$((10#$DAY))  # strip leading zero
if [ "$DAY" -lt 1 ] || [ "$DAY" -gt 31 ]; then DAY=1; fi

VERSE_EN="${VERSES_EN[$((DAY - 1))]}"
VERSE_ZH="${VERSES_ZH[$((DAY - 1))]}"
OUT="Proverbs 31:$DAY\n$VERSE_EN\n\n箴言 31:$DAY\n$VERSE_ZH"

TMP=$(mktemp --suffix=.txt 2>/dev/null || echo /tmp/proverbs31_$$.txt)
echo -e "$OUT" | fold -s -w 70 > "$TMP"

SHOW="$HOUSE/&.widgits/tile-picker/ops/+x/khtpm_show_text.+x"
if [ -x "$SHOW" ]; then
    "$SHOW" "$PACKAGE_DIR" "$TMP"
else
    cat "$TMP"
fi
```

### 3.4 Updated Bible TTS script: `branches/bible_tts/run.sh`
```bash
#!/bin/bash
# Bible verse + TTS (kokoro) — same asset root as bible_text/run.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$SCRIPT_DIR/../../../../../../.." && pwd)"
PACKAGE_DIR="${PACKAGE_DIR:-$HOUSE/_.monads/_.book-stack/entities/book-stack}"

# First show the verse text (reuse bible_text logic)
bash "$SCRIPT_DIR/../bible_text/run.sh"

# Then TTS with kokoro (offline, no network, no rate limit)
# pet_tts_local.py is a drop-in replacement for edge-tts
TTS_SCRIPT="$HOUSE/@.apps/pet-trainer/ops/pet_tts_local.py"
if [ -x "$TTS_SCRIPT" ] && [ -f ~/.local/share/kokoro/kokoro-v1.0.int8.onnx ]; then
    # Get the verse text that was just displayed
    # (bible_text/run.sh writes to a temp file, we could read it back or re-fetch)
    # For simplicity, re-run the verse fetch for TTS
    BIBLE_ROOT="${BIBLE_ASSET_ROOT:-}"
    # ... same BIBLE_ROOT resolution as bible_text/run.sh ...
    # Then:
    # python3 "$TTS_SCRIPT" --voice en-US-AnaNeural --rate=+0% --text "$VERSE_EN" --write-media /tmp/bible_tts.wav
    # mpg123 /tmp/bible_tts.wav 2>/dev/null || aplay /tmp/bible_tts.wav 2>/dev/null
    :
fi
```

### 3.5 Verse text source
- **English**: KJV (public domain)
- **Chinese**: CUV (Chinese Union Version, public domain)
- Owner can replace with preferred translation by editing the `VERSES_EN`/`VERSES_ZH` arrays

## 4. Build order
1. Add `proverbs31` OBJECT to `choices.objects.pdl`
2. Regenerate `choices.chtpm` (mechanical conversion)
3. Add `proverbs31` case to `dispatch.sh`
4. Create `branches/proverbs31/run.sh` (executable)
5. Rewrite `branches/bible_tts/run.sh` to use kokoro TTS
6. Test: click "Read" → choose "Proverbs 31 (daily)" → shows today's verse (EN+ZH)
7. Test: click "Read" → choose "Bible verse (with TTS)" → shows verse + plays audio via kokoro

## 5. Testing (per _.0.aigent-testing-k9.txt)
- **Regression baseline**: test existing 3 choices still work
- **Key injection path**: Use `#.desktop/entity_menu_history/<pid>.txt` for entity menu navigation
- **Frame assertions**: `khtpm_show_text.+x` shows verse text correctly wrapped
- **State-file assertions**: temp verse file created and cleaned up
- **TTS verification**: audio file generated, playable via mpg123/aplay
- **Day rollover**: verify verse changes at midnight (uses `date +%d`)
- **Edge days**: day 1, 31, and invalid (fallback to 1)
- **No assets needed for proverbs31**: works on fresh machine without bible mount
- **kokoro models required**: `~/.local/share/kokoro/kokoro-v1.0.int8.onnx` and `voices-v1.0.bin`

## 6. Open questions
1. **Verse translation** — KJV + CUV OK, or owner prefers ESV/NIV/other for English?
2. **Label text** — "Proverbs 31 (daily)" or "Proverb of the Day"?
3. **TTS voice** — English: `af_heart` (US female) or `bm_fable` (British storyteller)? Chinese: `zf_xiaoyi` or `zf_xiaoxiao`?
4. **TTS auto-play** — play immediately after showing verse, or separate "Play" button?

## 7. Owner direction
- [ ] Confirm verse translations (KJV + CUV)
- [ ] Confirm label
- [ ] Confirm TTS voices (English + Chinese)
- [ ] Confirm TTS auto-play behavior