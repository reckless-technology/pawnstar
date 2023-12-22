#pragma once
#include <cstdint>
#include <string_view>

#include "move.h"

struct Position;

bool    InitializeOpeningBookFromFile(std::string_view filename);
void    DisplayAvailableBookMoves(const Position& position);
Move    GetBookMove(uint64_t hash);
void    FreeOpeningBook();