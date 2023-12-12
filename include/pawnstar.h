#pragma once
/*
Global header file - included by each source file
*/
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "options.h"
#include "bitboard.h"
#include "move.h"
#include "types.h"
#include "function_prototypes.h"
#include "position.h"
#include "generated_data.h"

extern Game the_game;

#if DEBUGX
#define DEBUG_STATEMENT(x)  x
#define INCREMENT(x)        DebugXIncrement(x)
#define INCREMENT_IF(b,x)   DebugXIncrementIf(b,x)
#else
#define DEBUG_STATEMENT(x)
#define INCREMENT(x)
#define INCREMENT_IF(b,x)
#endif

#if EVAL_DEBUGX
#define EVAL_INCREMENT(x)   DebugXIncrement(x)
#else
#define EVAL_INCREMENT(x)
#endif
