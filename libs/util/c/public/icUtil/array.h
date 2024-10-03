#ifndef ZILKER_ARRAY_H
#define ZILKER_ARRAY_H

/**
 * Get the length of an array, in elements
 * @param a an array pointer
 * @example
 * @code
 * my_t arr[] = { memb, memb };
 * array_length(arr) // 2
 * @endcode
 */
#define ARRAY_LENGTH(a) sizeof((a)) / sizeof(*(a))

#endif // ZILKER_ARRAY_H
