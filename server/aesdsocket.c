#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>

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

#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"

typedef struct DataFileStruct {
    char *pcDataFilePath;
    FILE *pfDataFile;
    pthread_mutex_t *pfDataFileMutex;
} DataFileStruct;

DataFileStruct sDataFile = {NULL, NULL, NULL};

#define BACKLOG 10
char *pcPort = "9000";
int sfd = 0;
int sockfd = 0;

#define RECV_BUFF_SIZE 1024
#define READ_BUFF_SIZE 1024

/* completing any open connection operations,
 * closing any open sockets, and deleting the file /var/tmp/aesdsocketdata*/
void exit_cleanup(void) {

    /* Cleanup with reentrant functions only*/

    /* cleanup mutex ? */

    /* Remove datafile */
    if (sDataFile.pfDataFile != NULL) {
        close(fileno(sDataFile.pfDataFile));
        unlink(sDataFile.pcDataFilePath);
    }

    /* Close socket */
    if (sfd > 0) {
        close(sfd);
    }

    /* Close socket */
    if (sockfd > 0) {
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

/* Description:
 * Setup signals to catch
 *
 * Return:
 * - errno on error
 * - RET_OK when succeeded
 */
int setup_signals(void) {

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
 * Setup datafile to use
 *
 * Return:
 * - errno on error
 * - RET_OK when succeeded
 */
int setup_datafile(DataFileStruct *psDataFile) {
    /* Create and open destination file */

    psDataFile->pcDataFilePath = DATA_FILE_PATH;

    if ((sDataFile.pfDataFile = fopen(psDataFile->pcDataFilePath, "w+")) == NULL) {
        perror("fopen: %s");
        printf("Error opening: %s", psDataFile->pcDataFilePath);
        return errno;
    }

    if (pthread_mutex_init(psDataFile->pfDataFileMutex, NULL) != 0 ){
        perror("pthread_mutex_init");
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
int setup_socket(void) {

    struct addrinfo hints;
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
    int yes = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
        perror("setsockopt");
        return errno;
    }

    if (bind(sfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
        perror("bind");
        return errno;
    }

    /* Not needed anymore */
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
int file_send(DataFileStruct *psDataFile) {

    int ret;

    if (pthread_mutex_lock(psDataFile->pfDataFileMutex) != 0){
        perror("pthread_mutex_lock");
        return errno;
    }

    /* Send complete file */
    if ( fseek(psDataFile->pfDataFile, 0, SEEK_SET)  != 0 ){
        perror("fseek");
        ret = errno;
        goto exit;
    }

    char acReadBuff[READ_BUFF_SIZE];
    while (!feof(psDataFile->pfDataFile)) {
        //NOTE: fread will return nmemb elements
        //NOTE: fread does not distinguish between end-of-file and error,
        int iRead = fread(acReadBuff, 1, sizeof(acReadBuff), psDataFile->pfDataFile);
        if (ferror(psDataFile->pfDataFile) != 0) {
            perror("read");
            ret = errno;
            goto exit;
        }

        if (send(sockfd, acReadBuff, iRead, 0) < 0) {
            perror("send");
            ret = errno;
            goto exit;
        }
    }

    ret = RET_OK;

exit:
    if (pthread_mutex_unlock(psDataFile->pfDataFileMutex) != 0){
        perror("pthread_mutex_unlock");
        ret = errno;
    }

    return ret;
}

/* Description:
 * Write buff with size to datafile, threadsafe
 *
 * Return:
 * - errno on error
 * - RET_OK when succeeded
 */
int file_write(DataFileStruct *psDataFile,void *buff, int size) {

    pthread_mutex_lock(psDataFile->pfDataFileMutex);
    int ret;

    /* Append received data */
    fseek(psDataFile->pfDataFile, 0, SEEK_END);
    fwrite(buff, size, 1, psDataFile->pfDataFile);
    if (ferror(psDataFile->pfDataFile) != 0) {
        perror("write");
        ret = errno;
    } else {
        ret = RET_OK;
    }

    pthread_mutex_unlock(psDataFile->pfDataFileMutex);

    return ret;
}

int daemonize(void) {

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

    int fd0, fd1, fd2;
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);

    return RET_OK;
}

int main(int argc, char **argv) {

    int iDeamon = false;
    int iRet = 0;

    /* init syslog */
    openlog(NULL, 0, LOG_USER);

    if ((argc > 1) && strcmp(argv[0], "-d")) {
        iDeamon = true;
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

    if (iDeamon) {
        printf("Demonizing, listening on port %s\n", pcPort);
        if ((iRet = daemonize() != 0)) {
            do_exit(iRet);
        }
    } else {
        printf("Waiting for connections...\n");
    }

    /* Keep receiving clients */
    while (1) {

        /* Accept clients */
        struct sockaddr_storage their_addr;
        socklen_t addr_size;
        if ((sockfd = accept(sfd, (struct sockaddr *) &their_addr, &addr_size)) < 0) {
            perror("accept");
            sleep(1);
            continue;
        }

        /* TODO spawn new thread */

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
                do_exit(errno);
            } else if (iReceived == 0) {
                close(sockfd);
                syslog(LOG_DEBUG, "Connection closed from %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
                break;
            } else if (iReceived > 0) {

                char *pcEnd = NULL;
                pcEnd = strstr(acRecvBuff, "\n");
                if (pcEnd == NULL) {
                    /* not end of message, write all */
                    int ret = 0;
                    if ((ret = file_write(&sDataFile, acRecvBuff, iReceived)) != 0) {
                        do_exit(ret);
                    }
                } else {
                    /* end of message detected, write until message end */
                    int ret = 0;

                    // NOTE: Ee know that message end is in the buffer, so +1 here is allowed to
                    // also get the end of message '\n' in the file.
                    if ((ret = file_write(&sDataFile, acRecvBuff, (int) (pcEnd - acRecvBuff + 1))) != 0) {
                        do_exit(ret);
                    }

                    if ((ret = file_send(&sDataFile)) != 0) {
                        do_exit(ret);
                    }
                }
            }
        }
    }

    do_exit(RET_OK);
}