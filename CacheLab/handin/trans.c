/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include <string.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */


void transpose1(int M, int N, int A[N][M], int B[M][N]);
void transpose2(int M, int N, int A[N][M], int B[M][N]);
void transpose3(int M, int N, int A[N][M], int B[M][N]);
void trans(int M, int N, int A[N][M], int B[M][N]);

char transpose_submit_desc[] = "Transpose submission";

void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
    if (N == 32 && M == 32) transpose1(M, N, A, B);
    else if (N == 64 && M == 64) transpose2(M, N, A, B);
    else if (N == 67 && M == 61) transpose3(M, N, A, B);
}

const int maxn = 18;
void transpose3(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, p, q;

    for (i = 0; i < 67; i += maxn) {
        for (j = 0; j < 61; j += maxn) {
            for (p = i; p < 67 && p < i + maxn; ++p)
                for (q = j; q < 61 && q < j + maxn; ++q) {
                    B[q][p] = A[p][q];
                }
        }
    }
}

void transpose2(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, p;
    int a1, a2, a3, a4, a5, a6, a7, a0;

    for (i = 0; i < 64; i += 8) {
        for (j = 0; j < 64; j += 8) {
            for (p = 0; p < 4; ++p) {
                a0 = A[i + p][j + 0];
                a1 = A[i + p][j + 1];
                a2 = A[i + p][j + 2];
                a3 = A[i + p][j + 3];
                a4 = A[i + p][j + 4];
                a5 = A[i + p][j + 5];
                a6 = A[i + p][j + 6];
                a7 = A[i + p][j + 7];

                B[j + 0][i + p] = a0;
                B[j + 1][i + p] = a1;
                B[j + 2][i + p] = a2;
                B[j + 3][i + p] = a3;
                B[j + 0][i + p + 4] = a4;
                B[j + 1][i + p + 4] = a5;
                B[j + 2][i + p + 4] = a6;
                B[j + 3][i + p + 4] = a7;
            }

            for (p = 0; p < 4; ++p) {
                a0 = B[j + p][i + 4];
                a1 = B[j + p][i + 5];
                a2 = B[j + p][i + 6];
                a3 = B[j + p][i + 7];

                a4 = A[i + 4][j + p];
                a5 = A[i + 5][j + p];
                a6 = A[i + 6][j + p];
                a7 = A[i + 7][j + p];


                B[j + p][i + 4] = a4;
                B[j + p][i + 5] = a5;
                B[j + p][i + 6] = a6;
                B[j + p][i + 7] = a7;
                
                B[j + p + 4][i + 0] = a0;
                B[j + p + 4][i + 1] = a1;
                B[j + p + 4][i + 2] = a2;
                B[j + p + 4][i + 3] = a3;
            }

            for (p = 4; p < 8; ++p) {
                a0 = A[i + p][j + 4];
                a1 = A[i + p][j + 5];
                a2 = A[i + p][j + 6];
                a3 = A[i + p][j + 7];

                B[j + 4][i + p] = a0;
                B[j + 5][i + p] = a1;
                B[j + 6][i + p] = a2;
                B[j + 7][i + p] = a3;
            }
        }
    }

}

void transpose1(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, p, q;
    for (i = 0; i < 32; i += 8) {

        for (p = i; p < i + 8; ++p)
            for (q = i; q < i + 8; ++q)
                B[q][(p + 8) % N] = A[p][q];
        
        for (p = i; p < i + 8; ++p)
            for (q = i; q < i + 8; ++q)
                B[p][q] = B[p][(q + 8) % N];
    }

    for (i = 0; i < 32; i += 8) {
        for (j = 0; j < 32; j += 8) {
            if (i != j) {
                for (p = i; p < i + 8; ++p)
                    for (q = j; q < j + 8; ++q)
                        B[q][p] = A[p][q];
            }
            else continue;
        }
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

