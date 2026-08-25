# 📚 TPMOS_TEXTBOOK 🎓
## The Definitive Guide to the Mono-OS (基于件的系统指南)

### Version 05.00 (Exo-Sovereignty Edition)

---

## Table of Contents

### Part I: Foundations
1. [The Soul of a Piece (Philosophy)](CH1_PHILOSOPHY.md)
2. [The Filesystem is the Database](CH2_FILE_SYSTEM.md)
3. [The 12-Step Pipeline](CH3_PIPELINE.md)

### Part II: Development
4. [Muscles & Brains (Ops & Modules)](CH4_DEVELOPMENT.md)
5. [The App Factory](CH5_SYSTEM_APPS.md)
6. [PAL: The Assembly Language of TPMOS](CH6_PAL.md)

### Part III: Flagship Applications
7. [fuzz-op & op-ed: The Flagship Apps](CH7_FUZZ_OP_OP_ED.md)
8. [GL-OS: Beyond ASCII (3D TPMOS)](CH8_GL_OS.md)

### Part IV: Advanced Systems
9. [The Guardians (Testing & Simulation)](CH9_TESTING.md)
10. [The Great Expansion (AI, LSR, & P2P)](CH10_FUTURE_HORIZONS.md)
11. [The Infinite Loop (Recursive Forge)](CH11_RECURSIVE_FORGE.md)
12. [The Simulation Theater](CH12_SIMULATION_THEATER.md)
13. [Exo-Sovereignty (External Operating Exo-Bots)](CH17_EXO_SOVEREIGNTY.md)
14. [Dynamic Trait Menus (PDL-Driven UI)](CH18_DYNAMIC_TRAIT_MENUS.md)

### Part V: Vision & Future
15. [Piecemark Labs & The Sovereign Venture](CH13_BUSINESS_STRATEGY.md)
16. [The Soul Pen & The Multiverse](CH14_SOUL_PEN.md)
17. [Cross-Platform TPMOS](CH15_CROSS_PLATFORM.md)
18. [Common Pitfalls & Debugging Guide](CH16_PITFALLS_DEBUGGING.md)

---

## Appendices
- [Known Bugs & Research Tasks](KNOWN_BUGS.md)
- [Glossary](GLOSSARY.md)
- [Quiz](QUIZ.md)
- [Answer Key](ANSWER_KEY.md)

---

## Dependency Graph

```mermaid
graph TD
    subgraph Foundations
        CH1[CH1: Philosophy]
        CH2[CH2: Filesystem]
        CH3[CH3: Pipeline]
    end

    subgraph Development
        CH4[CH4: Ops & Modules]
        CH5[CH5: App Factory]
        CH6[CH6: PAL]
    end

    subgraph Applications
        CH7[CH7: fuzz-op & op-ed]
        CH8[CH8: GL-OS]
        CH18[CH18: Trait Menus]
    end

    subgraph Advanced
        CH9[CH9: Testing]
        CH10[CH10: P2P-NET]
        CH11[CH11: Recursive Forge]
        CH12[CH12: Simulation]
        CH17[CH17: Exo-Bots]
    end

    subgraph Vision
        CH13[CH13: Business]
        CH14[CH14: Soul Pen]
        CH15[CH15: Cross-Platform]
        CH16[CH16: Pitfalls]
    end

    CH1 --> CH2
    CH2 --> CH3
    CH3 --> CH4
    CH4 --> CH5
    CH4 --> CH6
    CH5 --> CH7
    CH6 --> CH7
    CH6 --> CH9
    CH6 --> CH10
    CH6 --> CH11
    CH7 --> CH8
    CH8 --> CH12
    CH9 --> CH10
    CH10 --> CH13
    CH11 --> CH12
    CH17 --> CH9
    CH15 --> CH16
    CH7 --> CH18

    style CH1 fill:#e1f5fe
    style CH4 fill:#f3e5f5
    style CH7 fill:#e8f5e9
    style CH10 fill:#fff3e0
    style CH17 fill:#e1f5fe
    style CH18 fill:#e8f5e9
    style CH16 fill:#ffebee
```

---

[Return to Main Index](../README.md)
