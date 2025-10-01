#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

typedef struct AVLNode {
    struct AVLNode *parent;
    struct AVLNode *right;
    struct AVLNode *left;
    uint32_t height;
    uint32_t cnt;
} AVLNode;

typedef struct AVLTree {
    AVLNode **nodes;
    size_t capacity;
    size_t count;
}AVLTree;

typedef struct AVLEntry {
    AVLNode AVLNode;
    int val;
} AVLEntry;

typedef struct Node {
    struct Node *left;
    struct Node *right;
}Node;

typedef struct Entry {
    struct Node node;
    bool root;
    int val;
}Entry;

// AVLEntry *root = NULL;
AVLTree *tree = NULL;
AVLNode *root = NULL;

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// trying something else also
void init_tree_node(int val) {
    AVLNode *i_node = calloc(1, sizeof(AVLNode));
    i_node->parent = NULL;
    i_node->left = NULL;
    i_node->right = NULL;

    AVLEntry *ent = calloc(3, sizeof(Entry));
    if(ent == NULL) {
        printf("there was an error init entry \n");
    }

    ent->val = val;
    ent->AVLNode = *i_node;
    root = &ent->AVLNode;
}

uint32_t avl_height(AVLNode *node) {
    return node ? node->height : 0;
}

uint32_t avl_cnt(AVLNode *node) {
    return node ? node->cnt : 0;
}

uint8_t avl_get_height_diff(AVLNode *node) {
    uintptr_t p = (uintptr_t)node->parent;
    return p & 0b11;                    // data from LSB
}

uint32_t max(uint32_t lhs, uint32_t rhs) {
    return lhs < rhs ? rhs : lhs;
}

// maintain the height and cnt field
void avl_update(AVLNode *node) {
    node->height = 1 + max(avl_height(node->left), avl_height(node->right));
    node->cnt = 1 + avl_cnt(node->left) + avl_cnt(node->right);
}

void rot_left(AVLNode *node) {
    AVLNode *parent = node->parent;
    AVLNode *new_node = node->right;
    AVLNode *inner = new_node->left;

    node->right = inner;

    if(inner) {
        inner->parent = node;
    }

    new_node->parent = parent;
    new_node->left = node;
    node->parent = new_node;

    avl_update(node);
    avl_update(new_node);

    new_node;
}

void rot_right(AVLNode *node) {
    AVLNode *parent = node->parent;
    AVLNode *new_node = node->left;
    AVLNode *inner = new_node->right;

    node->left = inner;

    if(inner) {
        inner->parent = node;
    }

    new_node->parent = parent;
    new_node->right = node;
    node->parent = new_node;

    avl_update(node);
    avl_update(new_node);

    new_node;
}

void avl_fix(AVLNode *node) {

}

bool less(AVLNode *node, AVLNode *new_node) {
    AVLEntry *node_v = container_of(node, AVLEntry, AVLNode);
    AVLEntry *new_node_v = container_of(new_node, AVLEntry, AVLNode);
    if (node_v->val > new_node_v->val){
        return true;
    }
    else {
        return false;
    }
}

void insert_val_loop(AVLNode **root, AVLNode *new_node, bool (*less)(AVLNode *, AVLNode *)) {
    AVLNode *parent = NULL;
    AVLNode **from = root;

    for (AVLNode *node = *from; node;) {
        from = less(node, new_node) ? &node->left : &node->right;
        parent = node;
        node = *from;
    }

    *from = new_node;
    new_node->parent = parent;

    // avl fix balance the tree after insertion
}

void insert_entry(int val) {
    AVLEntry *nent = calloc(3, sizeof(Entry));
    nent->val = val;
    // insert_val_tree(nent, root);
}

void insert_entry_root_node(int val) {
    AVLEntry *ent = calloc(3, sizeof(Entry));
    if(ent == NULL) {
        printf("there was an error init entry \n");
    }

    ent->val = val;
    ent->AVLNode.left = ent->AVLNode.right = ent->AVLNode.parent = NULL;
    insert_val_loop(&root, &ent->AVLNode, less);
}

