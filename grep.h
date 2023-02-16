#ifndef GREP_H
#define GREP_H

#define LINELENGTH 100

#include <vector>
#include <string>
#include <utility>

namespace grep
{
    typedef std::pair<unsigned, std::string>  number_and_line;

    typedef std::vector<number_and_line> lines_found;

    /* Only process with rank 0 read from the file,
     * other processes must get their lines from rank 0
     */
    void get_lines(std::vector<std::string> &input_string,
                   const std::string &file_name);

    /* the first input to this function is a vector containing the portion of file
     * that must be searched by each process
     */
    void search_string(const std::vector<std::string> & input_strings,
                       const std::string & search_string,
                       lines_found &lines, unsigned &local_lines_number);

    /* Prints to file is performed by rank 0 only */
    void print_result(const lines_found & lines, unsigned local_lines_number);
}

#endif // GREP_H