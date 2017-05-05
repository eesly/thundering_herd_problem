/*
 *
 *
 *
 *
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h> /* See NOTES */
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define MAXEVENTS 64

#define ERREXIT(msg) \
    perror(msg);     \
    exit(-1)

#define SUCEXIT(msg) \
    printf(msg);     \
    exit(0)

#define PARAMERROREXIT(argc, argv) \
    usage(argc, argv);             \
    exit(-1)

struct argset {
    char *addr;
    char *port;
    bool reuseaddr;
    bool reuseport;
    int proc_num;
    argset() : addr(NULL), port("9000"), reuseaddr(false), reuseport(false), proc_num(1) {}
};

void usage(int argc, char **argv) {
    printf("param error!\n");
    printf("usage: %s [-i ip] [-p port] [-ap] [-n proc_num]\n", argv[0]);
    printf("\t-a set reuse addr\n");
    printf("\t-p set reuse port\n");
    printf("\t-i set bind addr, default value is INADDR_ANY\n");
    printf("\t-p set bind port, default value is 9000\n");
    printf("\t-n set process num, default value is 1\n\n");
}

void parse_cmd(int argc, char **argv, struct argset *set) {
    if (set == NULL)
        return;

    int cur;
    opterr = 0;
    while ((cur = getopt(argc, argv, "i:t:apn:")) != -1) {
        switch (cur) {
        case 'i':
            set->addr = optarg;
            break;
        case 't':
            set->port = optarg;
            break;
        case 'a':
            set->reuseaddr = true;
            break;
        case 'p':
            set->reuseport = true;
            break;
        case 'n':
            set->proc_num = atoi(optarg);
            break;
        default:
            PARAMERROREXIT(argc, argv);
        }
    }

    printf("\nset addr = %s\n", set->addr ? set->addr : "INADDR_ANY(default value)");
    printf("set port = %s\n", set->port ? set->port : "9000(default value)");
    printf("set %s %s\n", set->reuseaddr ? "reuseaddr" : "", set->reuseport ? "reuseport" : "");
    printf("set proc num = %d\n\n", set->proc_num > 1 ? set->proc_num : 1);
}

void set_reuseaddr(int lfd) {
    int enable = 1;
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1) {
        ERREXIT("set reuseaddr failed!");
    }
}

void set_reuseport(int lfd) {
    int enable = 1;
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) == -1) {
        ERREXIT("set reuseport failed!");
    }
}

void set_noblock(int cfd) {
    int flag = fcntl(cfd, F_GETFL, 0);
    if (flag == -1) {
        ERREXIT("set set_noblock failed, get flag error!");
    }

    flag |= O_NONBLOCK;
    if (fcntl(cfd, F_SETFL, flag) == -1) {
        ERREXIT("set set_noblock failed, set flag error!");
    }
}

int main(int argc, char **argv) {
    struct argset argset;
    parse_cmd(argc, argv, &argset);

    int lfd = socket(PF_INET, SOCK_STREAM, 0);
    if (lfd == -1) {
        ERREXIT("call socket failed!");
    }

    if (argset.reuseaddr)
        set_reuseaddr(lfd);

    if (argset.reuseport)
        set_reuseport(lfd);

    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(atol(argset.port));
    if (argset.addr == NULL) {
        serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_aton(argset.addr, &(serveraddr.sin_addr)) == 0) {
            ERREXIT("convert ip addr error!");
        }
    }

    if (bind(lfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) == -1) {
        ERREXIT("call bind failed!");
    }

    listen(lfd, 1024);

    int pid = 0;
    for (int i = 0; i < argset.proc_num; ++i) {
        pid = fork();
        if (pid < 0) {
            ERREXIT("call fork failed!");
        }

        if (pid == 0)
            break;
    }

    if (pid > 0) {
        int status;
        wait(&status);
        return 0;
    }

    printf("start a new process [%d]\n", getpid());

    int efd = epoll_create1(0);
    if (efd == -1) {
        ERREXIT("call epoll_created failed!");
    }

    struct epoll_event event;

    event.data.fd = lfd;
    event.events = EPOLLIN | EPOLLET;

    if (epoll_ctl(efd, EPOLL_CTL_ADD, lfd, &event) == -1) {
        ERREXIT("call epoll_ctl failed!");
    }

    // malloc with init 0
    struct epoll_event *events = (struct epoll_event *)calloc(MAXEVENTS, sizeof(event));

    int wait_time_ms = -1;

    int client_num = 0;
    while (1) {
        int n = epoll_wait(efd, events, MAXEVENTS, wait_time_ms);
        printf("[%d] wake up\n", getpid());
        for (int i = 0; i < n; ++i) {
            if (events[i].events & EPOLLERR ||
                events[i].events & EPOLLHUP ||
                !(events[i].events & EPOLLIN)) {
                //event filter
                perror("epoll wait error!");
                close(events[i].data.fd);
                continue;
            } else if (events[i].data.fd == lfd) {
                //sleep(1);
                //one or more new connections
                while (1) {
                    int cfd = accept(lfd, (struct sockaddr *) NULL, NULL);
                    if (cfd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            //have proc all new connections
                            break;
                        } else {
                            ERREXIT("call accept failed!");
                        }
                    }

                    set_noblock(cfd);

                    event.data.fd = cfd;
                    event.events = EPOLLIN | EPOLLET;

                    if (epoll_ctl(efd, EPOLL_CTL_ADD, cfd, &event) == -1) {
                        ERREXIT("call epoll_ctl failed!");
                    }

                    printf("[%d] accept a new client\t [%d]\n", getpid(), ++client_num);
                }
                continue;
            } else {
                //have data need to read
                printf("[%d] recieve data from a client begin:\n", getpid());
                while (1) {
                    char *pbuf = new char[1024];
                    memset(pbuf, 0, 1024);

                    int nread = read(events[i].data.fd, pbuf, 1024);
                    if (nread <= 0) {
                        if (nread == 0 || errno == EAGAIN) {
                            //have proc all read data
                            break;
                        } else {
                            ERREXIT("call read data failed!");
                        }
                    }

                    printf("%s", std::string(pbuf, nread).c_str());

                    delete[] pbuf;
                }
                close(events[i].data.fd);
                printf("\n[%d] recieve data from a client end~\n", getpid());
            }
        }
    }

    free(events);
    close(lfd);

    return 0;
}