void detach_node_easy(AVLNode *node) {
    AVLEntry *ent = container_of(node, AVLEntry, AVLNode);
    printf("node that is received : %d \n", ent->val) ;

    AVLNode *child = node->left ? node->left : node->right;
    AVLNode *parent = node->parent;

    if(child) {
        child->parent = parent;
    }

    // we are deleting the root node in this case will 
    if(!parent){
        // return true;
    }

    AVLNode **from = parent->left == node ? &parent->left : &parent->right;
    *from = child;
}

void detach_node(AVLNode *node, AVLNode **root) {
    if(!node->left || !node->right) {
        detach_node_easy(node);
        return;
    }

    AVLNode *victim = node->right;
    while(victim->left) {
        victim = victim->left;
    }

    detach_node_easy(victim);

    *victim = *node;
    if(victim->left){
        victim->left->parent = victim;
    }
    if(victim->right){
        victim->right->parent = victim;
    }

    AVLNode **from = root;
    AVLNode *parent = node->parent;

    if(parent) {
        from = parent->left == node ? &parent->left : &parent->right;
    }
    *from = victim;
}

void delete_node(int val, AVLNode **root) {
    AVLNode *parent = NULL;
    AVLNode **from= root;

    AVLEntry *root_val = container_of(*from, AVLEntry, AVLNode);
    printf("the root starts with the value : %d \n", root_val->val);

    AVLNode *node = NULL;
    AVLNode *start_node = NULL; 

    if(root_val->val > val ) {
        start_node = (*from)->left;
    }
    else {
        start_node = (*from)->right;
    }

    while(start_node){
        AVLEntry *ent = container_of(start_node, AVLEntry, AVLNode);
        printf("reached somehwere here with delete : %d \n", ent->val);
        if(ent->val == val) {
            node = start_node;
            break;
        }
        if(ent->val > val) {
            if (!start_node->left){
                node = start_node;
                break;
            }
            start_node = start_node->left; 
        }
        else if(ent->val < val){
            if (!start_node->right){
                node = start_node;
                break;
            }
            start_node = start_node->right; 
        }
    }
    printf("detaching the node \n");
    detach_node(node, root);
    // for(AVLNode *node = *from; node;) {
        //     from = less(node, *from->left) ? &node->left : &node->right;
        //     parent = node;
        //     node = *from;

        //     AVLEntry *entry = container_of(from, AVLEntry, AVLNode);
        //     printf("the entry : %d \n", entry->val);
        //     if(entry->val == val) {
        //         // delete this node
        //         detach_node(node);
        //         return;
        //     }
        // }
}

static void print_tree_rec(AVLNode *node, int level) {
    if (!node) return;

    // go right first (so it's printed above)
    print_tree_rec(node->right, level + 1);

    // indent by level
    for (int i = 0; i < level; i++) {
        printf("    ");
    }

    // recover parent entry
    AVLEntry *entry = container_of(node, AVLEntry, AVLNode);
    printf("> %d\n", entry->val);

    // go left
    print_tree_rec(node->left, level + 1);
}

void print_tree(AVLNode *root) {
    print_tree_rec(root, 0);
}

int main () {
    init_tree_node(1); 
    insert_entry_root_node(2);
    insert_entry_root_node(3);
    insert_entry_root_node(4);
    insert_entry_root_node(5);
    insert_entry_root_node(3);
    insert_entry_root_node(6);
    print_tree_rec(root, 0);
    delete_node(5, &root);
    print_tree_rec(root, 0);
}

// data structure or tree without being intrusive having the data with it also
// void init_root(int value) {
//     AVLEntry *ent = calloc(10, sizeof(root));
//     if(ent == NULL) {
//         printf("there was an error allocating memory for ent \n");
//     }
//     ent->val = value;
//     ent->node.left = NULL;
//     ent->node.right = NULL;
//     root = ent;
// }

// tree data structure being independent from data and generic
// void init_tree() {
//     AVLNode *tree_ = malloc(sizeof(AVLTree)*16);
//     if (tree == NULL) {
//         printf("there was an error allocating memory for the tree \n");
//     }

//     tree->nodes = tree_;
//     tree->capacity = 16;
//     tree->size = 0;
// }

// Print tree sideways (right child on top, left child on bottom)
// void printTree(AVLNode *root) {
//     if (!root) return;

//     // Manual stack for traversal
//     struct Stack {
//         AVLNode *node;
//         int level;
//     } stack[1000];  // adjust as needed

