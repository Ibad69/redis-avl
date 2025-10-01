#include <ctype.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <poll.h>
#include <sys/poll.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <stdint.h>
#include "server.h"

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL
#define init_cap 16

enum {
    TAG_NIL = 0,    // nil
    TAG_ERR = 1,    // error code + msg
    TAG_STR = 2,    // string
    TAG_INT = 3,    // int64
    TAG_DBL = 4,    // double
    TAG_ARR = 5,    // arrayP
    T_ZSET = 6, // type ZSET 
    T_INIT = 7, // FOR INIT
    T_STR = 8, // type STRING
};

#define container_of(ptr, type, member) \
((type *)((char *)(ptr) - offsetof(type, member)))

uint64_t hash_key(char * key)  {
    uint64_t hash = FNV_OFFSET;
    for(const char *p = key; *p; p++){
        hash ^= (uint64_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

struct HNode {
    struct HNode *next;  
    uint64_t hcode;      
};

typedef struct HNode HNode;

void out_nil(buffer_type *out);

typedef struct  {
    HNode **tab;
    size_t mask;
    size_t size;
} HTab;

typedef struct  {
    HTab newer;
    HTab older;
    size_t migrate_pos; 
} HMap;

typedef struct AVLNode {
    struct AVLNode *parent;
    struct AVLNode *right;
    struct AVLNode *left;
    uint32_t height;
    uint32_t cnt;
} AVLNode;

typedef struct {
    AVLNode *root;
    HMap hmap;
}ZSet;

typedef struct {
    HNode hmap;
    AVLNode tree;

    char name[0];
    size_t len;
    double score;
} ZNode;

typedef struct {
    struct HNode node;
    ZSet zset;
    uint32_t type;
    char *key;
    char *string;
} Entry;

static struct {
    HMap db; // top level hashtable
}g_data;

// ******************HASHMAP*****************//
// static HMap *hmap = NULL; 

void h_init(HTab *htab, size_t n) {
    // assert(n > 0 && ((n - 1) & n) == 0);
    htab->tab = (HNode **)calloc(n, sizeof(HNode *));
    htab->mask = n - 1;
    htab->size = 0;
}

// hashtable insertion
void h_insert(HTab *htab, HNode *node) {
    printf("trying to insert some value in this \n");
    size_t pos = node->hcode & htab->mask;
    HNode *next = htab->tab[pos];
    printf("the position we are trying to add : %d \n", pos);
    node->next = next;
    htab->tab[pos] = node;
    htab->size++;
}

// hashtable look up subroutine.
// Pay attention to the return value. It returns the address of
// the parent pointer that owns the target node,
// which can be used to delete the target node.
static HNode **h_lookup(HTab *htab, HNode *key, bool (*eq)(HNode *, HNode *)) {
    if (!htab->tab) {
        return NULL;
    }

    size_t pos = key->hcode & htab->mask;
    HNode **from = &htab->tab[pos];     // incoming pointer to the target
    for (HNode *cur; (cur = *from) != NULL; from = &cur->next) {
        if (cur->hcode == key->hcode && eq(cur, key)) {
            return from;                // may be a node, may be a slot
        }
    }
    return NULL;
}

// remove a node from the chain
static HNode *h_detach(HTab *htab, HNode **from) {
    HNode *node = *from;    // the target node
    *from = node->next;     // update the incoming pointer to the target
    htab->size--;
    return node;
}

const size_t k_rehashing_work = 128;    // constant work

static void hm_help_rehashing(HMap *hmap) {
    size_t nwork = 0;
    while (nwork < k_rehashing_work && hmap->older.size > 0) {
        // find a non-empty slot
        HNode **from = &hmap->older.tab[hmap->migrate_pos];
        if (!*from) {
            hmap->migrate_pos++;
            continue;   // empty slot
        }
        // move the first list item to the newer table
        h_insert(&hmap->newer, h_detach(&hmap->older, from));
        nwork++;
    }
    // discard the old table if done
    if (hmap->older.size == 0 && hmap->older.tab) {
        free(hmap->older.tab);
        hmap->older = (HTab){0};
    }
}

static void hm_trigger_rehashing(HMap *hmap) {
    // assert(hmap->older.tab == NULL);
    // (newer, older) <- (new_table, newer)
    hmap->older = hmap->newer;
    h_init(&hmap->newer, (hmap->newer.mask + 1) * 2);
    hmap->migrate_pos = 0;
}

const size_t k_max_load_factor = 8;

void hm_insert(HMap *hmap, HNode *node) {
    if (!hmap->newer.tab) {
        h_init(&hmap->newer, 4);    // initialize it if empty
    }
    h_insert(&hmap->newer, node);   // always insert to the newer table

    if (!hmap->older.tab) {         // check whether we need to rehash
        size_t shreshold = (hmap->newer.mask + 1) * k_max_load_factor;
        if (hmap->newer.size >= shreshold) {
            hm_trigger_rehashing(hmap);
        }
    }
    hm_help_rehashing(hmap);        // migrate some keys
}

HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
    hm_help_rehashing(hmap);
    if (HNode **from = h_lookup(&hmap->newer, key, eq)) {
        return h_detach(&hmap->newer, from);
    }
    if (HNode **from = h_lookup(&hmap->older, key, eq)) {
        return h_detach(&hmap->older, from);
    }
    return NULL;
}

void hm_clear(HMap *hmap) {
    free(hmap->newer.tab);
    free(hmap->older.tab);
    *hmap = (HMap){0};
}

size_t hm_size(HMap *hmap) {
    return hmap->newer.size + hmap->older.size;
}

static bool h_foreach(HTab *htab, bool (*f)(HNode *, void *), void *arg) {
    for (size_t i = 0; htab->mask != 0 && i <= htab->mask; i++) {
        for (HNode *node = htab->tab[i]; node != NULL; node = node->next) {
            if (!f(node, arg)) {
                return false;
            }
        }
    }
    return true;
}

void hm_foreach(HMap *hmap, bool (*f)(HNode *, void *), void *arg) {
    h_foreach(&hmap->newer, f, arg) && h_foreach(&hmap->older, f, arg);
}

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
    hm_help_rehashing(hmap);
    HNode **from = h_lookup(&hmap->newer, key, eq);
    if (!from) {
        printf("searching from the older hashmap \n");
        from = h_lookup(&hmap->older, key, eq);
    }
    return from ? *from : NULL;
}

typedef struct  {
    struct HNode node;  // hashtable node
    char *key;
}LookupKey;

static bool entry_eq(HNode *node, HNode *key) {
    Entry *ent = container_of(node, Entry, node);
    LookupKey *keydata = container_of(key, LookupKey, node);
    printf("keydata -> checking what the key is : %s \n", keydata->key);
    if(ent == NULL) {
        printf("ent is null over here \n");
    }
    if(ent && keydata) {
        if(strcmp(ent->key, keydata->key) == 0) {
            return true;
        }
    }
    return false;
}

// void htab_init() {
//     hmap = malloc(sizeof(HMap));
//     if (hmap == NULL) {
//         printf("there was an error allocating the global hmap \n");
//     }

//     HNode **tab = calloc(init_cap, sizeof(HNode*));
//     if(tab == NULL) {
//         printf("there was an error allocating tabs array \n");
//     }
//     hmap->tab = tab;
//     hmap->size = 0;
//     hmap->mask = init_cap - 1;
//     hmap->capacity = init_cap;
// }


void zset_inset(ZSet *zset) {

}

// void htab_insert(HNode *node) {
//     printf("hmap mask : %d \n", hmap->mask);
//     size_t pos = node->hcode & hmap->mask;
//     HNode *next = hmap->tab[pos];
//     node->next = next;
//     hmap->tab[pos] = node;
//     hmap->size++;
// }

// void hmap_insert(HMap *hmap, HNode *node) {
//     size_t pos = node->hcode & hmap->mask;
//     HNode *next = hmap->tab[pos];
//     node->next = next;
//     hmap->tab[pos] = node;
//     hmap->size++;
// }

// char* h_findvalue(char *key) {
//     uint64_t hash = hash_key(key);
//     size_t pos = hash & hmap->mask; 
//     HNode *curr = hmap->tab[pos];
//     while(curr) {
//         Entry *ent = container_of(curr, Entry, node);
//         if(ent->key = key) {
//             printf("found value break out of the loop \n");
//             printf("printing the value out : %s \n", ent->value);
//             return ent->value;
//             break;
//         }
//         curr = curr->next;
//     }
// }

// Entry *h_lookup(char *key) {
//     uint64_t hash = hash_key(key);
//     size_t pos = hash & hmap->mask; 
//     HNode *curr = hmap->tab[pos];
//     while(curr) {
//         Entry *ent = container_of(curr, Entry, node);
//         return ent;
//     }
//     curr = curr->next;
// }

void insert_values_in_ht(char *key, char *value) {
    Entry *ent = malloc(sizeof(Entry));
    if (!ent) {
        perror("malloc failed");
        exit(1);
    }
    ent->key = key;
    ent->string = value;
    ent->node.hcode = hash_key(ent->key);
    ent->node.next = NULL;   // important initialization
    hm_insert(&g_data.db, &ent->node);
    // htab_insert(&ent->node);
}

// ******************ZSET**********************

// void zset_lookup(ZSet *zset, char *name) {
//     uint64_t hash = hash_key(key);
//     size_t pos = hash & hmap->mask; 
//     HNode *curr = hmap->tab[pos];
//     while(curr) {
//         Entry *ent = container_of(curr, Entry, node);
//         return ent;
//     }
//     curr = curr->next;
// }

// inner level hashmap 
typedef struct {
    HNode node;
    const char *name;
    size_t len;
} HKey;

static bool hcmp(HNode *node, HNode *key) {
    ZNode *znode = container_of(node, ZNode, hmap);
    HKey *hkey = container_of(key, HKey, node);
    printf("znode container key : %s \n", znode->name);
    printf("znode container key : %d \n", znode->len);
    printf("khey : %s \n", hkey->name);
    printf("kheey len: %d \n", hkey->len);

    if (znode->len != hkey->len) {
        printf("returning false because len don't match");
        return false;
    }
    return 0 == memcmp(znode->name, hkey->name, znode->len);
}

ZNode *zset_lookup(ZSet *zset, const char *name, size_t len) {
    // if (!zset->root) {
    //     printf("no root returning \n");
    //     return NULL;
    // }

    HKey key;
    key.node.hcode = hash_key(name);
    key.name = name;
    key.len = len;
    HNode *found = hm_lookup(&zset->hmap, &key.node, &hcmp);
    return found ? container_of(found, ZNode, hmap) : NULL;
}

typedef struct {
    char **items;   // array of strings
    size_t count;
} StringVec;

Conns* init_client_arr() {
    Conn *clients = calloc(100, sizeof(Conn));
    if (clients == NULL ) {
        printf("there was an error allocating buffer to the heap \n");
    }

    Conns *Conns_ = malloc(sizeof(Conns));
    if(Conns_ == NULL) {
        printf("there was an error allocating buffer to the heap \n");
    }
    
    Conns_->clients = clients;
    Conns_->size = 0;
    Conns_->capacity = 100;
    return Conns_;
}

// Compare lhs node against a (score, name) tuple
bool zless_tuple(
    AVLNode *lhs, double score, const char *name, size_t len)
{
    ZNode *zl = container_of(lhs, ZNode, tree);
    if (zl->score != score) {
        return zl->score < score;
    }
    int rv = memcmp(zl->name, name, zl->len < len ? zl->len : len);
    if (rv != 0) {
        return rv < 0;
    }
    return zl->len < len;
}

// Compare lhs node against rhs node
bool zless_node(AVLNode *lhs, AVLNode *rhs)
{
    ZNode *zr = container_of(rhs, ZNode, tree);
    return zless_tuple(lhs, zr->score, zr->name, zr->len);
}

void zset_insert(ZSet *zset, const char *name, double score, size_t len) {
    printf("trying to add in the second level hashmap \n");
    ZNode *node = calloc(7,sizeof(ZNode));

    node->len = len;
    node->score = score;
    memcpy(&node->name[0], name, len);

    node->hmap.hcode = hash_key(name);
    // find if this exists already

    hm_insert(&zset->hmap, &node->hmap);
    // tree_insert(&zset->root, &node->tree, zless_node);
}

void z_score(StringVec *cmd) {
    // upper level hashtable finding
    printf("finding the zscore \n");
    LookupKey key;
    key.key = malloc(strlen(cmd->items[1])+1);
    if(key.key) {
        strcpy(key.key, cmd->items[1]);
        key.node.hcode = hash_key(key.key);
    }
    // hashtable lookup
    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (!node) {
        printf("couldn't find nodee ------ \n");
        // return out_nil(out);
    }

    Entry *ent = container_of(node, Entry, node);

    ZNode *znode = zset_lookup(&ent->zset, cmd->items[2], strlen(cmd->items[2]));
    printf("znode score : %d \n", znode->score);
}

void insert_zadd(StringVec *cmd) {
    // check if that value already exists for us 
    Entry *ent = malloc(sizeof(Entry));
    if (!ent) {
        perror("malloc failed");
        exit(1);
    }

    ent->key = strdup(cmd->items[1]);
    // ent->value = value;
    ent->node.hcode = hash_key(ent->key);
    ent->node.next = NULL;   // important initialization
    memset(&ent->zset, 0, sizeof(ZSet));

    hm_insert(&g_data.db, &ent->node);

    size_t len = strlen(cmd->items[3]);
    double score = strlen(cmd->items[2]);
    char *name = cmd->items[3];
    printf("inserting the name : %s \n", name);

    zset_insert(&ent->zset, name, score, len);
}

// ************SERIALIZATION************************
void buff_append(buffer_type *buf, const uint8_t *data, size_t len) {
    if (buf->size + len > buf->capacity) {
        size_t new_capacity = buf->capacity ? buf->capacity * 2 : 64;
        while (new_capacity < buf->size + len)
        new_capacity *= 2;

        uint8_t *new_data = realloc(buf->buff, new_capacity);
        // if (!new_data) return -1; // out of memory
        
        buf->buff = new_data;
        buf->capacity = new_capacity;
    }

    // printf("buf size : %d \n", buf->size);
    // printf("data length : %d \n", len);

    memcpy(buf->buff + buf->size, data, len);
    buf->size += len;
    // return 0; 
}

void buff_append8(buffer_type *buf, uint8_t *data){
    buff_append(buf, data, 1);
}

void buff_append32(buffer_type *buf, uint32_t *data){
    buff_append(buf, (const uint8_t *)data, 4);
}

void buff_appendi64(buffer_type *buf, uint8_t *data){
    buff_append(buf, (const uint8_t *)data, 8);
}

void buff_append_double(buffer_type *buf, uint8_t *data){
    buff_append(buf, (const uint8_t *)data, 8);
}

void start_response(buffer_type *buf, size_t *header) {
    // *header = buf->size;
    buff_append32(buf, (uint32_t *)header);
}

void out_str(buffer_type *out, char *s, size_t size) {
    uint8_t val = TAG_STR;
    buff_append8(out, &val);
    buff_append32(out, (uint32_t *)&size);
    buff_append(out, (const uint8_t*)s, size);
}

void out_nil(buffer_type *out) {
    uint8_t val = TAG_NIL;
    buff_append8(out, &val);
}

void end_response(buffer_type *out, size_t *header) {
    size_t buf_size = (out->size - *header) - 4;

    // check for large / huge message
    uint32_t len = (uint32_t)(buf_size);
    //below we are copying 4 bytes from len to out to the start of the buf
    memcpy(&out->buff[*header], &len, 4);
}


bool less(AVLNode *node, AVLNode *new_node) {
    // ZNode *node_v = container_of(node, ZNode, tree);
    // ZNode *new_node_v = container_of(new_node, ZNode, tree);
    // if (node_v->val > new_node_v->val){
        // return true;
    // }
    // else {
        // return false;
    // }
}

void do_get(buffer_type *out, StringVec *strvec) {
    LookupKey key;
    key.key = malloc(strlen(strvec->items[1])+1);
    if(key.key) {
        strcpy(key.key, strvec->items[1]);
        key.node.hcode = hash_key(key.key);
    }
    // hashtable lookup
    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (!node) {
        printf("couldn't find nodee ------ \n");
        return out_nil(out);
    }
    // copy the value
    Entry *ent = container_of(node, Entry, node);
    if (ent->type != T_STR) {
        // return out_err(out, ERR_BAD_TYP, "not a string value");
    }
    return out_str(out, ent->string, strlen(ent->string));
}

void do_set(buffer_type *out, StringVec *strvec) {
    insert_values_in_ht(strvec->items[1], strvec->items[2]);
    out_nil(out);
}

void do_zadd(buffer_type *out, StringVec *cmd) {
   insert_zadd(cmd);
   out_nil(out);
}

void do_zscore(buffer_type *out, StringVec *cmd) {
    z_score(cmd);
}


void do_request(StringVec *strvec, buffer_type *buf) {
    if(strcmp(strvec->items[0], "get") == 0) {
        // printf("get command for items : %s \n", strvec->items[1]);
        do_get(buf, strvec);
    }
    if(strcmp(strvec->items[0], "set") == 0) {
        // printf("this is a set command for you \n");
        do_set(buf, strvec);
    }
    if(strcmp(strvec->items[0], "del") == 0) {
        // delete some value from the hashmap
        // printf("this is a del command for you \n");
    }
    if(strcmp(strvec->items[0], "zadd") == 0) {
        do_zadd(buf, strvec);
        // printf("this is a ZSet ADD Command \n");
        // printf("this is a ZSet ADD Command : %s \n", strvec->items[0]);
        // printf("this is a ZSet ADD Command : %s \n", strvec->items[1]);
        // printf("this is a ZSet ADD Command : %s \n", strvec->items[2]);
        // printf("this is a ZSet ADD Command : %s \n", strvec->items[3]);
    }
    if(strcmp(strvec->items[0], "zscore") == 0) {
        do_zscore(buf, strvec);
    }
}

buffer_type* buf_init(size_t buffsz) {

    uint8_t *buff = calloc(buffsz, sizeof(uint8_t));
    if (buff == NULL ) {
        printf("there was an error allocating buffer to the heap \n");
    }

    buffer_type *buf = malloc(sizeof(buffer_type));
    if(buf == NULL) {
        printf("there was an error allocating buffer to the heap \n");
    }
    
    buf->buff = buff;
    buf->size = 0;
    buf->capacity = 0;

    return buf;
}

void buf_free(buffer_type *buf) {
    free(buf->buff);
    buf->buff = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

void buff_consume(buffer_type *buf, size_t len) {
    buf->buff += len;
    buf->size-= len;
}

// -------------------------------TREE-------------------------

void tree_insert(AVLNode **root, AVLNode *new_node, bool (*zless)(AVLNode *, AVLNode *)) {
    AVLNode *parent = NULL;
    AVLNode **from = root;

    for (AVLNode *node = *from; node;) {
        from = zless(node, new_node) ? &node->left : &node->right;
        parent = node;
        node = *from;
    }

    *from = new_node;
    new_node->parent = parent;

    // avl fix balance the tree after insertion
}

// ----------------------------SERVER-----------------------
int get_listener() {
    struct addrinfo hints, *ai, *p;

    int listener;
    int yes=1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;

    if ((getaddrinfo(NULL, "6969", &hints, &ai)) != 0) {
        perror("error occured in initializing struct");
    }

    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype,
        p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // Lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes,
        sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            // close(listener);
            continue;
        }

        break;
    }

    // If we got here, it means we didn't get bound
    if (p == NULL) {
        return -1;
    }

    freeaddrinfo(ai); // All done with this

    // Listen
    if (listen(listener, 50) == -1) {
        return -1;
    }

    return listener;

}

