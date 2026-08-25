# 💼 Chapter 13: Piecemark Labs & The Sovereign Venture
TPMOS isn't just an OS—it's a **Sovereign Venture**. Piecemark Labs operates in stealth, building the infrastructure that will power the next generation of decentralized applications. 🏢🔐

---

## 🥷 The Stealth Strategy
Piecemark Labs follows a **Stealth-First** approach:
*   **Build in Silence:** Core infrastructure is built and tested internally before public release.
*   **SSI Guard:** Self-Sovereign Identity (SSI) is the foundation. Every user owns their data, their keys, their destiny.
*   **Infrastructure Moat:** The real value isn't the apps—it's the filesystem-as-database, the Piece architecture, and the PAL orchestration layer.

---

## 💰 Monetization Models
TPMOS supports multiple business models through its Piece-based architecture:

### B2B (Business-to-Business)
*   **Enterprise TPMOS:** Companies license TPMOS for internal operations (inventory, HR, project management).
*   **Custom Ops Development:** Build industry-specific ops for clients.

### B2C (Business-to-Consumer)
*   **Premium Apps:** Sell specialized apps (GL-OS games, AI-Labs integrations).
*   **Dividend Points:** Users earn Div-Points for participation, exchangeable for premium content.

### B2G (Business-to-Government)
*   **Civic Infrastructure:** TPMOS as a platform for transparent government services.
*   **Auditability:** All state is plaintext, fully auditable by design.

### B2E (Business-to-Employee)
*   **Internal Tooling:** Companies build custom tools for employees using TPMOS app factory.
*   **Onboarding Automation:** PAL scripts automate employee onboarding workflows.

---

## 🔄 The Onboarding Loop
New users are onboarded through automated PAL scripts:

### Code Example: Onboarding Bot PAL Script
```asm
; onboarding_new_user.asm
start:
    ; Create user profile
    OP user::create_profile "new_employee"
    sleep 200000

    ; Assign default permissions
    OP user::assign_role "new_employee" "standard_user"
    sleep 100000

    ; Create workspace
    OP playrm::create_piece "workspace" "new_employee_desk"
    sleep 100000

    ; Send welcome message
    OP p2p::compose_message "new_employee" "admin" "Welcome" "Welcome to TPMOS!"
    sleep 100000

    ; Log completion
    call bot_tester::log_pass "Onboarding complete"
    hit_frame
    halt
```

---

## 📜 Direct Marketing Contracts
In TPMOS, "Like = Contract". When a user likes a project or creator, a PAL-based contract is formed:

### Code Example: Smart Contract Template
```asm
; direct_marketing_contract.asm
start:
    ; Read the "like" action
    read_state r1, "action_piece", "type"
    beq r1, r0, no_like

    ; Verify both parties exist
    call user::create_profile "creator"
    call user::create_profile "supporter"

    ; Form contract: supporter gets updates, creator gets engagement
    OP p2p::escrow_create "contract_like_001" "supporter" "creator"
    sleep 100000

    ; Set contract terms
    set_state_int("contract_like_001", "type", 1)  ; 1 = marketing contract
    set_state_int("contract_like_001", "duration", 365)  ; 365 days
    hit_frame

    j contract_formed

no_like:
    halt

contract_formed:
    call bot_tester::log_pass "Marketing contract formed"
    halt
```

---

## 💎 Token/Div-Points Data Structure
Dividend Points are stored in Piece state:

```
[DIVIDEND_POINTS]
holder=wallet_001
balance=1500
last_claim=1773622576
earned_from=storage,compute,participation
```

---

## 🏛️ Business/Dev Bridge
How business logic maps to code:

| Business Concept | TPMOS Implementation |
|-----------------|---------------------|
| "Like = Contract" | PAL script forming escrow between two wallets |
| "Rent storage" | Piece with `rental` state, `capacity`, `price_per_gb` |
| "Sell compute" | Piece with `compute_capacity` state, `cpu_cores`, `price_per_hour` |
| "User onboarding" | PAL script calling `user::create_profile`, `assign_role`, etc. |
| "Marketing campaign" | PAL script targeting bot Pieces with `state.desire` updates |

---

## ⚖️ Legal Roadmap
*   **Phase 1:** Internal contracts (PAL scripts between company wallets)
*   **Phase 2:** Public contracts (open marketplace for contract formation)
*   **Phase 3:** Legal recognition (explore jurisdictional compliance)

---

## ⚠️ Crypto Cautions
*   **Regulatory Uncertainty:** Token models may face legal challenges in different jurisdictions.
*   **Security Risks:** Private keys must be protected. Never store plaintext keys in production.
*   **Scalability:** File-based blockchain has limits. Consider Merkle tree optimization for large chains.

---

## ⚠️ Common Pitfalls

### Pitfall 1: Hardcoded Business Logic
**Symptom:** Changing contract terms requires code recompilation.
**Cause:** Contract logic hardcoded in C instead of parameterized in PAL.
**Fix:** Store contract terms in state files, read them at runtime:
```c
int duration = get_state_int("contract", "duration");
int type = get_state_int("contract", "type");
```

### Pitfall 2: Onboarding Script Failures
**Symptom:** New users missing permissions or workspace.
**Cause:** PAL script fails silently on one step, continues to next.
**Fix:** Add error checking after each OP call:
```asm
OP user::create_profile "new_employee"
sleep 200000
read_state r1, "new_employee", "name"
beq r1, r0, onboarding_failed
```

### Pitfall 3: Div-Point Exploitation
**Symptom:** Users earning infinite Div-Points.
**Cause:** No rate limiting on point-earning actions.
**Fix:** Track last claim time and enforce cooldown:
```c
time_t last_claim = get_state_int("wallet", "last_claim");
time_t now = time(NULL);
if (now - last_claim < 3600) return;  // 1 hour cooldown
```

---

## 🏛️ Scholar's Corner: The "First Contract"
The first real-world contract formed on TPMOS wasn't between two humans—it was between two bots. A testing bot (Bot-33, from Chapter 9) "liked" a marketing bot's campaign, forming a PAL-based contract. The marketing bot automatically started sending updates to Bot-33, and Bot-33 started earning Div-Points for engagement. The developers watched in amazement as two autonomous agents formed a legally-binding (in TPMOS law) agreement without human intervention. This was the moment we realized: **"The bots are the customers too."** 🤖💼

---

## 📝 Study Questions
1.  Explain the "Like = Contract" concept in TPMOS.
2.  How does the onboarding PAL script automate employee setup?
3.  What are the four monetization models supported by TPMOS?
4.  **Write a PAL script** that forms a contract between a creator and supporter.
5.  **Scenario:** A user claims they earned 10,000 Div-Points in one hour. How would you investigate and fix this?
6.  **True or False:** All business logic in TPMOS must be written in C code.

---
[Return to Index](INDEX.md)
