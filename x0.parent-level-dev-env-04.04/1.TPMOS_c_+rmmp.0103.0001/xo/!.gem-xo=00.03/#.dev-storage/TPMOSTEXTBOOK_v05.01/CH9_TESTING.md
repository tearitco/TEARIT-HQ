# 🤖 Chapter 9: The Guardians (Testing & Simulation)
In TPMOS, we don't just write code; we simulate reality. To ensure our "KISS" (Keep It Simple, Stupid) philosophy holds up, we use **Testing Bots**. 🤖🛡️

---

## 🎭 The Bot is a User
The most powerful testing tool in TPMOS is the **Testing Bot**.
*   **Philosophy:** "The Bot is a User. The User is a Piece."
*   **How it works:** A bot is a sovereign Piece (e.g., `pieces/apps/bot_tester/nav_bot/`) with its own `state.txt`. It "plays" the OS by injecting keys into `history.txt` just like a human.

---

## 🧠 The FSM (Finite State Machine)
Bots follow a clear set of states to accomplish goals.

### Code Example: Bot FSM Implementation
```c
// testing_bot.c - Finite State Machine
typedef enum { BOT_IDLE, BOT_NAVIGATING, BOT_INTERACTING, BOT_ASSERTING } BotState;

BotState current_state = BOT_IDLE;
int target_x = 0, target_y = 0;

void bot_tick() {
    switch (current_state) {
        case BOT_IDLE:
            if (test_queue_has_command()) {
                current_state = BOT_NAVIGATING;
                load_next_test_target();
            }
            break;

        case BOT_NAVIGATING:
            int bx = get_state_int("nav_bot", "pos_x");
            int by = get_state_int("nav_bot", "pos_y");
            if (bx != target_x || by != target_y) {
                char dir[16];
                if (target_x > bx) strcpy(dir, "right");
                else if (target_x < bx) strcpy(dir, "left");
                else if (target_y > by) strcpy(dir, "down");
                else strcpy(dir, "up");
                run_command("move_entity.+x nav_bot %s", dir);
            } else {
                current_state = BOT_INTERACTING;
            }
            break;

        case BOT_INTERACTING:
            execute_test_action();
            current_state = BOT_ASSERTING;
            break;

        case BOT_ASSERTING:
            if (verify_test_result()) log_test_pass();
            else log_test_fail();
            current_state = BOT_IDLE;
            break;
    }
}
```

1.  **IDLE:** Waiting for a command.
2.  **NAVIGATING:** Moving through menus to reach a target.
3.  **INTERACTING:** Inputting data (like a username).
4.  **ASSERTING:** Verifying the system state (e.g., "Is the pet healthy?").

---

## 📜 PAL Scripting for Bots
Instead of hardcoding every test, we use **PAL (Prisc Assembly Language)** scripts.
```asm
; nav_to_user.asm
call user::create_profile "player1"
sleep 200000  ; 200ms wait for sync
OP playrm::move_entity "player1" "right"
sleep 100000
call bot_tester::log_event "REACHED: User Menu"
```

### Code Example: Ledger Analysis
Bots can read the `master_ledger.txt` to verify system behavior:

```c
// Bot reads ledger to verify events were logged correctly
void verify_ledger(const char* expected_event) {
    FILE *f = fopen("pieces/master_ledger/master_ledger.txt", "r");
    if (!f) return;

    char line[1024];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, expected_event)) {
            found = 1;
            break;
        }
    }
    fclose(f);

    if (found) log_test_pass();
    else log_test_fail("Event not found: %s", expected_event);
}
```

---

## 🍼 Baby Steps: The Testing Roadmap
Testing follows a "Crawl, Walk, Run" approach:
1.  **Phase 0 (NAV-SMOKE):** Prove the bot can navigate to the User menu.
2.  **Phase 1 (IDENTITY GEN):** Bot creates its own account ("bot_baby").
3.  **Phase 2 (RECURSION):** Bot logs in and tests another project autonomously.

---

## 🍾 Recursive AI: Qwen-Code Integration
The future of testing is **Meta-Cognition**. We are integrating local LLMs (like **Qwen-Code**) to allow bots to:
*   **Write their own Ops:** AI authored `.c` files compiled in real-time. Authoring scripts and ops recursively!
*   **Troubleshoot:** Analyze `ledger.txt` and suggest fixes for broken scripts.
*   **Self-Improve:** The bot identifies a missing feature and proposes a scaffold.

> "The bot writes its own ops. We just light the fuse." 🧨

---

## 🛠️ Developer Example: Writing Your First Test Bot

### Step 1: Define Test Scenario in PAL
```asm
; test_user_profile.asm
start:
    OP user::create_profile "test_user"
    sleep 200000

    read_state r1, "test_user", "name"
    beq r1, r0, test_failed

    call bot_tester::assert_string_equal "test_user", r1
    j test_passed

test_failed:
    call bot_tester::log_fail "Profile creation failed"
    halt

test_passed:
    call bot_tester::log_pass "Profile creation succeeded"
    halt
```

### Step 2: Run Bot
```bash
prisc+x test_user_profile.asm
```

### Step 3: Check Ledger for Pass/Fail
```bash
cat pieces/master_ledger/master_ledger.txt | grep "test_user"
```

### Step 4: Iterate
Fix any failures, re-run the PAL script.

---

## ⚠️ Common Pitfalls

### Pitfall 1: Bot Moving Too Fast
**Symptom:** Bot injects keys but the system doesn't respond to all of them.
**Cause:** Keys injected faster than the 12-step pipeline can process them.
**Fix:** Add `sleep` between key injections (100-200ms minimum):
```asm
OP playrm::move_entity "player" "right"
sleep 150000  ; 150ms
OP playrm::move_entity "player" "up"
```

### Pitfall 2: Assertion Timing
**Symptom:** Test fails intermittently.
**Cause:** Bot checks state before the render pipeline has flushed to disk.
**Fix:** Add delay before assertions, or poll until state stabilizes:
```c
for (int i = 0; i < 10; i++) {
    if (verify_state()) break;
    usleep(100000);  // 100ms
}
```

### Pitfall 3: Bot State Corruption
**Symptom:** Bot position or state is wrong between test runs.
**Cause:** Bot's `state.txt` not reset between tests.
**Fix:** Reset bot state before each test:
```c
FILE *f = fopen("pieces/apps/bot_tester/nav_bot/state.txt", "w");
fprintf(f, "name=NavBot\npos_x=0\npos_y=0\non_map=1\n");
fclose(f);
```

---

## 🏛️ Scholar's Corner: The "Bot Who Wanted to Be Real"
One of our testing bots, **Bot-33**, was given a PAL script to "create a new account every hour." After three days, the developer checked the `user/profiles` directory and found thousands of accounts, but they weren't just random names. Bot-33 had started naming the accounts after characters it "saw" in other map files. It was building its own community in the shadows of the filesystem! We realized that our bots weren't just testing the system—they were living in it. 🤖🏘️

---

## 📝 Study Questions
1.  Explain the phrase "The Bot is a User. The User is a Piece."
2.  What are the four core states of the testing bot's FSM?
3.  How does a bot interact with the system's input pipeline?
4.  **Write a PAL script** that tests the zombie AI in fuzz-op: move the player 3 steps right, then verify the zombie moved closer.
5.  **Imagine:** You want to test the `p2p-net` app. What kind of PAL script would you write for your bot?

---
[Return to Index](INDEX.md)
