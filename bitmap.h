
#ifndef _BITMAP_H
#define _BITMAP_H

typedef char  bitmap_t;
// bitmap will be represented over an array of chars (char is used as a byte)

// in the following functions
// b pointes to a bitmap
// n should always be >= 0 and < bitmap size (number of bits in the bitmap)

void bitmap_set(bitmap_t *b, unsigned n);
void bitmap_clear(bitmap_t *b, unsigned n);
int  bitmap_get(bitmap_t *b, unsigned n);
void bitmap_print(bitmap_t *b, unsigned size);

bitmap_t *bitmap_alloc(int nbits);
void bitmap_free(bitmap_t *b);

#endif