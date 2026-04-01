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

#include <cstdio>
#include <cstdlib>

#include "merger_parser.h"
// to have the PIPE macro work this library includes socketpair() func
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <vector>
// included for reading and writing to stdin stdout
#include <fstream>  // Required to read files safely in C++
#include <iostream>


// delete when submitting
#include <cerrno>   // Gives you access to the 'errno' variable
#include <cstring>  // Gives you access to strerror()
// macro for creating pipes
// TODO: change the third variable to PX_UNIX or something like that when submitting
#define PIPE(fd) socketpair(AF_UNIX, SOCK_STREAM, 0, fd)





// file class to save the pipe into this vector
class File {
       public:
        std::vector<std::string> lines;

        // Pass the stream as a reference so we can choose file vs stdin!
        void read_all(std::istream& in_stream) {
                std::string line;
                // TODO: not sure if it needs to stay or not because it works correctly without it as well.
                // in_stream.clear();
                // fprintf(stderr, "in the read_all\n");

                while (std::getline(in_stream, line)) {
                        lines.push_back(line + "\n");
                }
        }


        void write_interval(int start_line, int end_line, int file_index) {
                int num_lines = lines.size();
                if (num_lines == 0) {
                        fprintf(stderr, "EXITTED: Asked lines from %d to %d but file has %d lines", start_line, end_line, num_lines);
                        exit(1);
                }


                if (end_line > num_lines) {
                        fprintf(stderr, "Warning: end_line(%d) is bigger than number of lines so made end_line = num_lines(%d) \n", end_line, num_lines);
                        end_line = num_lines;
                }

                // fprintf(stderr, "Writing all the lines from %d to %d \n", start_line, end_line);
                for (int i = start_line - 1; i < end_line; i++) {
                        std::string line = lines[i];
                        // fprintf(stderr, "Line %s in pipe\n", line.c_str());
                        write(file_index, line.c_str(), line.length());
                }
        }
};



void execute_operator(operator_t op) {
        // set all the arguments of the given operator

        std::vector<std::string> args;

        // determine the operator
        if (op.type == OP_SORT)
                args.push_back("./sort");
        else if (op.type == OP_FILTER)
                args.push_back("./filter");
        else if (op.type == OP_UNIQUE)
                args.push_back("./unique");

        // add the common arguments
        args.push_back("-c");
        args.push_back(std::to_string(op.column));

        args.push_back("-t");
        if (op.col_type == TYPE_TEXT)
                args.push_back("text");
        else if (op.col_type == TYPE_NUM)
                args.push_back("num");
        else if (op.col_type == TYPE_DATE)
                args.push_back("date");

        // add operator-specific arguments
        if (op.type == OP_SORT && op.reverse) {
                args.push_back("-r");
        } else if (op.type == OP_FILTER) {
                // map the enum back to the string flag
                if (op.cmp == CMP_GT)
                        args.push_back("-g");
                else if (op.cmp == CMP_LT)
                        args.push_back("-l");
                else if (op.cmp == CMP_EQ)
                        args.push_back("-e");
                else if (op.cmp == CMP_GE)
                        args.push_back("-ge");
                else if (op.cmp == CMP_LE)
                        args.push_back("-le");
                else if (op.cmp == CMP_NE)
                        args.push_back("-ne");

                // add the threshold value
                args.push_back(op.cmp_value);
        }

        // convert std::string vector to the char* array expected by execvp
        std::vector<char*> exec_args;
        // fprintf(stderr, "Calling exec with arguments:\n");
        for (size_t i = 0; i < args.size(); i++) {
                // We must cast away constness because execvp requires char*, not const char*
                // fprintf(stderr, "Calling exec for %s with arguments: %s \n", args[0].c_str(), const_cast<char*>(args[i].c_str()));
                exec_args.push_back(const_cast<char*>(args[i].c_str()));
        }
        exec_args.push_back(nullptr);  // MUST be NULL-terminated!



        // execute the operator
        execvp(exec_args[0], exec_args.data());

        // If execvp returns, it failed to launch the program
        fprintf(stderr, "Exec command for %s failed with arguments: \n", args[0].c_str());
        for (size_t i = 0; i < args.size(); i++) {
                fprintf(stderr, "argument %zu: %s \n", i, (args[i].c_str()));
        }
        exit(1);
}







