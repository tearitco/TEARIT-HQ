#!/bin/sh
# the App Store builds ops/build_<id>.sh; True Swords Clash keeps its real recipe in build_tsc.sh
exec sh "$(dirname "$0")/build_tsc.sh"
