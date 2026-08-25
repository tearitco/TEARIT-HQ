# ⛓️ Chapter 10: The Great Expansion (AI, LSR, & P2P)
TPMOS is evolving from a single-user OS into a multi-agent, decentralized ecosystem. This expansion is driven by three pillars: **AI-Labs**, **LSR**, and **P2P-NET**. 🚀🌌

---

## 🤖 Pillar 1: AI-Labs (The Intelligence)
**AI-Labs** is the brain of the ecosystem. It manages local LLMs (Gemma, Qwen) via the `qwen` CLI.
*   **Knowledge Distillation:** Compress massive text corpuses into TPM-compliant ledgers.
*   **Chat Hub:** Spawn "Personality Pieces"—chat instances with their own history and state.
*   **Training Yard:** Fine-tune model pieces on specific curriculums.

---

## 🌕 Pillar 2: LSR (Lunar Streetrace Raider)
**LSR** is a "Civ-Lite" simulation where AI agents (FuzzPets) build civilizations on the moon. 🏘️🌑
*   **Dynamic R&D:** Local LLMs generate *actual* recipes and tech (e.g., "Mooncrete") based on research actions.
*   **Biological Engine:** Agents like "Fadam" and "Feve" have genetics, needs (hunger, social), and reproduce.
*   **Corporate Economy:** Companies hire bots to mine, farm, and research in a market-driven world.

---

## ⛓️ Pillar 3: P2P-NET (The Network)
P2P-NET brings sovereignty and trade to TPMOS. **Status: Infrastructure Ready (April 2026).** 🤝💎

### Code Example: P2P Manager - Multi-Agent Orchestration
The P2P manager (`projects/p2p-net/manager/p2p_manager.c`) handles profile switching and wallet management:

```c
// p2p_manager.c - Multi-agent orchestration
typedef struct {
    char wallet_id[64];
    char public_key[256];
    char private_key[256];
    int div_points;
    int connected;
} WalletProfile;

WalletProfile current_wallet;
char project_root[4096] = ".";

void switch_wallet(const char* wallet_id) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/projects/p2p-net/pieces/wallets/%s/state.txt",
             project_root, wallet_id);

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(trim_str(line), "wallet_id") == 0)
                strncpy(current_wallet.wallet_id, trim_str(eq+1), 63);
            else if (strcmp(trim_str(line), "div_points") == 0)
                current_wallet.div_points = atoi(trim_str(eq+1));
            else if (strcmp(trim_str(line), "connected") == 0)
                current_wallet.connected = atoi(trim_str(eq+1));
        }
    }
    fclose(f);
}
```

### Code Example: P2P Node Connection
The `connect_peer` op (`projects/p2p-net/ops/src/connect_peer.c`) establishes network connections:

```c
// connect_peer.c - P2P node connection
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int connect_to_peer(const char* host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    // Send handshake
    char handshake[256];
    snprintf(handshake, sizeof(handshake),
             "TPMOS-P2P-HELLO %s\n", current_wallet.wallet_id);
    send(sock, handshake, strlen(handshake), 0);

    // Read response
    char response[256];
    int n = recv(sock, response, sizeof(response)-1, 0);
    if (n > 0) {
        response[n] = '\0';
        if (strstr(response, "TPMOS-P2P-WELCOME")) {
            // Connection successful - update state
            set_state_int("wallet", "connected", 1);
            hit_frame_marker();
        }
    }

    return sock;
}
```

### Code Example: P2P Messaging
The mail system (`compose_message.c` / `check_inbox.c`) uses Pieces for message tracking:

```c
// compose_message.c - Create a mail Piece
void compose_message(const char* to_wallet, const char* subject, const char* body) {
    char msg_id[64];
    snprintf(msg_id, sizeof(msg_id), "msg_%ld", time(NULL));

    char path[4096];
    snprintf(path, sizeof(path), "%s/projects/p2p-net/pieces/mail/%s/state.txt",
             project_root, msg_id);

    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "type=message\n");
        fprintf(f, "from=%s\n", current_wallet.wallet_id);
        fprintf(f, "to=%s\n", to_wallet);
        fprintf(f, "subject=%s\n", subject);
        fprintf(f, "body=%s\n", body);
        fprintf(f, "status=sent\n");
        fprintf(f, "timestamp=%ld\n", time(NULL));
        fclose(f);
    }

    hit_frame_marker();
}
```

