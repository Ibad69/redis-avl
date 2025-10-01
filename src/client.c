#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#define K_MAX_MSG 4096

static void msg(const char *m) {
    fprintf(stderr, "%s\n", m);
}

static void die(const char *m) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, m);
    abort();
}

static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error or EOF
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t send_req(int fd, int argc, char **argv) {
    uint32_t len = 4; // initial length (for number of strings)
    for (int i = 0; i < argc; i++) {
        len += 4 + strlen(argv[i]);
    }
    if (len > K_MAX_MSG) {
        return -1;
    }

    char wbuf[4 + K_MAX_MSG];
    memcpy(&wbuf[0], &len, 4); // assume little endian

    uint32_t n = argc;
    memcpy(&wbuf[4], &n, 4);

    size_t cur = 8;
    for (int i = 0; i < argc; i++) {
        uint32_t p = (uint32_t)strlen(argv[i]);
        memcpy(&wbuf[cur], &p, 4);
        memcpy(&wbuf[cur + 4], argv[i], p);
        cur += 4 + p;
    }
    return write_all(fd, wbuf, 4 + len);
}

static int32_t read_res(int fd) {
    char rbuf[4 + K_MAX_MSG];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if (err) {
        if (errno == 0) {
            msg("EOF");
        } else {
            msg("read() error");
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4);  // assume little endian
    if (len > K_MAX_MSG) {
        msg("too long");
        return -1;
    }

    err = read_full(fd, &rbuf[4], len);
    if (err) {
        msg("read() error");
        return err;
    }

    uint32_t rescode = 0;
    if (len < 4) {
        msg("bad response");
        return -1;
    }
    memcpy(&rescode, &rbuf[4], 4);

    printf("server says: [%u] %.*s\n",
           rescode, (int)(len - 4), &rbuf[8]);
    return 0;
}

int main(int argc, char **argv) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6969);               // fixed port
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1

    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("connect");
    }

    int32_t err = send_req(fd, argc - 1, argv + 1);
    if (!err) {
        err = read_res(fd);
    }

    close(fd);
    return 0;
}


// // Send a request: write [len][payload]
// int32_t send_req_old(int fd, const char *data, size_t len) {
//     // uint32_t nlen = htonl((uint32_t)len);
//     uint32_t nlen = (uint32_t)len;

//     if (write(fd, &nlen, sizeof(nlen)) != sizeof(nlen)) {
//         perror("write length");
//         return -1;
//     }
//     if (write(fd, data, len) != (ssize_t)len) {
//         perror("write payload");
//         return -1;
//     }
//     return 0;
// }

// static int32_t send_req(int fd, const std::vector<std::string> &cmd) {
//     uint32_t len = 4;
//     for (const std::string &s : cmd) {
//         len += 4 + s.size();
//     }
//     if (len > k_max_msg) {
//         return -1;
//     }

//     char wbuf[4 + k_max_msg];
//     memcpy(&wbuf[0], &len, 4);  // assume little endian
//     uint32_t n = cmd.size();
//     memcpy(&wbuf[4], &n, 4);
//     size_t cur = 8;
//     for (const std::string &s : cmd) {
//         uint32_t p = (uint32_t)s.size();
//         memcpy(&wbuf[cur], &p, 4);
//         memcpy(&wbuf[cur + 4], s.data(), s.size());
//         cur += 4 + s.size();
//     }
//     return write_all(fd, wbuf, 4 + len);
// }

// // Read a response: read [len][payload]
// int32_t read_res(int fd) {
//     uint32_t nlen;
//     ssize_t n = read(fd, &nlen, sizeof(nlen));
//     if (n == 0) {
//         fprintf(stderr, "server closed connection\n");
//         return -1;
//     }
//     if (n != sizeof(nlen)) {
//         perror("read length");
//         return -1;
//     }

//     // uint32_t len = ntohl(nlen);
//     uint32_t len = (nlen);
//     char *buf = malloc(len + 1);
//     if (!buf) return -1;

//     size_t off = 0;
//     while (off < len) {
//         n = read(fd, buf + off, len - off);
//         if (n <= 0) {
//             perror("read payload");
//             free(buf);
//             return -1;
//         }
//         off += n;
//     }
//     buf[len] = '\0';
//     printf("Response: %s\n", buf);
//     free(buf);
//     return 0;
// }

// int main(void) {
//     // Connect to server
//     const char *server_ip = "127.0.0.1";
//     int server_port = 6969;

//     int sock = socket(AF_INET, SOCK_STREAM, 0);
//     if (sock < 0) {
//         perror("socket");
//         return 1;
//     }

//     struct sockaddr_in addr = {0};
//     addr.sin_family = AF_INET;
//     addr.sin_port = htons(server_port);
//     if (inet_pton(AF_INET, server_ip, &addr.sin_addr) <= 0) {
//         perror("inet_pton");
//         close(sock);
//         return 1;
//     }

//     if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
//         perror("connect");
//         close(sock);
//         return 1;
//     }

//     // Pipelined requests
//     const char *queries[] = {"hello1", "hello2", "hello3"};
//     size_t nqueries = sizeof(queries) / sizeof(queries[0]);

//     // Send all requests without waiting
//     for (size_t i = 0; i < nqueries; i++) {
//         if (send_req(sock, queries[i], strlen(queries[i])) < 0) {
//             close(sock);
//             return 1;
//         }
//     }

//     // Then read responses
//     for (size_t i = 0; i < nqueries; i++) {
//         if (read_res(sock) < 0) {
//             close(sock);
//             return 1;
//         }
//     }

//     close(sock);
//     return 0;
// }

