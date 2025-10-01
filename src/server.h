#include <stdio.h>
#include <stdint.h>
#include <poll.h>
#include <sys/poll.h>

typedef struct {
    uint8_t *buffer_begin;
    uint8_t *data_begin;
    uint8_t *data_end;
    uint8_t *buffer_end;
    uint8_t *buff;
    size_t capacity;
    size_t size;
    size_t tmp_size;
} buffer_type;

typedef struct {
    int fd;
    int want_write;
    int want_read;
    int want_close;
    buffer_type *incoming_buff;
    buffer_type *outgoing_buff;
} Conn;

typedef struct {
  size_t size;  
  size_t capacity;
  Conn *clients;
} Conns;

int get_listener();
int accept_connection(int fd, struct pollfd **pfds, int *fd_count, int *fd_size);
void recv_all(int fd, void *buf, struct pollfd **pfds, int *fd_count, int i, size_t *bufsz);
void sendall(void *buf, ssize_t total_len, int fd);
void handle_client_data(int fd, int *fd_count, int i, struct pollfd **pfds, Conns *clients); 
void process_connection(struct pollfd **pfds, int *fd_count, int *fd_size, int listener, Conns *clients);
void add_to_pfds (int fd, struct pollfd **pfds, int *fd_count, int *fd_size);
void del_from_pfd(struct pollfd **pfds, int *fd_count, int i);
Conns* init_client_arr();



