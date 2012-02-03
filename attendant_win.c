#include <Windows.h>
#include "attendant.h"
#include "errors.h"

typedef void (*abend_handler_t)();

struct stdio {
  attendant__pipe_t parent[3];
  attendant__pipe_t child[3];
};

struct process_t {
  int instance;
  struct stdio stdio;
  struct attendant__errors errors;
};

static struct process_t process;

#define PIPE_STDIN  0
#define PIPE_STDOUT 1
#define PIPE_STDERR 2

static attendant__pipe_t stdio(int pipe) {
  return NULL;
}


static void free_pipe_handles() {
  int i;
  for (i = 0; i < 3; i++) {
    if (process.stdio.parent[i]) {
      /* Free handle. */
    }
    if (process.stdio.child[i]) {
      /* Free handle. */
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
  int success;
  SECURITY_ATTRIBUTES security; 

  security.nLength = sizeof(SECURITY_ATTRIBUTES); 
  security.bInheritHandle = TRUE; 
  security.lpSecurityDescriptor = NULL; 
  
  success = CreatePipe(&process.stdio.parent[0], &process.stdio.child[0],
       &security, 0);
  okay(success, INITIALIZE_CANNOT_CREATE_STDIN_PIPE);

  success = CreatePipe(&process.stdio.child[1], &process.stdio.parent[1],
       &security, 0);
  okay(success, INITIALIZE_CANNOT_CREATE_STDOUT_PIPE);

  success = CreatePipe(&process.stdio.child[2], &process.stdio.parent[2],
       &security, 0);
  okay(success, INITIALIZE_CANNOT_CREATE_STDERR_PIPE);
done:
  if (!success) {
    free_pipe_handles();
  }
  return 0;
}

static int start(const char* path, char const* argv[], abend_handler_t abend) {
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
