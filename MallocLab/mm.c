/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "mm.h"
#include "memlib.h"

#define max(a, b) (a) > (b) ? (a) : (b)

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (sizeof(int))

/* 
 * mm_init - initialize the malloc package.
 */

void *p[25];
void *maxn;
void *minn;

int mm_init(void) {
    void *ptr_init = mem_sbrk(4);
    for (int i = 0; i < 25; ++i) p[i] = NULL;
    maxn = 0;
    minn = 0;
    return 0;
}

inline void del(void *ptr, int nlen) {
    bool flag = true;
    for (int pos = 4; pos < 25 && flag; ++pos) {
        if ((1 << pos) >= nlen) {
            void **lst = &p[pos];
            for (void *pp = p[pos]; pp != NULL; pp = *(void **)pp) {
                if (pp == ptr) {
                    *lst = *(void **)pp;
                    flag = false;
                    break;
                }
                lst = pp;
            }
        }
    }
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t ssize) {
    int size = (int)ssize;
    int delta = 0;
    for (int i = 5; i < 10; ++i) {
        if ((1 << i) >= size) {
            if ((1 << i) - size <= (1 << (i - 3)))
                delta = (1 << i) - size;
            break;
        }
    }
    if (size < sizeof(void *)) size = sizeof(void *);
    int newsize = ALIGN(size + delta + SIZE_T_SIZE * 2);
    int pos = 5;
    bool flag = true;
    void *np = NULL;
    for (; pos < 25 && flag; ++pos)
        if ((1 << pos) >= newsize) {
            void **lst = &p[pos];
            int minn = 0;
            for (void *nxt = p[pos]; nxt != NULL; nxt = *(void **)nxt) {
                int len = *(int *)((char *)nxt - SIZE_T_SIZE);
                if (len >= newsize) {
                    if (minn == 0) minn = len;
                    else if (len < minn) minn = len;
                    flag = false;
                }
            }
            if (!flag) {
                for (void *nxt = p[pos]; nxt != NULL; nxt = *(void **)nxt) {
                    int len = *(int *)((char *)nxt - SIZE_T_SIZE);
                    if (len == minn) {
                        *lst = *(void **)nxt;
                        np = nxt;
                        flag = false;
                        break;
                    }
                    lst = nxt;
                }
            }
        }
    if (pos == 25) {
        int ts = newsize;
        void *p = mem_sbrk(ts);
        if (p == (void *)-1) return NULL;
        else {
            *(int *)p = ts;
            *(int *)((char *)p + ts - SIZE_T_SIZE) = ts;
            p = (char *)p + SIZE_T_SIZE;
            if (minn == 0) minn = p;
            if (p > maxn) maxn = p;
            np = p;
        }
    }

    int len = *(int *)((char *)np - SIZE_T_SIZE);
    if (len > newsize + 24) {
        *(int *)((char *)np - SIZE_T_SIZE) = newsize | 1;
        *(int *)((char *)np + newsize - 2 * SIZE_T_SIZE) = newsize | 1;
        
        void *nn = (void *)((char *)np + newsize);
        *(int *)((char *)nn - SIZE_T_SIZE) = len - newsize;
        *(int *)((char *)nn + (len - newsize) - 2 * SIZE_T_SIZE) = len - newsize;

        mm_free(nn);
    }
    else {
        len = *(int *)((char *)np - SIZE_T_SIZE);
        *(int *)((char *)np - SIZE_T_SIZE) |= 1;
        *(int *)((char *)np + len - 2 * SIZE_T_SIZE) = len | 1;
    }

    return np;
}

void *merge(void *ptr) {
    while (1) {
        int len = *(int *)((char *)ptr - SIZE_T_SIZE);
        void *nxt = (void *)((char *)ptr + len);
        if (nxt > maxn) break;
        int nlen = *(int *)((char *)nxt - SIZE_T_SIZE);
        if (nlen & 1) break;
        int sum_len = len + nlen;
        *(int *)((char *)ptr - SIZE_T_SIZE) = sum_len;
        *(int *)((char *)ptr + sum_len - 2 * SIZE_T_SIZE) = sum_len;
        del(nxt, nlen);
    }
    while (1) {
        if (ptr == minn) break;
        int len = *(int *)((char *)ptr - 2 * SIZE_T_SIZE);
        if (len & 1) break;
        void *nxt = (void *)((char *)ptr - len);
        int nlen = *(int *)((char *)nxt - SIZE_T_SIZE);
        len = *(int *)((char *)ptr - SIZE_T_SIZE);
        int sum_len = len + nlen;
        *(int *)((char *)nxt - SIZE_T_SIZE) = sum_len;
        *(int *)((char *)nxt + sum_len - 2 * SIZE_T_SIZE) = sum_len;
        del(nxt, nlen);
        ptr = nxt;
    }
    return ptr;
}

