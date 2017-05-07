/*
 *
 *
 *
 *
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h> /* See NOTES */
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

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

#define LOG_ERROR(fmt, ...) \
    printf("error:[%d] " fmt "\n", getpid(), ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    printf("info :[%d] " fmt "\n", getpid(), ##__VA_ARGS__)

#ifdef DEBUG
#define LOG_DEBUG(fmt, ...) \
    printf("debug:[%d] " fmt "\n", getpid(), ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif

#define EPOLL_LOCK "epoll_lock"

int client_num = 0;

struct argset {
    char *addr;
    char *port;
    bool reuseaddr;
    bool reuseport;
    int proc_num;
    argset() : addr(NULL), port((char *)"9000"), reuseaddr(false), reuseport(false), proc_num(1) {}
};

void usage(int argc, char **argv) {
    LOG_INFO("param error!");
    LOG_INFO("usage: %s [-i ip] [-p port] [-ap] [-n proc_num]", argv[0]);
    LOG_INFO("\t-a set reuse addr");
    LOG_INFO("\t-p set reuse port");
    LOG_INFO("\t-i set bind addr, default value is INADDR_ANY");
    LOG_INFO("\t-p set bind port, default value is 9000");
    LOG_INFO("\t-n set process num, default value is 1\n");
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

    LOG_INFO("set addr = %s", set->addr ? set->addr : "INADDR_ANY(default value)");
    LOG_INFO("set port = %s", set->port ? set->port : "9000(default value)");
    LOG_INFO("set %s %s", set->reuseaddr ? "reuseaddr" : "", set->reuseport ? "reuseport" : "");
    LOG_INFO("set proc num = %d\n", set->proc_num > 1 ? set->proc_num : 1);
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

void proc_accept_event(int lfd, int efd) {
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

        struct epoll_event event;
        event.data.fd = cfd;
        event.events = EPOLLIN | EPOLLET;
        if (epoll_ctl(efd, EPOLL_CTL_ADD, cfd, &event) == -1) {
            ERREXIT("call epoll_ctl failed!");
        }

        LOG_INFO("accept a new client\t [%d]", ++client_num);
    }
}

void proc_rw_event(int fd) {
#if 0
    //have data need to send
    LOG_DEBUG("send data to one client begin:");
    char *pbuf = new char[1024];
    memset(pbuf, 0, 1024);

    snprintf(pbuf, 1024, "[%d] accept a new client\n", getpid());
    LOG_INFO(pbuf);
    send(fd, pbuf, strlen(pbuf) + 1, 0);

    delete[] pbuf;
    close(fd);
    LOG_DEBUG("send data to a client end~");
#endif

#if 1
    //have data need to read
    LOG_DEBUG("recieve data from a client begin:");
    while (1) {
        char *pbuf = new char[1024];
        memset(pbuf, 0, 1024);

        int nread = read(fd, pbuf, 1024);
        if (nread <= 0) {
            if (nread == 0 || errno == EAGAIN) {
                //have proc all read data
                break;
            } else {
                ERREXIT("call read data failed!");
            }
        }
        LOG_INFO("%s", std::string(pbuf, nread).c_str());

        delete[] pbuf;
    }
    close(fd);
    LOG_DEBUG("recieve data from a client end~");
#endif
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

    set_noblock(lfd);

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

    //init epoll listen lock
    sem_unlink(EPOLL_LOCK);
    sem_t *lock = sem_open(EPOLL_LOCK, O_CREAT, 0666, 1);
    if (lock == SEM_FAILED) {
        ERREXIT("semaphore initilization!");
    }

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

    LOG_INFO("start a new process");

    int efd = epoll_create1(0);
    if (efd == -1) {
        ERREXIT("call epoll_created failed!");
    }

    struct epoll_event event;

    // malloc with init 0
    struct epoll_event *events = (struct epoll_event *) calloc(MAXEVENTS, sizeof(event));

    int wait_time_ms = 1000;
    bool is_got_lock = false;
    while (1) {
        std::vector<struct epoll_event *> delay_events;
        //try to get the lock
        if (sem_trywait(lock) == 0) {
            if (is_got_lock == false) {
                event.data.fd = lfd;
                event.events = EPOLLIN | EPOLLET;
                if (epoll_ctl(efd, EPOLL_CTL_ADD, lfd, &event) == -1) {
                    ERREXIT("call epoll_ctl failed!");
                }
                is_got_lock = true;
            }
            LOG_DEBUG("get lock");
        } else {
            if (errno != EAGAIN) {
                ERREXIT("call sem_trywait failed!");
            }
            if (is_got_lock) {
                if (epoll_ctl(efd, EPOLL_CTL_DEL, lfd, NULL) == -1) {
                    if (errno != ENOENT) {
                        ERREXIT("call epoll_ctl failed!");
                    }
                }
                is_got_lock = false;
            }
            LOG_DEBUG("don't get lock");
        }

        int n = epoll_wait(efd, events, MAXEVENTS, wait_time_ms);
        for (int i = 0; i < n; ++i) {
            if (events[i].events & EPOLLERR ||
                events[i].events & EPOLLHUP ||
                !(events[i].events & EPOLLIN)) {
                //event filter
                LOG_ERROR("epoll wait error!");
                close(events[i].data.fd);
                continue;
            } else if (events[i].data.fd == lfd) {
                LOG_INFO("wake up");
                //sleep(1);
                //one or more new connections
                proc_accept_event(lfd, efd);
                continue;
            } else {
                if (is_got_lock)
                    delay_events.push_back(&(events[i]));
                else
                    proc_rw_event(events[i].data.fd);
            }
        }

        if (is_got_lock) {
            sem_post(lock);
            LOG_DEBUG("put lock");
            std::vector<struct epoll_event *>::iterator it;
            for (it = delay_events.begin(); it != delay_events.end(); ++it) {
                proc_rw_event((*it)->data.fd);
                sleep(1);
            }
        }
    }

    free(events);
    close(lfd);

    return 0;
}
