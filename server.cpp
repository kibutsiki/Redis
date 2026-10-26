// stdlib
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
// system
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
// C++
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
// project
#include "hashtable.h"

#define container_of(ptr, T, member) \
    ((T*)((char*)(ptr) - offsetof(T, member)))

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void msg_errno(const char *msg) {
    fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

static void die(const char *msg) {
    fprintf(stderr, "[%d] %s\n", errno, msg);
    abort();
}

static void fd_set_nb(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        die("fcntl error");
        return;
    }

    flags |= O_NONBLOCK;

    errno = 0;
    (void)fcntl(fd, F_SETFL, flags);
    if (errno) {
        die("fcntl error");
    }
}

const size_t k_max_msg = 32 << 20;  // likely larger than the kernel buffer

struct Conn {
    int fd = -1;
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;
    
    std::vector<uint8_t> incoming;
    std::vector<uint8_t> outgoing;
};
// append to the back
static void
buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
    if (len != 0) {
        buf.insert(buf.end(), data, data + len);
    }
}



static void buf_consume(std::vector<uint8_t> &buf, size_t n) {
    buf.erase(buf.begin(), buf.begin() + n);
}

static Conn *handle_accept(int fd) {
    // accept
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
    if (connfd < 0) {
        msg_errno("accept() error");
        return NULL;
    }
    uint32_t ip = client_addr.sin_addr.s_addr;
    fprintf(stderr, "new client from %u.%u.%u.%u:%u\n",
        ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, ip >> 24,
        ntohs(client_addr.sin_port)
    );

    fd_set_nb(connfd);

    Conn *conn = new Conn();
    conn->fd = connfd;
    conn->want_read = true;
    return conn;
}

const size_t k_max_args = 200 * 1000;

static bool read_u32(const uint8_t *& cur, const uint8_t *end, uint32_t &out){
    if(cur + 4 > end){
        return false;
    }

    memcpy(&out, cur, 4);
    cur += 4;
    return true;
}

static bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n, std::string &out){
    if(cur + n > end){
        return false;
    }
    out.assign(cur, cur + n);
    cur += n;
    return true;
}

static int32_t parse_req(const uint8_t *data, size_t size, std::vector<std::string> &out){
    const uint8_t *end{data + size};
    uint32_t nstr{};
    if(!read_u32(data,end,nstr)){
        return -1;
    }
    if(nstr > k_max_args){
        return -1;
    }
    while(out.size() < nstr){
        uint32_t len{};
        if(!read_u32(data,end,len)){
            return -1;
        }
        out.push_back(std::string());
        if(!read_str(data, end, len, out.back())){
            return -1;
        }
    }
    if(data != end){
        return -1;
    }
    return 0;
}

using Buffer = std::vector<uint8_t>;

enum : uint8_t {
    TAG_NIL = 0,
    TAG_ERR = 1,
    TAG_STR = 2,
    TAG_INT = 3,
    TAG_DBL = 4,
    TAG_ARR = 5,
};

static struct {
    HMap db;
} g_data;

struct Entry {
    HNode node;
    std::string key;
    std::string value;
};

static bool entry_eq(HNode* lhs, HNode* rhs) {
    Entry* left = container_of(lhs, Entry, node);
    Entry* right = container_of(rhs, Entry, node);
    return left->key == right->key;
}

static uint64_t str_hash(const uint8_t* data, std::size_t len) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (std::size_t i = 0; i < len; ++i) {
        hash ^= data[i];
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

static HNode* make_key_node(const std::string& key, Entry& entry) {
    entry.key = key;
    entry.node.hcode = str_hash(
        reinterpret_cast<const uint8_t*>(entry.key.data()),
        entry.key.size()
    );
    return &entry.node;
}

static void out_nil(Buffer& out) {
    uint8_t tag = TAG_NIL;
    buf_append(out, &tag, 1);
}

static void out_str(Buffer& out, const char* s, size_t size) {
    uint8_t tag = TAG_STR;
    buf_append(out, &tag, 1);
    uint32_t len = static_cast<uint32_t>(size);
    buf_append(out, reinterpret_cast<const uint8_t*>(&len), 4);
    buf_append(out, reinterpret_cast<const uint8_t*>(s), size);
}

static void out_int(Buffer& out, int64_t value) {
    uint8_t tag = TAG_INT;
    buf_append(out, &tag, 1);
    buf_append(out, reinterpret_cast<const uint8_t*>(&value), 8);
}

static void out_err(Buffer& out, const char* message) {
    uint8_t tag = TAG_ERR;
    buf_append(out, &tag, 1);
    uint32_t len = static_cast<uint32_t>(strlen(message));
    buf_append(out, reinterpret_cast<const uint8_t*>(&len), 4);
    buf_append(out, reinterpret_cast<const uint8_t*>(message), len);
}

static void do_get(std::vector<std::string>& cmd, Buffer& out) {
    Entry key;
    make_key_node(cmd[1], key);

    HNode* node = hm_lookup(&g_data.db, &key.node, entry_eq);
    if (node == nullptr) {
        out_nil(out);
        return;
    }

    const Entry* entry = container_of(node, Entry, node);
    out_str(out, entry->value.data(), entry->value.size());
}

static void do_set(std::vector<std::string>& cmd, Buffer& out) {
    Entry key;
    make_key_node(cmd[1], key);

    HNode* node = hm_lookup(&g_data.db, &key.node, entry_eq);
    if (node != nullptr) {
        Entry* entry = container_of(node, Entry, node);
        entry->value.swap(cmd[2]);
        out_nil(out);
        return;
    }

    Entry* entry = new Entry();
    entry->key.swap(key.key);
    entry->value.swap(cmd[2]);
    entry->node.hcode = key.node.hcode;
    hm_insert(&g_data.db, &entry->node);
    out_nil(out);
}

static void do_del(std::vector<std::string>& cmd, Buffer& out) {
    Entry key;
    make_key_node(cmd[1], key);

    HNode* node = hm_delete(&g_data.db, &key.node, entry_eq);
    if (node != nullptr) {
        delete container_of(node, Entry, node);
    }
    out_int(out, node != nullptr ? 1 : 0);
}

static void do_request(std::vector<std::string>& cmd, Buffer& out) {
    if (cmd.size() == 2 && cmd[0] == "get") {
        do_get(cmd, out);
    } else if (cmd.size() == 3 && cmd[0] == "set") {
        do_set(cmd, out);
    } else if (cmd.size() == 2 && cmd[0] == "del") {
        do_del(cmd, out);
    } else {
        out_err(out, "bad request");
    }
}

static bool try_one_request(Conn *conn) {
    if (conn->incoming.size() < 4) {
        return false;
    }
    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);
    if (len > k_max_msg) {
        msg("too long");
        conn->want_close = true;
        return false;
    }
    // message body
    if (4 + len > conn->incoming.size()) {
        return false;
    }
    const uint8_t *request = &conn->incoming[4];

    std::vector<std::string> cmd{};

    if(parse_req(request, len, cmd) < 0){
        conn->want_close = true;
        return false;
    }
    size_t response_start = conn->outgoing.size();
    uint32_t response_len = 0;
    buf_append(conn->outgoing, reinterpret_cast<const uint8_t*>(&response_len), 4);
    size_t payload_start = conn->outgoing.size();
    do_request(cmd, conn->outgoing);
    response_len = static_cast<uint32_t>(conn->outgoing.size() - payload_start);
    memcpy(conn->outgoing.data() + response_start, &response_len, 4);

    buf_consume(conn->incoming, 4 + len);
    return true;
}

