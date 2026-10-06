// Self-tests for the A1 user programs: runs each command with given
// stdin, captures stdout and stderr, and checks the exit status and
// exact output. Trace output comes from the kernel console, so it is
// checked by hand instead (see tracetest).
#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "user/user.h"

#define OUTMAX 4096

static char out[OUTMAX], err[OUTMAX];
static int failures, total;

// Read fd until EOF into buf, nul-terminated.
static void
slurp(int fd, char *buf)
{
  int n, len = 0;

  while ((n = read(fd, buf + len, OUTMAX - 1 - len)) > 0)
    len += n;
  buf[len] = 0;
}

// Run argv with stdin from the string in, capturing stdout in out and
// stderr in err. Returns the exit status.
static int
run(char **argv, char *in)
{
  int fd, pid, status, po[2], pe[2];

  fd = open("a1in", O_CREATE | O_TRUNC | O_WRONLY);
  write(fd, in, strlen(in));
  close(fd);

  pipe(po);
  pipe(pe);
  pid = fork();
  if (pid == 0) {
    close(0);
    open("a1in", O_RDONLY);
    close(1);
    dup(po[1]);
    close(2);
    dup(pe[1]);
    close(po[0]);
    close(po[1]);
    close(pe[0]);
    close(pe[1]);
    exec(argv[0], argv);
    exit(127);
  }
  close(po[1]);
  close(pe[1]);
  slurp(po[0], out);
  slurp(pe[0], err);
  close(po[0]);
  close(pe[0]);
  wait(&status);
  unlink("a1in");
  return status;
}

// Sort the lines of s in place (insertion sort; outputs are small).
static void
sortlines(char *s)
{
  static char copy[OUTMAX];
  static char *lines[256];
  int n = 0, i, j;
  char *p, *t;

  strcpy(copy, s);
  for (p = copy; *p && n < 256;) {
    lines[n++] = p;
    while (*p && *p != '\n')
      p++;
    if (*p)
      *p++ = 0;
  }
  for (i = 1; i < n; i++)
    for (j = i; j > 0 && strcmp(lines[j - 1], lines[j]) > 0; j--) {
      t = lines[j];
      lines[j] = lines[j - 1];
      lines[j - 1] = t;
    }
  s[0] = 0;
  p = s;
  for (i = 0; i < n; i++) {
    strcpy(p, lines[i]);
    p += strlen(p);
    *p++ = '\n';
    *p = 0;
  }
}

// Check one run: exit status, exact stdout (sorted if asked), and
// that stderr is (non)empty as expected. errwant 0: stderr must be
// empty; otherwise stderr must equal errwant.
static void
expect(char *name, char **argv, char *in, int wantstatus, char *wantout,
       int sorted, char *wanterr)
{
  int st;
  static char want[OUTMAX];

  total++;
  st = run(argv, in);
  strcpy(want, wantout);
  if (sorted) {
    sortlines(out);
    sortlines(want);
  }
  if (st != wantstatus || strcmp(out, want) != 0 ||
      (wanterr == 0 && err[0] != 0) ||
      (wanterr != 0 && strcmp(err, wanterr) != 0)) {
    failures++;
    printf("FAIL %s\n  status %d (want %d)\n  stdout [%s]\n  want   [%s]\n"
           "  stderr [%s]\n",
           name, st, wantstatus, out, want, err);
  }
}

#define A(...) ((char *[]){__VA_ARGS__, 0})

static void
mk(char *path)
{
  if (mkdir(path) < 0) {
    printf("a1test: mkdir %s failed (run in a fresh fs.img)\n", path);
    exit(1);
  }
}

static void
touch(char *path)
{
  int fd = open(path, O_CREATE | O_WRONLY);
  close(fd);
}

static void
testsleep(void)
{
  char *u = "usage: sleep <ticks>\n";
  expect("sleep no arg", A("sleep"), "", 1, "", 0, u);
  expect("sleep extra", A("sleep", "1", "2"), "", 1, "", 0, u);
  expect("sleep negative", A("sleep", "-1"), "", 1, "", 0, u);
  expect("sleep not digits", A("sleep", "1x"), "", 1, "", 0, u);
  expect("sleep letters", A("sleep", "abc"), "", 1, "", 0, u);
  expect("sleep empty", A("sleep", ""), "", 1, "", 0, u);
  expect("sleep plus", A("sleep", "+3"), "", 1, "", 0, u);
  expect("sleep 0", A("sleep", "0"), "", 0, "", 0, 0);

  int t0 = uptime();
  expect("sleep 10", A("sleep", "10"), "", 0, "", 0, 0);
  if (uptime() - t0 < 10) {
    failures++;
    printf("FAIL sleep 10 returned after %d ticks\n", uptime() - t0);
  }
}

