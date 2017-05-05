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
#include <sys/socket.h>
#include <sys/types.h> /* See NOTES */
#include <sys/wait.h>
#include <unistd.h>

const int PROC_NUM = 2;

#define ERREXIT(msg) \
    perror(msg);     \
    exit(-1)

#define SUCEXIT(msg) \
    printf(msg);     \
    exit(0)

#define PARAMERROREXIT(argc, argv) \
    usage(argc, argv);           \
    exit(-1)

struct argset {
    char *addr;
    char *port;
    bool reuseaddr;
    bool reuseport;
    int proc_num;
    argset() : addr(NULL), port("9000"), reuseaddr(false), reuseport(false), proc_num(1) {}
};

void usage(int argc, char ** argv){
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
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1){
        ERREXIT("set reuseaddr failed!");
    }
}

void set_reuseport(int lfd) {
    int enable = 1;
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) == -1){
        ERREXIT("set reuseport failed!");
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

    int client_num = 0;
    while (1) {
        int connfd = accept(lfd, (struct sockaddr *) NULL, NULL);
        if (connfd == -1) {
            ERREXIT("call accept failed!");
        }
        char *pbuf = new char[1024];
        memset(pbuf, 0, 1024);
        snprintf(pbuf, 1024, "[%d] accept a new client\t [%d]\n", getpid(), ++client_num);

        printf(pbuf);
        send(connfd, pbuf, strlen(pbuf) + 1, 0);

        close(connfd);
    }

    return 0;
}