//     int top = -1;
//     AVLNode *node = root;
//     int level = 0;

//     while (node || top >= 0) {
//         // Go right as far as possible
//         while (node) {
//             stack[++top].node = node;
//             stack[top].level = level;
//             node = node->right;
//             level++;
//         }

//         // Pop from stack
//         node = stack[top].node;
//         level = stack[top].level;
//         top--;

//         // Print node
//         for (int i = 0; i < level; i++) {
//             printf("    "); // indent per level
//         }
//         printf("> %d\n", node->data);

//         // Go left
//         node = node->left;
//         level++;
//     }
// }

// void insert_val_tree(Entry *ent, Node *node) {
//     // printf("checking \n");
//     // printf("Node at this stage \n", node);
//     // printf("ent at this stage \n", ent->val);
//     if(ent->val == NULL) {
//         printf("no value provided to insert \n");
//     }
//     if (ent->val > root->val) {
//         // printf("should not come here \n");
//         if(node->right == NULL) {
//             node->right = &ent->node;
//             return;
//         }
//         else if(node->right!=NULL) {
//             insert_val_tree(ent, node->right);
//             return;
//         }
//     }
//     if(ent->val < root->val){
//         // printf("should left come here \n");
//         if(node->left == NULL){
//             node->left = &ent->node;
//             return;
//         }
//         else if(node->left!=NULL) {
//             insert_val_tree(ent, node->left);
//             return;
//         }
//     }
//     if(node == NULL) {
//         Entry *nent = calloc(1, sizeof(Entry));
//         nent->val = ent->val;
//         return;
//     }
// }

// Recursive pretty print
// void printTreeV1(AVLEntry *entry, int level) {
//     if (entry == NULL) return;

//     // Cast to entry for left/right
//     AVLEntry *right = (AVLEntry*) entry->node.right;
//     AVLEntry *left  = (AVLEntry*) entry->node.left;

//     // Right subtree first
//     printTreeV1(right, level + 1);

//     // Indentation based on level
//     for (int i = 0; i < level; i++) {
//         printf("    ");  // 4 spaces
//     }
//     printf("> %d\n", entry->val);

//     // Left subtree
//     printTreeV1(left, level + 1);
// }

// // Compute height of the tree
// int treeHeight(struct Entry *root) {
//     if (!root) return 0;
//     int lh = treeHeight((struct Entry*)root->node.left);
//     int rh = treeHeight((struct Entry*)root->node.right);
//     return (lh > rh ? lh : rh) + 1;
// }

// // Print tree level by level
// void printLevel(struct Entry *root, int level, int indentSpace) {
//     if (!root) {
//         // Print spaces when no node
//         for (int i = 0; i < indentSpace; i++) printf(" ");
//         printf(" ");
//         return;
//     }
//     if (level == 1) {
//         for (int i = 0; i < indentSpace; i++) printf(" ");
//         printf("%d", root->val);
//     } else if (level > 1) {
//         printLevel((struct Entry*)root->node.left, level - 1, indentSpace/2);
//         printLevel((struct Entry*)root->node.right, level - 1, indentSpace/2);
//     }
// }

// // Pretty print in pyramid form
// void printTree(struct Entry *root) {
//     int h = treeHeight(root);
//     int maxWidth = pow(2, h) * 2;

//     for (int i = 1; i <= h; i++) {
//         int spaces = maxWidth / pow(2, i);
//         printLevel(root, i, spaces);
//         printf("\n");
//     }
// }

// search and print
// void printTreeV1(AVLNode **root) {
//     if (entry == NULL) return;

//     // Cast to entry for left/right
//     AVLEntry *right = (AVLEntry*) entry->node.right;
//     AVLEntry *left  = (AVLEntry*) entry->node.left;

//     // Right subtree first
//     printTreeV1(right, level + 1);

//     // Indentation based on level
//     for (int i = 0; i < level; i++) {
//         printf("    ");  // 4 spaces
//     }
//     printf("> %d\n", entry->val);

//     // Left subtree
//     printTreeV1(left, level + 1);
// }

// void insert_entry_intrusive(int val) {
//     AVLEntry *nent = calloc(3, sizeof(Entry));
//     nent->val = val;
//     nent->node = NULL;
//     insert_val_loop(tree->tree,nent->node);
// }

