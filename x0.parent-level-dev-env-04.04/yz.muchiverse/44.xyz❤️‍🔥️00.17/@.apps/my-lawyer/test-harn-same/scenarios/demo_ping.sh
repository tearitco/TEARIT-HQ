#!/bin/bash
# demo_ping.sh - minimal smoke test, NOT a real gameplay scenario. This
# project's own "demo" action used to point at demo_research_and_end_
# turn.sh, which was never actually written (an empty scenarios/ dir -
# confirmed 2026-08-20 during the house-wide harness sweep). Writing a
# real gameplay scenario needs real knowledge of my-lawyer's own UI/
# mechanics that this session doesn't have - rather than fake a deep
# test, this is an honest floor: does button.sh actually launch a real
# session and render a real frame. See $.crypts/ping-project.sh for the
# shared implementation and how to reuse it in any other project.
HARNESS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_DIR="$(cd "$HARNESS_DIR/.." && pwd)"
HOUSE_DIR="$(cd "$PROJECT_DIR/../.." && pwd)"
exec bash "$HOUSE_DIR/\$.crypts/ping-project.sh" "$PROJECT_DIR"
