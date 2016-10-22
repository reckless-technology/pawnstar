#include "pawnstar.h"

static uint64 x;

/*
XORSHIFT* Very simple, platform-independent, pseudo-random number generator.
https://en.wikipedia.org/wiki/Xorshift#xorshift.2A
*/
int NextRandom(void)
{
	if (x == 0)
	{
		x = (uint64)time(NULL);
        x *= NextRandom();
	}
	x ^= x >> 12;
	x ^= x << 25;
	x ^= x >> 27;
	return (int)(x * 2685821657736338717ull);
}