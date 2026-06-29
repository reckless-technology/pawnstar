/// @file dump_magics.cpp Offline generator for the magic-bitboard multipliers baked into
/// src/generated_data.h (kBishopMagicNumbers / kRookMagicNumbers).
///
/// The engine itself does NOT search for magics at startup — it fills its attack tables from the constants
/// in generated_data.h. This tool runs the one-off randomised search that found those constants and prints
/// them as ready-to-paste C++ array initialisers. Regenerate (and paste the output back into
/// generated_data.h) only if the occupancy-mask or attack generators change; the search is deterministic
/// (fixed seed), so an unchanged engine reproduces byte-identical magics.
///
///   make tools && ./build/dump_magics            # prints both arrays + total table size to stderr
///
/// A "magic" for a square is a multiplier M such that, for every relevant-occupancy subset `occ` of that
/// square's mask, `(occ * M) >> (64 - popcount(mask))` indexes a collision-free slot (distinct occupancies
/// may share a slot only if they have identical attack sets — "constructive" collisions are fine).

#include "generated_data.h"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

using gendata_detail::BishopAttacks;       // ray-scan reference attack generators
using gendata_detail::BishopOccupancyMask; // relevant-occupancy masks
using gendata_detail::EnumerateMaskCombinations;
using gendata_detail::RookAttacks;
using gendata_detail::RookOccupancyMask;

namespace
{

using MaskFn = uint64_t (*)(uint8_t);
using AttFn  = uint64_t (*)(uint64_t, uint8_t);

/// @brief A sparse magic candidate (AND of three draws) — sparse multipliers scatter occupancy bits into
/// the high word far more often than dense ones, so the search converges in a handful of attempts.
uint64_t SparseMagicCandidate(std::mt19937_64 &rng)
{
    return rng() & rng() & rng();
}

/// @brief Find a collision-free magic for one square by randomised search.
uint64_t FindMagic(uint8_t sq, MaskFn mask_fn, AttFn attack_fn, std::mt19937_64 &rng)
{
    const uint64_t mask  = mask_fn(sq);
    const unsigned bits  = static_cast<unsigned>(std::popcount(mask));
    const unsigned shift = 64 - bits;

    const std::vector<uint64_t> occupancies = EnumerateMaskCombinations(mask);
    std::vector<uint64_t>       reference(occupancies.size());
    for (size_t i = 0; i < occupancies.size(); ++i)
    {
        reference[i] = attack_fn(occupancies[i], sq);
    }

    std::vector<uint64_t> table(size_t{1} << bits);
    std::vector<uint64_t> used(size_t{1} << bits, 0); // epoch stamp per slot (0 = never written)
    for (uint64_t epoch = 1;; ++epoch)
    {
        const uint64_t magic = SparseMagicCandidate(rng);
        if (std::popcount((mask * magic) & 0xFF00000000000000ull) < 6) // cheap spread reject
        {
            continue;
        }
        bool ok = true;
        for (size_t i = 0; i < occupancies.size(); ++i)
        {
            const size_t idx = static_cast<size_t>((occupancies[i] * magic) >> shift);
            if (used[idx] != epoch)
            {
                used[idx]  = epoch;
                table[idx] = reference[i];
            }
            else if (table[idx] != reference[i])
            {
                ok = false;
                break;
            }
        }
        if (ok)
        {
            return magic;
        }
    }
}

void Dump(const char *name, MaskFn mask_fn, AttFn attack_fn)
{
    std::mt19937_64 rng{0xD1CE5EED5EED5EEDull}; // fixed seed → deterministic, reproducible magics
    std::printf("inline constexpr std::array<uint64_t, 64> %s = {{\n", name);
    size_t slots = 0;
    for (uint8_t sq = 0; sq < 64; ++sq)
    {
        const uint64_t magic = FindMagic(sq, mask_fn, attack_fn, rng);
        slots += size_t{1} << std::popcount(mask_fn(sq));
        if (sq % 4 == 0)
        {
            std::printf("    ");
        }
        std::printf("0x%016llxull,%s", static_cast<unsigned long long>(magic), (sq % 4 == 3) ? "\n" : " ");
    }
    std::printf("}};\n");
    // `slots` is the total hash range (sum of 1<<popcount(mask)); the engine compresses each slot to a
    // 1-byte index into a small de-duplicated attack table, so this is the size of the indices_ arrays.
    std::fprintf(stderr, "%s: %zu hash slots (%.1f KB of 1-byte indices)\n", name, slots, slots / 1024.0);
}

} // namespace

int main()
{
    Dump("kBishopMagicNumbers", BishopOccupancyMask, BishopAttacks);
    Dump("kRookMagicNumbers", RookOccupancyMask, RookAttacks);
    return 0;
}
