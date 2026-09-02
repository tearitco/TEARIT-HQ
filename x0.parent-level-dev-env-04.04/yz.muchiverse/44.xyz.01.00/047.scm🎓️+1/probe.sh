#!/bin/sh
# selection sanity probes: greedy-ish run at low temperature; wants a hit
# each line: "message --expect=feature|goal" (goal is loose, read the output)
cd "$(dirname "$0")"
B=+x/scm_cli
echo "--- hello (expect greet-heavy, non-generic)"
$B select corpuses/small-talk "hello there!" --seed 7 --scores
echo "--- a direct question (expect question-biased phrase)"
$B select corpuses/small-talk "is there any news today?" --seed 7 --scores
echo "--- a complaint / no-thanks (expect neg-leaning phrase)"
$B select corpuses/small-talk "no thanks, im not interested" --seed 7 --scores
echo "--- slot template under /rp tavern (sweep seeds to hit the {drink} phrase)"
for s in 1 2 3 4 5 6 7 8; do
  echo -n "seed $s: "
  $B select corpuses/tavern_rp "i need a drink" --seed $s --rp
done
echo "--- meta grumpy override suppresses warm opener"
$B select corpuses/small-talk "hello there!" --seed 7 --meta grumpy --scores
