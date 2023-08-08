#include "win.hh"

#include <cstdlib>
#include <stdio.h>
#include <algorithm>

#include <cygwin/version.h>
#include <sys/cygwin.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <cstring>
#include <fcntl.h>
#include <utmp.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <cygwin/version.h>

int win_fd;
int log_fd = -1;

extern "C" {
#include "child.h"
#include "winpriv.h"
extern void exit_fatty(int exit_val);

extern int cs_wcstombs(char *s, const wchar *ws, size_t len);


void child_onexit(int sig) {
    for (Tab& tab : win_tabs()) {
        if (tab.chld->pid)
            kill(-tab.chld->pid, SIGHUP);
    }
    signal(sig, SIG_DFL);
    kill(getpid(), sig);
}

static void sigexit(int sig) {
    child_onexit(sig);
}

void child_init() {
    // xterm and urxvt ignore SIGHUP, so let's do the same.
    signal(SIGHUP, SIG_IGN);

    signal(SIGINT, sigexit);
    signal(SIGTERM, sigexit);
    signal(SIGQUIT, sigexit);

    win_fd = open("/dev/windows", O_RDONLY);

    // Open log file if any
    if (cfg.logging) {
      // option Logging=yes => initially open log file if configured
      open_logfile(false);
    }
}

#define patch_319

void child_proc() {
  win_tab_clean();
  if (win_tabs().size() == 0)
    return;

    // this code is ripped from child.c
  for (;;) {
    for (Tab& t : win_tabs()) {
      if (t.terminal->no_scroll)
        continue;
      if (t.terminal->paste_buffer)
        (term_send_paste)(t.terminal.get());
    }

    struct timeval timeout = {0, 100000}, *timeout_p = 0;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(win_fd, &fds);
    int highfd = win_fd;
    for (Tab& t : win_tabs()) {
      if (t.terminal->no_scroll)
        continue;
      if (t.chld->pty_fd > highfd) highfd = t.chld->pty_fd;
      if (t.chld->pty_fd >= 0)
        FD_SET(t.chld->pty_fd, &fds);
#ifndef patch_319
      else
#endif
      if (t.chld->pid) {
        int status;
        if (waitpid(t.chld->pid, &status, WNOHANG) == t.chld->pid) {
          t.chld->pid = 0;

          char *s = 0;
          bool err = true;
          if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            if (code == 0)
              err = false;
            if ((code || cfg.exit_write) /*&& cfg.hold != HOLD_START*/)
              //__ %1$s: client command (e.g. shell) terminated, %2$i: exit code
              asprintf(&s, _("%s: Exit %i"), t.chld->cmd, code);
          }
          else if (WIFSIGNALED(status))
            asprintf(&s, "%s: %s", t.chld->cmd, strsignal(WTERMSIG(status)));

          if (!s && cfg.exit_write) {
            //__ default inline notification if ExitWrite=yes
            s = _("TERMINATED");
          }
          if (s) {
            const char * wsl_pre = "\0337\033[H\033[L";
            const char * wsl_post = "\0338\033[B";
            if (err && support_wsl)
              (term_write)(t.terminal.get(), wsl_pre, strlen(wsl_pre));
            (childerror)(t.terminal.get(), s, false, 0, err ? 41 : 42);
            if (err && support_wsl)
              (term_write)(t.terminal.get(), wsl_post, strlen(wsl_post));
          }

          if (cfg.exit_title && *cfg.exit_title)
            (win_tab_set_title)(t.terminal.get(), (wchar_t *)cfg.exit_title);
        }
#ifdef patch_319
        if (t.chld->pid != 0 && t.chld->pty_fd < 0) // Pty gone, but process still there: keep checking
#else
        else // Pty gone, but process still there: keep checking
#endif
          timeout_p = &timeout;
      }
    }

    if (select(highfd + 1, &fds, 0, 0, timeout_p) > 0) {
      for (Tab& t : win_tabs()) {
        if (t.terminal->no_scroll)
          continue;
        struct child* child_p = t.chld.get();
        if (child_p->pty_fd >= 0 && FD_ISSET(child_p->pty_fd, &fds)) {
          // Pty devices on old Cygwin versions (pre 1005) deliver only 4 bytes
          // at a time, and newer ones or MSYS2 deliver up to 256 at a time.
          // so call read() repeatedly until we have a worthwhile haul.
          // this avoids most partial updates, results in less flickering/tearing.
          static char buf[4096];
          uint len = 0;
#if CYGWIN_VERSION_API_MINOR >= 74
          if (cfg.baud > 0) {
            uint cps = cfg.baud / 10; // 1 start bit, 8 data bits, 1 stop bit
            uint nspc = 2000000000 / cps;

            static ulong prevtime = 0;
            static ulong exceeded = 0;
            static ulong granularity = 0;
            struct timespec tim;
            if (!granularity) {
              clock_getres(CLOCK_MONOTONIC, &tim); // cygwin granularity: 539ns
              granularity = tim.tv_nsec;
            }
            clock_gettime(CLOCK_MONOTONIC, &tim);
            ulong now = tim.tv_sec * (long)1000000000 + tim.tv_nsec;
            //printf("baud %d ns/char %d prev %ld now %ld delta\n", cfg.baud, nspc, prevtime, now);
            if (now < prevtime + nspc) {
              ulong delay = prevtime ? prevtime + nspc - now : 0;
              if (delay < exceeded)
                exceeded -= delay;
              else {
                tim.tv_sec = delay / 1000000000;
                tim.tv_nsec = delay % 1000000000;
                clock_nanosleep(CLOCK_MONOTONIC, 0, &tim, 0);
                clock_gettime(CLOCK_MONOTONIC, &tim);
                ulong then = tim.tv_sec * (long)1000000000 + tim.tv_nsec;
                //printf("nsleep %ld -> %ld\n", delay, then - now);
                if (then - now > delay)
                  exceeded = then - now - delay;
                now = then;
              }
            }
            prevtime = now;

            int ret = read(child_p->pty_fd, buf, 1);
            if (ret > 0)
              len = ret;
          }
          else
#endif
          do {
            int ret = read(child_p->pty_fd, buf + len, sizeof buf - len);
//            trace_line("read", ret, buf + len, ret);
            //if (kb_trace) printf("[%lu] read %d\n", mtime(), ret);
            if (ret > 0)
              len += ret;
            else
              break;
          } while (len < sizeof buf);

          if (len > 0) {
            (term_write)(child_p->term, buf, len);
//            trace_line("twrt", len, buf, len);
            // accelerate keyboard echo if (unechoed) keyboard input is pending
            if (kb_input) {
              kb_input = false;
              if (cfg.display_speedup)
                // undocumented safeguard in case something goes wrong here
                (win_update_term)(child_p->term, false);
            }
            if (log_fd >= 0 && logging)
              write(log_fd, buf, len);
          }
          else {
            child_p->pty_fd = -1;
            (term_hide_cursor)(child_p->term);
          }
        }
        if (FD_ISSET(win_fd, &fds)) {
          win_tab_clean();
          return;
        }
      }
    }
  }
}

void child_terminate(struct child *child_p) {
    pid_t pid = child_p->pid;
    if (pid) {
      kill(-pid, SIGHUP);
      win_callback(500, [pid]() {
          // We are still here even after half a second?
          // Really, lets just die. It would be really annoying not to...
          kill(-pid, SIGKILL);
      });
      child_p->pid = 0;
    }
}

bool child_is_any_parent() {
    std::vector<Tab>& tabs = win_tabs();
    return std::any_of(tabs.begin(), tabs.end(), [](Tab& tab) {
        return child_is_parent(tab.chld.get());
    });
}

}
