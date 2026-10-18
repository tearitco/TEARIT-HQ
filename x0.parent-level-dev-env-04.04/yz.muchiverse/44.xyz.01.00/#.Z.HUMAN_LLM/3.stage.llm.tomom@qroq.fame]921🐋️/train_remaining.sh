#!/bin/bash
cd "$TIM"

for vocab in \
    curriculum/Grade6_science/Grade6_science.txt \
    curriculum/Grade7_math/Grade7_math.txt \
    curriculum/Grade7_science/Grade7_science.txt \
    curriculum/Grade8_math/Grade8_math.txt \
    curriculum/Grade8_science/Grade8_science.txt \
    curriculum/HighSchool_algebra/HighSchool_algebra.txt \
    curriculum/HighSchool_biology/HighSchool_biology.txt \
    curriculum/HighSchool_chemistry/HighSchool_chemistry.txt \
    curriculum/HighSchool_physics/HighSchool_physics.txt \
    curriculum/extra_credit/extra_credit.txt \
    curriculum/recess/recess.txt \
    curriculum/remedial/remedial.txt \
    curriculum/teacher_time/teacher_time.txt; do
    
    dir=$(dirname "$vocab")
    if [ -f "$dir/loss.txt" ]; then
        echo "[SKIP] $vocab (already trained)"
        continue
    fi
    
    echo "[START] $vocab"
    ./+x/trainer.+x "$vocab" 2>&1 | grep -E "Loss:" | tail -1
    echo "[DONE] $vocab"
done
