#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/uproc.h"
#include "user/user.h"

static char *states[] = {
  [1] = "used",
  [2] = "sleeping",
  [3] = "runnable",
  [4] = "running",
  [5] = "zombie",
};

static struct uproc procs[NPROC];

int
main(int argc, char *argv[])
{
  int i, n;
  char *state;

  n = getprocs(procs, NPROC);
  if (n < 0) {
    fprintf(2, "ps: getprocs failed\n");
    exit(1);
  }

  printf("PID\tPPID\tSTATE\tSIZE\tNAME\n");
  for (i = 0; i < n; i++) {
    if (procs[i].state > 0 && procs[i].state < 6)
      state = states[procs[i].state];
    else
      state = "unknown";
    printf("%d\t%d\t%s\t%lu\t%s\n", procs[i].pid, procs[i].ppid, state,
           procs[i].sz, procs[i].name);
  }
  exit(0);
}
