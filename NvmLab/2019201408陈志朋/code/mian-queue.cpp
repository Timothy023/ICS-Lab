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
#define MAX_NODE 1000000

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
    char key[KEY_LEN + 1], val[VAL_LEN + 1];
};

std::map<std::string, std::string> state;
std::unordered_map <std::string, int> mp;
bool create = false;

static inline int file_exists(char const *file) { return access(file, F_OK); }

inline int INSERT(PMEMobjpool *pop, struct tree *p, const char *key, const char *val) {
    int pos = -1;
    TX_BEGIN(pop) {
        pos = (p->cnt + 1);
        TOID(struct node) x = TX_ALLOC(struct node, sizeof(struct node));
        pmemobj_memcpy_persist(pop, D_RW(x)->key, key, KEY_LEN); D_RW(x)->key[KEY_LEN] = '\0';
        pmemobj_memcpy_persist(pop, D_RW(x)->val, val, VAL_LEN); D_RW(x)->val[VAL_LEN] = '\0';
        p->nd[pos] = x;
    } TX_END
    p->cnt += 1;
    return pos;
}

inline void CHANGE(PMEMobjpool *pop, struct tree *p, int pos, const char *val) {
    TX_BEGIN(pop) {
        pmemobj_memcpy_persist(pop, D_RW(p->nd[pos])->val, val, VAL_LEN);
    } TX_END
}

int tree_constructor(PMEMobjpool *pop, void *ptr, void *arg) {
	struct tree *q = static_cast <struct tree *> (ptr);
	q->cnt = 0;
	return 0;
}

void mian(std::vector<std::string> args) {
    auto filename = args[0].c_str();
    PMEMobjpool *pop;
    if (file_exists(filename) != 0) {
        create = true;
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
        int tmp = POBJ_ALLOC(pop, &rootp->root, struct tree, 146000008, tree_constructor, &nentries);
    }
    if (!create) {
        struct tree *p = D_RW(rootp->root);
        int n = p->cnt;
        for (int i = 1; i <= n; ++i) {
            state[D_RO(p->nd[i])->key] = D_RO(p->nd[i])->val;
        }
    }

    while (1) {
        Query q = nextQuery();
        switch (q.type) {
            case Query::SET:
                if (mp.count(q.key)) 
                    CHANGE(pop, D_RW(rootp->root), mp[q.key], q.value.c_str());
                else {
                    int pos = INSERT(pop, D_RW(rootp->root), q.key.c_str(), q.value.c_str());
                    mp[q.key] = pos;
                }
                break;

            case Query::GET:
                if (state.count(q.key)) {
                    q.callback(state[q.key]);
                }
                else {
                    q.callback("-");
                }
                break;

            case Query::NEXT:
                if (auto it = state.upper_bound(q.key); it != state.end())
                    q.callback(it->first);
                else
                    q.callback("-");
                break;

            default:
                throw std::invalid_argument(std::to_string(q.type));
        }
    }

    pmemobj_close(pop);
}