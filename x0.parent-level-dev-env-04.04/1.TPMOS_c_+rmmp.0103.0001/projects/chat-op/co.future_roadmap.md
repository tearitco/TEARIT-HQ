# Chat-Op Future Roadmap & User Identity Strategy (High-Level)

## 1. Future Feature Roadmap
We will implement these as modular sub-layouts accessible via the main Chat interface.

### A. BlockExplorer (Chain/Minting)
- Allow users to view the current blockchain state.
- Enable minting of custom digital tokens/assets.

### B. The Marketplace (Store)
- A global store where users can browse assets:
  - Consumables (potions).
  - Collectibles (monsters, cards).
  - Real Estate (houses, planets).
  - Transportation (cars, ships).

### C. Financial Layer
- **Send Coin:** Direct P2P transfer functionality integrated into the chat context.
- **Auction House:** A peer-to-peer marketplace for buying/selling assets and coins using a bidding system.

### D. Battle System
- Interactive turn-based or real-time battles between users using minted assets/monsters.

---

## 2. User Identity Strategy
We need to bridge the gap between anonymous P2P chat (current) and authenticated accounts.

### A. Current State
- Users are currently identified by a transient `node_hash` generated at startup.

### B. High-Level Plan for Persistent Identity
1. **Module Integration:** Integrate the existing `pieces/apps/user` module to handle login/signup.
2. **Account Binding:**
   - On startup, check for a valid local session file.
   - If logged in: Map the `node_hash` to the `user_id`. 
   - If guest: Retain `node_hash` but flag as `(GUEST)` in the UI.
3. **Identity Sync:** During the handshake, pass a `DISPLAY_NAME` and `PUBLIC_KEY` (or `user_id`) along with the `node_hash`.
4. **Permissions:** Host enforces permissions based on `user_id` (e.g., moderation tools for logged-in owners).

---

## 3. Next Session Priorities
- [ ] Review current `user` module to define integration points.
- [ ] Design the UI flow for logging in within the Chat-Op layout.
- [ ] Define the data contract for passing `user_id` during the P2P handshake.
