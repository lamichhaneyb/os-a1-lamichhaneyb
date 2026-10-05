// Tests for getprocs: bad arguments, max smaller than the process
// count, and a fork/exit stress run while getprocs is called.
#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/uproc.h"
#include "user/user.h"

static int failed;
static struct uproc buf[NPROC];

static void
check(int ok, char *what)
{
  if (!ok) {
    printf("getprocstest: FAIL %s\n", what);
    failed = 1;
  }
}

int
main(int argc, char *argv[])
{
  int n, i, j, pid, found;

  n = getprocs(buf, NPROC);
  check(n >= 2, "at least init and this process");
  found = 0;
  for (i = 0; i < n; i++) {
    if (buf[i].pid == getpid()) {
      found = 1;
      check(buf[i].state == 4, "own state is running");
      check(strcmp(buf[i].name, "getprocstest") == 0, "own name");
      check(buf[i].sz > 0, "own size");
    }
    if (buf[i].pid == 1)
      check(buf[i].ppid == 0, "init has ppid 0");
  }
  check(found, "found own pid");

  check(getprocs(buf, -1) == -1, "max < 0");
  check(getprocs(buf, 0) == 0, "max == 0");
  check(getprocs(buf, 1) == 1, "max == 1");
  check(getprocs(buf, 1) == 1 && buf[0].pid == 1, "first entry is init");
  check(getprocs((struct uproc *)0xffffffffffULL, NPROC) == -1, "bad pointer");
  check(getprocs((struct uproc *)(sbrk(0) - sizeof(struct uproc)), 2) == -1,
        "buffer runs past end of memory");

  // Stress: children fork and exit while the parent calls getprocs.
  for (i = 0; i < 4; i++) {
    pid = fork();
    if (pid < 0)
      break;
    if (pid == 0) {
      for (j = 0; j < 50; j++) {
        int c = fork();
        if (c == 0)
          exit(0);
        if (c > 0)
          wait(0);
      }
      exit(0);
    }
  }
  for (j = 0; j < 300; j++) {
    n = getprocs(buf, NPROC);
    check(n > 0 && n <= NPROC, "stress count");
    for (i = 0; i < n; i++)
      check(buf[i].state >= 1 && buf[i].state <= 5, "stress state");
  }
  while (wait(0) > 0)
    ;

  if (failed) {
    printf("getprocstest: FAILED\n");
    exit(1);
  }
  printf("getprocstest: OK\n");
  exit(0);
}