static void handle_write(Conn *conn) {
    assert(conn->outgoing.size() > 0);
    ssize_t rv = write(conn->fd, &conn->outgoing[0], conn->outgoing.size());
    if (rv < 0 && errno == EAGAIN) {
        return;
    }
    if (rv < 0) {
        msg_errno("write() error");
        conn->want_close = true; 
        return;
    }

    buf_consume(conn->outgoing, (size_t)rv);

    if (conn->outgoing.size() == 0) { 
        conn->want_read = true;
        conn->want_write = false;
    } 
}

static void handle_read(Conn *conn) {

    uint8_t buf[64 * 1024];
    ssize_t rv = read(conn->fd, buf, sizeof(buf));
    if (rv < 0 && errno == EAGAIN) {
        return;
    }
    if (rv < 0) {
        msg_errno("read() error");
        conn->want_close = true;
        return;
    }
    if (rv == 0) {
        if (conn->incoming.size() == 0) {
            msg("client closed");
        } else {
            msg("unexpected EOF");
        }
        conn->want_close = true;
        return;
    }
    buf_append(conn->incoming, buf, (size_t)rv);

    while (try_one_request(conn)) {}

    if (conn->outgoing.size() > 0) {
        conn->want_read = false;
        conn->want_write = true;

        return handle_write(conn);
    }
}

int main() {
    // the listening socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // bind
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0);    // wildcard address 0.0.0.0
    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("bind()");
    }

    // set the listen fd to nonblocking mode
    fd_set_nb(fd);

    // listen
    rv = listen(fd, SOMAXCONN);
    if (rv) {
        die("listen()");
    }

    // a map of all client connections, keyed by fd
    std::vector<Conn *> fd2conn;
    // the event loop
    std::vector<struct pollfd> poll_args;
    while (true) {
        // prepare the arguments of the poll()
        poll_args.clear();
        // put the listening sockets in the first position
        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);
        // the rest are connection sockets
        for (Conn *conn : fd2conn) {
            if (!conn) {
                continue;
            }
            // always poll() for error
            struct pollfd pfd = {conn->fd, POLLERR, 0};
            // poll() flags from the application's intent
            if (conn->want_read) {
                pfd.events |= POLLIN;
            }
            if (conn->want_write) {
                pfd.events |= POLLOUT;
            }
            poll_args.push_back(pfd);
        }

        // wait for readiness
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
        if (rv < 0 && errno == EINTR) {
            continue;   // not an error
        }
        if (rv < 0) {
            die("poll");
        }

        // handle the listening socket
        if (poll_args[0].revents) {
            if (Conn *conn = handle_accept(fd)) {
                // put it into the map
                if (fd2conn.size() <= (size_t)conn->fd) {
                    fd2conn.resize(conn->fd + 1);
                }
                assert(!fd2conn[conn->fd]);
                fd2conn[conn->fd] = conn;
            }
        }

        // handle connection sockets
        for (size_t i = 1; i < poll_args.size(); ++i) { // note: skip the 1st
            uint32_t ready = poll_args[i].revents;
            if (ready == 0) {
                continue;
            }

            Conn *conn = fd2conn[poll_args[i].fd];
            if (ready & POLLIN) {
                assert(conn->want_read);
                handle_read(conn);  // application logic
            }
            if (ready & POLLOUT) {
                assert(conn->want_write);
                handle_write(conn); // application logic
            }

            // close the socket from socket error or application logic
            if ((ready & POLLERR) || conn->want_close) {
                (void)close(conn->fd);
                fd2conn[conn->fd] = NULL;
                delete conn;
            }
        }   // for each connection sockets
    }   // the event loop
    return 0;
}