# solar-sandbox (step 1 of SOLAR-SANDBOX-AND-PLANET-PHYSICS-DESIGN.md)

Tracked template files. A live session keeps its desks under `xyzfs/users/<uuid>/home/livedesk/sessions/<id>/` (untracked user data), so nothing here is active until installed.

Install (by hand, into one session, never into a code branch):
1. copy `desks/*.pdl` to `sessions/<id>/desks/`
2. copy `game.pdl` to `sessions/<id>/game.pdl`
3. switch to desk `solar-system`. Every `DESK` row points at `&.widgits/solar-sandbox/pals/<entity>` under the house root, so the entity folders stay here.

Bodies are static emoji entities (sun, Earth, Moon, Mars). Right-click menu rows run the existing `events-hq/ops/+x/mr_transfer_desk.+x` through an inline `sh -c` METHOD row (resolves user, session, then the op). Pages: `solar-system` (hub), `solar-earth`, `solar-moon`, `solar-mars`; each body page has a `<body>_surface` entity (Leave orbit plus Teleport rows) and a shared `leave_orbit` rocket. No `cursword` row on these desks (the player body is the user's own).

Proof: `&.widgits/_shared-lib/harness/solar_sandbox.pal` (cases/solar_sandbox.pdl).
