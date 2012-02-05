/* The vagaries of process management in UNIX have already greatly limited our
 * expectations of our process monitor. We've limited ourselves to command line
 * arguments and standard I/O for IPC, and then only to get the ball rolling.
 * This leaves us with process management that is more or less, process
 * management a la Windows.
 *
 * Out Windows attendant still requires that we shield the child process from
 * inherited file handles. The problem of inherited file handles in Windows is
 * more onerous that in UNIX, since a file handle will lock the file as well. If
 * the host application has is opening and closing a file that our plugin server
 * process has inherited, it will fail at the next reopen because the plugin
 * server process will hold an open file handle.
 *
 * Apparently, handles to kernel objects are just numbers, and you can [pass them
 * around as the string representation of those
 * numbers](http://comsci.liu.edu/~murali/win32/Handles.htm). Each process has
 * its own set of handles, but with `DuplicateHandle` you can create a handle in
 * another process. You need some way to tell that other process about the
 * handle, though. We can use either named pipes or mailslots.
 *
 * Thus, we still need our relay program. Our relay program is created without
 * handle inheritence, but with a command line that describes the handle of an
 * anonymous pipe. The relay program will read that argument for the handle of
 * the anonymous pipe.
 *
 * The anonymous pipe is used to pass inheritable child ends of the standard I/O
 * pipes. Our relay program launches our target program, with the target
 * arguments, and with inherience set, and with our 
 *
 * TODO Jobs.
 *
 *  * [CreateProcess such that child process is killed when parent is
 *  killed?](http://stackoverflow.com/questions/6259055/createprocess-such-that-child-process-is-killed-when-parent-is-killed).
 *  * [How do I automatically destroy child processes in
 *  Windows?](http://stackoverflow.com/questions/53208/how-do-i-automatically-destroy-child-processes-in-windows).
 *
 * We don't care about [named pipe
 * security](http://msdn.microsoft.com/en-us/library/windows/desktop/aa365600.aspx)
 *
 * TODO Duplicate handle.
 *
 *  * [DuplicateHandle(), use in first or second
 *  process?](http://stackoverflow.com/questions/819710/duplicatehandle-use-in-first-or-second-process/819735#819735)
 *  *
 *  [DuplicateHandle](http://msdn.microsoft.com/en-us/library/windows/desktop/ms724251.aspx).
 *  * [Child process inheriting all handles](http://www.codeguru.com/forum/archive/index.php/t-432338.html).
 *  * [Inheritance](http://msdn.microsoft.com/en-us/library/windows/desktop/ms683463.aspx)
 *  * [Pipe Handle Inheritence](http://msdn.microsoft.com/en-us/library/windows/desktop/aa365782.aspx)
 *
 * TODO Critical Sections.
 *
 * * [Critical Sections](http://msdn.microsoft.com/en-us/library/windows/desktop/ms682530.aspx).
 */ 

/* */
#include <Windows.h>
#include "attendant.h"
#include "errors.h"

/* The abend handler function type. */
typedef void (*abend_handler_t)();

/* A structure that gathers and organizes our standard I/O pipe handles. */
struct stdio {
  attendant__pipe_t parent[3];
  attendant__pipe_t child[3];
};

/* The one and only process we watch. Static variables are gathered into this
 * structure so that when reading the code below, it is easy to see which are
 * static variables and which are local variables.*/

/* &#9824; */
struct process {
  /* Count of the number of times that the server has started and restarted. */
  int instance;
  /* A collection of standard I/O pipe handles. */
  struct stdio stdio;
  /* The attendant error code and system error code for last error. */
  struct attendant__errors errors;
  /* &mdash; */
};

/* The variable structure for our once and only instance of the plugin
 * attendant. */

/* &mdash; */
static struct process process;

/* Handles to standard I/O pipes are stored in arrays indexed by the traditional
 * UNIX file descriptor number. */
#define PIPE_STDIN  0
#define PIPE_STDOUT 1
#define PIPE_STDERR 2

static attendant__pipe_t stdio(int pipe) {
  return NULL;
}

/* Free the allocated pipe handles. */
static void free_pipe_handles() {
  int i;
  for (i = 0; i < 3; i++) {
    if (process.stdio.parent[i]) {
      CloseHandle(process.stdio.parent[i]);
      process.stdio.parent[i] = NULL;
    }
    if (process.stdio.child[i]) {
      CloseHandle(process.stdio.child[i]);
      process.stdio.child[i] = NULL;
    }
  }
}

#define okay(condition, error)                  \
  do {                                          \
    if (!condition) {                           \
        process.errors.attendant = error;       \
        process.errors.system = GetLastError(); \
        goto done;                              \
    }                                           \
  } while (0);

