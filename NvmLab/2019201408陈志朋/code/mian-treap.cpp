#include "mian.h"

#include <fstream>
#include <iostream>
#include <libpmemobj.h>
#include <map>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>
#include <ctime>

#define KEY_LEN 16
#define VAL_LEN 128
#define MAX_NODE 100000

#define tag 4
#define tago 15
#define node_size 16

POBJ_LAYOUT_BEGIN(treap);
POBJ_LAYOUT_ROOT(treap, struct my_root);
POBJ_LAYOUT_TOID(treap, struct node);
POBJ_LAYOUT_TOID(treap, struct tree);
POBJ_LAYOUT_END(treap);

struct my_root {
    TOID(struct tree) root;
};

struct tree {
    int root, cnt;
    TOID(struct node) nd[];
};

struct node {
    char key[KEY_LEN * node_size], val[VAL_LEN * node_size];
    int ls[node_size], rs[node_size], id[node_size];
};

std::unordered_map<std::string, int> mp;
bool create = true;
int INDEX, st = 233, sum = 0;
struct tree *p;

static inline int file_exists(char const *file) { return access(file, F_OK); }

int cmp(const char *x, const char *y) {
    for (int i = 0; i < 16; ++i)
        if (x[i] > y[i]) return 1;
        else if (x[i] < y[i]) return -1;
    return 0;
}

inline int NewNode(PMEMobjpool *pop, const char *key, const char *val) {
    int pos = -1, offset;
    TX_BEGIN(pop) {
        pos = (p->cnt += 1);
        TOID(struct node) x;
        offset = (pos - 1) & tago;
        pos = (pos - 1) >> tag;
        if (offset == 0) {
            x = TX_ALLOC(struct node, sizeof(struct node));
            p->nd[pos] = x;
        }
        D_RW(p->nd[pos])->id[offset] = st = (st * 482711);
        pmemobj_memcpy_persist(pop, D_RW(p->nd[pos])->key + offset * KEY_LEN, key, KEY_LEN);
        pmemobj_memcpy_persist(pop, D_RW(p->nd[pos])->val + offset * VAL_LEN, val, VAL_LEN);
    } TX_END
    return p->cnt;
}

void split(int ro, int &r1, int &r2, const char *key) {
    if (!ro) {
        r1 = r2 = 0;
        return;
    }
    int pos = (ro - 1 >> tag);
    int offset = (ro - 1 & tago);
    if (cmp(D_RO(p->nd[pos])->key + offset * KEY_LEN, key) == -1) {
        r1 = ro;
        split(D_RO(p->nd[pos])->rs[offset], (D_RW(p->nd[pos])->rs[offset]), r2, key);
    }
    else {
        r2 = ro;
        split(D_RO(p->nd[pos])->ls[offset], r1, (D_RW(p->nd[pos])->ls[offset]), key);
    }
}

void merge(int &ro, int x, int y) {
    if (!x || !y) {
        ro = x | y;
        return;
    }
    int px = (x - 1 >> tag), py = (y - 1 >> tag);
    int ox = (x - 1 & tago), oy = (y - 1 & tago);
    if (D_RO(p->nd[px])->id[ox] < D_RO(p->nd[py])->id[oy]) {
        ro = x;
        int pos = (ro - 1 >> tag);
        int offset = (ro - 1 & tago);
        merge(D_RW(p->nd[pos])->rs[offset], D_RO(p->nd[pos])->rs[offset], y);
    }
    else {
        ro = y;
        int pos = (ro - 1 >> tag);
        int offset = (ro - 1 & tago);
        merge(D_RW(p->nd[pos])->ls[offset], x, D_RO(p->nd[pos])->ls[offset]);
    }
}

int FIND(int ro, const char *key) {
    if (ro == 0) return -1;
    int pos = (ro - 1 >> tag);
    int offset = (ro - 1 & tago);
    int flag = cmp(D_RO(p->nd[pos])->key + offset * KEY_LEN, key);
    if (flag == 0) return ro;
    if (flag == -1) {
        return FIND(D_RO(p->nd[pos])->rs[offset], key);
    }
    else {
        return FIND(D_RO(p->nd[pos])->ls[offset], key);
    }
}

