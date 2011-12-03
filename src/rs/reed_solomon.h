#ifndef __REED_SOLOMON_H__
#define __REED_SOLOMON_H__

#include <inttypes.h>

typedef struct rs_matrix {
    uint8_t data; // data count (also, column count)
    uint8_t cs; // "checksum" count
    uint8_t *m; 
} rs_matrix;

rs_matrix * rs_create(int n, int m); // creates a data x (data+cs) matrix - including the n x n identity submatrix at 0,0
rs_matrix * rs_create_reversal(rs_matrix *mat, uint8_t *exists); // creates a data x data matrix
void rs_free(rs_matrix *mat);

void rs_apply_checksum(rs_matrix *mat, uint8_t *in, uint8_t *out);
void rs_apply_reversal(rs_matrix *mat, uint8_t *in, uint8_t *out);

#endif

