# TPMOS Documentation Index

**Last Updated:** April 2, 2026  
**Status:** Active

Welcome to the TPMOS documentation hub. This index organizes all documentation by purpose and currency.

---

## 🚀 START HERE

### For New Developers/Agents
| Document | Purpose | Location |
|----------|---------|----------|
| **README.md** | Main project overview | Project root |
| **AGENT_ONBOARDING.txt** | Agent-specific onboarding | `#.docs/` |
| **TPMOS_TEXTBOOK_v03.00/** | Comprehensive textbook | Project root |

### Quick Reference
| Document | Purpose |
|----------|---------|
| **OPS_DICTIONARY.txt** | Complete ops catalog |
| **TPMOS_APP_REFERENCE.txt** | App vs Project topology |
| **COMPILE_SYSTEM.md** | Build system guide |
| **PITFALLS_ACTIVE_2026-03-18.txt** | Known issues & pitfalls |

---

## 📊 INVESTIGATION REPORTS (April 2026)

Recent deep-dive investigations into system components:

| Report | Status | Reference |
|--------|--------|-----------|
| **Fondu Investigation** | ✅ 100% Complete | `fondu-investigation-report.txt` |
| **op-ed Investigation** | ⚠️ 81% Complete | `op-ed-investigation-report.txt` |

### Key Findings:
- **Fondu:** Fully implemented (Phase 1) - lifecycle manager working
- **op-ed:** 13/16 features working - missing delete piece & mirror sync
- **Windows:** Working with limitations (arrow keys, XInput untested)

---

## 📚 HOW-TO GUIDES

### Fondu Lifecycle Manager
| Guide | Location |
|-------|----------|
| How-to Guide | `#.main/#.fondu.2.do/^.fondu-how2.txt` |
| KPIs & Testing | `#.main/#.fondu.2.do/#.fondu-kpis.txt` |
| Architecture Spec | `#.main/#.fondu.2.do/#.fondu-clarified-ap1.txt` |

### op-ed (RMMP Editor)
| Guide | Location |
|-------|----------|
| Testing Guide | `#.testing/op-ed-testing-guide.txt` |
| Feature Comparison | `#.main/15.oped-vs-edleg.txt` |

### Development
| Guide | Location |
|-------|----------|
| CPU-Safe Template | `#.docs/cpu_safe_module_template.c` |
| PITFALLS Active | `#.docs/^.pmo.ld-faq+8/PITFALLS_ACTIVE_2026-03-18.txt` |
| Compile Clean | `#.docs/^.pmo.ld-faq+8/^.compile_clean.txt` |

---

## 🗂️ DOCUMENTATION BY DIRECTORY

### `#.docs/` - Main Documentation
Active reference docs, dictionaries, and standards.

### `#.docs/^.pmo.ld-faq+8/` - FAQ & Pitfalls
Development FAQ, known issues, and standards.

### `#.docs/future-facing/` - Future Concepts
Planning documents, conceptual specs, and roadmap items.

### `#.main/` - System Specifications
Architecture specs for major systems (Fondu, op-ed, etc.).

### `#.testing/` - Testing Documentation
Test plans, testing guides, and verification procedures.

### `#.docs/ARCHIVE/` - Historical Documents
Superseded docs, dev sprint notes, and historical reference.

---

## 📋 CURRENT STATUS SUMMARY

### ✅ Completed (April 2026)
- Fondu Lifecycle Manager (Phase 1)
- op-ed 81% feature complete
- Windows support (with documented limitations)
- Documentation cleanup Phase 1

### 🎯 In Progress
- op-ed final features (delete piece, mirror sync)
- Documentation consolidation Phase 2
- Project topology cleanup

### 📅 Planned
- P2P-NET (conceptual - not implemented)
- AI-Labs integration
- GL-OS enhancements

---

## 🔗 EXTERNAL LINKS

| Resource | Description |
|----------|-------------|
| `pieces/buttons/WINDOWS-RUN-GUIDE.md` | Windows execution guide |
| `TPMOSTEXTBOOK_v03.00/INDEX.md` | Textbook table of contents |
| `doc_diff.txt` | Documentation diff report |

---

## 📞 NEED HELP?

1. **Check PITFALLS_ACTIVE** - Your issue may be documented
2. **Review investigation reports** - Fondu/op-ed status
3. **Consult the Textbook** - Comprehensive TPMOS guide
4. **Ask in #.docs/** - Many questions already answered

---

*"Softness wins. The empty center of the flexbox holds ten thousand things."*
