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
#include <sys/resource.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/random.h>
#include <time.h>

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

#define SOCKET_FAIL -1
#define RET_OK 0

/* socket send and recv buffers, per client */
#define RECV_BUFF_SIZE 1024     /* bytes */
#define SEND_BUFF_SIZE 1024     /* bytes */

/* Timpestamp interval */
#define TIMESTAMP_INTERVAL 10 /* seconds */

/* Global datafile */
#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"

typedef struct DataFile {
    char *pcFilePath;       /* path */
    FILE *pFile;               /* filehandle */
    pthread_mutex_t pMutex;     /* thread safe datafile access */
} sDataFile;

sDataFile sGlobalDataFile = {
        .pcFilePath = DATA_FILE_PATH,
        .pFile = NULL,
        .pMutex = PTHREAD_MUTEX_INITIALIZER
};

/* Connected client info */
typedef struct sClient {
    int32_t iSockfd;         /* socket clients connected on */
    bool bIsDone;             /* client disconnected  ? */
    sDataFile *psDataFile;  /* global data file to use*/
    struct sockaddr_storage sTheirAddr;
    socklen_t tAddrSize;
    char acSendBuff[SEND_BUFF_SIZE];    /* data sending */
    char acRecvBuff[RECV_BUFF_SIZE];    /* data reception */
} sClient;

/* List struct for client thread tracking */
typedef struct sClientThreadEntry {
    pthread_t sThread;      /* pthread info */
    sClient sClient;        /* connected client information */
    long lID;               /* random unique id of the thread*/
    LIST_ENTRY(sClientThreadEntry) sClientThreadEntry;
} sClientThreadEntry;

#define BACKLOG 10
#define PORT "9000"

int32_t iSfd = 0;

bool bTerminateProg = false;

timer_t g_timer_id;

/* Thread list for clients connections
 * List actions thread safe with 'ListMutex'
 * */
LIST_HEAD(listhead, sClientThreadEntry);
struct listhead sClientThreadListHead;
pthread_mutex_t ListMutex = PTHREAD_MUTEX_INITIALIZER;

/* Loop the thread list, thread safe.
 * Cleanup thread sClientThreadEntry that are not serving clients anymore
 * optionally return piStillActive
 */
static int32_t cleanup_client_list(int32_t *piStillActive) {

    int32_t iCount = 0;
    sClientThreadEntry *curr, *next = NULL;

    /* Note: when looping the list, and something needs to be removed, then start the
     * list loop again to prevent pointer errors and memory leaks */
    int iSomethingRemoved;
    do {
        iSomethingRemoved = 0;

        if (pthread_mutex_lock(&ListMutex) != 0) {
            return errno;
        }

        LIST_FOREACH(curr, &sClientThreadListHead, sClientThreadEntry) {
            if (curr->sClient.bIsDone == true) {
                pthread_join(curr->sThread, NULL);
                printf("Done with client thread: %lu\n", curr->lID);
                LIST_REMOVE(curr, sClientThreadEntry);
                free(curr);
                iSomethingRemoved++;
                break; /* loop list again */
            } else {
                iCount++;   /* count still active clients, to be returned to the caller */
            }
        }

        if (pthread_mutex_unlock(&ListMutex) != 0) {
            return errno;
        }

    } while (iSomethingRemoved);

    /* Optionally return amount of still connected clients */
    if (piStillActive != NULL) {
        *piStillActive = iCount;
    }

    return RET_OK;
}

/* completing any open connection operations,
 * closing any open sockets, and deleting the file /var/tmp/aesdsocketdata*/
static void exit_cleanup(void) {

    /* Wait for all clients to finish */
    int32_t iCount;
    do {
        cleanup_client_list(&iCount);
        (iCount > 0) ? usleep(100 * 1000) : (0);
    } while (iCount);

    /* Remove datafile */
    if (sGlobalDataFile.pFile != NULL) {
        close(fileno(sGlobalDataFile.pFile));
        unlink(sGlobalDataFile.pcFilePath);
    }

    /* Close socket */
    if (iSfd > 0) {
        close(iSfd);
    }

    /* No more syslog needed */
    closelog();
}

/* Signal actions with cleanup */
void sig_handler(const int ciSigno) {

    if (ciSigno != SIGINT && ciSigno != SIGTERM) {
        return;
    }

    syslog(LOG_INFO, "Got signal: %d", ciSigno);

    bTerminateProg = true;
}