/*
 * mm_free - Freeing a block does nothing.
 */ 
void mm_free(void *ptr) {
    *(int *)((char *)ptr - SIZE_T_SIZE) &= (~1);
    int len = *(int *)((char *)ptr - SIZE_T_SIZE);
    *(int *)((char *)ptr + len - 2 * SIZE_T_SIZE) &= (~1);
    ptr = merge(ptr);
    len = *(int *)((char *)ptr - SIZE_T_SIZE);
    for (int pos = 4; pos < 25; ++pos) {
        if ((1 << pos) >= len) {
            *(void **)ptr = p[pos];
            p[pos] = ptr;
            break;
        }
    }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t ssize) {
    int size = (int)ssize;
    void *oldptr = ptr;
    void *newptr;
    int copySize;
    
    int newsize = ALIGN(size + SIZE_T_SIZE * 2);
    int len = *(int *)((char *)ptr - SIZE_T_SIZE) & (~1);
    int nlen = 0;
    if (ptr + len <= maxn) nlen = *(int *)((char *)ptr + len - SIZE_T_SIZE);

    if (len >= newsize) return ptr;

    if (ptr != minn) {
        int tl = *(int *)((char *)ptr - 2 * SIZE_T_SIZE);
        if ((tl & 1) == 0) {
            if (tl + len >= newsize) {
                void *pre = (void *)((char *)ptr - tl);
                del(pre, nlen);

                copySize = (*(int *)((char *)oldptr - SIZE_T_SIZE) & (~1)) - SIZE_T_SIZE * 2;
                if (size < copySize) copySize = size;

                void *nn = pre;
                len = tl + len;
                *(int *)((char *)pre - SIZE_T_SIZE) = len | 1;
                *(int *)((char *)pre + len - 2 * SIZE_T_SIZE) = len | 1;

                ptr = pre;
                if (len > newsize + 24) {
                    *(int *)((char *)ptr - SIZE_T_SIZE) = len - newsize;
                    *(int *)((char *)ptr + (len - newsize) - 2 * SIZE_T_SIZE) = len - newsize;
                    
                    nn = (void *)((char *)ptr + (len - newsize));
                    *(int *)((char *)nn - SIZE_T_SIZE) = newsize;
                    *(int *)((char *)nn + newsize - 2 * SIZE_T_SIZE) = newsize;

                    len = *(int *)((char *)nn - SIZE_T_SIZE);
                    *(int *)((char *)nn - SIZE_T_SIZE) |= 1;
                    *(int *)((char *)nn + len - 2 * SIZE_T_SIZE) = len | 1;

                    mm_free(ptr);
                }
                memcpy(nn, oldptr, copySize);
                return nn;
            }
        }
    }

    if ((nlen & 1) || (nlen + len < newsize)) {
        newptr = mm_malloc(size);
        if (newptr == NULL) return NULL;
        copySize = (*(int *)((char *)oldptr - SIZE_T_SIZE) & (~1)) - SIZE_T_SIZE * 2;
        if (size < copySize) copySize = size;
        memcpy(newptr, oldptr, copySize);
        mm_free(oldptr);
        return newptr;
    }
    else {
        void *nxt = (void *)((char *)ptr + len);
        len = len + nlen;
        *(int *)((char *)ptr - SIZE_T_SIZE) = len | 1;
        *(int *)((char *)ptr + len - 2 * SIZE_T_SIZE) = len | 1;
        del(nxt, nlen);

        if (len > newsize + 24) {
            *(int *)((char *)ptr - SIZE_T_SIZE) = newsize;
            *(int *)((char *)ptr + newsize - 2 * SIZE_T_SIZE) = newsize;
            
            void *nn = (void *)((char *)ptr + newsize);
            *(int *)((char *)nn - SIZE_T_SIZE) = len - newsize;
            *(int *)((char *)nn + (len - newsize) - 2 * SIZE_T_SIZE) = len - newsize;

            len = *(int *)((char *)ptr - SIZE_T_SIZE);
            *(int *)((char *)ptr - SIZE_T_SIZE) |= 1;
            *(int *)((char *)ptr + len - 2 * SIZE_T_SIZE) = len | 1;

            mm_free(nn);
        }
        return ptr;
    }
}