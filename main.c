#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <strings.h>

#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <pthread.h>

#include <ev.h>

#define HELPSTRING "Usage: cdemo -p [port]\n"
#define SA struct sockaddr

static int sockfd;

struct ev_loop *main_loop;
struct ev_loop *conn_loop;

enum Msg {
    msg_exit, msg_port, msg_in, msg_out
};

struct ev_port {
    enum Msg msg;
    long port;
};

struct ev_exit {
    enum Msg msg;
};

struct ev_in_out {
    enum Msg msg;
    char *str;
    int connfd;
};

union ev_msg {
    struct ev_port p;
    struct ev_exit e;
    struct ev_in_out io;
};

long get_port(int argc, char *argv[]) {
    if (argc < 3) {
        printf(HELPSTRING);
        exit(0);
    }

    if (strcmp(argv[1], "-p") == 0) {
        const char *nptr = argv[1];
        char *endptr = NULL;
        errno = 0;
        long port = strtol(argv[2], &endptr, 10);
        if (nptr == endptr)
            errno = 1;
        else if (errno == 0 && nptr && *endptr != 0)
            errno = 1;

        if (errno != 0 || port <= 0) {
            printf("Invalid port value\n");
            exit(0);
        }
        return port;
    } else {
        printf(HELPSTRING);
        exit(0);
    }
}

int setup_sock(long port) {
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sockfd == -1) {
        printf("Socket creation failed\n");
        exit(0);
    } else
        printf("Socket created\n");
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    char *s = inet_ntoa(servaddr.sin_addr);
    printf("IP address: %s\n", s);
    printf("Port: %li \n", port);

    if ((bind(sockfd, (SA *) &servaddr, sizeof(servaddr))) != 0) {
        printf("Socket bind failed\n");
        exit(0);
    } else
        printf("Socket successfully bound\n");

    if ((listen(sockfd, 5)) != 0) {
        printf("Listen failed\n");
        exit(0);
    } else
        printf("Server listening\n");

    return sockfd;
}

#define MAX_MSG_LEN 80

static void
stdin_cb(EV_P_ ev_io *w, int revents) {
    char buff[MAX_MSG_LEN];

    bzero(buff, MAX_MSG_LEN);

    read(w->fd, buff, sizeof(buff));

    if (strncmp("exit", buff, 4) == 0) {
        printf("Server received shutdown command\n");

        ev_async signal;
        union ev_msg m;
        m.e.msg = msg_exit;
        signal.data = &m;

        printf("Sending msg_exit to main loop\n");

        ev_async_send(main_loop, &signal);
        return;
    } else if (strncmp("port", buff, 4) == 0) {
        printf("Changing port\n");

        const char *nptr = &buff[5];
        char *endptr = NULL;
        errno = 0;
        long port = strtol(nptr, &endptr, 10);
        if (nptr == endptr)
            errno = 1;
        else if (errno == 0 && nptr && *endptr != 0)
            errno = 1;

        if (errno != 0 || port <= 0) {
            printf("Invalid port value\n");
            exit(0);
        }

        ev_async signal;
        union ev_msg m;
        m.p.msg = msg_port;
        m.p.port = port;
        signal.data = &m;

        printf("Sending msg_port to main loop\n");
        ev_async_send(main_loop, &signal);
    }
}

static void *handle_cli() {
    struct ev_loop *loop = EV_DEFAULT;
    ev_io stdin_watcher;

    ev_io_init (&stdin_watcher, stdin_cb, /*STDIN_FILENO*/ 0, EV_READ);
    ev_io_start(loop, &stdin_watcher);

    ev_run(loop, 0);
}

//conn socket callback
static void cb_accept(EV_P_ ev_io *w, int revents) {
    struct sockaddr_in client;

    unsigned int len = sizeof(client);

    int connfd = accept(w->fd, (SA *) &client, &len);
    if (connfd < 0) {
        printf("Accept failed\n");
        exit(0);
    } else
        printf("Connection accepted\n");

    char *buff = malloc(MAX_MSG_LEN);
    bzero(buff, MAX_MSG_LEN);
    read(connfd, buff, sizeof(buff));

    ev_async signal;
    union ev_msg m;
    m.io.msg = msg_in;
    m.io.str = buff;
    m.io.connfd = connfd;
    signal.data = &m;

    printf("Sending msg_in to main loop\n");
    ev_async_send(main_loop, &signal);
}

//conn msg callback
static void cb_respond(EV_P_ struct ev_async *w, int revents) {
    union ev_msg m = *(union ev_msg *) w->data;

    switch (m.io.msg) {
        case msg_exit:
            ev_break(EV_A_ EVBREAK_ONE);
            break;
        case msg_port:
            //need to restart thread
            ev_break(EV_A_ EVBREAK_ONE);
            break;
        case msg_in:
            printf("Invalid msg in cb_respond\n");
            break;
        case msg_out:
            errno = 0;
            write(m.io.connfd, m.io.str, MAX_MSG_LEN);
            if (errno != 0) {
                printf("Connection closed\n");
                break;
            }
    }
}

pthread_t connthread;

void *handle_connection() {
    conn_loop = EV_DEFAULT;

    //watch for incoming connections and pass them to main thread
    ev_io socket_watcher;
    ev_io_init(&socket_watcher, cb_accept, sockfd, EV_READ);
    ev_io_start(conn_loop, &socket_watcher);

    ev_async msg_watcher;
    ev_async_init(&msg_watcher, cb_respond);
    ev_async_start(conn_loop, &msg_watcher);

    ev_run(conn_loop, 0);
}

static void cb_msg(EV_P_ ev_async *w, int revents) {
    union ev_msg m = *(union ev_msg *) w->data;
    switch (m.io.msg) {
        case msg_exit:
            ev_break(EV_A_ EVBREAK_ONE);
            break;
        case msg_port: {
            ev_async signal;
            signal.data = &m;

            printf("Sending msg_port to conn loop\n");
            ev_async_send(conn_loop, &signal);
            close(sockfd);

            sockfd = setup_sock(m.p.port);
            pthread_create(&connthread, NULL, handle_connection, &sockfd);
        }
            break;
        case msg_in: {
            //find last non-empty char
            char *buff = m.io.str;
            int last;
            for (last = MAX_MSG_LEN - 1; last > 1; last--) {
                if (buff[last] != 0) {
                    break;
                }
            }

            //invert string
            for (int i = 0; i < last / 2; i++) {
                char temp = buff[last - i];
                buff[i] = buff[last - i];
                buff[last - i] = temp;
            }

            ev_async signal;
            union ev_msg mo;
            mo.io.msg = msg_out;
            mo.io.str = buff;
            mo.io.connfd = m.io.connfd;
            signal.data = &mo;

            printf("Sending msg_out to conn loop\n");
            ev_async_send(conn_loop, &signal);
        }
            break;
        case msg_out:
            printf("Invalid msg in cb_msg\n");
            break;
    }
}


int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);
    long port = get_port(argc, argv);

    sockfd = setup_sock(port);
    printf("Socket %i", sockfd);

    main_loop = EV_DEFAULT;
    ev_async msg_watcher;
    ev_async_init(&msg_watcher, cb_msg);
    ev_async_start(main_loop, &msg_watcher);

    pthread_t clithread;
    pthread_create(&clithread, NULL, handle_cli, NULL);

    pthread_create(&connthread, NULL, handle_connection, NULL);

    ev_run(main_loop, 0);

    close(sockfd);

    return 0;
}


