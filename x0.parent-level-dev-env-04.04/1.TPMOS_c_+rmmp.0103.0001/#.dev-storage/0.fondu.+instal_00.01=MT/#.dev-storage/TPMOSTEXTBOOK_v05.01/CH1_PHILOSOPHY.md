# 🧬 Chapter 1: The Soul of a Piece
**The True Piece Method (TPM)** is the governing philosophy of this entire OS. In TPMOS, we don't think in terms of objects or classes; we think in terms of **Pieces**. 🧱

---

## 🏗️ The PMO Hierarchy
The system follows a strict thought priority. If you violate this, the "Mirror" will crack! 🪞

1.  **PIECE (The Atomic Unit / Soul) 🧱**
    *   Everything is a Piece. A button, a player, a map, or a galaxy.
    *   **Rule:** A Piece owns its state *exclusively*.
    *   **File:** `piece.pdl` (DNA) + `state.txt` (Mirror).

2.  **MODULE (The Logic Agent / Brain) 🧠**
    *   The Module is the "Manager". It listens to the user and decides what the Pieces should do.
    *   **Mandate:** It should be a "Thin Brain". It doesn't do the heavy lifting; it delegates.

3.  **OS / CHTPM (The Theater / View) 🎭**
    *   The OS is the stage where Pieces perform. It handles the UI, the layouts, and the "Magic" (variable substitution).

---

## 🧬 Anatomy of Piece DNA (`.pdl`)
Every Piece has a `.pdl` file. This is its blueprint. Think of it as the code that defines what a Piece *is* and what it *can do*.

```pdl
<piece_id>my_hero</piece_id>
<traits>
    <trait>movable</trait>
    <trait>auditable</trait>
</traits>
<methods>
    <method id="move_north" cmd="./move_entity.+x north" />
    <method id="say_hello" cmd="echo 'Hello World!' > last_response.txt" />
</methods>
```

### 🪞 The Mirror (`state.txt`)
While DNA defines the *potential*, the **Mirror** defines the *now*. It is a flat text file for high-speed reading.
*   `x=10`
*   `y=5`
*   `health=100`
*   `status=idle`

> 💡 **Pro Tip:** "If it's not in a file, it's a lie." We never trust memory. If a process dies, the file remains. Reality persists. 💾

---

## 🧘‍♂️ The Zen of TPM
*   **Data Sovereignty:** No piece touches another's files without permission.
*   **Auditability:** Every major change is logged to the `master_ledger.txt`.
*   **Recursive Reality:** A Piece can contain other Pieces (Inception style! 🌀). This is known as the **Scale-Free Container Model**. 

### 📦 The Scale-Free Container Model
In TPMOS, directory containment is the authoritative source of truth.
*   **Maps are Containers:** A map is just a Piece that contains other Pieces (Player, NPCs, Trees).
*   **Pieces are Containers:** A Player Piece can contain a "Bag" Piece, which contains a "Crayon Box" Piece, which contains "Crayon" Pieces. 
*   **Unified Rule:** The engine treats all containers uniformly. Whether it's a Galaxy containing Solar Systems or a Chest containing Gold, the logic is identical. 🌌💰

---

## 🏛️ Scholar's Corner: The "Mirror of Tomokazu"
There is a legendary anecdote among TPMOS engineers known as the **"Mirror of Tomokazu."** Early in development, a rogue developer tried to optimize the system by keeping a player's HP in a global variable instead of writing it to `state.txt`. During a stress test, the process crashed. When the system rebooted, the player had 0 HP and was "permanently dead," despite having been full health seconds before. This disaster led to the founding mandate: **"If it's not in a file, it's a lie."** From that day on, every piece of reality had to be mirrored in a file, ensuring immortality through persistence. 🕯️

---

## 📝 Study Questions
1.  What are the three layers of the PMO hierarchy?
2.  What is the difference between a Piece's "DNA" (.pdl) and its "Mirror" (state.txt)?
3.  Why does TPMOS avoid using in-memory variables for long-term state?
4.  **True or False:** A galaxy and a button can both be considered "Pieces" in TPMOS.

---
[Return to Index](INDEX.md)
