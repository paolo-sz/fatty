#ifndef CHILD_H
#define CHILD_H

#include "std.h"

#include <termios.h>

extern bool logging;

struct term;

struct child
{
  char *home, *cmd;
  string dir;

  pid_t pid;
  int pty_fd;
  struct term* term;
};

#define CHILD_VAR_REF(check)                  \
   if (check) assert(child_p);    \
   char *&home __attribute__((unused)) = child_p->home;          \
   char *&cmd __attribute__((unused)) = child_p->cmd;            \
   string &child_dir __attribute__((unused)) = child_p->dir;     \
   pid_t &pid __attribute__((unused)) = child_p->pid;            \
   int &pty_fd __attribute__((unused)) = child_p->pty_fd;        \
   struct term *&term_p __attribute__((unused)) = child_p->term; \
   struct term &term __attribute__((unused)) = *child_p->term;

#define childerror(...) (childerror)(term_p, ##__VA_ARGS__)
extern void (childerror)(struct term* term_p, char * action, bool from_fork, int errno_code, int code);
#define child_update_charset(...) (child_update_charset)(child_p, ##__VA_ARGS__)
extern void (child_update_charset)(struct child * child_p);
#define child_create(...) (child_create)(child_p, term_p, ##__VA_ARGS__)
extern void (child_create)(struct child* child_p, struct term* term, char *argv[], struct winsize *winp, const char* path);
extern void open_logfile(bool toggling);
extern void toggle_logging(void);
extern void child_free(struct child* child_p);
extern void child_proc(void);
extern void child_terminate(struct child* child_p);
//extern void child_kill(bool point_blank);
#define child_write(...) (child_write)(child_p, ##__VA_ARGS__)
extern void (child_write)(struct child* child_p, const char *, uint len);
#define child_break(...) (child_break)(child_p, ##__VA_ARGS__)
extern void (child_break)(struct child* child_p);
#define child_printf(...) (child_printf)(child_p, ##__VA_ARGS__)
extern void (child_printf)(struct child* child_p, const char * fmt, ...) __attribute__((format(printf, 2, 3)));
#define child_send(...) (child_send)(child_p, ##__VA_ARGS__)
extern void (child_send)(struct child* child_p, const char *, uint len);
#define child_sendw(...) (child_sendw)(child_p, ##__VA_ARGS__)
extern void (child_sendw)(struct child* child_p, const wchar *, uint len);
#define child_resize(...) (child_resize)(child_p, ##__VA_ARGS__)
extern void (child_resize)(struct child* child_p, struct winsize * winp);
extern bool child_is_alive(struct child* child_p);
extern bool child_is_parent(struct child* child_p);
//extern wchar * grandchild_process_list(void);
//extern char * child_tty(void);
extern bool child_is_any_parent();
#define foreground_prog(...) (foreground_prog)(child_p, ##__VA_ARGS__)
extern char * (foreground_prog)(struct child* child_p);  // to be free()d
#define user_command(...) (user_command)(child_p, ##__VA_ARGS__)
extern void (user_command)(struct child* child_p, wstring commands, int n);
#define child_conv_path(...) (child_conv_path)(child_p, ##__VA_ARGS__)
extern wstring (child_conv_path)(struct child* child_p, wstring, bool adjust_dir);
extern void child_fork(struct child* child_p, int argc, char * argv[], int moni, bool config_size);
#define child_set_fork_dir(...) (child_set_fork_dir)(child_p, ##__VA_ARGS__)
extern void (child_set_fork_dir)(struct child* child_p, char *);
extern void setenvi(char * env, int val);
#define child_launch(...) (child_launch)(child_p, ##__VA_ARGS__)
extern void (child_launch)(struct child* child_p, int n, int argc, char * argv[], int moni);

void child_onexit(int sig);
void child_init();

extern int win_fd;
extern int log_fd;

#endif
