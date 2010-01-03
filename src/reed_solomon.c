#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <assert.h>
#include <string.h>

#include "galois_field.h"
#include "reed_solomon.h"

rs_matrix * rs_create(int n, int m) {
    rs_matrix *mat = calloc( 1, sizeof(rs_matrix) );
    mat->m = calloc( (n+m)*n, sizeof(uint8_t) );
    mat->data = n;
    mat->cs = m;

    assert(n+m < 255);
    assert(n > 1);
    assert(m > 0);

    // vadermonde matrix of [i^j] over the galois field
    for (uint8_t i = 0; i < n+m; i++) {
        uint8_t acc = i;

        for (uint8_t j = 0; j < n; j++) {
            int idx = i*n + j;

            if ( i == 0 && j == 0 )
                mat->m[idx] = 1;
            else if ( i == 0 )
                mat->m[idx] = 0;
            else {
                mat->m[idx] = acc;
                acc = gf_mult(acc, i);
            }
        }
    }
    // solve for the identity submatrix in the top n x n
    for (int i = 0; i < n; i++) {
        if ( mat->m[i*(n+1)] == 0 ) {
            // we need to swap a column to get a nonzero here

            int f_j = -1;
            for (int j = i+1; j < n; j++) {
                if ( mat->m[i*n+j] != 0 ) {
                    f_j = j;
                    break;
                }
            }
            assert(f_j != -1);

            // swap columns
            int x;
            for (int k = 0; k < n+m; k++) {
                x = mat->m[k*n+i];
                mat->m[k*n+i] = mat->m[k*n+f_j];
                mat->m[k*n+f_j] = x;
            }

            assert(mat->m[i*(n+1)] != 0);
        }

        if ( mat->m[i*(n+1)] != 1 ) {
            // divide out the constant to get a 1 here

            uint8_t inv = gf_div(1, mat->m[i*(n+1)]);

            for (int k = 0; k < n+m; k++)
                mat->m[k*n+i] = gf_mult(inv, mat->m[k*n+i]);

            assert(mat->m[i*(n+1)] == 1);
        }

        // get zeroes in the other columns of this row
        for (int j = 0; j < n; j++) {
            if ( j == i             ) continue;
            if ( mat->m[i*n+j] == 0 ) continue;

            uint8_t val = mat->m[i*n+j];

            for (int k = 0; k < n+m; k++)
                mat->m[k*n+j] = gf_add(mat->m[k*n+j], gf_mult(val, mat->m[k*n+i]));

            assert(mat->m[i*n+j] == 0);
        }
    }

    return mat;
}

void rs_free(rs_matrix *mat) {
    free(mat->m);
    free(mat);
}

static void rs_apply(uint8_t *m, int offset, int inct, uint8_t *in, int outct, uint8_t *out) {
    memset(out, 0, outct);

    for (int i = 0; i < inct; i++)
        for (int j = 0; j < outct; j++)
            out[j] = gf_add(out[j], gf_mult(m[offset+j*inct+i], in[i]));
}

// *in is a n-array, *out is an m-array
void rs_apply_checksum(rs_matrix *mat, uint8_t *in, uint8_t *out) {
    rs_apply(mat->m, mat->data*mat->data, mat->data, in, mat->cs, out);
}

void rs_apply_reversal(rs_matrix *mat, uint8_t *in, uint8_t *out) {
    rs_apply(mat->m, 0, mat->data, in, mat->data, out);
}

rs_matrix * rs_create_reversal(rs_matrix *old, uint8_t *exists) {
    int n = old->data;
    int m = old->cs;

    rs_matrix *new = calloc( 1, sizeof(rs_matrix) );
    uint8_t *rhs = new->m = calloc( n*n, sizeof(uint8_t) );
    new->data = n;
    new->cs = m;

    // initialize rhs to the identity
    for (int i = 0; i < n; i++)
        rhs[i*(n+1)] = 1;

    // lhs holds the right side of the matrix to be solved into the identity
    uint8_t *lhs = calloc( n*n, sizeof(uint8_t) );

    // add the live rows of *old into lhs
    int cur = 0;
    for (int i = 0; i < n+m; i++)
        if ( exists[i] ) {
            assert(cur < n);
            for (int j = 0; j < n; j++)
                lhs[cur*n+j] = old->m[i*n+j];
            cur++;
        }

    assert(cur == n);
    
    // solve for identity in the lhs while keeping track of the changes in rhs
    // [A|I] ~ [I|Ainv] by elementary row operations
    for (int row = 0; row < n; row++) {
        if ( lhs[row*(n+1)] == 0 ) {
            // swap with another row whose value is nonzero in this column
            
            int found = -1;
            for (int i = row+1; i < n; i++) {
                if ( lhs[i*n+row] != 0 ) {
                    found = i;
                    break;
                }
            }
            assert(found != -1);

            // swap row #row and row #found
            int x;
            for (int i = 0; i < n; i++) {
                x = lhs[row*n+i];
                lhs[row*n+i] = lhs[found*n+i];
                lhs[found*n+i] = x;

                x = rhs[row*n+i];
                rhs[row*n+i] = rhs[found*n+i];
                rhs[found*n+i] = x;
            }
        }
        assert(lhs[row*(n+1)] != 0);

        if ( lhs[row*(n+1)] != 1 ) {
            // divide out the constant factor

            uint8_t inv = gf_div(1, lhs[row*(n+1)]);

            for (int i = 0; i < n; i++) {
                lhs[row*n+i] = gf_mult(inv, lhs[row*n+i]);
                rhs[row*n+i] = gf_mult(inv, rhs[row*n+i]);
            }
        }
        assert(lhs[row*(n+1)] == 1);

        // get zeroes in the other rows in this column
        for (int other = 0; other < n; other++) {
            if ( other == row ) continue;
            if ( lhs[other*n+row] == 0 ) continue;

            uint8_t val = lhs[other*n+row];
            for (int i = 0; i < n; i++) {
                lhs[other*n+i] = gf_add(lhs[other*n+i], gf_mult(val, lhs[row*n+i]));
                rhs[other*n+i] = gf_add(rhs[other*n+i], gf_mult(val, rhs[row*n+i]));
            }

            assert(lhs[other*n+row] == 0);
        }
    }

    // clean up
    free(lhs);

    return new;
}

