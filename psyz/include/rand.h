#ifndef RAND_H
#define RAND_H

#define RAND_MAX 0x7FFF /**< Maximum value for rand() (32767) */

typedef unsigned long u_long;

/**
 * @brief Generate a random number
 *
 * Generates a pseudo-random number from 0 to RAND_MAX (0x7FFF=32767).
 *
 * @return The generated pseudo-random number
 */
int rand(void);

/**
 * @brief Initialize the random number generator
 *
 * Sets a new starting point for random number generation. The default is 1.
 *
 * @param seed Random number seed
 */
void srand(u_long seed);

#endif
