# sample-hq

A real, deliberately tiny, disposable-content app kept in the house on
purpose (2026-09-15, direct live instruction: "u can commit the
testing-app, i may use it myself when testing ur work on this") as a
safe fixture for exercising create-package.sh / export-hq / reintegrate.sh
without ever risking a real app's real files.

Not meant to actually launch as a working window (button.sh below is a
stub, not a real khtpm module) - it exists purely to have a real
toy.pdl (so it shows up as a real create-package.sh/export-hq
candidate and in the taskbar's own Toys scan) plus one real file worth
editing, for round-trip testing:

  1. Package it:      &.hq-apps/create-package/create-package.sh sample-hq /tmp/sample-hq-pkg
  2. Edit the copy:    edit /tmp/sample-hq-pkg/44.xyz.01.00/&.hq-apps/sample-hq/README.md
  3. Add a new house file (proves reintegrate.sh never deletes):
                       echo hi > &.hq-apps/sample-hq/NEW-FROM-HOUSE.txt
  4. Dry run:          /tmp/sample-hq-pkg/reintegrate.sh <house_root>
  5. Apply:            /tmp/sample-hq-pkg/reintegrate.sh <house_root> --apply
  6. Verify: the edit landed AND NEW-FROM-HOUSE.txt is still there.

SENTINEL: original-content-v1
