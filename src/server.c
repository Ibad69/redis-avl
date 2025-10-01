#include "server.h"
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

typedef struct AVLNode {
   AVLNode *parent; 
   AVLNode *right;
   AVLNode *left;
   uint32_t height;
   uint32_t cnt;
}AVLNode;

typedef struct ZSet {
    AVLNode *root;
    HMap hmap;
}ZSet;


typedef struct ZNode {
    HNode node;
    AVLNode tree;
    double score;
    size_t len;
    char name[0];
}ZNode;

enum {
    T_INIT = 0;
    T_STR = 1;
    T_ZSET = 2;
}

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

void buff_append8(buffer_type *buf, uint8_t *data){
    buff_append(buf, data, 1);
}

void buff_append32(buffer_type *buf, uint8_t *data){
    buff_append(buf, (const uint8_t *)data, 4);
}

void buff_appendi64(buffer_type *buf, uint8_t *data){
    buff_append(buf, (const uint8_t *)data, 8);
}

void buff_append_double(buffer_type *buf, uint8_t *data){
    buff_append(buf, (const uint8_t *)data, 8);
}

void start_response(buffer_type *buf, size_t *header) {
    *header = buf->size;
    buff_append(buf, 0, 4);
}

void end_response(StringVec *out, size_t *header) {
    size_t buf_size = out->size - header - 4;

    // check for large / huge message
    uint32_t len = (uint32_t)(buf_size);
    //below we are copying 4 bytes from len to out to the start of the buf
    memcpy(out[header], &len, 4)
}

void do_request(StringVec *out) {
    if(strcmp(out->items[0] == "get") == 0) {
        printf("this is a get command for you \n");
        // find the element from the hashmap and append to the output
    }
    if(strcmp(out->items[0] == "set") == 0) {
        printf("this is a set command for you \n");
        // add or insert some value to the hashmap
    }
    if(strcmp(out->items[0] == "del") == 0) {
        // delete some value from the hashmap
        printf("this is a del command for you \n");
    }
}


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

    memcpy(buf->buff + buf->size, data, len);
    buf->size += len;
    // return 0; 
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

    printf("nstr is : %d \n", nstr);

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

int try_one_request(Conn *client) {

    if(client->incoming_buff->size < 4) {
        // we need to check for more reads;
        printf("returning because incoming buff size is less than 4 \n");
        return 0;
    }
    uint32_t len = 0;
    printf("data beefore mcmpy : %s \n", client->incoming_buff);
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
    printf("the request that is received with data : %s \n", request);

    StringVec *out = malloc(sizeof(StringVec));
    out->count = 0;
    out->items = malloc(2);

    if(parse_req(&request, len, out) == -1){
        printf("there was an error parsing the request \n");
        return false;
    };

    for (size_t i = 0; i < strlen(out->items[1]); i++) {
        printf("%02X ", (unsigned char) out->items[1][i]);
    }
    printf("\n");

    size_t header_pos = 0;
    start_response(out, &header);
    do_request(out); 
    end_response(out, &header);

    // // // add the data to send back to the client in the outgoing buffer
    // buff_append(client->outgoing_buff, (const uint8_t *)&len, 4);
    // buff_append(client->outgoing_buff, request, len);

    // // consume the buffer and decrease the size also 
    buff_consume(client->incoming_buff, 4+len);

    return true;
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
        for (int i = 0; i < 16; i++) {
            printf("%02x ", client.incoming_buff->buff[i]);
        }
        printf("\n");

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