int accept_connection(int fd,
struct pollfd **pfds,
int *fd_count,
int *fd_size) {
    socklen_t socklen;
    struct sockaddr_storage their_addr;
    int a = accept(fd, (struct sockaddr*)&their_addr, &socklen);
    if (a == -1) {
        perror("accept");
        return -1;
    }
    add_to_pfds(a, pfds, fd_count, fd_size);
    return a;
}

void recv_all(int fd,
void *buf,
struct pollfd **pfds,
int *fd_count,
int i,
size_t *bufsz) {
}

bool read_u32(const uint8_t **req, uint8_t *end, uint32_t *out) {
    if(*req + 4 > end) {
        printf("request has corrupted \n");
        return false;
    }

    memcpy(out, *req, 4);
    *req+=4;
    return true;
}

static bool read_u32_v1(const uint8_t *cur, const uint8_t *end, uint32_t *out) {
    if (cur + 4 > end) return false;
    *out = (uint32_t)( (cur)[0] |
    ((cur)[1] << 8) |
    ((cur)[2] << 16) |
    ((cur)[3] << 24) );
    cur += 4;
    return true;
}

bool read_str(const uint8_t **req, uint8_t *end, size_t len, StringVec *out) {
    if (*req + len > end)  {
        printf("corrupted string i guess\n");
        return false;
    }

    // allocate one more slot for the new string
    char **tmp = realloc(out->items, sizeof(char*) * (out->count + 1));
    if (!tmp) {
        return false;
    }
    out->items = tmp;

    // allocate memory for the actual string
    char *s = malloc(len + 1);
    if (!s) return false;
    memcpy(s, *req, len);
    s[len] = '\0';

    out->items[out->count] = s;
    out->count++;

    *req += len;

    return true;
}

