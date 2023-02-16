#include <sstream>
#include <iostream>
#include <fstream>

#include <mpi.h>

#include "grep.h"

namespace grep {
    void get_lines(std::vector<std::string> &input_string,
                   const std::string &file_name){

        // world size is the number of precesses and rank is the rank of each process
        int world_size, rank;
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        // these vector of string is used to read all the file lines
        std::vector<std::string> file_lines;

        unsigned int chunkSize;

        char *input;
        char *local;

        // only the process with rank 0 read the file line by line
        if (rank == 0) {
            int total_lines_number = 0;
            std::ifstream f_stream(file_name);

            // read input file line by line
            for (std::string line; std::getline (f_stream, line);) {
                //increment total number of lines
                ++total_lines_number;
                file_lines.push_back(line);
            }

            // padding is added to the file in order to make number of lines equally divisible
            // by the number of processes
            while(file_lines.size() % world_size != 0) {
                file_lines.push_back("");
                ++total_lines_number;
            }

            // chunk size is equal to the file lines (padding included) divided by the number of processes
            chunkSize = file_lines.size() / world_size;

            // the vector of string is converted into an array of char in order to
            // make possible to send it using MPI Scatter, in this way bytes are contiguous in memory
            input = static_cast<char *>(malloc(total_lines_number * LINELENGTH * sizeof(char)));

            // each line is padded at the end using white spaces in order to have all lines of
            // equal length, which is LINE LENGTH
            for(int k=0; k<file_lines.size(); k++){
                for(int j=0; j<LINELENGTH; j++){
                    if(j < file_lines[k].length())
                        input[j + (k*LINELENGTH)] = file_lines[k][j];
                    else
                        input[j + (k*LINELENGTH)] = ' ';
                }
            }

        }

        // process 0 broadcast the chunk size to all processes
        MPI_Bcast(&chunkSize, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

        // each process allocate memory for an amount of char equal to line length multiplied
        // by the chunk size in order to be able to receive data
        local = static_cast<char *>(malloc(chunkSize * LINELENGTH * sizeof(char)));

        // process 0 send in scatter equal part of the char array to all processes
        MPI_Scatter(&input[0], chunkSize * LINELENGTH, MPI_CHAR, &local[0], chunkSize * LINELENGTH, MPI_CHAR, 0, MPI_COMM_WORLD);

        // each process reconstruct the string starting from the chunk received and save them in
        // input_string parameter which will be passed to search_string function
        for(int i=0; i<chunkSize; i++){
            std::string s = "";
            for(int j=i*LINELENGTH; j< (i+1)*LINELENGTH; j++){
                s = s + local[j];
            }
            input_string.push_back(s);
        }

        // memory free
        if(rank == 0){
            free(input);
        }
        free(local);
    }

    void search_string(const std::vector<std::string> & input_strings,
                       const std::string & search_string,
                       lines_found &lines, unsigned &local_lines_number){

        // world size is the number of precesses and rank is the rank of each process
        int world_size, rank;
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        // each process reconstruct the chunk size using input_string size
        int chunkSize = input_strings.size();

        // each process has a local lines variable in which save the string and number of local line
        grep::lines_found local_lines;
        double initTime = MPI_Wtime();
        for (unsigned i=0; i<input_strings.size(); i++) {
            if (input_strings[i].find(search_string) != std::string::npos) {
                // local lines number is the number of lines each process found
                ++local_lines_number;
                local_lines.push_back(std::make_pair(i, input_strings[i]));
            }
        }

        local_lines_number = local_lines.size();

        // each process convert the lines found from vector of string to array of char
        // so to be able to send them back to process 0 using gather
        char *output;
        output = static_cast<char *>(malloc(local_lines_number * LINELENGTH * sizeof(char)));
        for(int k=0; k<local_lines_number; k++){
            for(int j=0; j<LINELENGTH; j++){
                if(j < local_lines[k].second.length())
                    output[j + (k*LINELENGTH)] = local_lines[k].second[j];
                else
                    output[j + (k*LINELENGTH)] = ' ';
            }
        }

        // process with rank 0 receive from each process the amount of lines found locally
        std::vector<unsigned> lines_found_process;
        lines_found_process.resize(world_size);
        // gather is used since each process must send only one integer
        MPI_Gather(&local_lines_number, 1, MPI_UNSIGNED, &lines_found_process[0], 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

        // process 0 computes the sum of total lines found as sum of the lines found by each process
        int sum = 0;
        for (auto& n : lines_found_process)
            sum += (int) n;

        // process 0 allocates memory for a char array of dimension given by the line length
        // multiplied by the total amount of line found
        char *grep_output;
        if(rank == 0)
            grep_output = static_cast<char *>(malloc( sum * LINELENGTH * sizeof(char)));

        // computation of counts received and displacements for gatherV for the lines found
        int *counters;
        counters = static_cast<int *>(malloc(world_size * sizeof(int)));
        for(int i=0; i<world_size; i++){
            counters[i] = (int)lines_found_process[i] * LINELENGTH;
        }
        int *displacements;
        displacements = static_cast<int *>(malloc(world_size * sizeof(int)));
        displacements[0] = 0;
        for(int i=1; i<lines_found_process.size(); i++){
            displacements[i] = displacements[i - 1] + counters[i - 1];
        }

        // allocation of memory for the line numbers for process 0
        int *line_num;
        line_num = static_cast<int *>(malloc( sum * sizeof(int)));

        // allocation of memory for array for containing the local number of line
        int *local_line_num;
        local_line_num = static_cast<int *>(malloc( local_lines_number * sizeof(int)));
        for(int i=0; i<local_lines.size(); i++){
            local_line_num[i] = (chunkSize * rank) + (int) local_lines[i].first + 1;
        }

        // computation of counts received and displacements for gatherV for the number of the lines found
        int *countersNum;
        countersNum = static_cast<int *>(malloc(world_size * sizeof(int)));
        for(int i=0; i<world_size; i++){
            countersNum[i] = (int)lines_found_process[i];
        }
        int *displacementsNum;
        displacementsNum = static_cast<int *>(malloc(world_size * sizeof(int)));
        displacementsNum[0] = 0;
        for(int i=1; i<lines_found_process.size(); i++){
            displacementsNum[i] = displacementsNum[i - 1] + countersNum[i - 1];
        }

        // GatherV performed in order to send to process 0 how many lines each processes has found
        MPI_Gatherv(&local_line_num[0], local_lines_number, MPI_INT,
                    &line_num[0], countersNum, displacementsNum, MPI_INT, 0, MPI_COMM_WORLD);

        // GatherV performed in order to send to process 0 the lines found
        MPI_Gatherv(&output[0], LINELENGTH*local_lines_number, MPI_CHAR,
                    &grep_output[0], counters, displacements, MPI_CHAR, 0, MPI_COMM_WORLD);

        // process 0 reconstruct the string from the array of char received
        if(rank == 0) {
            for (int i = 0; i < sum; i++) {
                std::string s = "";
                for (int j = i * LINELENGTH; j < (i + 1) * LINELENGTH; j++) {
                    s = s + grep_output[j];
                }
                // lines are added in lines_found parameter with the corresponding line number
                lines.push_back(std::make_pair(line_num[i], s));
            }
        }

        // memory free
        if(rank == 0) {
            free(grep_output);
        }
        free(output);
        free(counters);
        free(displacements);
        free(countersNum);
        free(displacementsNum);
        free(line_num);
        free(local_line_num);
    }

    void print_result(const lines_found & lines, unsigned local_lines_number){

        // world size is the number of precesses and rank is the rank of each process
        int world_size, rank;
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        // process 0 print the result in program_result.txt file
        if(rank == 0){
            std::ofstream out("program_result.txt");
            for (auto& pair_found: lines){
                // Padding at the end of the line is removed
                std::string s = pair_found.second;
                size_t end = s.find_last_not_of(' ');
                (end == std::string::npos) ? "" : s.erase(end+1, end);
                // Result is printed
                out << pair_found.first << ":" << s;
                out << '\n';
            }
            out.close();
        }
    }
}