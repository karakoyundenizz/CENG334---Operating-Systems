/*
 * Merger Skeleton - CENG 334 Homework 1 (C++)
 * Complete this skeleton to implement the merge operator controller.
 *
 * TODO: Parse input from stdin using parse_merger_input()
 * TODO: For each chain, fork and set up pipes
 * TODO: Only the main merger reads the CSV file; it reads the whole file and selects lines for each chain.
 * TODO: A sub-merger does not open the file; it receives spec first, then all of its CSV data on stdin.
 * TODO: Chain lines still use start_line/end_line; merger headers do not.
 * TODO: Exec operators in pipeline (sort | filter | unique)
 * TODO: Merge chain outputs in chain order and write stdout
 * TODO: Handle recursive merger chains; start sub-mergers, pass data via stdin, print EXIT-STATUS lines at end, and leave no zombies.
 */

#include "merger_parser.h"
#include <cstdio>
#include <cstdlib>
// to have the PIPE macro work this library includes socketpair() func
#include <sys/socket.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>
#include <string>
#include <vector>

// macro for creating pipes
#define PIPE(fd) socketpair(AF_UNIX, SOCK_STREAM, PF_UNIX, fd)

void connect_and_close_pipes(int cur_pipe_indx, std::vector<std::vector<int>> file_dscrs, int num_pipes)
{

    // child process since fork returned 0
    int stdin_pipe = cur_pipe_indx;
    int stdout_pipe = stdin_pipe + 1;

    // ith pipe will be our current stdin pipe. connect read end of the current pipe to stdin
    dup2(file_dscrs[stdin_pipe][0], 0); // 0 is stdin

    // i+1th pipe will be stdout pipe. connect stdout to write end of the next pipe
    dup2(file_dscrs[stdout_pipe][1], 1); // 1 is stdout

    // close all the pipes and write end of the stdin pipe and read end of stdout pipe
    for (int j = 0; j < num_pipes; j++)
    {
        close(file_dscrs[j][0]);
        close(file_dscrs[j][1]);
    }
}

int main(int argc, char **argv)
{
    merger_node_t *root = parse_merger_input(stdin);
    if (!root)
    {
        fprintf(stderr, "merger: parse error\n");
        return 1;
    }

    // to avoid memory access for each checking create limit var
    int num_chains = root->num_chains;

    // create a loop to iterate each operator chain
    for (int chain_num = 0; chain_num < num_chains; chain_num++)
    {
        // get the current operator chain and check whether it is su-merger or normal chain with operators
        operator_chain_t cur_chain = root->chains[chain_num];

        int num_ops = cur_chain.num_ops;

        // create each operator process and connect the created pipes between operators and close the unsused ones

        bool is_submerge = (cur_chain.merger_child == NULL) ? false : true;

        if (is_submerge)
        {
            // CASE 1: The sub-merger case
            
        }

        else
        {
            // CASE 2: Chain is composed of series of operators

            // number of pipes to create to provide communication among processes
            int num_pipes = num_ops + 1;
            // a vevctor to keep all the file descriptor indexes
            std::vector<std::vector<int>> file_dscrs(num_pipes, std::vector<int>(2));

            // list of child pids
            std::vector<pid_t> child_pids(num_ops);

            // is process parent or child
            bool is_parent = true;

            // to create all the pipes from the begining
            // file_dscrs[0] will be feeding pipe from merger to starting of the chain operation
            // file_dscrs[-1] will be result pipe from last operator to merger
            for (int j = 0; j < num_pipes; j++)
            {
                int fd[2];
                PIPE(fd);
                file_dscrs[j][0] = fd[0];
                file_dscrs[j][1] = fd[1];
            }

            for (int op_num = 0; op_num < num_ops; op_num++)
            {
                if ((child_pids[op_num] = fork()) == 0)
                {
                    // CASE 2.1: CHILD PROCESS

                    // change the variable is_parent so that it is false
                    is_parent = false;
                    // connect read end and write end and close all the pipes
                    connect_and_close_pipes(op_num, file_dscrs, num_pipes);

                    // run operator with specific arguments of the operator
                }
            }

            if (is_parent)
            {
                // CASE 2.2: PARENT PROCESS

                // wait for all the child processes
                for (int proc = 0; proc < num_ops; proc++)
                {
                    int status;
                    waitpid(child_pids[proc], &status, 0);
                    printf("EXIT-STATUS %d %d", child_pids[proc], status);
                }
            }
        }
    }

    // create 2 pipes one is for stdin for first chain and one is for result of the chain to the merger

    // visit all the operator chains using num_chains variable

    /* TODO: Implement execution logic
     * 1. If this is the main merger, open the CSV file and select lines for each chain.
     * 2. If this is a sub-merger, read all remaining stdin as its local CSV block; chain ranges are relative to that block.
     * 3. For each chain: if merger_child, start a sub-merger and write sub-spec + CSV data to its stdin; else start an operator pipeline.
     * 4. Make input feeding and output draining progress concurrently enough to avoid deadlock on large inputs.
     * 5. Read each chain output in chain order and print the combined result to stdout.
     * 6. Wait for all children, then print EXIT-STATUS lines to stdout.
     */

    free_merger_tree(root);
    return 0;
}
