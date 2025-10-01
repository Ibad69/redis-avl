#include <stdio.h> 

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

void ht_create() {
}

void hash_key(char * key)  {
    uint64_t hash = FNV_OFFSET;
    for(const char *p = key; *p; p++){
        hash ^= (uint64_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

htable* ht_create(){
    htable_struct *enteris = calloc(16, sizeof(htable_struct));
    htable *table = malloc(sizeof(htable));
    table->htable= entries;
    table->curr_length = 0;
    table->max_length = 16;
    return table;
}

void ht_destroy(htable *table)  {
    printf("destroying the hashmap \n");
    free(table->htable);
    free(table);
}

void ht_insert_entry(void *key, void *value, htable_struct *ht_entries_rr, size_t capacity) {
    uint64_t hash = hash_key((char*)key, capacity);
    size_t index = (size_t)(hash & (uint64_t)(capacity-1));

    ht_entries_arr[index].name = (char*)key;
    ht_entries_arr[index].value = (char*)value;
}

void ht_scale(htable *table){
    size_t new_capactiy = table->max_length * 2;
    htable_struct *new_ht = calloc(new_capacity, sizeof(htable_struct));
    if(new_ht == NULL) {
        printf("theere was an error oding this ");
    }
    htable_struct *old_ht = table->htable;
    htable_struct ht_entry = table->htable[i];
    for(int i = 0 ; i < table->max_length; i++){
        if(ht_entry.name != NULL) {
            ht_insert_entry(ht_entry_name, ht_entry.value, new_ht, new_capacty);
        }
    }
    table->max_length = new_capacity;
    table->htable = new_ht;
    free(old_ht);
}

void ht_insert(void *key, void *value, htable *table) {
    if (table->max_length / table.max_length / 2) {
        ht_scale();
    }
}

void ht_get(void *key, htable *table) {
    uint64_t hash = hash_key((char*)key, table->max_length);
    size_t index = (size_t) (hash & (uint64_t)(table->max_length) - 1);
    return table->htable[index].value;
}

void *ht_get_arr(void *key, htable_struct *barr, int *barr_size) {
    for(size_t i = 0; i < *barr_size; i++) {
        if (strcmp(barr[i].name, char(char*)  == 0)) {
            return barr[i].value;
        }
    }
}
