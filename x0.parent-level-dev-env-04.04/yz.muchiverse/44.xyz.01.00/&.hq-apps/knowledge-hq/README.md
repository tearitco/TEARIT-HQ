# knowledge-hq

X11-HQ window "Knowledge HQ": browses the labeled chemistry knowledge in
`&.widgits/concept-bank` (`data/chem`, `data/hidden`, `proposals/chem_assoc`):
86 compounds (filter ALL/OK/BAD/APPROVED/REJECTED/UNREVIEWED), a card per compound
(label fields, proposed association as three bars, manager + owner review, approved
vector, Laplace exam scores from `chem_feedback.txt`), and the 118 elements as a
wrapping tile grid.

Shape = `concept-bank-hq`: `knowledge-hq.xhtpm` + `.css` + ONE manager
(`ops/knowledge_manager.c`, built by `ops/build_knowledge_manager.sh` into git-ignored
`ops/+x/`). No renderer changes. Launch: `sh open_knowledge_hq.sh <house_root>`.

Verbs (`knowledge_action.txt`: `seq=<n>` / `cmd=<VERB[:arg]>`; published state is
`knowledge_ui.txt`): `SHOW:compounds|elements`, `FILTER:<f>`, `SEL:<n>`, `SELEL:<Z>`,
`BACK`, `RELOAD`, and the owner review verbs `ACCEPT`, `REJECT`, `ADJUST:<e>:<f>:<m>`
(decimals in [-1,1], at most 2 decimals, else refused on screen). The review verbs append
one `REVIEW | n | accept|reject|adjust | ... | by=owner | via knowledge-hq` row to
`proposals/chem_assoc/review_owner.txt`; that is the only file the manager ever writes
outside its own package dir. Owner rows outrank `review.txt`; the later owner row wins.

`knowledge_manager --once <house_root> <pkg>` applies one pending action and exits
(harness mode): `_shared-lib/harness/knowledge_hq.pal`.
ADJUST has no button (no text input in the layout): drive it through the action file.
