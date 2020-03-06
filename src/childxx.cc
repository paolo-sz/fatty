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

void child_proc() {
    win_tab_clean();
    if (win_tabs().size() == 0)
      return;

    // this code is ripped from child.c
    for (;;) {
        for (Tab& t : win_tabs()) {
            if (t.terminal->paste_buffer)
                (term_send_paste)(t.terminal.get());
       }

        struct timeval timeout = {0, 100000}, *timeout_p = 0;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(win_fd, &fds);  
        int highfd = win_fd;
        for (Tab& t : win_tabs()) {
            if (t.chld->pty_fd > highfd) highfd = t.chld->pty_fd;
            if (t.chld->pty_fd >= 0) {
                FD_SET(t.chld->pty_fd, &fds);
            } else if (t.chld->pid) {
                int status;
                if (waitpid(t.chld->pid, &status, WNOHANG) == t.chld->pid) {
                    t.chld->pid = 0;
                }
                else {// Pty gone, but process still there: keep checking
                    timeout_p = &timeout;
                }
            }
        }

        if (select(highfd + 1, &fds, 0, 0, timeout_p) > 0) {
            for (Tab& t : win_tabs()) {
                struct child* child_p = t.chld.get();
                if (child_p->pty_fd >= 0 && FD_ISSET(child_p->pty_fd, &fds)) {
#if CYGWIN_VERSION_DLL_MAJOR >= 1005
                    static char buf[4096];
                    int len = read(child_p->pty_fd, buf, sizeof buf);
#else
                    // Pty devices on old Cygwin version deliver only 4 bytes at a time,
                    // so call read() repeatedly until we have a worthwhile haul.
                    static char buf[512];
                    uint len = 0;
                    do {
                        int ret = read(child_p->pty_fd, buf + len, sizeof buf - len);
                        if (ret > 0)
                            len += ret;
                        else
                            break;
                    } while (len < sizeof buf);
#endif
                    if (len > 0) {
                        (term_write)(child_p->term, buf, len);
                        if (log_fd >= 0)
                            write(log_fd, buf, len);
                    } else {
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
