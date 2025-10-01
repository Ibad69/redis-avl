#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL
#define init_cap 16

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

struct HNode {
    struct HNode *next;  
    uint64_t hcode;      
};

typedef struct HNode HNode;

typedef struct  {
    HNode **tab;
    size_t mask;
    size_t size;
    size_t capacity;
} HMap;

typedef struct {
    struct HNode node;
    char *value;
    char *key;
} Entry;

static HMap *hmap = NULL; 

void htab_init() {
    hmap = malloc(sizeof(HMap));
    if (hmap == NULL) {
        printf("there was an error allocating the global hmap \n");
    }

    HNode **tab = calloc(init_cap, sizeof(HNode*));
    if(tab == NULL) {
        printf("there was an error allocating tabs array \n");
    }
    hmap->tab = tab;
    hmap->size = 0;
    hmap->mask = init_cap - 1;
    hmap->capacity = init_cap;
}

void htab_insert(HNode *node) {
    printf("hmap mask : %d \n", hmap->mask);
    size_t pos = node->hcode & hmap->mask;
    HNode *next = hmap->tab[pos];
    node->next = next;
    hmap->tab[pos] = node;
    hmap->size++;
}

void print_list(){
    if (!hmap->tab) {
        printf("htab is null \n");
        // return NULL;
    }
    for (size_t i = 0; i < init_cap; i++) {
        HNode *curr = hmap->tab[i];
        if (!curr) continue;

        printf("Index %zu:\n", i);
        while (curr) {
            Entry *ent = container_of(curr, Entry, node);
            printf("  key = %s, value = %s, hcode = %llu\n",
            ent->key, ent->value,
            (unsigned long long) curr->hcode);
            curr = curr->next;
        }
    }
}

uint64_t hash_key(char * key)  {
    uint64_t hash = FNV_OFFSET;
    for(const char *p = key; *p; p++){
        hash ^= (uint64_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

void h_findvalue(char *key) {
    uint64_t hash = hash_key(key);
    size_t pos = hash & hmap->mask; 
    printf("the position as received : %d \n", pos);
    HNode *curr = hmap->tab[pos];
    while(curr) {
        Entry *ent = container_of(curr, Entry, node);
        if(ent->key = key) {
            printf("found value break out of the loop \n");
            printf("printing the value out : %s \n", ent->value);
            break;
        }
        curr = curr->next;
    }
}

// void h_lookupchar(HNode *key, bool (*eq)(HNode *, HNode *)){
//     if (!hmap->tab) {
//         // return NULL;
//     }
//     size_t pos = key->hcode & hmap->mask;
//     HNode **from = &htab->tab[pos];     // incoming pointer to the target
//     for (HNode *cur; (cur = *from) != NULL; from = &cur->next) {
//         if (cur->hcode == key->hcode && eq(cur, key)) {
//             // return from;    // Q: Why not return `cur`? This is for deletion.
//         }
//     }
//     // return NULL;
// }


void insert_values_in_ht(char *key, char *value) {
    Entry *ent = malloc(sizeof(Entry));
    if (!ent) {
        perror("malloc failed");
        exit(1);
    }
    ent->key = key;
    ent->value = value;
    ent->node.hcode = hash_key(ent->key);
    ent->node.next = NULL;   // important initialization
    htab_insert(&ent->node);
}

int main () {
    htab_init();
    insert_values_in_ht("Hello", "World");
    insert_values_in_ht("World111", "Hello");
    insert_values_in_ht("World111", "Hello");
    insert_values_in_ht("World111", "Hello");
    insert_values_in_ht("Hello", "World");
    insert_values_in_ht("testing", "69696969");
    h_findvalue("testing");
    print_list();
}


