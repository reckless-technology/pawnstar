#pragma once
#include <cstdint>

#include "move.h"

struct Position;

bool    InitializeDefaultOpeningBook();
bool    InitializeOpeningBookFromFile(const char* filename);
void    DisplayAvailableBookMoves(const Position& position);
Move    GetBookMove(uint64_t hash);
void    FreeOpeningBook(void);