static int initalize(const char *relay, int canary) {
  int success, i;
  SECURITY_ATTRIBUTES security;

  /* We're not up for the task of defending against attackers that have access
   * to the system as the current user. We go with the [default security
   * attributes](
   * http://msdn.microsoft.com/en-us/library/windows/desktop/aa365600.aspx). If
   * the system so far compromised that we need to restrict anonymous pipes
   * using ACLS, then there are so many attack vectors, it is well beyond the
   * compexlity bugdet of an NPAPI or similar plugin.
   *
   * I'm always concersed that I'm opening myself up to trollish criticism,
   * that this is some sort of adminission that I don't care about security.
   * It's not. I'd be curious to hear a ranting challenge to these comments,
   * because I can only imagine the irrationality.
   */
  security.nLength = sizeof(SECURITY_ATTRIBUTES); 
  security.bInheritHandle = TRUE; 
  security.lpSecurityDescriptor = NULL; 
  
  /* Create the [standard I/O redirection pipes](
   * http://msdn.microsoft.com/en-us/library/windows/desktop/ms682499.aspx
   * ). We keep these pipes around between recovered instances, so that we do
   * not have to gaurd the handles with a critical section. That is, they are
   * set once and remain valid for the lifetime of the plugin stub.
   */
  success = CreatePipe(&process.stdio.child[0], &process.stdio.parent[0],
       &security, 0);
  okay(success, INITIALIZE_CANNOT_CREATE_STDIN_PIPE);

  success = CreatePipe(&process.stdio.parent[1], &process.stdio.child[1],
       &security, 0);
  okay(success, INITIALIZE_CANNOT_CREATE_STDOUT_PIPE);

  success = CreatePipe(&process.stdio.parent[2], &process.stdio.child[2],
       &security, 0);
  okay(success, INITIALIZE_CANNOT_CREATE_STDERR_PIPE);

  /* Ensure that the parent ends of the standard I/O pipes are not inherited. */
  for (i = PIPE_STDIN; i <= PIPE_STDERR; i++) {
    success = SetHandleInformation(process.stdio.parent[i],
        HANDLE_FLAG_INHERIT, 0);
    okay(success, INITIALIZE_CANNOT_CREATE_STDIN_PIPE + i);
  }
done:
  if (!success) {
    free_pipe_handles();
  }
  return 0;
}

static void send_recv() {
  const char* when = "echo\n", *exit = "exit\n";
  char buffer[1024];
  DWORD written, read;
  int success;
  success = ReadFile(process.stdio.parent[1], buffer, sizeof(buffer), &read, NULL);
  success = WriteFile(process.stdio.parent[0], when, 5, &written, NULL);
  FlushFileBuffers(process.stdio.parent[0]);
  success = ReadFile(process.stdio.parent[1], buffer, sizeof(buffer), &read, NULL);
  success = WriteFile(process.stdio.parent[0], exit, 5, &written, NULL);
  FlushFileBuffers(process.stdio.parent[0]);
}

/* Command line parsing in Windows is a [sorry state of
 * affairs](http://www.autohotkey.net/~deleyd/parameters/parameters.htm). You
 * can only count on it to do simple things, and if you need to include white
 * space, you might be in trouble. Do your utmost to bootstrap through pipes.
 *
 * More
 * [on Windows command line
 * escaping](http://stackoverflow.com/questions/2403647/how-to-escape-parameter-in-windows-command-line).
 */
static int start(const char* path, char const* argv[], abend_handler_t abend) {
  int success;
  wchar_t application[MAX_PATH];
  STARTUPINFOW startup;
  PROCESS_INFORMATION info;

  GetCurrentDirectoryW(MAX_PATH, application);
  wcscat_s(application, MAX_PATH, L"\\Debug\\server.exe");
  memset( &startup, 0L, sizeof(STARTUPINFO) );
  startup.cb = sizeof(STARTUPINFO); 
  startup.hStdInput = process.stdio.child[0];
  startup.hStdOutput = process.stdio.child[1];
  startup.hStdError = process.stdio.child[2];
  startup.dwFlags |= STARTF_USESTDHANDLES;
  success = CreateProcessW(application, L"", NULL, NULL, TRUE, 0, NULL, NULL, &startup, &info);
  okay(success, START_CANNOT_EXECV);

  send_recv();
done:
  return 0;
}

static int ready() {
  return 0;
}

static int retry(int milliseconds) {
  return 0;
}

static int attendant__shutdown() {
  return 0;
}

static int done(int timeout) {
  return 0;
}

static int scram() {
  return 0;
}

static struct attendant__errors errors() {
  struct attendant__errors errors = { 0, 0 };
  return errors;
}

static int destroy() {
  free_pipe_handles();
  return 0;
}

struct attendant attendant =
{ initalize
, start
, ready
, stdio
, retry
, attendant__shutdown
, done
, scram
, errors
, destroy
};
