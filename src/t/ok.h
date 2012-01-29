#define CHECK(op, cond)       \
  do {                        \
    (op);                     \
    if (cond) {               \
      if (errno == EINTR) {   \
        continue;             \
      }                       \
      bail(strerror(errno));  \
    }                         \
  } while (0)

void ok(int cond, char const *format, ...);
void bail(const char* message);
