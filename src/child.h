#ifndef CHILD_H
#define CHILD_H

#include <sys/termios.h>

struct term;

struct child
{
  char *home, *cmd;

  pid_t pid;
  bool killed;
  int pty_fd;
  struct term* term;
};

extern void child_create(struct child* child, struct term* term,
    char *argv[], struct winsize *winp, const char* path);
extern void child_free(struct child* child);
extern void child_proc();
extern void child_kill();
extern void child_terminate(struct child* child);
extern void child_write(struct child* child, const char *, unsigned int len);
extern void child_printf(struct child* child, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
extern void child_send(struct child* child, const char *, uint len);
extern void child_sendw(struct child* child, const wchar *, uint len);
extern void child_resize(struct child* child, struct winsize *winp);
extern bool child_is_alive(struct child* child);
extern bool child_is_parent(struct child* child);
extern bool child_is_any_parent();
extern wstring child_conv_path(struct child*, wstring);
extern void child_fork(struct child* child, int argc, char *argv[], int moni);

void child_onexit(int sig);
void child_init();

extern int child_win_fd;
extern int child_log_fd;


#endif