static void
testfind(void)
{
  char *u = "usage: find <dir> <pattern> [-type f|d]\n";

  mk("ft");
  mk("ft/b");
  mk("ft/b/deep");
  mk("ft/b/deep/er");
  touch("ft/b/x.c");
  touch("ft/y.c");
  touch("ft/ab");
  touch("ft/axxbyy");
  touch("ft/a");
  touch("ft/b/deep/er/ab");
  touch("ft/b/deep/er/z.c");
  touch("ft/cab");
  mk("ft/abdir");

  // The handout's examples.
  expect("find *.c", A("find", "ft", "*.c"), "", 0,
         "ft/y.c\nft/b/x.c\nft/b/deep/er/z.c\n", 1, 0);
  expect("find -type d", A("find", "ft", "b", "-type", "d"), "", 0,
         "ft/b\n", 1, 0);
  expect("find a*b*", A("find", "ft", "a*b*"), "", 0,
         "ft/ab\nft/axxbyy\nft/b/deep/er/ab\nft/abdir\n", 1, 0);
  // * at start, middle, end, alone, and doubled.
  expect("find *b", A("find", "ft", "*b"), "", 0,
         "ft/ab\nft/b\nft/b/deep/er/ab\nft/cab\n", 1, 0);
  expect("find a*", A("find", "ft", "a*", "-type", "f"), "", 0,
         "ft/ab\nft/axxbyy\nft/a\nft/b/deep/er/ab\n", 1, 0);
  expect("find a*y", A("find", "ft", "a*y"), "", 0, "ft/axxbyy\n", 1, 0);
  expect("find **", A("find", "ft/b", "**"), "", 0,
         "ft/b/x.c\nft/b/deep\nft/b/deep/er\nft/b/deep/er/ab\n"
         "ft/b/deep/er/z.c\n",
         1, 0);
  expect("find exact", A("find", "ft", "a"), "", 0, "ft/a\n", 1, 0);
  expect("find no match", A("find", "ft", "zzz"), "", 0, "", 0, 0);
  expect("find -type f", A("find", "ft", "*", "-type", "f"), "", 0,
         "ft/y.c\nft/ab\nft/axxbyy\nft/a\nft/cab\nft/b/x.c\n"
         "ft/b/deep/er/ab\nft/b/deep/er/z.c\n",
         1, 0);
  expect("find -type d all", A("find", "ft", "*", "-type", "d"), "", 0,
         "ft/b\nft/b/deep\nft/b/deep/er\nft/abdir\n", 1, 0);
  // . and .. are never printed or entered; dir itself never printed.
  expect("find dots", A("find", "ft", "."), "", 0, "", 0, 0);
  expect("find dotdot", A("find", "ft", ".."), "", 0, "", 0, 0);
  expect("find self", A("find", "ft", "ft"), "", 0, "", 0, 0);
  // Devices count as files.
  expect("find device", A("find", ".", "console", "-type", "f"), "", 0,
         "./console\n", 0, 0);
  expect("find device d", A("find", ".", "console", "-type", "d"), "", 0, "",
         0, 0);

  // Dir spelled with a trailing slash, as an absolute path, or as ".".
  expect("find trailing /", A("find", "ft/", "y.c"), "", 0, "ft/y.c\n", 0, 0);
  expect("find absolute", A("find", "/ft", "x.c"), "", 0, "/ft/b/x.c\n", 0,
         0);
  chdir("ft");
  expect("find .", A("/find", ".", "z.c"), "", 0, "./b/deep/er/z.c\n", 0, 0);
  chdir("/");

  // A name of exactly DIRSIZ (14) chars has no nul in its dirent.
  touch("ft/abcdefghijklmn");
  expect("find DIRSIZ name", A("find", "ft", "*klmn"), "", 0,
         "ft/abcdefghijklmn\n", 0, 0);
  expect("find DIRSIZ exact", A("find", "ft", "abcdefghijklmn"), "", 0,
         "ft/abcdefghijklmn\n", 0, 0);
  // Pattern longer than any name, and * in several places at once.
  expect("find long pattern", A("find", "ft", "abcdefghijklmnop"), "", 0, "",
         0, 0);
  expect("find *x*.*", A("find", "ft", "*x*.*"), "", 0, "ft/b/x.c\n", 0, 0);
  expect("find *", A("find", "ft/b/deep/er", "*"), "", 0,
         "ft/b/deep/er/ab\nft/b/deep/er/z.c\n", 1, 0);

  // Deep tree: deeper than NOFILE (16) and a path longer than MAXPATH
  // (128), so it only works if find doesn't hold an fd per level and
  // doesn't pass the full path to open().
  static char deep[1024];
  int i;
  mk("dt");
  chdir("dt");
  strcpy(deep, "dt");
  for (i = 0; i < 20; i++) {
    char name[16];
    strcpy(name, "level_dir_00");
    name[10] = '0' + i / 10;
    name[11] = '0' + i % 10;
    mk(name);
    chdir(name);
    strcpy(deep + strlen(deep), "/");
    strcpy(deep + strlen(deep), name);
  }
  touch("bottom.c");
  chdir("/");
  strcpy(deep + strlen(deep), "/bottom.c\n");
  expect("find deep tree", A("find", "dt", "*.c"), "", 0, deep, 0, 0);
  deep[strlen(deep) - strlen("/bottom.c\n")] = 0;
  strcpy(deep + strlen(deep), "\n");
  expect("find deep -type d", A("find", "dt", "level_dir_19", "-type", "d"),
         "", 0, deep, 0, 0);

  // Errors.
  expect("find missing dir", A("find", "nosuch", "x"), "", 1, "", 0,
         "find: cannot open nosuch\n");
  expect("find no args", A("find"), "", 1, "", 0, u);
  expect("find one arg", A("find", "ft"), "", 1, "", 0, u);
  expect("find bad type", A("find", "ft", "x", "-type", "z"), "", 1, "", 0,
         u);
  expect("find bad flag", A("find", "ft", "x", "-tipe", "f"), "", 1, "", 0,
         u);
  expect("find type no arg", A("find", "ft", "x", "-type"), "", 1, "", 0, u);
}

