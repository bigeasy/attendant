/* To report errors, we provide you with a morass of error codes. You can
 * combine this with errno to get a picture of what went wrong. There is no
 * logging available. We surrender immediately at the first error. There is
 * really not much you can do in response to a failure, especially as a library
 * dynamically loaded into a host that used up all its file handles (we only
 * need what, six, is that too much to ask?)
 *
 * There is no rhyme or reason to these codes. They are numbered according the
 * order in which I thought them up. They are long and verbose to be self
 * documenting.
 *
 * The last error is obtained using the error method. You can get the last errno
 * associated with the error too. To understand what the errno means, have a
 * look at the source, then have a look at the man pages.
 */
#define START_ALREADY_RUNNING                 1
#define START_CANNOT_FORK                     2
#define START_CANNOT_CREATE_STDIN_PIPE        3
#define START_CANNOT_CREATE_STDOUT_PIPE       4
#define START_CANNOT_CREATE_STDERR_PIPE       5
#define START_CANNOT_CREATE_FORK_PIPE         6
#define START_CANNOT_CREATE_EXEC_PIPE         7
#define START_CANNOT_CLOSE_STDIN_PIPE         8
#define START_CANNOT_CLOSE_STDOUT_PIPE        9
#define START_CANNOT_CLOSE_STDERR_PIPE       10
#define START_CANNOT_DUP_STDIN_PIPE          11
#define START_CANNOT_DUP_STDOUT_PIPE         12
#define START_CANNOT_DUP_STDERR_PIPE         13
#define RELAY_PROGRAM_MISSING                14
#define RELAY_PROGRAM_PATH_NOT_ABSOLUTE      15
#define RELAY_CANNOT_OPEN_DEV_FD             16
#define RELAY_CANNOT_EXEC                    17 

void send_error(int fd, int code);
