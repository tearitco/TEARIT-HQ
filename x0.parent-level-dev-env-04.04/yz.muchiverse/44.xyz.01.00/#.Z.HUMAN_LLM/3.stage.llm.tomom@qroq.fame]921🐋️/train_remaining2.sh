#!/bin/bash
for vocab in \
    curriculum/Grade8_science/Grade8_science.txt \
    curriculum/HighSchool_algebra/HighSchool_algebra.txt \
    curriculum/HighSchool_biology/HighSchool_biology.txt \
    curriculum/HighSchool_chemistry/HighSchool_chemistry.txt \
    curriculum/HighSchool_physics/HighSchool_physics.txt \
    curriculum/College_calculus/College_calculus.txt \
    curriculum/College_organic_chemistry/College_organic_chemistry.txt \
    curriculum/College_quantum_physics/College_quantum_physics.txt \
    curriculum/remedial/remedial.txt \
    curriculum/teacher_time/teacher_time.txt \
    curriculum/extra_credit/extra_credit.txt \
    curriculum/recess/recess.txt; do
    
    dir=$(dirname "$vocab")
    if [ -f "$dir/loss.txt" ]; then continue; fi
    echo "[START] $vocab"
    ./+x/trainer.+x "$vocab" 2>&1 | grep "Epoch 50/"
    echo "[DONE] $vocab"
done