static void do_exit(const int32_t ciExitval) {
    exit_cleanup();

    printf("Goodbye!\n");

    exit(ciExitval);
}

static void do_exit_with_errno(int32_t iLine, const int32_t ciErrno) {
    fprintf(stderr, "Exit with %d: %s. Line %d.\n", ciErrno, strerror(ciErrno), iLine);

    do_exit(ciErrno);
}

/* Description:
 * Setup signals to catch
 *
 * Return:
 * - errno on error
 * - RET_OK when succeeded
 */
static int32_t setup_signals(void) {

    /* SIGINT or SIGTERM terminates the program with cleanup */
//    struct sigaction sSigAction = {0};

    struct sigaction sSigAction;

    sigemptyset(&sSigAction.sa_mask);

    sSigAction.sa_flags = 0;
    sSigAction.sa_handler = sig_handler;

    if (sigaction(SIGINT, &sSigAction, NULL) != 0) {
        return errno;
    }

    if (sigaction(SIGTERM, &sSigAction, NULL) != 0) {
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
static int32_t setup_datafile(sDataFile *psDataFile) {

    if ((psDataFile->pFile = fopen(psDataFile->pcFilePath, "w+")) == NULL) {
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
static int32_t setup_socket(void) {

    struct addrinfo sHints = {0};
    struct addrinfo *psServinfo = NULL;

    memset(&sHints, 0, sizeof(sHints)); // make sure the struct is empty
    sHints.ai_family = AF_INET;
    sHints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    sHints.ai_flags = AI_PASSIVE;     // bind to all interfaces

    if ((getaddrinfo(NULL, PORT, &sHints, &psServinfo)) != 0) {
        return errno;
    }

    if ((iSfd = socket(psServinfo->ai_family, psServinfo->ai_socktype, psServinfo->ai_protocol)) < 0) {
        return errno;
    }

    // lose the pesky "Address already in use" error message
    int32_t iYes = 1;
    if (setsockopt(iSfd, SOL_SOCKET, SO_REUSEADDR, &iYes, sizeof iYes) == -1) {
        return errno;
    }

    if (bind(iSfd, psServinfo->ai_addr, psServinfo->ai_addrlen) < 0) {
        return errno;
    }

    /* psServinfo not needed anymore */
    freeaddrinfo(psServinfo);

    if (listen(iSfd, BACKLOG) < 0) {
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
static int32_t file_send(sClient *psClient, sDataFile *psDataFile) {

    int32_t iRet;

    if (pthread_mutex_lock(&psDataFile->pMutex) != 0) {
        return errno;
    }

    /* Send complete file */
    if (fseek(psDataFile->pFile, 0, SEEK_SET) != 0) {
        iRet = errno;
        goto exit;
    }

    while (!feof(psDataFile->pFile)) {
        //NOTE: fread will return nmemb elements
        //NOTE: fread does not distinguish between end-of-file and error,
        int32_t iRead = fread(psClient->acSendBuff, 1, sizeof(psClient->acSendBuff), psDataFile->pFile);
        if (ferror(psDataFile->pFile) != 0) {
            iRet = errno;
            goto exit;
        }

        if (send(psClient->iSockfd, psClient->acSendBuff, iRead, 0) < 0) {
            iRet = errno;
            goto exit;
        }
    }

    iRet = RET_OK;

    exit:
    if (pthread_mutex_unlock(&psDataFile->pMutex) != 0) {
        iRet = errno;
    }

    return iRet;
}

/* Description:
 * Write cvpBuff with ciSize to datafile, threadsafe
 *
 * Return:
 * - errno on error
 * - RET_OK when succeeded
 */
static int32_t file_write(sDataFile *psDataFile, const void *cvpBuff, const int32_t ciSize) {

    if (pthread_mutex_lock(&psDataFile->pMutex) != 0) {
        return errno;
    }

    /* Append received data */
    fseek(psDataFile->pFile, 0, SEEK_END);
    fwrite(cvpBuff, ciSize, 1, psDataFile->pFile);
    if (ferror(psDataFile->pFile) != 0) {
        return errno;
    }

    /* Make sure data ends up in the global datafile, otherwise the unitest could fail */
    if (fflush(psDataFile->pFile) != 0) {
        return errno;
    }

    if (pthread_mutex_unlock(&psDataFile->pMutex) != 0) {
        return errno;
    }

    return RET_OK;
}

static int32_t daemonize(void) {

    /* Clear file creation mask */
    umask(0);

    /* Get fd limts for later */
    struct rlimit sRlim;
    if (getrlimit(RLIMIT_NOFILE, &sRlim) < 0) {
        fprintf(stderr, "Can't get file limit. Line %d.\n", __LINE__);
        do_exit(1);
    }

    pid_t pid;
    if ((pid = fork()) < 0) {
        return errno;
    } else if (pid != 0) {
        /* Exit parent */
        exit(EXIT_SUCCESS);
    }

    /* Close all fd's */
    if (sRlim.rlim_max == RLIM_INFINITY) {
        sRlim.rlim_max = 1024;
    } else {
        int i;
        for (i = 0; i < sRlim.rlim_max; i++) {
            close(i);
        }
    }

    if (setsid() < 0) {
        return errno;
    };

    if (chdir("/") < 0) {
        return errno;
    };

    int32_t fd0, fd1, fd2;
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);

    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        fprintf(stderr, "Error setting up file descriptors. Line %d.\n", __LINE__);
        do_exit(1);
    }

    /* init syslog */
    openlog(NULL, 0, LOG_USER);

    return RET_OK;
}

static void *client_serve(void *arg) {

    sClient *psClient = (sClient *) arg;

    /* Get IP connecting client */
    struct sockaddr_in *sin = (struct sockaddr_in *) &psClient->sTheirAddr;
    unsigned char *ip = (unsigned char *) &sin->sin_addr.s_addr;
    syslog(LOG_DEBUG, "Accepted connection from %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);

    /* Keep receiving data until error or disconnect*/
    int32_t iReceived = 0;
    int32_t iRet = 0;

    while (1) {
        iReceived = recv(psClient->iSockfd, psClient->acRecvBuff, RECV_BUFF_SIZE, 0);
        if (iReceived < 0) {
            pthread_exit((void *) errno);
        } else if (iReceived == 0) {
            /* This is the only way a client can disconnect */

            close(psClient->iSockfd);
            syslog(LOG_DEBUG, "Connection closed from %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);

            /* Signal housekeeping */
            psClient->bIsDone = true;

            pthread_exit((void *) RET_OK);

        } else if (iReceived > 0) {
            char *pcEnd = NULL;
            pcEnd = strstr(psClient->acRecvBuff, "\n");
            if (pcEnd == NULL) {
                /* not end of message, write all */
                if ((iRet = file_write(psClient->psDataFile, psClient->acRecvBuff, iReceived)) != 0) {
                    pthread_exit((void *) iRet);
                }
            } else {
                /* end of message detected, write until message end */

                // NOTE: Ee know that message end is in the buffer, so +1 here is allowed to
                // also get the end of message '\n' in the file.
                if ((iRet = file_write(psClient->psDataFile, psClient->acRecvBuff,
                                       (int32_t) (pcEnd - psClient->acRecvBuff + 1))) != 0) {
                    pthread_exit((void *) iRet);
                }

                if ((iRet = file_send(psClient, psClient->psDataFile)) != 0) {
                    pthread_exit((void *) iRet);
                }
            }
        }
    }

    pthread_exit((void *) iRet);
}

/* handle finished client */
static void *housekeeping(void *arg) {

    while (1) {

        /* Loop list, see if a thread is done. When it is then remove from the list */
        cleanup_client_list(NULL);

        usleep(100 * 1000);

        /* Found a exit signal */
        if (bTerminateProg == true) {
            pthread_exit((void *) 0);
        }
    }
}

/* Write a RFC 2822 timestring to global data file */
static void timestamp(const union sigval arg) {

    int iRet;
    char acTime[50];
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);

    strftime(acTime, sizeof(acTime), "timestamp:%a, %d %b %Y %T %z\n", tmp);

    if ((iRet = file_write(&sGlobalDataFile, acTime, strlen(acTime))) != RET_OK) {
        do_exit_with_errno(__LINE__, iRet);
    }

}

static int32_t setup_timer(const int32_t ciPeriod, FILE *const cpFile) {

    struct itimerspec sTs;
    struct sigevent sSE;

    sSE.sigev_notify = SIGEV_THREAD;
    sSE.sigev_value.sival_ptr = cpFile;
    sSE.sigev_notify_function = timestamp;
    sSE.sigev_notify_attributes = NULL;

    sTs.it_value.tv_sec = ciPeriod;
    sTs.it_value.tv_nsec = 0;
    sTs.it_interval.tv_sec = ciPeriod;
    sTs.it_interval.tv_nsec = 0;

    if (timer_create(CLOCK_REALTIME, &sSE, &g_timer_id) == -1) {
        return errno;
    }

    if (timer_settime(g_timer_id, 0, &sTs, 0) == -1) {
        return errno;
    }

    return RET_OK;
}

int32_t main(int32_t argc, char **argv) {

    bool bDeamonize = false;
    int32_t iRet = 0;

    if ((argc > 1) && strcmp(argv[0], "-d")) {
        bDeamonize = true;
    }

    /* Going to run as service or not > */
    if (bDeamonize) {
        printf("Demonizing, listening on port %s\n", PORT);
        if ((iRet = daemonize() != 0)) {
            do_exit_with_errno(__LINE__, errno);
        }
    }

    if ((iRet = setup_signals()) != RET_OK) {
        do_exit_with_errno(__LINE__, errno);
    }

    if ((iRet = setup_datafile(&sGlobalDataFile)) != RET_OK) {
        do_exit_with_errno(__LINE__, errno);
    }

    /* Opens a stream socket, failing and returning -1 if any of the socket connection steps fail. */
    if ((iRet = setup_socket()) != RET_OK) {
        fprintf(stderr, "Exit with %d: %s. Line %d.\n", iRet, strerror(iRet), __LINE__);
        do_exit(SOCKET_FAIL);
    }

    if (!bDeamonize) {
        printf("Waiting for connections...\n");
    }

    /* Init the client thread list */
    LIST_INIT (&sClientThreadListHead);

    /* spinup housekeeping thread to handle finished client connections */
    pthread_t Cleanup;
    if (pthread_create(&Cleanup, NULL, housekeeping, NULL) != 0) {
        do_exit_with_errno(__LINE__, errno);
    }

    /* spinup timestamp timer */
    if ((iRet = setup_timer(TIMESTAMP_INTERVAL, sGlobalDataFile.pFile)) != RET_OK) {
        do_exit_with_errno(__LINE__, errno);
    }

    /* Keep receiving clients */
    while (1) {

        /* Pre-prepare list item */
        sClientThreadEntry *psClientThreadEntry = NULL;
        if ((psClientThreadEntry = malloc(sizeof(sClientThreadEntry))) == NULL) {
            do_exit_with_errno(__LINE__, errno);
        }

        /* Link global data file */
        psClientThreadEntry->sClient.psDataFile = &sGlobalDataFile;

        psClientThreadEntry->sClient.tAddrSize = sizeof(psClientThreadEntry->sClient.sTheirAddr);
        psClientThreadEntry->sClient.bIsDone = false;

        /* Add random ID for tracking */
        psClientThreadEntry->lID = random();

        /* Accept clients, and fill client information struct */
        if ((psClientThreadEntry->sClient.iSockfd = accept(iSfd,
                                                           (struct sockaddr *) &psClientThreadEntry->sClient.sTheirAddr,
                                                           &psClientThreadEntry->sClient.tAddrSize)) < 0) {
            do_exit_with_errno(__LINE__, errno);
        }

        printf("Spinning up client thread: %lu\n", psClientThreadEntry->lID);

        /* Insert client thread tracking on list sClientThreadListHead */
        if (pthread_mutex_lock(&ListMutex) != 0) {
            do_exit_with_errno(__LINE__, errno);
        }

        LIST_INSERT_HEAD(&sClientThreadListHead, psClientThreadEntry, sClientThreadEntry);
        if (pthread_mutex_unlock(&ListMutex) != 0) {
            do_exit_with_errno(__LINE__, errno);
        }

        /* Spawn new thread and serve the client */
        if (pthread_create(&psClientThreadEntry->sThread, NULL, client_serve, &psClientThreadEntry->sClient) < 0) {
            do_exit_with_errno(__LINE__, errno);
        }

        /* Found a exit signal */
        if (bTerminateProg == true) {
            break;
        }
    }

    do_exit(RET_OK);
}