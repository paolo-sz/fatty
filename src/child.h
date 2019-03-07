#ifndef CHILD_H
#define CHILD_H

#include <termios.h>

extern bool icon_is_from_shortcut;
extern bool clone_size_token;
extern bool logging;

struct term;

struct child
{
  char *home, *cmd;
  string dir;

  pid_t pid;
  bool killed;
  int pty_fd;
  struct term* term;
};

extern void child_update_charset(struct child * child);
extern void child_create(struct child* child, struct term* term,
    char *argv[], struct winsize *winp, const char* path);
extern void open_logfile(bool toggling);
extern void toggle_logging(void);
extern void child_free(struct child* child);
extern void child_proc(void);
extern void child_kill(void);
extern void child_terminate(struct child* child);
extern void child_write(struct child* child, const char *, uint len);
extern void child_break(struct child* child);
extern void child_printf(struct child* child, const char * fmt, ...) __attribute__((format(printf, 2, 3)));
extern void child_send(struct child* child, const char *, uint len);
extern void child_sendw(struct child* child, const wchar *, uint len);
extern void child_resize(struct child* child, struct winsize * winp);
extern bool child_is_alive(struct child* child);
extern bool child_is_parent(struct child* child);
extern bool child_is_any_parent();
extern char * foreground_prog(struct child* child);  // to be free()d
extern void user_command(struct child* child, wstring commands, int n);
extern wstring child_conv_path(struct child* child, wstring, bool adjust_dir);
extern void child_fork(struct child* child, int argc, char * argv[], int moni);
extern void child_set_fork_dir(struct child* child, char *);
extern void child_launch(struct child* child, int n, int argc, char * argv[], int moni);

void child_onexit(int sig);
void child_init();

extern int child_win_fd;
extern int child_log_fd;

//extern void child_kill(bool point_blank);
//extern char * child_tty(void);

#endif
