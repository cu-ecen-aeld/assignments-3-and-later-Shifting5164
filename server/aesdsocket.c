#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>


/*
2. Create a socket based program with name aesdsocket in the “server” directory which:

     a. Is compiled by the “all” and “default” target of a Makefile in the “server” directory and supports cross
     compilation, placing the executable file in the “server” directory and named aesdsocket.
     b. Opens a stream socket bound to port 9000, failing and returning -1 if any of the socket connection steps fail.
     c. Listens for and accepts a connection
     d. Logs message to the syslog “Accepted connection from xxx” where XXXX is the IP address of the connected client.
     e. Receives data over the connection and appends to file /var/tmp/aesdsocketdata, creating this file if it doesn’t
     exist.
        - Your implementation should use a newline to separate data packets received.  In other words a packet is
        considered complete when a newline character is found in the input receive stream, and each newline should
        result in an append to the /var/tmp/aesdsocketdata file.
        - You may assume the data stream does not include null characters (therefore can be processed using string
        handling functions).
        - You may assume the length of the packet will be shorter than the available heap size.  In other words, as
        long as you handle malloc() associated failures with error messages you may discard associated over-length
        packets.
     f. Returns the full content of /var/tmp/aesdsocketdata to the client as soon as the received data packet completes.
            - You may assume the total size of all packets sent (and therefore size of /var/tmp/aesdsocketdata) will
            be less than the size of the root filesystem, however you may not assume this total size of all packets
            sent will be less than the size of the available RAM for the process heap.
     g. Logs message to the syslog “Closed connection from XXX” where XXX is the IP address of the connected client.
     h. Restarts accepting connections from new clients forever in a loop until SIGINT or SIGTERM is received
     (see below).
     i. Gracefully exits when SIGINT or SIGTERM is received, completing any open connection operations, closing any
     open sockets, and deleting the file /var/tmp/aesdsocketdata.
*/

#define SOCKET_FAIL -1

char *pcDataFilePath = "/var/tmp/aesdsocketdata";
FILE *pfDataFile = NULL;

#define BACKLOG 10
char *pcPort = "9000";
struct addrinfo *servinfo = NULL;
int sfd = 0;
int sockfd = 0;

#define RECV_BUFF_SIZE 500
#define READ_BUFF_SIZE 500

/* completing any open connection operations, closing any open sockets, and deleting the file /var/tmp/aesdsocketdata*/
void exit_cleanup(void) {

    /* Cleanup with reentrant functions */

    /* Remove datafile */
    if (pfDataFile != NULL) {
        close(fileno(pfDataFile));
        unlink(pcDataFilePath);
    }

    /* Close socket */
    // todo, not needed
//    if (sfd > 0) {
//        close(sfd);
//    }

    /* Close socket */
    if (sockfd > 0) {
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
    }
}

/* Signal actions with cleanup */
void sig_handler(int signo, siginfo_t *info, void *context) {
    syslog(LOG_INFO, "Got signal: %d", signo);
    exit_cleanup();
    exit(EXIT_SUCCESS);
}

void do_exit(int exitval) {
    exit_cleanup();
    exit(exitval);
}

int setup_signals(void) {

    /* SIGINT or SIGTERM terminates the program with cleanup */
    struct sigaction sSigAction = {0};
    sSigAction.sa_sigaction = &sig_handler;
    if (sigaction(SIGINT, &sSigAction, NULL) != 0) {
        perror("Setting up SIGINT");
        return (-1);
    }
    if (sigaction(SIGTERM, &sSigAction, NULL) != 0) {
        perror("Setting up SIGTERM");
        return (-1);
    }

    return (1);
}

int setup_datafile(void) {
    /* Create and open destination file */

    if ((pfDataFile = fopen(pcDataFilePath, "w+")) == NULL) {
        perror("fopen: %s");
        printf("Error opening: %s", pcDataFilePath);
        return (-1);
    }

    return (1);
}

