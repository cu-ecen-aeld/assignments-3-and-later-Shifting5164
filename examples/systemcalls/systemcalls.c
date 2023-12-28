#include "systemcalls.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/

    if ( system( cmd ) == 0 ){
        return true;
    }
    return false;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
//    command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/

    pid_t pid;
    bool ret = false;

    pid = fork();
    if ( pid == -1 ) {
        return false ;      /* failure, no children*/
    }else if ( pid == 0 ){
        /*Child from here */

        /* Replace child process , and return execv exitcode for the parent to pick up later with exit() (WEXITSTATUS())*/
        exit ( execv( command[0],  command) );
    }else {
        /*Parent from here */

        int status;

        /*
         * The status value returned by wait() and waitpid() allows us to distinguish the follow-
    ing events for the child:
    zThe child terminated by calling _exit() (or exit()), specifying an integer exit status.
    zThe child was terminated by the delivery of an unhandled signal.
    zThe child was stopped by a signal, and waitpid() was called with the WUNTRACED flag.
    z
    The child was resumed by a SIGCONT signal, and waitpid() was called with the
    WCONTINUED flag.
         */

        if (waitpid(pid, &status, 0) == -1) {
            ret = false;

            /* WIFEXITED: This macro returns true if the child process exited normally. In this case,
    the macro WEXITSTATUS(status) returns the exit status of the child process.*/
        } else if (WIFEXITED(status) != 0 ) {
            /* WEXITSTATUS: return code from child */
            if (WEXITSTATUS(status) == 0 ) {
                ret = true;
            }else {
                ret = false;
            }
        }
    }

    va_end(args);

    return ret;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
//    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
//    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/


//    execlp( "/bin/sh", "/bin/sh", "-c", *command, ">", *outputfile, (char *)NULL );

    fflush(stdout);

    pid_t pid;
    bool ret = false;

    /* On success, the PID of the child process is returned in the
       parent, and 0 is returned in the child. */
    pid = fork();
    if ( pid == -1 ) {
        perror("fork");
        return false ;      /* failure, no children*/
    }else if ( pid == 0 ){
        /*Child from here */

        /* redirect stdout to outputfile */
        int stdout_fd = open(outputfile, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
        if ( stdout_fd < 0){
            perror("open");
            return false;
        }

        if ( dup2(stdout_fd, STDOUT_FILENO) < 0 ){ /* STDOUT_FILENO = posix name */
            perror("dup2");
            return false;
        }

        /* set the working directory to the root directory */
        if ( chdir ("/") < 0 ){
            perror("chdir");
            return false;
        }

        /* Replace child process , and return execv exitcode for the parent to pick up later with exit() (WEXITSTATUS())*/
        int ret_exec = execv( command[0],  command);

        if(ret_exec == -1)
        {
            perror("execv");
            exit(EXIT_FAILURE);
        }

    }else {
        /* Parent from here */

        int status;

        /*
         * The status value returned by wait() and waitpid() allows us to distinguish the follow-
    ing events for the child:
    zThe child terminated by calling _exit() (or exit()), specifying an integer exit status.
    zThe child was terminated by the delivery of an unhandled signal.
    zThe child was stopped by a signal, and waitpid() was called with the WUNTRACED flag.
    z
    The child was resumed by a SIGCONT signal, and waitpid() was called with the
    WCONTINUED flag.
         */

        if (waitpid(pid, &status, 0) == -1) {
            ret = false;

            /* WIFEXITED: This macro returns true if the child process exited normally. In this case,
    the macro WEXITSTATUS(status) returns the exit status of the child process.*/
        } else if (WIFEXITED(status) != 0 ) {
            /* WEXITSTATUS: return code from child */
            if (WEXITSTATUS(status) == 0 ) {
                ret = true;
            }else {
                ret = false;
            }
        }
    }

    va_end(args);

    return ret;

}