void FIND_NEXT(int ro, const char *key) {
    if (ro == 0) return;
    int pos = (ro - 1 >> tag);
    int offset = (ro - 1 & tago);
    int flag = cmp(D_RO(p->nd[pos])->key + offset * KEY_LEN, key);
    if (flag <= 0) {
        FIND_NEXT(D_RO(p->nd[pos])->rs[offset], key);
    }
    else {
        INDEX = ro;
        FIND_NEXT(D_RO(p->nd[pos])->ls[offset], key);
    }
}

void print(int ro) {
    if (ro == 0) return;
    int pos = (ro - 1 >> tag);
    int offset = (ro - 1 & tago);
    print(D_RO(p->nd[pos])->ls[offset]);
    std::cout <<ro <<", " <<pos <<", " <<offset <<", " <<D_RO(p->nd[pos])->ls[offset] <<", " <<D_RO(p->nd[pos])->rs[offset] <<": ";
    std::cout <<std::string(D_RO(p->nd[pos])->key + offset * KEY_LEN, KEY_LEN) <<"  ";
    std::cout <<std::string(D_RO(p->nd[pos])->val + offset * VAL_LEN, VAL_LEN) <<std::endl;
    print(D_RO(p->nd[pos])->rs[offset]);
}

inline void INSERT(PMEMobjpool *pop, int r3, const char *key, const char *val) {
    int r1 = 0, r2 = 0;
    split(p->root, r1, r2, key);
    merge(r1, r1, r3);
    merge(p->root, r1, r2);
}


int tree_constructor(PMEMobjpool *pop, void *ptr, void *arg) {
	struct tree *q = static_cast <struct tree *> (ptr);
	q->cnt = 0;
	pmemobj_persist(pop, q, 8);
	return 0;
}

void mian(std::vector<std::string> args) {
    auto filename = args[0].c_str();
    PMEMobjpool *pop;
    if (file_exists(filename) != 0) {
        pop = pmemobj_create(filename, POBJ_LAYOUT_NAME(treap), 500000000, 0666);
    }
    else {
        create = false;
        pop = pmemobj_open(filename, POBJ_LAYOUT_NAME(treap));
    }

    TOID(struct my_root) root = POBJ_ROOT(pop, struct my_root);
	struct my_root *rootp = D_RW(root);
    if (create) {
        int nentries = MAX_NODE;
        int tmp = POBJ_ALLOC(pop, &rootp->root, struct tree, sizeof(struct tree) + sizeof(struct node) * nentries, tree_constructor, &nentries);
    }
    p = D_RW(rootp->root);
    int pos;
    while (1) {
        sum += 1;
        Query q = nextQuery();
        switch (q.type) {
            case Query::SET:
                pos = mp[q.key];
                if (pos == 0) {
                    pos = NewNode(pop, q.key.c_str(), q.value.c_str());
                    INSERT(pop, pos, q.key.c_str(), q.value.c_str());
                    mp[q.key] = pos;
                }
                else {
                    int po = pos - 1 >> tag;
                    int of = pos - 1 & tago;
                    pmemobj_memcpy_persist(pop, D_RW(p->nd[po])->val + of * VAL_LEN, q.value.c_str(), VAL_LEN);
                }
                break;
            case Query::GET:
                INDEX = FIND(p->root, q.key.c_str());
                if (INDEX == -1)
                    q.callback("-");
                else {
                    int po = INDEX - 1 >> tag;
                    int of = INDEX - 1 & tago;
                    q.callback(std::string(D_RW(p->nd[po])->val + of * VAL_LEN, VAL_LEN));
                }
                break;
            case Query::NEXT:
                INDEX = -1;
                FIND_NEXT(p->root, q.key.c_str());
                if (INDEX == -1)
                    q.callback("-");
                else {
                    int po = INDEX - 1 >> tag;
                    int of = INDEX - 1 & tago;
                    q.callback(std::string(D_RW(p->nd[po])->key + of * KEY_LEN, KEY_LEN));
                }
                break;
        }
    }
    pmemobj_close(pop);
}