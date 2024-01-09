#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/random.h>


/*

 Assigment 5

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

/*
 Continuation of Assignment 5:

    Modify your socket based program to accept multiple simultaneous connections, with each connection spawning
    a new thread to handle the connection.

        a. Writes to /var/tmp/aesdsocketdata should be synchronized between threads using a mutex, to ensure data
        written by synchronous connections is not intermixed, and not relying on any file system synchronization.

                i. For instance, if one connection writes “12345678” and another connection writes “abcdefg” it
                should not be possible for the resulting /var/tmp/aesdsocketdata file to contain a mix like
                “123abcdefg456”, the content should always be “12345678”, followed by “abcdefg”.  However for
                any simultaneous connections, it's acceptable to allow packet writes to occur in any order in the
                socketdata file.

        b. The thread should exit when the connection is closed by the client or when an error occurs in the send
        or receive steps.

        c. Your program should continue to gracefully exit when SIGTERM/SIGINT is received, after requesting an
        exit from each thread and waiting for threads to complete execution.

        d. Use the singly linked list APIs discussed in the video (or your own implementation if you prefer) to
        manage threads.

                i. Use pthread_join() to join completed threads, do not use detached threads for this assignment.

2. Modify your aesdsocket source code repository to:

        a. Append a timestamp in the form “timestamp:time” where time is specified by the RFC 2822 compliant
        strftime format

, followed by newline.  This string should be appended to the /var/tmp/aesdsocketdata file every 10 seconds,
 where the string includes the year, month, day, hour (in 24 hour format) minute and second representing the
 system wall clock time.

        b. Use appropriate locking to ensure the timestamp is written atomically with respect to socket data

        Hint:

	        Think where should the timer be initialized. Should it be initialized in parent or child?

3. Use the updated sockettest.sh script (in the assignment-autotest/test/assignment6 subdirectory) . You can run
 this manually outside the `./full-test.sh` script by:

        a. Starting your aesdsocket application

        b. Executing the sockettest.sh script from the assignment-autotest subdirectory.

        c. Stopping your aesdsocket application.

4. The `./full-test.sh` script in your aesd-assignments repository should now complete successfully.

5. Tag the assignment with “assignment-<assignment number>-complete” once the final commit is pushed onto your
 repository. The instructions to add a tag can be found here

*/

/* TODO
 * - thread signal killing
 * - RECV_BUFF_SIZE + READ_BUFF_SIZE cosilidation
 * -
*/

#define SOCKET_FAIL -1
#define RET_OK 0

#define RECV_BUFF_SIZE 1024
#define READ_BUFF_SIZE 1024

#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"

typedef struct DataFileStruct {
    char *pcFilePath;
    FILE *pFile;
    pthread_mutex_t pMutex;
} DataFileStruct;

DataFileStruct sDataFile = {
        .pcFilePath = DATA_FILE_PATH,
        .pFile = NULL,
        .pMutex = PTHREAD_MUTEX_INITIALIZER
};

typedef struct sClient {
    int32_t sockfd;        /* socket clients connected on */
    bool bDone;             /* client disconnected  ? */
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
} sClient;

typedef struct sClientThreadEntry {
    pthread_t sThread;
    sClient sClient;
    long iID;   /* random unique id of the thread*/
    LIST_ENTRY(sClientThreadEntry) entries;
} sClientThreadEntry;

#define BACKLOG 10
char *pcPort = "9000";
int32_t sfd = 0;

/* TODO, make it work */
bool bTerminateProg = false;

/* completing any open connection operations,
 * closing any open sockets, and deleting the file /var/tmp/aesdsocketdata*/
void exit_cleanup(void) {

    /* Cleanup with reentrant functions only */

    /* Remove datafile */
    if (sDataFile.pFile != NULL) {
        close(fileno(sDataFile.pFile));
        sDataFile.pFile = NULL;
        unlink(sDataFile.pcFilePath);
    }

    /* Close socket */
    if (sfd > 0) {
        close(sfd);
        sfd = 0;
    }

    /* TODO*/
    /* Close socket */
//    if (sockfd > 0) {
//        close(sockfd);
//        sockfd = 0;
//    }
}

/* Signal actions with cleanup */
//454 pthread_sigmask
void sig_handler(int32_t signo, siginfo_t *info, void *context) {

    if (signo != SIGINT && signo != SIGTERM) {
        return;
    }

    syslog(LOG_INFO, "Got signal: %d", signo);
    bTerminateProg = true;
}

void do_exit(int32_t exitval) {
    exit_cleanup();
    exit(exitval);
}

/* Description:
 * Setup signals to catch
 *
 * Return:
 * - errno on error
 * - RET_OK when succeeded
 */