static void
testxargs(void)
{
  char *u = "usage: xargs [-n N] cmd [args...]\n";
  static char in[1024], want[1024];
  char *p;
  int i;

  // The handout's examples.
  expect("xargs basic", A("xargs", "echo", "x"), "a b\n", 0, "x a b\n", 0, 0);
  expect("xargs -n 2", A("xargs", "-n", "2", "echo", "x"), "a b c d e\n", 0,
         "x a b\nx c d\nx e\n", 0, 0);

  // Lines, empty lines, tabs, no trailing newline.
  expect("xargs lines", A("xargs", "echo", "x"), "a b\nc\n", 0,
         "x a b\nx c\n", 0, 0);
  expect("xargs empty lines", A("xargs", "echo", "x"), "\n\na\n\n \t\nb\n\n",
         0, "x a\nx b\n", 0, 0);
  expect("xargs tabs", A("xargs", "echo"), "a\tb  c\t\n", 0, "a b c\n", 0, 0);
  expect("xargs no newline", A("xargs", "echo"), "a b", 0, "a b\n", 0, 0);
  expect("xargs no input", A("xargs", "echo", "x"), "", 0, "", 0, 0);
  expect("xargs only blanks", A("xargs", "echo"), " \n\t\n\n", 0, "", 0, 0);

  // -n ignores line boundaries.
  expect("xargs -n across lines", A("xargs", "-n", "2", "echo"),
         "a\nb c\n\nd\ne\n", 0, "a b\nc d\ne\n", 0, 0);
  expect("xargs -n 1", A("xargs", "-n", "1", "echo", "-"), "a b\nc\n", 0,
         "- a\n- b\n- c\n", 0, 0);
  expect("xargs -n big", A("xargs", "-n", "10", "echo"), "a\nb\nc\n", 0,
         "a b c\n", 0, 0);

  // Exit status.
  expect("xargs all ok", A("xargs", "sleep"), "0\n1\n", 0, "", 0, 0);
  expect("xargs one fails", A("xargs", "sleep"), "0\nx\n0\n", 1, "", 0,
         "usage: sleep <ticks>\n");
  expect("xargs last fails", A("xargs", "-n", "1", "sleep"), "0 0 bad", 1, "",
         0, "usage: sleep <ticks>\n");
  // A failure in the middle doesn't stop later runs.
  expect("xargs keeps going", A("xargs", "find"), "nosuch x\nft ab\n", 1,
         "ft/ab\nft/b/deep/er/ab\n", 1, "find: cannot open nosuch\n");

  // Bad arguments.
  expect("xargs no cmd", A("xargs"), "", 1, "", 0, u);
  expect("xargs -n no N", A("xargs", "-n"), "", 1, "", 0, u);
  expect("xargs -n no cmd", A("xargs", "-n", "2"), "", 1, "", 0, u);
  expect("xargs -n 0", A("xargs", "-n", "0", "echo"), "", 1, "", 0, u);
  expect("xargs -n -1", A("xargs", "-n", "-1", "echo"), "", 1, "", 0, u);
  expect("xargs -n x", A("xargs", "-n", "x", "echo"), "", 1, "", 0, u);

  // Full path command and fixed args mixed with -n.
  expect("xargs /echo", A("xargs", "/echo", "p", "q"), "r\n", 0, "p q r\n", 0,
         0);
  expect("xargs -n fixed", A("xargs", "-n", "1", "echo", "p", "q"), "r s\n",
         0, "p q r\np q s\n", 0, 0);

  // Input bigger than one 512-byte read: 150 four-char words, 10 per
  // line, so words straddle read boundaries.
  static char big[1024], bigwant[1024], bigwant7[1024];
  char *q, *r;
  p = big;
  q = bigwant;
  r = bigwant7;
  for (i = 0; i < 150; i++) {
    char w[4] = {'w', '0' + i / 100, '0' + i / 10 % 10, '0' + i % 10};
    memmove(p, w, 4);
    p += 4;
    *p++ = (i % 10 == 9) ? '\n' : ' ';
    memmove(q, w, 4);
    q += 4;
    *q++ = (i % 10 == 9) ? '\n' : ' ';
    memmove(r, w, 4);
    r += 4;
    *r++ = (i % 7 == 6 || i == 149) ? '\n' : ' ';
  }
  *p = *q = *r = 0;
  expect("xargs big lines", A("xargs", "echo"), big, 0, bigwant, 0, 0);
  expect("xargs big -n 7", A("xargs", "-n", "7", "echo"), big, 0, bigwant7, 0,
         0);

  // The most fixed args xargs itself can be exec'd with (echo + 29)
  // leaves room for exactly one input word per run.
  expect("xargs fixed max",
         A("xargs", "echo", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
           "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21",
           "22", "23", "24", "25", "26", "27", "28", "29"),
         "a b\n", 0,
         "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 "
         "26 27 28 29 a\n"
         "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 "
         "26 27 28 29 b\n",
         0, 0);

  // MAXARG: exec takes at most MAXARG-1 args plus the 0, so echo + 30
  // words fit in one run and the rest of the line goes to a second run.
  p = in;
  for (i = 0; i < 32; i++) {
    *p++ = 'a' + (i % 26);
    *p++ = ' ';
  }
  *p++ = '\n';
  *p = 0;
  p = want;
  for (i = 0; i < 32; i++) {
    *p++ = 'a' + (i % 26);
    *p++ = (i == 29 || i == 31) ? '\n' : ' ';
  }
  *p = 0;
  expect("xargs MAXARG", A("xargs", "echo"), in, 0, want, 0, 0);
  expect("xargs MAXARG -n", A("xargs", "-n", "40", "echo"), in, 0, want, 0,
         0);
}

