// Tests for the sysinfo system call. The handout's provided version
// was missing from the starter repo, so this one checks the same things.
#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/sysinfo.h"
#include "kernel/fcntl.h"
#include "user/user.h"

static int failed;

static void
check(int ok, char *what)
{
  if (!ok) {
    printf("sysinfotest: FAIL %s\n", what);
    failed = 1;
  }
}

static void
get(struct sysinfo *info)
{
  if (sysinfo(info) < 0) {
    printf("sysinfotest: FAIL sysinfo returned -1\n");
    exit(1);
  }
}

static void
testmem(void)
{
  struct sysinfo a, b;

  get(&a);
  if (sbrk(10 * PGSIZE) == SBRK_ERROR) {
    printf("sysinfotest: sbrk failed\n");
    exit(1);
  }
  get(&b);
  check(b.freemem <= a.freemem - 10 * PGSIZE, "freemem after sbrk");
  sbrk(-10 * PGSIZE);
  get(&b);
  check(b.freemem == a.freemem, "freemem after sbrk(-n)");
  check(a.freemem % PGSIZE == 0, "freemem is whole pages");
}

static void
testproc(void)
{
  struct sysinfo a, b;
  int fds[2], pid, status;
  char c;

  get(&a);
  check(a.nproc >= 2, "nproc at least 2");
  pipe(fds);
  pid = fork();
  if (pid < 0) {
    printf("sysinfotest: fork failed\n");
    exit(1);
  }
  if (pid == 0) {
    close(fds[1]);
    read(fds[0], &c, 1); // block until the parent closes the pipe
    exit(0);
  }
  get(&b);
  check(b.nproc == a.nproc + 1, "nproc after fork");
  close(fds[0]);
  close(fds[1]);
  wait(&status);
  get(&b);
  check(b.nproc == a.nproc, "nproc after wait");
}

static void
testfile(void)
{
  struct sysinfo a, b;
  int fd, fds[2];

  get(&a);
  fd = open("sysinfotest.tmp", O_CREATE | O_RDWR);
  check(fd >= 0, "open");
  get(&b);
  check(b.nopenfile == a.nopenfile + 1, "nopenfile after open");

  check(pipe(fds) == 0, "pipe");
  get(&b);
  check(b.nopenfile == a.nopenfile + 3, "nopenfile after pipe");

  // dup shares a file-table entry, so the count does not change.
  int d = dup(fd);
  get(&b);
  check(b.nopenfile == a.nopenfile + 3, "nopenfile after dup");
  close(d);

  close(fds[0]);
  get(&b);
  check(b.nopenfile == a.nopenfile + 2, "nopenfile after close pipe end");
  close(fds[1]);
  close(fd);
  unlink("sysinfotest.tmp");
  get(&b);
  check(b.nopenfile == a.nopenfile, "nopenfile after close all");
}

static void
testbad(void)
{
  check(sysinfo((struct sysinfo *)0xffffffffffULL) == -1, "huge pointer");
  check(sysinfo((struct sysinfo *)(sbrk(0) + PGSIZE)) == -1,
        "pointer past end of memory");
  // A struct that straddles the end of user memory must also fail.
  check(sysinfo((struct sysinfo *)(sbrk(0) - 8)) == -1, "straddling pointer");
}

int
main(int argc, char *argv[])
{
  testmem();
  testproc();
  testfile();
  testbad();
  if (failed) {
    printf("sysinfotest: FAILED\n");
    exit(1);
  }
  printf("sysinfotest: OK\n");
  exit(0);
}
