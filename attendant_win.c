typedef void (*abend_handler_t)();

struct process_t {
  int instance;
};

static struct process_t process;

#define PIPE_STDIN  0
#define PIPE_STDOUT 1
#define PIPE_STDERR 2

static pipe_t stdio(int pipe) {
  return NULL;
}

static int initalize(const char *relay, int canary) {
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

static int shutdown() {
  return 0;
}

static int done(int timeout) {
  return 0;
}

static int scram() {
  return 0;
}

static struct attendant__errors errors() {
  return NULL;
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
, shutdown
, done
, scram
, errors
, destroy
};