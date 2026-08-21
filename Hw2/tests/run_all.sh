#!/bin/bash
# run_all.sh - Runs hw2 on all test cases and saves outputs to outputs/

BINARY="./hw2"
SEED=42
TESTCASE_DIR="testcases"
OUTPUT_DIR="outputs"

if [ ! -f "$BINARY" ]; then
    echo "ERROR: $BINARY not found. Run 'make' first."
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

TESTCASES=(
    "testcase_ta"
    "testcase_simple1"
    "testcase_simple2"
    "testcase_simple3"
    "testcase_simple4"
    "testcase_simple5"
    "testcase_simple6"
    "testcase_comp1"
    "testcase_comp2"
    "testcase_comp3"
    "testcase_stress"
)

echo "Running all test cases with seed=$SEED"
echo "======================================="

for tc in "${TESTCASES[@]}"; do
    input="$TESTCASE_DIR/${tc}.txt"
    output="$OUTPUT_DIR/${tc}_output.txt"

    if [ ! -f "$input" ]; then
        echo "SKIP: $input not found"
        continue
    fi

    echo -n "Running $input -> $output ... "
    $BINARY $SEED $input > $output 2>&1
    if [ $? -eq 0 ]; then
        echo "OK ($(wc -l < $output) events)"
    else
        echo "FAILED (exit code $?)"
    fi
done

echo ""
echo "Done. Run 'python3 grader.py' to evaluate."