int32_t setup_signals(void) {

    /* TODO ,threading ? */

    /* SIGINT or SIGTERM terminates the program with cleanup */
    struct sigaction sSigAction = {0};
    sSigAction.sa_sigaction = &sig_handler;

    if (sigaction(SIGINT, &sSigAction, NULL) != 0) {
        perror("Setting up SIGINT");
        return errno;
    }

    if (sigaction(SIGTERM, &sSigAction, NULL) != 0) {
        perror("Setting up SIGTERM");
        return errno;
    }

    return RET_OK;
}

/* Description:
 * Setup datafile to use, including mutex
 *
 * Return:
 * - errno on error
 * - RET_OK when succeeded
 */
int32_t setup_datafile(DataFileStruct *psDataFile) {

    if ((psDataFile->pFile = fopen(psDataFile->pcFilePath, "w+")) == NULL) {
        perror("fopen: %s");
        printf("Error opening: %s", psDataFile->pcFilePath);
        return errno;
    }

    return RET_OK;
}

/* Description:
 * Setup socket handling
 * https://beej.us/guide/bgnet/html/split/system-calls-or-bust.html#system-calls-or-bust
 *
 * Return:
 * - errno on error
 * - RET_OK when succeeded
 */
int32_t setup_socket(void) {

    struct addrinfo hints = {0};
    struct addrinfo *servinfo = NULL;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // bind to all interfaces

    if ((getaddrinfo(NULL, pcPort, &hints, &servinfo)) != 0) {
        perror("getaddrinfo");
        return errno;
    }

    if ((sfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) < 0) {
        perror("socket");
        return errno;
    }

    // lose the pesky "Address already in use" error message
    int32_t yes = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
        perror("setsockopt");
        return errno;
    }

    if (bind(sfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
        perror("bind");
        return errno;
    }

    /* servinfo not needed anymore */
    freeaddrinfo(servinfo);

    if (listen(sfd, BACKLOG) < 0) {
        perror("listen");
        return errno;
    }

    return RET_OK;
}

/* Description:
 * Send complete file through socket to the client, threadsafe
 *
 * Return:
 * - errno on error
 * - RET_OK when succeeded
 */
int32_t file_send(int32_t sockfd, DataFileStruct *psDataFile) {

    int32_t iRet;

    if (pthread_mutex_lock(&psDataFile->pMutex) != 0) {
        perror("pthread_mutex_lock");
        return errno;
    }

    /* Send complete file */
    if (fseek(psDataFile->pFile, 0, SEEK_SET) != 0) {
        perror("fseek");
        iRet = errno;
        goto exit;
    }

    char acReadBuff[READ_BUFF_SIZE];
    while (!feof(psDataFile->pFile)) {
        //NOTE: fread will return nmemb elements
        //NOTE: fread does not distinguish between end-of-file and error,
        int32_t iRead = fread(acReadBuff, 1, sizeof(acReadBuff), psDataFile->pFile);
        if (ferror(psDataFile->pFile) != 0) {
            perror("read");
            iRet = errno;
            goto exit;
        }

        if (send(sockfd, acReadBuff, iRead, 0) < 0) {
            perror("send");
            iRet = errno;
            goto exit;
        }
    }

    iRet = RET_OK;

    exit:
    if (pthread_mutex_unlock(&psDataFile->pMutex) != 0) {
        perror("pthread_mutex_unlock");
        iRet = errno;
    }

    return iRet;
}

/* Description:
 * Write buff with size to datafile, threadsafe
 *
 * Return:
 * - errno on error
 * - RET_OK when succeeded
 */
int32_t file_write(DataFileStruct *psDataFile, void *buff, int32_t size) {

    pthread_mutex_lock(&psDataFile->pMutex);
    int32_t iRet;

    /* Append received data */
    fseek(psDataFile->pFile, 0, SEEK_END);
    fwrite(buff, size, 1, psDataFile->pFile);
    if (ferror(psDataFile->pFile) != 0) {
        perror("write");
        iRet = errno;
    } else {
        iRet = RET_OK;
    }

    pthread_mutex_unlock(&psDataFile->pMutex);

    return iRet;
}

int32_t daemonize(void) {

    umask(0);

    pid_t pid;
    if ((pid = fork()) < 0) {
        perror("fork");
        return errno;
    } else if (pid != 0) {
        /* Exit parent */
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        perror("chdir");
        return errno;
    };

    if (chdir("/") < 0) {
        perror("chdir");
        return errno;
    };

    int32_t fd0, fd1, fd2;
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);

    return RET_OK;
}