/* https://beej.us/guide/bgnet/html/split/system-calls-or-bust.html#system-calls-or-bust */
int setup_socket(void) {

    struct addrinfo hints;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = INADDR_ANY;     // bind to all interfaces

    if ((getaddrinfo(NULL, pcPort, &hints, &servinfo)) != 0) {
        perror("getaddrinfo");
        return SOCKET_FAIL;
    }

    if ((sfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) < 0) {
        perror("socket");
        return SOCKET_FAIL;
    }

    if (bind(sfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
        perror("bind");
        return SOCKET_FAIL;
    }

    // realease servinfo ?


    if (listen(sfd, BACKLOG) < 0) {
        perror("listen");
        return SOCKET_FAIL;
    }

    return (EXIT_SUCCESS);
}

void file_send(void) {
    /* Send complete file */
    fseek(pfDataFile, 0, SEEK_SET);
    char acReadBuff[READ_BUFF_SIZE];
    while (!feof(pfDataFile)) {
        //NOTE: fread will return nmemb elements
        //NOTE: does not distinguish between end-of-file and error,
        int iRead = fread(acReadBuff, 1, sizeof(acReadBuff), pfDataFile);
        if (ferror(pfDataFile) != 0) {
            perror("read");
            do_exit(EXIT_FAILURE);
        }

//                    printf("read: %d\n", iRead); //DEBUG

        // #########################################################
        // DEBUG
        // #########################################################
//                    char acSendBuff2[60];
//                    snprintf(acSendBuff2, sizeof(acSendBuff2), "%s", acSendBuff);
//                    /* Local echo for testing*/
//                    printf("debugout: %s\n", acSendBuff2);
//                    send(sockfd, "Got Data\n", 9, 0); /* TEST*/
        // #########################################################

        int iSend;
        if ((iSend = send(sockfd, acReadBuff, iRead, 0)) < 0) {
            perror("send");
            do_exit(EXIT_FAILURE);
        }

//                    printf("send: %d\n", iSend); //DEBUG
//                    printf("--------------------------"); //DEBUG

    }
}

void file_write(void *buff, int size) {
    /* Append received data */
    fseek(pfDataFile, 0, SEEK_END);
    fwrite(buff, size, 1, pfDataFile);
    if (ferror(pfDataFile) != 0) {
        perror("write");
        do_exit(EXIT_FAILURE);
    }
}

int main(int argc, char **argv) {

    /* init syslog */
    openlog(NULL, 0, LOG_USER);

    if (setup_signals() < 0) {
        do_exit(EXIT_FAILURE);
    }

    if (setup_datafile() < 0) {
        do_exit(EXIT_FAILURE);
    }

    /* Opens a stream socket, failing and returning -1 if any of the socket connection steps fail. */
    if (setup_socket() < 0) {
        do_exit(SOCKET_FAIL);
    }

    printf("server: waiting for connections...\n");

    /* Keep receiving clients */
    while (1) {

        /* Accept clients */
        struct sockaddr_storage their_addr;
        socklen_t addr_size;
        if ((sockfd = accept(sfd, (struct sockaddr *) &their_addr, &addr_size)) < 0) {
            perror("accept");
            continue;
        }

        /* Get IP connecting client */
        struct sockaddr_in *sin = (struct sockaddr_in *) &their_addr;
        unsigned char *ip = (unsigned char *) &sin->sin_addr.s_addr;
        syslog(LOG_DEBUG, "Accepted connection from %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);

        /* Keep receiving data until error or disconnect*/
        int iReceived = 0;
        char acRecvBuff[RECV_BUFF_SIZE];
        while (1) {
            iReceived = recv(sockfd, &acRecvBuff, sizeof(acRecvBuff), 0);
            if (iReceived < 0) {
                perror("recv");
                do_exit(EXIT_FAILURE);
            } else if (iReceived == 0) {
                syslog(LOG_DEBUG, "Connection closed from %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
                close(sockfd);
                break;
            } else if (iReceived > 0) {

                char *pcEnd = NULL;
                pcEnd = strstr(acRecvBuff, "\n" );
                if (pcEnd == NULL) {
                    /* not end of message, write all */
                    file_write(acRecvBuff, iReceived);
                } else {
                    /* end of message detected, write until message end */

                    // NOTE, we know that message end is in the buffer, so +1 here is allowed to
                    // also get the end of message '\n' in the file.
                    file_write(acRecvBuff, (int)(pcEnd - acRecvBuff + 1) );
                    file_send();
                }
            }
        }
    }

    do_exit(EXIT_SUCCESS);

}