int32_t parse_req(const uint8_t **request, size_t len, StringVec *out) {

    // read the nstr first 4 bytes
    uint8_t *end = *request + len;
    uint32_t nstr = 0;

    if(!read_u32(request, end, &nstr)) {
        printf("there was an error reading u32 nstr \n");
        return -1;
    }

    // printf("nstr is : %d \n", nstr);

    while(out->count < nstr){
        uint32_t len = 0;
        if(!read_u32(request, end, &len)){
            printf("there was an error reading u32 len \n");
            return -1;
        };
        if(!read_str(request, end, len, out)){
            return -1;
        };
    }

    return 0;
}

int try_one_request(Conn *client) {

    if(client->incoming_buff->size < 4) {
        // we need to check for more reads;
        // printf("returning because incoming buff size is less than 4 \n");
        return 0;
    }
    uint32_t len = 0;
    // printf("data beefore mcmpy : %s \n", client->incoming_buff);
    memcpy(&len, client->incoming_buff->buff, 4);
    // len = ntohl(len);   // <--- this is critical if receiving data in endian big-endian

    // if(len > k_max_msg) {
        //     conn->want_close = 1;
        //     return false;
        // }

        if(4 + len > client->incoming_buff->size) {
            printf("4 + len incoming buff size \n");
            return 0;
        }

        const uint8_t *request = &client->incoming_buff->buff[4];
        // printf("the request that is received with data : %s \n", request);

        StringVec *out = malloc(sizeof(StringVec));
        out->count = 0;
        out->items = malloc(2);

        if(parse_req(&request, len, out) == -1){
            // printf("there was an error parsing the request \n");
            return false;
        };

        // for (size_t i = 0; i < strlen(out->items[1]); i++) {
        //     printf("%02X ", (unsigned char) out->items[1][i]);
        // }
        // printf("\n");

        size_t header_pos = 0;
        start_response(client->outgoing_buff, &header_pos);
        do_request(out, client->outgoing_buff); 
        end_response(client->outgoing_buff, &header_pos);

        printf("conn outgoing size : %d \n", client->outgoing_buff->size);
        // // // add the data to send back to the client in the outgoing buffer
        // buff_append(client->outgoing_buff, (const uint8_t *)&len, 4);
        // buff_append(client->outgoing_buff, request, len);

        // // consume the buffer and decrease the size also 
        buff_consume(client->incoming_buff, 4+len);

        return true;
    }


