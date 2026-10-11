# pet_evolve.asm - Pet evolves (increase level, mutate traits)
# Used by: brain_dispatch.asm -> evolve
# Runs wsr_evolve to mutate trait thresholds in behavior.pdl

start:
    console_print "Fuzzball: *glows with energy*"
    exec ./ops/+x/wsr_evolve.+x pieces/display/fitness.txt pieces/apps/fuzzpet_app/traits/ pieces/display/pet_population.txt
    relay_write EVOLVE|level_up|fuzzball
    hit_frame
    halt
