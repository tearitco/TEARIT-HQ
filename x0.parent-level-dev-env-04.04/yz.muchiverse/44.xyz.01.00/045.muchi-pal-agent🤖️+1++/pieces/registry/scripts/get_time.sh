#!/bin/bash
# get_time.sh - example provider_kind=script trigger target, triggered
# by the word "time" (see pieces/registry/triggers/trigger_list.txt's
# own header comment). Called as: get_time.sh "<user's chat message>" -
# argv[1] is the raw message that matched the trigger word, available
# if a real script wants to react to more than just the trigger itself.
# stdout becomes the chat response verbatim - send_message.c's own
# run_script_capture trims trailing newlines only, nothing else, and
# treats a nonzero exit as failure.
echo "It is currently $(date '+%A, %B %d, %Y at %I:%M %p')."
