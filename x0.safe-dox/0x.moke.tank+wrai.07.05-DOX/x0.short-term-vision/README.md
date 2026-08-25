# Short-Term Vision Index
Date: 2026-06-28

This directory is the near-term planning surface for the current TPMOS push.

Priority order:
1. Wraith shell and filesystem consolidation
2. Wraith-native project/app discovery and nested nav
3. Wraith settings/customization baseline
4. Wraith editor bridge
5. XO-pets controller/editor proving lane

Documents:
- `jun-29-2do.txt`
- `wraith-shell-fs-roadmap-j28.md`
- `xo-editor-bridge-roadmap-j28.md`
- `0x-pet-wraith-architecture-j29.md`
- `ops-surface-j28.md`
- `ops-audit-table-j29.md`

Working assumptions taken from current docs:
- TPMOS standards come from `!.TPMOS_ONBORD_BIBLE_10.md`
- Wraith remains the main visual shell lane
- Wraith internal projects belong under `projects/wraith/wraith-projects/`
- Managers stay thin; reusable behavior belongs in ops first, then PAL/Prisc orchestration over those ops
- Testing should follow the key-injection and frame-report workflow from `_.0.aigent-testing-k3.txt`
- Active implementation target for current Wraith work is `1.TPMOS_c_+rmmp.0102.0027`, not the Wraith copy inside `x0.moke-pet-project-04.03`

Immediate decision:
- Do not branch into network or local-LLM work during this push.
- Do not start a broad new editor rewrite.
- Use Wraith filesystem/nav work as the enabling layer for later editor and XO work.
- Do not copy Wraith changes into `x0.moke-pet-project-04.03` yet. That tree is reference-only for now and should only receive Wraith updates after the `1.TPMOS` Wraith lane is actually polished.

Execution rule:
- If a shell, filesystem, launch, save/load, settings, or controller action can be expressed as a reusable op with arguments, do that before adding manager-specific logic.
- PAL/Prisc should later call those same ops rather than duplicating the behavior in new C paths.
- Reuse order is strict: use an existing op first, widen an existing op second, create a new op last.
- If an existing op needs risky modification, copy and modify it locally first, prove the local variant, keep backward compatibility for the old behavior, and only replace/shared-upgrade it after the local version is validated.