void strip_newline(char *s) {
    char *p = s;
    while (*p) {
        if (*p == '\r' || *p == '\n') {
            *p = '\0';
            break;
        }
        p++;
    }
}




    void handle_write(Conn *client) {
        ssize_t rv = write(client->fd, &client->outgoing_buff->buff[0], client->outgoing_buff->size);
        // if (rv < 0 && errno == EAGAIN) {
            //     return; // actually not ready
            // }
            if (rv < 0) {
                printf("inside the error condition \n");
                // msg_errno("write() error");
                client->want_close = true;    // error handling
                return;
            }

            buff_consume(client->outgoing_buff, (size_t)rv);

            // update the readiness intention
            if (client->outgoing_buff->size == 0) {   // all data written
            client->want_read = true;
            client->want_write = false;
        } // else: want write
    }

    void handle_client_data
    (
    int fd,
    int *fd_count,
    int i,
    struct pollfd **pfds,
    Conns *clients
    )
    {
        Conn client = clients->clients[(*pfds)[i].fd];
        if (client.want_read == 1) {
            uint8_t tmpbuf[64*1024];
            size_t rv = recv(fd, tmpbuf, 1245, 0);
            if (rv == -1) {
                perror("msgrecv");
                close(fd);
                del_from_pfd(pfds, fd_count, i);
            }
            if (rv == 0) {
                // printf("client is disconnecting \n");
                close(fd);
                del_from_pfd(pfds, fd_count, i);
            }

            // printf("buff incoming : %s \n", client.incoming_buff);

            buff_append(client.incoming_buff, tmpbuf, (rv));
            // for (int i = 0; i < 16; i++) {
            //     printf("%02x ", client.incoming_buff->buff[i]);
            // }
            // printf("\n");

            // printf("the data reeceived by client : %s \n", client.incoming_buff->buff);
            while(try_one_request(&client) == 1){}

            if(client.outgoing_buff->size > 4 ) {
                client.want_write = 1;
                client.want_read = 0;
                handle_write(&client);
            }
            // then maybe do outgoing scenario over here make the socket write
        }
        if (client.want_write == 1) {
            handle_write(&client);
        }
    }


    // void buff_append_outgoing(Conn *conn, size_t size) {
        //     conn->outgoing_buff
        // }

        void process_connection(struct pollfd **pfds,
        int *fd_count,
        int *fd_size,
        int listener,
        Conns *clients) {
            for (int i = 0; i < *fd_count; i ++){
                if ((*pfds)[i].revents == POLLIN) {
                    if ((*pfds)[i].fd == listener) {
                        int fd = accept_connection(listener, pfds, fd_count, fd_size);
                        Conn *conn = malloc(sizeof(Conn));
                        buffer_type *incoming_buff = buf_init(1024);
                        buffer_type *outgoing_buff = buf_init(1024);
                        conn->fd = fd;
                        conn->incoming_buff = incoming_buff;
                        conn->outgoing_buff = outgoing_buff;
                        conn->want_read = 1;
                        // check for capacity over here if it exceed you may want to realloc
                        clients->clients[fd] = *conn;
                        clients->size++;
                    }
                    else {
                        buffer_type *incbuf = clients->clients[(*pfds)[i].fd].incoming_buff;
                        buffer_type *outbuf = clients->clients[(*pfds)[i].fd].outgoing_buff;
                        handle_client_data((*pfds)[i].fd, fd_count, i, pfds, clients);
                    }
                }
            }
        }

        void add_to_pfds (int fd, struct pollfd **pfds, int *fd_count, int *fd_size) {
            if (*fd_count == *fd_size) {
                // printf("reallocating doubling the size \n");
                *fd_size *= 2;
                *pfds = realloc(*pfds, sizeof(**pfds)*(*fd_size)); 
                if (pfds == NULL) {
                    printf("could not reallocate memory \n");
                }
            }

            (*pfds)[*fd_count].fd = fd;
            (*pfds)[*fd_count].revents = 0;
            (*pfds)[*fd_count].events = POLLIN;
            (*fd_count)++;
        }

        void del_from_pfd(struct pollfd **pfds, int *fd_count, int i) {
            // printf("deleting the connection from pfds \n");
            (*pfds)[i] = (*pfds)[*fd_count-1];
            (*fd_count)--;
        }
        #include <stdio.h>

        int main () {
            int fd_count = 0;
            int fd_size = 5;    

            struct pollfd *pfds = malloc(sizeof *pfds * fd_size);
            if (pfds == NULL) {
                printf("an error occured in allocating memory for pfds array \n");
            }

            // init the clients array
            Conns* clients = init_client_arr();

            int s = get_listener();

            pfds[0].fd = s;
            pfds[0].events = POLLIN;

            fd_count++;

            // htab_init();

            while (1) {
                printf("waiting for some events to ocme ~~~~ fds ==> : %d \n", fd_count);
                int p = poll(pfds, fd_count,  -1);
                if (p == -1) {
                    perror("error in poll \n");
                }
                process_connection(&pfds, &fd_count, &fd_size, s, clients);
            }

            free(pfds);

            return 0;
        }
        // void sendall(void *buf, ssize_t total_len, int fd) {
        //     Int Total = 0;
        //     int bytesleft = *total_len;
        //     int n;
        //     while (total < *total_len) {
            //         int s = send(fd, (char*)buf+total, bytesleft, 0);
            //         if (s == -1) {
                //             perror("sending-error");
                //             break;
                //         }
                //         total += n;
                //         bytesleft -= n;
                //         printf("sent the response : %s \n", (char*)buf);
                //     }
                //     *total_len = total;
                // }