void *client_serve(void *arg) {

    sClient *psClient = (sClient *) arg;

    /* Get IP connecting client */
    struct sockaddr_in *sin = (struct sockaddr_in *) &psClient->their_addr;
    unsigned char *ip = (unsigned char *) &sin->sin_addr.s_addr;
    syslog(LOG_DEBUG, "Accepted connection from %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);

    /* Keep receiving data until error or disconnect*/
    int32_t iReceived = 0;
    int32_t iRet = 0;
//    char acRecvBuff[RECV_BUFF_SIZE];        //TODO
    char *acRecvBuff = malloc(RECV_BUFF_SIZE);

    while (1) {
        iReceived = recv(psClient->sockfd, acRecvBuff, RECV_BUFF_SIZE, 0);
        if (iReceived < 0) {
            perror("recv");
//            pthread_exit((void *)errno);
            pthread_exit((void *) -1);
        } else if (iReceived == 0) {
            close(psClient->sockfd);
            syslog(LOG_DEBUG, "Connection closed from %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
            break;
        } else if (iReceived > 0) {
            char *pcEnd = NULL;
            pcEnd = strstr(acRecvBuff, "\n");
            if (pcEnd == NULL) {
                /* not end of message, write all */
                if ((iRet = file_write(&sDataFile, acRecvBuff, iReceived)) != 0) {
//                    pthread_exit((void *)iRet);
                    pthread_exit((void *) -1);
                }
            } else {
                /* end of message detected, write until message end */

                // NOTE: Ee know that message end is in the buffer, so +1 here is allowed to
                // also get the end of message '\n' in the file.
                if ((iRet = file_write(&sDataFile, acRecvBuff, (int32_t) (pcEnd - acRecvBuff + 1))) != 0) {
//                    pthread_exit((void *)iRet);
                    pthread_exit((void *) -1);
                }

                if ((iRet = file_send(psClient->sockfd, &sDataFile)) != 0) {
//                    pthread_exit((void *)iRet);
                    pthread_exit((void *) -1);
                }
            }
        }
    }

    free(acRecvBuff);
    psClient->bDone = true;
//    pthread_exit((void *)iRet);
    pthread_exit((void *) 0);
}

int32_t main(int32_t argc, char **argv) {

    bool bDeamonize = false;
    int32_t iRet = 0;

    /* init syslog */
    openlog(NULL, 0, LOG_USER);

    if ((argc > 1) && strncmp(argv[0], "-d", 2) == 0) {
        bDeamonize = true;
    }

    if ((iRet = setup_signals()) != RET_OK) {
        do_exit(iRet);
    }

    if ((iRet = setup_datafile(&sDataFile)) != RET_OK) {
        do_exit(iRet);
    }

    /* Opens a stream socket, failing and returning -1 if any of the socket connection steps fail. */
    if (setup_socket() != RET_OK) {
        do_exit(SOCKET_FAIL);
    }

    if (bDeamonize) {
        printf("Demonizing, listening on port %s\n", pcPort);
        if ((iRet = daemonize() != 0)) {
            do_exit(iRet);
        }
    } else {
        printf("Waiting for connections...\n");
    }

    /* Create list */
    LIST_HEAD(listhead, sClientThreadEntry);
    struct listhead head;
    LIST_INIT (&head);
    sClientThreadEntry *np = NULL;

    /* List housekeeping */
//    sClientThreadEntry *tail = head;

    /* Keep receiving clients */
    while (1) {

        /* TODO
         * block accepting connections
         *  wait for thread to complete and then exit
        */

        /* Pre-prepare list item */
        sClientThreadEntry *psClientThreadEntry = NULL;
        if ((psClientThreadEntry = malloc(sizeof(sClientThreadEntry))) == NULL) {
            perror("malloc");
            /*TODO*/
            exit(errno);
        }

        /* TODO, make non blocking */
        /* Accept clients */
        do {
            if ((psClientThreadEntry->sClient.sockfd = accept(sfd,
                                                              (struct sockaddr *) &psClientThreadEntry->sClient.their_addr,
                                                              &psClientThreadEntry->sClient.addr_size)) < 0) {
                if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                    usleep(100 * 1000);
                    continue;
                }

                /*TODO */
                perror("accept");
                exit(1);
            }
        } while (psClientThreadEntry->sClient.sockfd < 0 );


        /* Insert head */
        LIST_INSERT_HEAD(&head, psClientThreadEntry, entries);

        /* Add random ID for tracking */
        psClientThreadEntry->iID = random();
        printf("Spinning up client: %lu\n", psClientThreadEntry->iID);

        /* Add client info */
        psClientThreadEntry->sClient.bDone = false;

        /* Spawn new thread */
        pthread_create(&psClientThreadEntry->sThread, NULL, client_serve, &psClientThreadEntry->sClient);

        /* Loop list, see if done */
        /* remove from list */
        LIST_FOREACH(np, &head, entries) {
            if (np->sClient.bDone == true) {
                printf("Done with client: %lu\n", np->iID);
                pthread_join(np->sThread, NULL);
                free(np);
                LIST_REMOVE(np, entries);
            }
        }

        /* Wait for threads to complete */
        if (bTerminateProg == true) {
            break;
        }

    }

    exit_cleanup();

    do_exit(RET_OK);
}