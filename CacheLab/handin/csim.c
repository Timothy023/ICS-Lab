#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

int hit, miss, evic;
int s, E, b, step, bin[64];
char *file;
bool flag;

long long **cache;
long long **tim;

void init_cache() {
    cache = malloc((1 << s) * sizeof(long long *));
    for (int i = 0; i < (1 << s); ++i) {
        cache[i] = malloc(E * sizeof(long long));
    }
    for (int i = 0; i < (1 << s); ++i) {
        for (int j = 0; j < E; ++j)
            cache[i][j] = 0;
    }

    tim = malloc((1 << s) * sizeof(long long *));
    for (int i = 0; i < (1 << s); ++i) {
        tim[i] = malloc(E * sizeof(long long));
    }
    for (int i = 0; i < (1 << s); ++i) {
        for (int j = 0; j < E; ++j)
            tim[i][j] = 0;
    }
}

void htob(char *addr, int l, int r, long long *tag, int *sid) {
    (*tag) = 0; (*sid) = 0;
    for (int i = 0; i < 64; ++i) bin[i] = 0;
    for (int i = r - 1, p = 0; i >= l; --i) {
        if (addr[i] == '1' || addr[i] == '3' || addr[i] == '5' || addr[i] == '7' ||
            addr[i] == '9' || addr[i] == 'b' || addr[i] == 'd' || addr[i] == 'f') bin[p] = 1;
        p += 1;

        if (addr[i] == '2' || addr[i] == '3' || addr[i] == '6' || addr[i] == '7' ||
            addr[i] == 'a' || addr[i] == 'b' || addr[i] == 'e' || addr[i] == 'f') bin[p] = 1;
        p += 1;

        if (addr[i] == '4' || addr[i] == '5' || addr[i] == '6' || addr[i] == '7' ||
            addr[i] == 'c' || addr[i] == 'd' || addr[i] == 'e' || addr[i] == 'f') bin[p] = 1;
        p += 1;

        if (addr[i] == '8' || addr[i] == '9' || addr[i] == 'a' || addr[i] == 'b' ||
            addr[i] == 'c' || addr[i] == 'd' || addr[i] == 'e' || addr[i] == 'f') bin[p] = 1;
        p += 1;
    }

    for (int i = b + s - 1; i >= b; --i)
        (*sid) =  (*sid) * 2 + bin[i];
    
    for (int i = 63; i >= b + s; --i)
        (*tag) = (*tag) * 2 + bin[i];

/*
    printf ("%s: ", addr);
    for (int i = 63; i >= 0; i -= 4) {
        printf ("%d%d%d%d ", bin[i], bin[i - 1], bin[i - 2], bin[i - 3]);
    }
    puts("");
    printf ("tag = %lld; sid = %d\n", *tag, *sid);
*/
}
/*
1 0001
2 0010
3 0011
4 0100
5 0101
6 0110
7 0111
8 1000
9 1001
A 1010
B 1011
C 1100
D 1101
E 1110
F 1111
*/

void load_memory(long long tag, int sid) {
    int pos = 0;
    bool isfree = false, isfind = false;
    for (int i = 0; i < E; ++i) {
        if (cache[sid][i] == tag && tim[sid][i] != 0) {
            isfind = true;
            tim[sid][i] = step;
            break;
        }
        if (tim[sid][i] == 0) isfree = true;
        if (tim[sid][i] < tim[sid][pos]) pos = i;

    }
    if (isfind == true) {
        hit += 1;
        if (flag) printf (" hit");
    }
    else {
        miss += 1;
        if (flag) printf (" miss");
        if (!isfree) {
            evic += 1;
            printf (" eviction");
        }
        cache[sid][pos] = tag;
        tim[sid][pos] = step;
    }
}

void store_memory(long long tag, int sid) {
    load_memory(tag, sid);
}

void solve() {
    FILE *F = freopen (file, "r", stdin);
    
    init_cache();
    char inst[5], addr[100];
    long long tag;
    int l, r, sid;
    while (scanf ("%s", inst) != EOF) {
        scanf ("%s", addr);
        step += 1;
        for (l = 0, r = 0; addr[r] != ','; ++r);
        htob(addr, l, r, &tag, &sid);
        
        if (inst[0] == 'I') continue;
        else if (inst[0] == 'L') {
            if (flag) printf ("%s %s", inst, addr);
            load_memory(tag, sid);
            if (flag) puts("");
        }
        else if (inst[0] == 'S') {
            if (flag) printf ("%s %s", inst, addr);
            store_memory(tag, sid);
            if (flag) puts("");
        }
        else if (inst[0] == 'M') {
            if (flag) printf ("%s %s", inst, addr);
            load_memory(tag, sid);
            store_memory(tag, sid);
            if (flag) puts("");
        }
    }

    fclose (F);
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i][1] == 'v') {
            flag = true;
        }
        else if (argv[i][1] == 's') {
            s = atoi(argv[i + 1]);
            i += 1;
        }
        else if (argv[i][1] == 'E') {
            E = atoi(argv[i + 1]);
            i += 1;
        }
        else if (argv[i][1] == 'b') {
            b = atoi(argv[i + 1]);
            i += 1;
        }
        else if (argv[i][1] == 't') {
            file = argv[i + 1];
            i += 1;
        }
    }
/*
    printf ("s = %d\n", s);
    printf ("E = %d\n", E);
    printf ("b = %d\n", b);
    printf ("t = %s\n", file);
*/
    solve();
    printSummary(hit, miss, evic);
    return 0;
}