### Blockchain Block Format
The blockchain is a file-backed ledger of verified blocks:

```
[BLOCK]
index=42
timestamp=1773622576
prev_hash=abc123def456...
transactions=tx_001,tx_002,tx_003
nonce=12345
hash=789xyz012abc...
miner=wallet_001
```

### Developer Example: Creating a P2P Identity

#### Step 1: Create Wallet Piece
```bash
mkdir -p projects/p2p-net/pieces/wallets/my_wallet
cat > projects/p2p-net/pieces/wallets/my_wallet/state.txt << EOF
wallet_id=my_wallet
public_key=abc123...
div_points=0
connected=0
EOF
```

#### Step 2: Connect to Network
```bash
./projects/p2p-net/ops/+x/connect_peer.+x my_wallet 192.168.1.100 8000
```

#### Step 3: Send First Message
```bash
./projects/p2p-net/ops/+x/compose_message.+x my_wallet friend_wallet "Hello" "Welcome to P2P-NET!"
```

#### Step 4: Check Inbox
```bash
./projects/p2p-net/ops/+x/check_inbox.+x my_wallet
```

### Code Example: P2P Discovery (Chat-Op)
Chat-Op (`projects/chat-op/`) implements a decentralized discovery model where nodes announce their presence via a shared registry.

```c
// p2p_heartbeat.c - Register self and discover peers
void heartbeat(const char *hash, int port, const char *subnet) {
    char *reg_dir = "pieces/network/registry";

    // 1. Register self (write status file)
    char *self_path;
    asprintf(&self_path, "%s/node_%s.txt", reg_dir, hash);
    FILE *sf = fopen(self_path, "w");
    fprintf(sf, "hash=%s\nport=%d\nsubnet=%s\nlast_seen=%ld\n", 
            hash, port, subnet, time(NULL));
    fclose(sf);

    // 2. Discover peers (read all status files)
    DIR *dir = opendir(reg_dir);
    while ((entry = readdir(dir)) != NULL) {
        // Read peer files, prune if last_seen > 15s
        if (now - p_last > 15) unlink(path);
        else {
            // Add to UI's connected_peers_list
            peer_count++;
        }
    }
}
```

### P2P Port Mapping
Nodes automatically bind to the first available port in the **8000-8010** range. This allows multiple instances of Chat-Op to run on the same physical machine for local network testing.

---

## 📜 Pillar 4: Smart & Legal Contracts
 (The Sovereign Law)
TPMOS brings the same "Law" to both the digital and physical simulations. ⚖️🤖

### Code Example: PAL Smart Contract
Direct marketing contract ("Like = Contract") implemented as a PAL script:

```asm
; direct_marketing_contract.asm
; When user A "likes" user B, a contract is formed
start:
    ; Read the "like" action
    read_state r1, "action_piece", "type"
    beq r1, r0, no_like  ; If no like action, exit

    ; Verify both parties exist
    call user::create_profile "buyer"
    call user::create_profile "seller"

    ; Create escrow
    OP p2p::escrow_create "item_001" "buyer" "seller"
    sleep 100000

    ; Wait for buyer confirmation
    read_state r2, "escrow_001", "status"
    addi r3, r0, 1  ; r3 = 1 (confirmed)
    beq r2, r3, confirmed

    ; Not confirmed, cancel
    OP p2p::escrow_cancel "item_001"
    halt

confirmed:
    ; Release funds to seller
    OP p2p::escrow_release "item_001" "seller"
    hit_frame
    halt

no_like:
    halt
```

### File-Based Escrow State Machine
```
[escrow_001/state.txt]
type=escrow
item=item_001
buyer=wallet_001
seller=wallet_002
amount=100
status=pending  ; pending -> confirmed -> released
created=1773622576
```

