#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define WORDBUF 2048
#define MAXWORD 512

static char *args[MAXARG + 1]; // command, fixed args, then input words
static int nfixed;             // count of command + fixed args
static int nargs;              // current total count in args
static char words[WORDBUF];    // storage for input words of one run
static int wlen;               // bytes used in words
static int status;             // exit status of xargs

static char inbuf[512];
static int inpos, inlen;

static void
usage(void)
{
  fprintf(2, "usage: xargs [-n N] cmd [args...]\n");
  exit(1);
}

// Return the next byte of stdin, or -1 at end of input.
static int
getbyte(void)
{
  if (inpos == inlen) {
    inlen = read(0, inbuf, sizeof(inbuf));
    inpos = 0;
    if (inlen <= 0) {
      inlen = 0;
      return -1;
    }
  }
  return (unsigned char)inbuf[inpos++];
}

// Run the command with the collected words, wait for it, and reset.
static void
run(void)
{
  int pid, xstatus;

  if (nargs == nfixed)
    return;
  args[nargs] = 0;

  pid = fork();
  if (pid < 0) {
    fprintf(2, "xargs: fork failed\n");
    exit(1);
  }
  if (pid == 0) {
    exec(args[0], args);
    fprintf(2, "xargs: exec %s failed\n", args[0]);
    exit(1);
  }
  if (wait(&xstatus) < 0 || xstatus != 0)
    status = 1;

  nargs = nfixed;
  wlen = 0;
}

static int
isspace(int c)
{
  return c == ' ' || c == '\t' || c == '\n';
}

int
main(int argc, char *argv[])
{
  int i, c, n, limit, len;
  char word[MAXWORD];
  char *s;

  i = 1;
  n = 0;
  if (argc > 1 && strcmp(argv[1], "-n") == 0) {
    if (argc < 3 || argv[2][0] == 0)
      usage();
    for (s = argv[2]; *s; s++)
      if (*s < '0' || *s > '9')
        usage();
    n = atoi(argv[2]);
    if (n < 1)
      usage();
    i = 3;
  }
  if (i >= argc)
    usage();
  if (argc - i > MAXARG) {
    fprintf(2, "xargs: too many arguments\n");
    exit(1);
  }

  for (nfixed = 0; i < argc; i++)
    args[nfixed++] = argv[i];
  nargs = nfixed;

  // Words per run: N with -n, otherwise as many as fit.
  limit = MAXARG - nfixed;
  if (n > 0 && n < limit)
    limit = n;
  if (limit < 1) {
    fprintf(2, "xargs: too many arguments\n");
    exit(1);
  }

  c = getbyte();
  while (c >= 0) {
    if (isspace(c)) {
      // Without -n, each line is its own run.
      if (c == '\n' && n == 0)
        run();
      c = getbyte();
      continue;
    }

    // Read one word.
    len = 0;
    while (c >= 0 && !isspace(c)) {
      if (len >= MAXWORD) {
        fprintf(2, "xargs: word too long\n");
        exit(1);
      }
      word[len++] = c;
      c = getbyte();
    }
    if (nargs - nfixed == limit || wlen + len + 1 > WORDBUF)
      run();
    memmove(words + wlen, word, len);
    args[nargs++] = words + wlen;
    wlen += len;
    words[wlen++] = 0;

    if (n > 0 && nargs - nfixed == limit)
      run();
  }
  run();

  exit(status);
}
