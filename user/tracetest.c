// Exercises trace output formats. Run inside xv6 and read the
// kernel's trace lines: no-arg syscalls, bad string pointers, and a
// forked child that inherits the mask.
#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char *bad = (char *)0xffffffffffULL;
  char *argv2[] = {"echo", "child-exec", 0};
  int pid;

  trace((1 << SYS_fork) | (1 << SYS_getpid) | (1 << SYS_uptime) |
        (1 << SYS_sync) | (1 << SYS_open) | (1 << SYS_exec) |
        (1 << SYS_mkdir) | (1 << SYS_unlink) | (1 << SYS_close) |
        (1 << SYS_kill) | (1 << SYS_link) | (1 << SYS_mknod) |
        (1 << SYS_chdir));

  getpid();
  uptime();
  sync();
  open(bad, 0);           // open(?) -> -1
  mkdir(bad);             // mkdir(?) -> -1
  link(bad, "x");         // link(?) -> -1
  mknod(bad, 1, 1);       // mknod(?) -> -1
  chdir("nosuchdir");     // chdir("nosuchdir") -> -1
  unlink("nosuchfile");   // unlink("nosuchfile") -> -1
  close(-5);              // close(-5) -> -1
  kill(-3);               // kill(-3) -> -1
  exec(bad, argv2);       // exec(?) -> -1

  pid = fork();
  if (pid == 0) {
    getpid();             // printed with the child's pid
    exec("echo", argv2);  // exec("echo") -> 2, then echo's own output
    exit(1);
  }
  wait(0);
  trace(0);
  getpid();               // not printed
  exit(0);
}
