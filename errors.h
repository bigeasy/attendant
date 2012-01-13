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
#define INITIALIZE_CANNOT_CREATE_STDIN_PIPE         201
#define INITIALIZE_CANNOT_CREATE_STDOUT_PIPE        202
#define INITIALIZE_CANNOT_CREATE_STDERR_PIPE        203
#define INITIALIZE_CANNOT_CREATE_REAPER_PIPE        203
#define START_ALREADY_RUNNING                   101
#define START_ABEND_REQUIRED                   102
#define LAUNCH_CANNOT_FORK                       102
#define LAUNCH_INCOMPLETE_ERROR_CODE                       102
#define START_CANNOT_MALLOC                     103
#define START_CANNOT_EXECV                      104
#define LAUNCH_CANNOT_CREATE_STDIN_PIPE         201
#define LAUNCH_CANNOT_CREATE_STDOUT_PIPE        202
#define LAUNCH_CANNOT_CREATE_STDERR_PIPE        203
#define LAUNCH_CANNOT_CREATE_FORK_PIPE          204
#define LAUNCH_CANNOT_CREATE_STATUS_PIPE        205
#define LAUNCH_CANNOT_MALLOC                    206
#define START_CANNOT_CLOSE_STDIN_PIPE         8
#define START_CANNOT_CLOSE_STDOUT_PIPE        9
#define START_CANNOT_CLOSE_STDERR_PIPE       10
#define START_CANNOT_DUP_STDIN_PIPE          11
#define START_CANNOT_DUP_STDOUT_PIPE         12
#define START_CANNOT_DUP_STDERR_PIPE         13
#define RELAY_PROGRAM_MISSING                14
#define START_CANNOT_SPAWN_THREAD           -14
#define RELAY_PROGRAM_PATH_NOT_ABSOLUTE      15
#define RELAY_CANNOT_OPEN_DEV_FD             16
#define RELAY_CANNOT_EXEC                    17 
#define RELAY_PULSE_PIPE_MALFORMED                    198 
#define RELAY_PULSE_PIPE_MISSING                    117 
#define REAPER_CANNOT_READ_REAPER_PIPE 501
#define REAPER_TRUNCATED_READ_REAPER_PIPE 502
#define LAUNCH_RELAY_PIPE_STDOUT_FAILED 1099
#define LAUNCH_RELAY_PIPE_RELAY_FAILED 1099

#define REAPER_UNEXPECTED_CANARY_PIPE_EVENT 0
#define REAPER_UNEXPECTED_REAPER_PIPE_EVENT 0
#define LAUNCH_IMMEDIATE_RELAY_EXIT 0
#define LAUNCH_RELAY_PIPE_HUNG_UP 0

#define PARTIAL_FORK_ERROR_CODE                 901
#define PARTIAL_EXEC_ERROR_CODE                 902
#define PARTIAL_STDOUT_STATUS_PIPE_NUMBER                 903
#define PARTIAL_STATUS_PIPE_NUMBER                 904

void send_error(int pipe, int code);
