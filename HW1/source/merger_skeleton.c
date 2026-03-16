/*
 * Merger Skeleton - CENG 334 Homework 1
 * Complete this skeleton to implement the merge operator controller.
 *
 * TODO: Parse input from stdin using parse_merger_input()
 * TODO: For each chain, fork and set up pipes
 * TODO: Only the main merger reads the CSV file; it reads the whole file and selects lines for each chain.
 * TODO: A sub-merger does not open the file; it receives spec first, then all of its CSV data on stdin.
 * TODO: Chain lines still use start_line/end_line; merger headers do not.
 * TODO: Operators are provided; they do not read the file, only stdin and their arguments.
 * TODO: Exec operators in pipeline (sort | filter | unique)
 * TODO: Merge chain outputs in chain order (order of appearance) and write to stdout
 * TODO: Handle recursive merger chains (merger is always followed by sub-input on the next lines).
 * TODO: Reap all children (no zombies); print operator exit status lines to stdout at the end.
 */

#include "merger_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char **argv) {
    merger_node_t *root = parse_merger_input(stdin);
    if (!root) {
        fprintf(stderr, "merger: parse error\n");
        return 1;
    }

    /* TODO: Implement execution logic
     * 1. If this is the main merger, open the CSV file and use chain ranges to choose which lines go to each child chain.
     * 2. If this is a sub-merger, read all remaining stdin as its local CSV block; chain ranges are relative to that local block.
     * 3. For each chain: if merger_child, start a sub-merger and write sub-spec + CSV data to its stdin; else start an operator pipeline.
     * 4. Make input feeding and output draining progress concurrently enough to avoid pipe deadlock on large inputs.
     * 5. Read each chain's output in chain order and print the combined result to stdout.
     * 6. Wait for all children (no zombies); then print EXIT-STATUS lines to stdout.
     */

    free_merger_tree(root);
    return 0;
}
