#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

#define WANT_ANY 0
#define WANT_FILE 1
#define WANT_DIR 2

// Path printed for the current entry. find walks the tree with
// chdir() and opens entries by their short name, so paths longer
// than the kernel's MAXPATH and trees deeper than NOFILE still work.
static char path[4096];
static char *pattern;
static int want = WANT_ANY;

static void
die(char *msg, char *arg)
{
  fprintf(2, "find: %s %s\n", msg, arg);
  exit(1);
}

// Return 1 if name matches pat, where '*' matches any sequence of
// characters (including an empty one) and everything else is literal.
int
match(char *pat, char *name)
{
  if (*pat == 0)
    return *name == 0;
  if (*pat == '*') {
    while (pat[1] == '*')
      pat++;
    if (match(pat + 1, name))
      return 1;
    return *name != 0 && match(pat, name + 1);
  }
  return *name == *pat && match(pat + 1, name + 1);
}

// Search the current working directory. path holds its printable
// name and has length len.
void
find(int len)
{
  int fd, i, n, cap, nlen;
  struct stat st;
  struct dirent de, *ents;
  char name[DIRSIZ + 1];

  if ((fd = open(".", O_RDONLY)) < 0)
    die("cannot open", path);
  if (fstat(fd, &st) < 0)
    die("cannot stat", path);

  // Read the whole directory first so no fd stays open while recursing.
  cap = st.size / sizeof(de) + 1;
  if ((ents = malloc(cap * sizeof(de))) == 0)
    die("out of memory at", path);
  n = 0;
  while (n < cap && read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;
    ents[n++] = de;
  }
  close(fd);

  for (i = 0; i < n; i++) {
    memmove(name, ents[i].name, DIRSIZ);
    name[DIRSIZ] = 0;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
      continue;

    nlen = strlen(name);
    if (len + 1 + nlen + 1 > sizeof(path))
      die("path too long at", path);
    if (len > 0 && path[len - 1] == '/') {
      memmove(path + len, name, nlen + 1);
      nlen += len;
    } else {
      path[len] = '/';
      memmove(path + len + 1, name, nlen + 1);
      nlen += len + 1;
    }

    if (stat(name, &st) < 0)
      die("cannot stat", path);

    if (match(pattern, name)) {
      if (want == WANT_ANY ||
          (want == WANT_DIR && st.type == T_DIR) ||
          (want == WANT_FILE && (st.type == T_FILE || st.type == T_DEVICE)))
        printf("%s\n", path);
    }

    if (st.type == T_DIR) {
      if (chdir(name) < 0)
        die("cannot open", path);
      find(nlen);
      if (chdir("..") < 0)
        die("cannot return from", path);
    }
    path[len] = 0;
  }
  free(ents);
}

static void
usage(void)
{
  fprintf(2, "usage: find <dir> <pattern> [-type f|d]\n");
  exit(1);
}

int
main(int argc, char *argv[])
{
  int fd;
  struct stat st;

  if (argc == 5) {
    if (strcmp(argv[3], "-type") != 0)
      usage();
    if (strcmp(argv[4], "f") == 0)
      want = WANT_FILE;
    else if (strcmp(argv[4], "d") == 0)
      want = WANT_DIR;
    else
      usage();
  } else if (argc != 3) {
    usage();
  }
  pattern = argv[2];

  if ((fd = open(argv[1], O_RDONLY)) < 0) {
    fprintf(2, "find: cannot open %s\n", argv[1]);
    exit(1);
  }
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", argv[1]);
    close(fd);
    exit(1);
  }
  close(fd);

  // <dir> itself is never printed, so a non-directory has no results.
  if (st.type != T_DIR)
    exit(0);

  if (strlen(argv[1]) + 1 > sizeof(path)) {
    fprintf(2, "find: path too long\n");
    exit(1);
  }
  strcpy(path, argv[1]);
  if (chdir(argv[1]) < 0) {
    fprintf(2, "find: cannot open %s\n", argv[1]);
    exit(1);
  }
  find(strlen(path));
  exit(0);
}
