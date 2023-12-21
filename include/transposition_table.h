#pragma once
#include <cstdint>

#include "move.h"

/**
 * @brief Alpha beta search tree node types.
*/
enum NodeType
{
    NODE_CUT,   /**< beta cutoff occurred       */
    NODE_ALL,   /**< no move exceeded alpha     */
    NODE_PV,    /**< principal variation node   */
};

/**
 * @brief A transposition table entry.
 * A transposition is the result of a previous search containing pertinent
 * information about what was found when this position was searched earlier.
*/
struct Transposition
{
    uint64_t    hash;           /**< Zobrist hash of this position                                  */
    Move        move;           /**< Best move from this position, if any                           */
    int         score;          /**< The score computed from this position                          */
    int16_t     depth;          /**< The depth to which this position was searched                  */
    uint8_t     node_type;      /**< The alpha-beta tree search node type (cut, all, pv)            */
    bool        is_previous;    /**< This entry is from a previous search and can thus be replaced  */
};

bool    FindTransposition(uint64_t hash, Transposition& transposition);
void    FreeTranspositionTable();
void    InitializeTranspositionTable(size_t megabytes);
void    RecordTransposition(uint64_t hash, int depth, int score, Move move, int node_type);
void    ShowTableUsage();
void    AgeTranspositionTable();