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
#define INITIALIZE_CANNOT_CREATE_STDIN_PIPE     101
#define INITIALIZE_CANNOT_CREATE_STDOUT_PIPE    102
#define INITIALIZE_CANNOT_CREATE_STDERR_PIPE    103
#define INITIALIZE_CANNOT_CREATE_REAPER_PIPE    104
#define START_ALREADY_RUNNING                   105
#define START_ABEND_REQUIRED                    106
#define LAUNCH_CANNOT_FORK                      107
#define LAUNCH_INCOMPLETE_ERROR_CODE            108
#define START_CANNOT_MALLOC                     109
#define START_CANNOT_EXECV                      110
#define LAUNCH_CANNOT_CREATE_STDIN_PIPE         111
#define LAUNCH_CANNOT_CREATE_STDOUT_PIPE        112
#define LAUNCH_CANNOT_CREATE_STDERR_PIPE        113
#define LAUNCH_CANNOT_CREATE_FORK_PIPE          114
#define LAUNCH_CANNOT_CREATE_STATUS_PIPE        115
#define LAUNCH_CANNOT_MALLOC                    116
#define START_CANNOT_CLOSE_STDIN_PIPE           117
#define START_CANNOT_CLOSE_STDOUT_PIPE          118
#define START_CANNOT_CLOSE_STDERR_PIPE          119
#define START_CANNOT_DUP_STDIN_PIPE             120
#define START_CANNOT_DUP_STDOUT_PIPE            121
#define START_CANNOT_DUP_STDERR_PIPE            122
#define RELAY_PROGRAM_MISSING                   123
#define START_CANNOT_SPAWN_THREAD               124
#define RELAY_PROGRAM_PATH_NOT_ABSOLUTE         125
#define RELAY_CANNOT_OPEN_DEV_FD                126
#define RELAY_CANNOT_EXEC                       127 
#define RELAY_PULSE_PIPE_MALFORMED              128 
#define RELAY_PULSE_PIPE_MISSING                129 
#define REAPER_CANNOT_READ_REAPER_PIPE          130
#define REAPER_TRUNCATED_READ_REAPER_PIPE       131
#define LAUNCH_RELAY_PIPE_STDOUT_FAILED         132
#define LAUNCH_RELAY_PIPE_RELAY_FAILED          133

#define REAPER_UNEXPECTED_CANARY_PIPE_EVENT     134
#define REAPER_UNEXPECTED_REAPER_PIPE_EVENT     135
#define LAUNCH_IMMEDIATE_RELAY_EXIT             136
#define LAUNCH_RELAY_PIPE_HUNG_UP               137

#define PARTIAL_FORK_ERROR_CODE                 138
#define PARTIAL_EXEC_ERROR_CODE                 139
#define PARTIAL_STDOUT_STATUS_PIPE_NUMBER       140
#define PARTIAL_STATUS_PIPE_NUMBER              141

void send_error(int pipe, int code);
