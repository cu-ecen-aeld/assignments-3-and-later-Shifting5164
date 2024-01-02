#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    ERROR_LOG("thread start");

    // DONE: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    thread_data *ps_thread_data = (thread_data *) thread_param;

    // Start a thread which sleeps @param wait_to_obtain_ms number of milliseconds, then obtains the
    // mutex in @param mutex, then holds for @param wait_to_release_ms milliseconds, then releases.
    usleep(ps_thread_data->wait_to_obtain_ms * 1000 );

    //The pthread_mutex_trylock() function shall be equivalent to
    //pthread_mutex_lock(), except that if the mutex object referenced
    //by mutex is currently locked (by any thread, including the
    //current thread), the call shall return immediately.
    while (pthread_mutex_trylock(ps_thread_data->mutex) != 0 );
    usleep(ps_thread_data->wait_to_release_ms * 1000 );

    if ( pthread_mutex_unlock(ps_thread_data->mutex) != 0){
        ERROR_LOG("pthread_mutex_unlock error");
    }else {
        ps_thread_data->thread_complete_success = true;
    }
    ERROR_LOG("thread done");

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms) {
    /**
     * DONE: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    bool ret = false;

    //The start_thread_obtaining_mutex function should use dynamic memory allocation for thread_data
    //structure passed into the thread. The number of threads active should be limited only by the
    //amount of available memory.
    thread_data *ps_thread_data = NULL;
    ps_thread_data = malloc(sizeof(thread_data));
    if (ps_thread_data == NULL) {
        ERROR_LOG("malloc error");
        return false;
    }

    ps_thread_data->thread_complete_success = false;
    ps_thread_data->wait_to_obtain_ms = wait_to_obtain_ms;
    ps_thread_data->wait_to_release_ms = wait_to_release_ms;
    ps_thread_data->mutex = mutex;

    //If a thread was started successfully @param thread should be filled with the pthread_create thread ID
    //coresponding to the thread which was started.
    //
    //https://www.man7.org/linux/man-pages/man3/pthread_create.3.html
    //Before returning, a successful call to pthread_create() stores
    //the ID of the new thread in the buffer pointed to by thread; this
    //identifier is used to refer to the thread in subsequent calls to
    //other pthreads functions.
    if (pthread_create(thread, NULL, threadfunc, ps_thread_data) == 0) {
        ret = true;
    }

    //The start_thread_obtaining_mutex function should only start the thread and should not block for
    //the thread to complete.
    //No pthread_join()!

    return ret;
}