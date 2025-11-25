
#include <stdio.h>
#include <stdlib.h>

#include "bitmap.h"

#define WORDSZ (8*(int)sizeof(bitmap_t))  // bits in each bitmap word

// bitmap will be represented over an array of bitmap_t defined in bitmap.h
// usually an array of bytes (char)

// in the following functions
// b points to a bitmap
// n should always be >= 0 and < bitmap size (number of bits in the bitmap)

/** sets bit n to 1
 */
void bitmap_set(bitmap_t *b, unsigned n) {
    int word = n / WORDSZ;
    int offset = n % WORDSZ;
    b[word] |= 1 << offset;
}

/** clears bit n (sets to 0)
 */
void bitmap_clear(bitmap_t *b, unsigned n) {
    int word = n / WORDSZ;
    int offset = n % WORDSZ;
    b[word] &= ~(1 << offset);
}

/** reads bit n from bitmap array b
 */
int  bitmap_get(bitmap_t *b, unsigned n) {
    int word = n / WORDSZ;
    int offset = n % WORDSZ;
    return (b[word] >> offset) & 1;
}

/** alloc an array with nbits
*/
bitmap_t * bitmap_alloc(int nbits) {
    printf("bits: %i; word size %u;  allocated %i words\n", nbits, WORDSZ, (nbits+WORDSZ-1)/WORDSZ);
    bitmap_t *b = calloc((nbits+WORDSZ-1)/WORDSZ, sizeof(bitmap_t));  // and sets it to zeros
    return b;
}

/** free bitmap array b
*/
void bitmap_free(bitmap_t *b) {
    free(b);
}


// for debuging:

void bitmap_print(bitmap_t *b, unsigned size) {
    int col=0;
    for (int i = 0; i < size; i++) {
        printf("%i", bitmap_get(b, i));
        col++;
        if (col==79) {
            col=0; putchar('\n');
        } else if (col%10 == 0) {
            putchar(' ');
        } 
    }
    putchar('\n');
}