void connect_and_close_pipes(int cur_pipe_indx, std::vector<std::vector<int>>& file_dscrs, int num_pipes) {
        // child process since fork returned 0
        int stdin_pipe = cur_pipe_indx;
        int stdout_pipe = stdin_pipe + 1;

        // ith pipe will be our current stdin pipe. connect read end of the current pipe to stdin
        // 0 is stdin
        if (dup2(file_dscrs[stdin_pipe][0], 0) < 0) {
                fprintf(stderr, "EXITTED: Couldnt change stdin to %d indexed file descriptor %s \n", stdin_pipe, strerror(errno));
                exit(1);
        }



        // i+1th pipe will be stdout pipe. connect stdout to write end of the next pipe
        // 1 is stdout
        if (dup2(file_dscrs[stdout_pipe][1], 1) < 0) {
                fprintf(stderr, "EXITTED: Couldnt change stdout to %d indexed file descriptor %s \n", stdout_pipe, strerror(errno));
                exit(1);
        }

        // fprintf(stderr, "tamam doğru yerleştrilmiş akkkk\n");

        // close all the pipes and write end of the stdin pipe and read end of stdout pipe
        for (int j = 0; j < num_pipes; j++) {
                close(file_dscrs[j][0]);
                close(file_dscrs[j][1]);
        }
}


void read_pipe_until_EOF(int read_fd) {
        char buffer[4096];
        ssize_t bytes_read;

        int count = 0;
        // fprintf(stderr, "Writing everything from pipe to stdout\n");

        // read() pulls bytes directly from the pipe. It returns 0 when the child closes it (EOF).
        while (true) {
                bytes_read = read(read_fd, buffer, sizeof(buffer));

                if (bytes_read == 0) {
                        // fprintf(stderr, "PARENT: EOF reached safely on fd %d (Operator finished) on %d loop \n", read_fd, count);
                        break;
                }
                if (bytes_read < 0) {
                        fprintf(stderr, "PARENT FATAL: read failed on fd %d: on loop :%d %s\n", read_fd, count, strerror(errno));
                        break;
                }

                // fprintf(stderr, "PARENT: Successfully read %d bytes!\n", (int)bytes_read);
                // Push the bytes to stdout so the grader can see them
                write(STDOUT_FILENO, buffer, bytes_read);
                count++;
        }
}






