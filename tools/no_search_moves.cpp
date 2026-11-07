/// @file no_search_moves.cpp Report the moves a match's PGN shows played with no search, and which were forced.
///
/// The logic, and why it is here, is `no_search_moves.h`, tested by `test_no_search_moves`.
///
/// Usage: no_search_moves <match.pgn>...

#include "no_search_moves.h"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    return no_search_moves::Run(std::vector<std::string>(argv + 1, argv + argc), std::cout, std::cerr);
}