**Automated Enforcement:**
*   **P2P:** Escrow logic automatically holds and releases NFTs or TPM-Coins based on verifiable Piece states.
*   **LSR:** A "Gov" piece can automatically raid or tax a Company piece if a legal contract (like a land permit) is violated.
*   **Transparency:** All contracts are Piece-based, meaning their state and logic are fully auditable in plaintext. "If it's not in the file, it's not the law."

---

## 🏗️ Pillar 5: The Infrastructure Layer (The Gamified Gig Economy)
TPMOS isn't just an OS; it's a **Resource Exchange**. 🖥️🔋
*   **Trading Storage:** Users can "rent out" unused disk space as Piece containers for other users' encrypted backups.
*   **Trading Compute:** Need to run a massive AI training job? "Buy" the CPU cycles of other idle nodes in the network.
*   **Gamification:** This "Gig Economy" is built directly into the UI. Earning TPM-Coins by providing storage feels like a "Mining" minigame, bringing real-world value to the user.

### Dividend Points Data Structure
```
[DIVIDEND_POINTS]
holder=wallet_001
balance=1500
last_claim=1773622576
earned_from=storage,compute,participation
```

---

## 🔭 The Infinite Horizon
These systems are built on the same **True Piece Method** foundations. Whether it's a lunar base or a blockchain node, it's all just Pieces, Mirrors, and Pulses.

> "Softness wins. The empty center of the flexbox holds ten thousand things." 🧘‍♂️

---

## ⚠️ Common Pitfalls

### Pitfall 1: Ghost Port Listeners
**Symptom:** P2P-NET fails to start with "Address already in use" error.
**Cause:** Previous instance didn't close socket, or another process using port 8000.
**Fix:** Kill all processes (`./button.sh kill`), verify port is free:
```bash
lsof -i :8000  # Check what's using port
kill -9 <PID>  # Kill the process
```

### Pitfall 2: Blockchain File Corruption
**Symptom:** Chain validation fails, blocks don't link.
**Cause:** Multiple processes writing to blockchain simultaneously, or crash mid-write.
**Fix:** Use file locking for blockchain writes:
```c
#include <fcntl.h>
int fd = open("blockchain.txt", O_WRONLY | O_CREAT, 0644);
flock(fd, LOCK_EX);  // Exclusive lock
// Write block
flock(fd, LOCK_UN);  // Release
close(fd);
```

### Pitfall 3: Wallet State Mismatch
**Symptom:** Wallet shows connected=1 but no network activity.
**Cause:** State file not updated after disconnect, or network error not propagated.
**Fix:** Always update state on connect AND disconnect:
```c
// On disconnect:
set_state_int("wallet", "connected", 0);
hit_frame_marker();
```

### Pitfall 4: Cross-Project PAL Call Failures
**Symptom:** `OP p2p::connect_peer` does nothing.
**Cause:** P2P-NET not installed via Fondu, or ops not in catalog.
**Fix:** `./fondu --install p2p-net` first.

---

## 🏛️ Scholar's Corner: The "Minting of the Mooncrete"
In the early days of the `lsr` (Lunar Streetrace) simulation, the economy was purely hardcoded—food cost 10 coins, ore cost 50 coins. One day, a developer connected the helix-labs R&D center to the `ai-labs` local model. A bot "discovered" a new building material called **"Mooncrete."** The AI didn't just invent the name; it wrote the `.pdl` file for the material, set its weight and durability, and even calculated its trade value based on the rarity of its ingredients. This marked the birth of the **Dynamic R&D Engine**, where the world's tech tree is literally written by the AI as you play. 🌕🧪💎

---

## 📝 Study Questions
1.  How does the P2P manager handle multi-agent orchestration?
2.  What is the significance of the "Unified Contract Model" for both P2P and LSR?
3.  How can users "mine" TPM-Coins through the Infrastructure Layer?
4.  **Write a PAL script** that creates an escrow contract between two wallets.
5.  **Imagine:** You want to lease out your unused compute power to another user. What "Piece" do you need to publish to the network?
6.  **Scenario:** Your P2P node connects successfully but messages aren't being delivered. List three possible causes.

---
[Return to Index](INDEX.md)