// Return 1 if s starts with prefix.
static int
startswith(char *s, char *prefix)
{
  while (*prefix)
    if (*s++ != *prefix++)
      return 0;
  return 1;
}

// Return 1 if needle occurs in s.
static int
contains(char *s, char *needle)
{
  for (; *s; s++)
    if (startswith(s, needle))
      return 1;
  return 0;
}

static void
testps(void)
{
  char *line, *p;
  int tabs, n, st;

  total++;
  st = run(A("ps"), "");
  if (st != 0 || err[0] != 0) {
    failures++;
    printf("FAIL ps status %d stderr [%s]\n", st, err);
  }
  total++;
  if (!startswith(out, "PID\tPPID\tSTATE\tSIZE\tNAME\n")) {
    failures++;
    printf("FAIL ps header [%s]\n", out);
    return;
  }
  // Every row: 5 tab-separated fields, a known state name.
  n = 0;
  for (line = out + 25; *line; line = p + 1) {
    for (p = line, tabs = 0; *p && *p != '\n'; p++)
      if (*p == '\t')
        tabs++;
    *p = 0;
    n++;
    if (tabs != 4 ||
        !(contains(line, "\tused\t") || contains(line, "\tsleeping\t") ||
          contains(line, "\trunnable\t") || contains(line, "\trunning\t") ||
          contains(line, "\tzombie\t"))) {
      failures++;
      printf("FAIL ps row [%s]\n", line);
    }
  }
  if (n < 3) {
    failures++;
    printf("FAIL ps printed only %d rows\n", n);
  }
}

int
main(int argc, char *argv[])
{
  testsleep();
  testfind();
  testxargs();
  testps();
  printf("a1test: %d/%d passed\n", total - failures, total);
  if (failures) {
    printf("a1test: FAILED\n");
    exit(1);
  }
  printf("a1test: OK\n");
  exit(0);
}