void sub_merger(merger_node_t* root, File file) {
        // to avoid memory access for each checking create limit var
        // fprintf(stderr, "In submerger function\n");
        int num_chains = root->num_chains;

        // list of child pids and file indexes to wait or write or read later
        std::vector<int> input_write_fds(num_chains);
        std::vector<int> output_read_fds(num_chains);
        std::vector<std::vector<pid_t>> chain_child_pids(num_chains);
        std::vector<pid_t> submerger_pids;

        // fprintf(stderr, "Merger has %d chains\n", num_chains);
        //  create a loop to iterate each operator chain
        for (int chain_num = 0; chain_num < num_chains; chain_num++) {
                // get the current operator chain and check whether it is su-merger or normal chain with operators
                operator_chain_t cur_chain = root->chains[chain_num];
                bool is_submerge = (cur_chain.merger_child == NULL) ? false : true;


                // create each operator process and connect the created pipes between operators and close the unsused ones

                if (is_submerge) {
                        // CASE 1: SUB-MERGER

                        // fprintf(stderr, "Chain is a sub-merger \n");
                        //  create 2 pipes one for merger to sub merger feed another for result to merger
                        int P_to_sub[2];
                        int P_from_sub[2];
                        PIPE(P_to_sub);
                        PIPE(P_from_sub);

                        // create new process
                        pid_t child_pid = fork();

                        if (child_pid == 0) {
                                // CASE 1.1: SUB-MERGER CHILD PROCESS

                                // make the P_to_sub pipe stdin
                                dup2(P_to_sub[0], 0);  // 0 is stdin and P_to_sub[0] is read end

                                // make the P_from_sub pipe stdout
                                dup2(P_from_sub[1], 1);  // 1 is stdout and P_from_sub[1] is write end

                                // close all the pipes
                                close(P_to_sub[0]);
                                close(P_to_sub[1]);
                                close(P_from_sub[0]);
                                close(P_from_sub[1]);

                                for (int i = 0; i < chain_num; i++) {
                                        close(input_write_fds[i]);
                                        close(output_read_fds[i]);
                                }
                                // since it can be considered as seperate tree, call the function again
                                File file;
                                file.read_all(std::cin);
                                // fprintf(stderr, "This is forked child from sub-merger \n");
                                sub_merger(cur_chain.merger_child, file);
                                // fprintf(stderr, "This is forked child from sub-merger exitting \n");
                                exit(0);
                        }

                        else {
                                // CASE 1.2: SUB-MERGER PARENT PROCESS

                                // fprintf(stderr, "This is parent of forked child(%d) from sub-merger \n", child_pid);
                                //  parent doesnt read from feed
                                close(P_to_sub[0]);
                                // parent doesnt write to out
                                close(P_from_sub[1]);

                                input_write_fds[chain_num] = P_to_sub[1];
                                output_read_fds[chain_num] = P_from_sub[0];
                                submerger_pids.push_back(child_pid);
                        }
                }

                else {
                        // CASE 2: OPERATOR CHAIN

                        // fprintf(stderr, "Chain is compesed of series of operators \n");
                        int num_ops = cur_chain.num_ops;
                        // fprintf(stderr, "Chain %d has %d operators\n", chain_num, num_ops);
                        //  number of pipes to create to provide communication among processes
                        int num_pipes = num_ops + 1;
                        // a vevctor to keep all the file descriptor indexes
                        std::vector<std::vector<int>> file_dscrs(num_pipes, std::vector<int>(2));

                        // file_dscrs[0] will be feeding pipe from merger to starting of the chain operation
                        // file_dscrs[-1] will be result pipe from last operator to merger
                        // to create all the pipes from the begining
                        // TODO: arrange this vector and array types
                        for (int j = 0; j < num_pipes; j++) {
                                PIPE(file_dscrs[j].data());
                        }

                        chain_child_pids[chain_num].resize(num_ops);

                        for (int op_num = 0; op_num < num_ops; op_num++) {
                                if ((chain_child_pids[chain_num][op_num] = fork()) == 0) {
                                        // CASE 2.1: CHAIN OPERATOR CHILD PROCESS

                                        // fprintf(stderr, "This is forked child from operator chain %d with pid: %d \n", chain_num, chain_child_pids[chain_num][op_num]);
                                        connect_and_close_pipes(op_num, file_dscrs, num_pipes);

                                        for (int i = 0; i < chain_num; i++) {
                                                close(input_write_fds[i]);
                                                close(output_read_fds[i]);
                                        }
                                        execute_operator(cur_chain.ops[op_num]);
                                }
                        }

                        // CASE 2.2: CHAIN OPERATOR PARENT PROCESS

                        // fprintf(stderr, "This is parent in loop: %d\n", chain_num);
                        // since exec will never reach here
                        // ave the exact feed and out pipes
                        // fprintf(stderr, "input pipe fd: %d  output pipe fd: %d \n", file_dscrs[0][1], file_dscrs[num_pipes - 1][0]);
                        input_write_fds[chain_num] = file_dscrs[0][1];
                        output_read_fds[chain_num] = file_dscrs[num_pipes - 1][0];


                        close(file_dscrs[0][0]);              // Close feed read
                        close(file_dscrs[num_pipes - 1][1]);  // Close out write
                        // close everything else
                        for (int j = 1; j < num_pipes - 1; j++) {
                                close(file_dscrs[j][0]);
                                close(file_dscrs[j][1]);
                        }
                }
        }

        // fprintf(stderr, "Outside of the pipe loop \n");


        // TO BE ABLE TO WRITE TO EVERY CHAIN CONCURRENTLY WE NEED TO FORK FOR EACH CHAIN


        // dont wait inside of the loop start all of the chains and then wait all chains in order


        // 3: FORK FOR FEEDING EVERY CHAIN INDEPENDENTLY


        std::vector<pid_t> feeder_pids;

        for (int chain_num = 0; chain_num < num_chains; chain_num++) {
                pid_t f_pid = fork();
                if (f_pid == 0) {
                        // CHILD FEEDER

                        for (int i = 0; i < num_chains; i++) {
                                close(output_read_fds[i]);  // Feeders never read
                                if (i != chain_num) {
                                        close(input_write_fds[i]);  // Close all feeds except mine
                                }
                        }
                        operator_chain_t cur_chain = root->chains[chain_num];
                        file.write_interval(cur_chain.start_line, cur_chain.end_line, input_write_fds[chain_num]);

                        // Send EOF to the pipe!
                        // TODO: check if the shutdown is necessary for sending EOF
                        shutdown(input_write_fds[chain_num], SHUT_WR);

                        close(input_write_fds[chain_num]);
                        exit(0);
                } else {
                        feeder_pids.push_back(f_pid);
                }
        }

        // 3.2: Parent MUST close its copies of the write-ends or EOF will never happen
        for (int fd : input_write_fds) {
                close(fd);
        }

        // sleep(2);

        //  3.3: Parent reads strictly in chain order
        for (int chain_num = 0; chain_num < num_chains; chain_num++) {
                read_pipe_until_EOF(output_read_fds[chain_num]);
                close(output_read_fds[chain_num]);
        }




        // 4: WAITPID CLEANUP

        // 4.1: Reap Feeders silently
        for (pid_t f_pid : feeder_pids) {
                waitpid(f_pid, nullptr, 0);
        }

        // 4.2: Reap Sub-mergers silently
        for (pid_t sub_pid : submerger_pids) {
                waitpid(sub_pid, nullptr, 0);
        }

        // 4.3: Reap Operators and print EXIT-STATUS strictly in chain order
        for (int chain = 0; chain < num_chains; chain++) {
                for (pid_t op_pid : chain_child_pids[chain]) {
                        int status;
                        waitpid(op_pid, &status, 0);
                        if (WIFEXITED(status)) {
                                printf("EXIT-STATUS %d %d\n", op_pid, WEXITSTATUS(status));
                        }
                }
        }
}





int main(int argc, char** argv) {
        merger_node_t* root = parse_merger_input(stdin);
        if (!root) {
                fprintf(stderr, "merger: parse error\n");
                return 1;
        }


        // open the csv file, since the initial merger will get its input from there

        // TODO: Look at the filename convention in black box testing
        std::string csv_path = std::string("tests/") + root->filename;
        bool check_inside_csv = false;

        // learn .c_str()
        //  Use .c_str() to give the open() function the raw C-style pointer it needs
        std::ifstream csv_stream(csv_path);
        if (!csv_stream.is_open()) {
                fprintf(stderr, "ERROR opening CSV file(%s) ERROR: %s \n", csv_path.c_str(), strerror(errno));
                exit(1);
        }

        // connect roots stdin to csv_file


        File file;
        file.read_all(csv_stream);

        if (check_inside_csv) {
                for (int i = 0; i < file.lines.size(); i++) {
                        fprintf(stderr, "Line %d is : %s", i + 1, file.lines[i].c_str());
                }
        }

        csv_stream.close();

        // since the original root is also a sub_merger with no parent we can directly pass it to sub_merge function without any change on stdin nor stdout
        // fprintf(stderr, "Calling sub_merger \n");
        sub_merger(root, file);
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
