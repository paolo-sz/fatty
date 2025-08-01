// termclip.c (part of FaTTY)
// Copyright 2015 Juho Peltonen
// Based on code from mintty by 2008-23 Andy Koppe, 2024-2025 Thomas Wolff
// Adapted from code from PuTTY-0.60 by Simon Tatham and team.
// Licensed under the terms of the GNU General Public License v3 or later.

#define _GNU_SOURCE

#include <algorithm>

using std::max;

extern "C" {
  
#include "termpriv.h"

#include "win.h"
#include "child.h"
#include "charset.h"

typedef struct {
  size_t capacity; // number of items allocated for text/cattrs
  size_t len;      // number of actual items at text/cattrs (incl. NUL)
  // the text buffer is needed to fill the Unicode clipboard in one chunk
  wchar * text;    // text to copy (eventually null terminated)
  // the attributes part of the buffer is only filled as requested
  bool with_attrs;
  cattr * cattrs;  // matching cattr for each wchar of text
} clip_workbuf;

static void
destroy_clip_workbuf(clip_workbuf * b, bool with_text)
{
  assert(b && b->capacity); // we're only called after get_selection, which always allocates
  if (with_text)
    free(b->text);
  if (b->with_attrs)
    // the attributes part of the buffer was only filled as requested
    free(b->cattrs);
  free(b);
}

// All b members must be 0 initially, ca may be null if the caller doesn't care
static void
clip_addchar(clip_workbuf * b, wchar chr, cattr * ca, bool tabs, ulong sizehint)
{
  // ensure sizehint > 0
  sizehint = max(sizehint, 8UL);

  if (tabs && chr == ' ' && ca && ca->attr & TATTR_CLEAR && ca->attr & ATTR_BOLD) {
    // collapse TAB
    int l0 = b->len;
    while (l0) {
      l0--;
      if (b->text[l0] == ' ' && b->cattrs[l0].attr & TATTR_CLEAR && b->cattrs[l0].attr & ATTR_DIM)
        b->len--;
      else
        break;
    }
    chr = '\t';
  }

  bool err = false;

  if (b->len >= b->capacity) {
    //b->capacity = b->len ? b->len * 2 : 1024;  // x2 strategy, 1K chars initially
    //b->capacity += sizehint;
    //b->capacity = b->capacity ? b->capacity * 3 / 2 : sizehint;
    b->capacity = b->capacity ? b->capacity * 5 / 4 : sizehint;

    wchar * _text = renewn(b->text, b->capacity);
    if (_text)
      b->text = _text;
    else
      err = true;
    if (b->with_attrs) {
      // the attributes part of the buffer is only filled as requested
      cattr * _cattrs = renewn(b->cattrs, b->capacity);
      if (_cattrs)
        b->cattrs = _cattrs;
      else
        err = true;
    }
  }
  if (err) {
    //printf("buf alloc err\n");
    return;
  }

  cattr copattr = ca ? *ca : CATTR_DEFAULT;
  //printf("setting clipbuf[%ld] = %02X\n", b->len, chr);
  if (copattr.attr & TATTR_CLEAR) {
    if (!tabs)
      copattr.attr &= ~(ATTR_BOLD | ATTR_DIM | TATTR_CLEAR);
  }

  b->text[b->len] = chr;
  if (b->with_attrs)
    // the attributes part of the buffer is only filled as requested
    b->cattrs[b->len] = copattr;

  b->len++;
}

#define get_selection(...) (get_selection)(term_p, ##__VA_ARGS__)
// except OOM, guaranteed at least emtpy null terminated wstring and one cattr
static clip_workbuf *
(get_selection)(struct term* term_p, bool attrs, pos start, pos end, bool rect, bool allinline, bool with_tabs)
{
  TERM_VAR_REF(true)
  
  //printf("get_selection attrs %d all %d tabs %d\n", attrs, allinline, with_tabs);

  if (with_tabs)
    attrs = true;  // ensure we can check expanded TABs (#1269)
  clip_workbuf *buf = newn(clip_workbuf, 1);
  *buf = (clip_workbuf){0, 0, 0, attrs, 0};

  // estimate buffer size needed, to give memory allocation increments a hint
  int lines = end.y - start.y;
  long hint = (long)lines * term.cols / 8;
  //printf("get_selection %d...%d (%d)\n", start.y, end.y, lines);
  // check overflow
  if (lines < 0 || hint < 0) {
    //printf("buf start > end %d\n", lines);
    return buf;
  }

  int old_top_x = start.x;    /* needed for rect==1 */

  while (poslt(start, end)) {
    bool nl = false;
    termline *line = fetch_line(start.y);

    if (allinline) {
      // this tweak (commit 975403 "export HTML: consider cursor", 2.9.1)
      // causes cursor artefacts in connection with ClicksPlaceCursor=yes
      // now guarded to cases of HTML copy/export
      if (start.y == term.curs.y) {
        line->chars[term.curs.x].attr.attr |= TATTR_ACTCURS;
      }
    }

    pos nlpos;
    wchar * sixel_clipp = (wchar *)cfg.sixel_clip_char;

   /*
    * nlpos will point at the maximum position on this line we
    * should copy up to. So we start it at the end of the line...
    */
    nlpos.y = start.y;
    nlpos.x = term.cols;
    nlpos.r = false;

   /*
    * ... move it backwards if there's unused space at the end
    * of the line (and also set `nl' if this is the case,
    * because in normal selection mode this means we need a
    * newline at the end)...
    */
    if (allinline) {
      if (poslt(nlpos, end))
        nl = true;
    }
    else if (!(line->lattr & LATTR_WRAPPED)) {
      //printf("pos %d\n", nlpos.x);
      while (nlpos.x && line->chars[nlpos.x - 1].chr == ' ' &&
             (cfg.trim_selection ||
              (line->chars[nlpos.x - 1].attr.attr & TATTR_CLEAR)) &&
             !line->chars[nlpos.x - 1].cc_next && poslt(start, nlpos))
        decpos(nlpos);
      if (poslt(nlpos, end))
        nl = true;
      //printf("pos %d nl %d\n", nlpos.x, nl);
    }
    else {
     /* Strip added space in wrapped line after window resizing */
      //printf("wr x %d w %d\n", nlpos.x, line->wrappos);
      while (nlpos.x > line->wrappos + !(line->lattr & LATTR_WRAPPED2) &&
             line->chars[nlpos.x - 1].chr == ' ' &&
             (cfg.trim_selection ||
              (line->chars[nlpos.x - 1].attr.attr & TATTR_CLEAR)) &&
             !line->chars[nlpos.x - 1].cc_next && poslt(start, nlpos))
        decpos(nlpos);
      //printf("-> x %d w %d\n", nlpos.x, line->wrappos);
    }

   /*
    * ... and then clip it to the terminal x coordinate if
    * we're doing rectangular selection. (In this case we
    * still did the above, so that copying e.g. the right-hand
    * column from a table doesn't fill with spaces on the right.)
    */
    if (rect) {
      if (nlpos.x > end.x)
        nlpos.x = end.x;
      nl = (start.y < end.y);
    }

    while (poslt(start, end) && poslt(start, nlpos)) {
      wchar cbuf[16], *p;
      int x = start.x;

      if (line->chars[x].chr == UCSWIDE) {
        start.x++;
        continue;
      }

      while (1) {
        wchar c = line->chars[x].chr;
        cattr *pca = &line->chars[x].attr;
        if (c == SIXELCH && *cfg.sixel_clip_char) {
          // copy replacement into clipboard
          if (!*sixel_clipp)
            sixel_clipp = (wchar *)cfg.sixel_clip_char;
          c = *sixel_clipp++;
        }
        else
          sixel_clipp = (wchar *)cfg.sixel_clip_char;
        cbuf[0] = c;
        cbuf[1] = 0;

        for (p = cbuf; *p; p++)
          clip_addchar(buf, *p, pca, with_tabs, hint);

        if (line->chars[x].cc_next)
          x += line->chars[x].cc_next;
        else
          break;
      }
      start.x++;
    }
    if (nl) {
      clip_addchar(buf, '\r', 0, false, hint);
      // mark lineend with line attributes, particularly double-width/height
      cattr lcattr = CATTR_DEFAULT;
      lcattr.link = line->lattr;
      clip_addchar(buf, '\n', &lcattr, false, hint);
    }
    start.y++;
    start.x = rect ? old_top_x : 0;

    release_line(line);
  }
  clip_addchar(buf, 0, 0, false, hint);
  //printf("get_selection done\n");
  return buf;
}

#define get_sel_str(...) (get_sel_str)(term_p, ##__VA_ARGS__)
static wchar *
(get_sel_str)(struct term* term_p, pos start, pos end, bool rect, bool allinline, bool with_tabs)
{
  TERM_VAR_REF(true)
  
  clip_workbuf * buf = get_selection(false, start, end, rect, allinline, with_tabs);
  wchar * selstr = buf->text;
  destroy_clip_workbuf(buf, false);
  return selstr;
}

void
(term_copy_as)(struct term* term_p, char what)
{
  TERM_VAR_REF(true)
  
  if (!term.selected)
    return;

  bool with_tabs = what == 'T' || ((!what || what == 't') && cfg.copy_tabs);
  if (what == 'T' || what == 'p') // map "text with TABs" and "plain" to text
    what = 't';
  clip_workbuf *buf = get_selection(true, term.sel_start, term.sel_end, term.sel_rect,
                                    false, with_tabs);
  // for CopyAsHTML, get_selection will be called another time
  // but with different parameters
  win_copy_as(buf->text, buf->cattrs, buf->len, what);
  destroy_clip_workbuf(buf, true);
}

void
(term_copy)(struct term* term_p)
{
  term_copy_as(0);
}

void
(term_open)(struct term* term_p)
{
  TERM_VAR_REF(true)
  
  if (!term.selected)
    return;

  wchar * selstr = get_sel_str(term.sel_start, term.sel_end, term.sel_rect, false, false);

  // Don't bother opening if it's all whitespace.
  wchar * p = selstr;
  while (iswspace(*p))
    p++;

  if (*p)
    win_open(selstr, true);  // win_open frees its argument
  else
    free(selstr);
}

static bool filter_NUL;
static char filter[69];

#define set_filter(...) (set_filter)(child_p, ##__VA_ARGS__)
static void
(set_filter)(struct child* child_p, string s)
{
  CHILD_VAR_REF(true)

  auto do_filter = [&](string tag) -> bool
  {
#if CYGWIN_VERSION_API_MINOR >= 171
    char * match = strcasestr(s, tag);
#else
    char * match = strstr(s, tag);
#endif
    //return match;  // a bit simplistic, we should probably properly parse...
    if (!match)
      return false;
    return (match == s || *(match - 1) < '@')
           && match[strlen(tag)] < '@';
  };

  filter_NUL = false;
  *filter = 0;
  if (do_filter("C0")) {
    filter_NUL = true;
    strcat(filter, "\t\n\r");
  }
  else {
    if (do_filter("BS")) strcat(filter, "\b");
    if (do_filter("HT")) strcat(filter, "\t");
    if (do_filter("NL")) strcat(filter, "\n");
    if (do_filter("CR")) strcat(filter, "\r");
    if (do_filter("FF")) strcat(filter, "\f");
    if (do_filter("ESC")) strcat(filter, "\e");
  }
  if (do_filter("DEL"))
    strcat(filter, "\177");
  if (do_filter("C1")) {
    filter_NUL = true;
    strcat(filter, "\200\201\202\203\204\205\206\207"
                   "\210\211\212\213\214\215\216\217"
                   "\220\221\222\223\224\225\226\227"
                   "\230\231\232\233\234\235\236\237");
  }
  if (do_filter("STTY")) {
    int i = strlen(filter);
    auto addchar = [&](wchar c)
    {
      if (c)
        filter[i++] = c;
      else
        filter_NUL = true;
    };
    uchar * c_cc = child_termios_chars();
    addchar(c_cc[VINTR]);
    addchar(c_cc[VQUIT]);
    addchar(c_cc[VSUSP]);
    addchar(c_cc[VSWTC]);
    filter[i] = 0;
  }
}

static bool
isin_filter(wchar c)
{
  if (c & 0xFFFFFF00)
    return false;
  if (!c)
    return filter_NUL;
  return strchr(filter, c);
}

void
(term_paste)(struct term* term_p, wchar *data, uint len, bool all)
{
  TERM_VAR_REF(true)
  
  // set/refresh list of characters to be filtered;
  // stty settings may have changed
  set_filter(cfg.filter_paste);

  if (cfg.confirm_multi_line_pasting
      && !(strchr(filter, '\r') && strchr(filter, '\n')))
  {
    // check multi-line pasting
    bool multi_line = false;
    for (uint i = 0; i < len; i++) {
      if (data[i] == '\r' || data[i] == '\n') {
        multi_line = true;
        break;
      }
    }
    if (multi_line
        && !win_confirm_text(data, const_cast<wchar *>(W("Multi-line pasting – confirm?"))))
    {
      return;
    }
  }

  term_cancel_paste();

  uint size = len;
  term.paste_buffer = newn(wchar, len);
  term.paste_len = term.paste_pos = 0;

  bool bracketed_paste_split_by_line = term.bracketed_paste 
       && cfg.bracketed_paste_split
       && (cfg.bracketed_paste_split > 1 || !term.on_alt_screen);

  // Copy data to the paste buffer, converting both Windows-style \r\n and
  // Unix-style \n line endings to \r, because that's what the Enter key sends.
  for (uint i = 0; i < len; i++) {
    // swallow closing paste bracket if included in clipboard contents,
    // in order to prevent (malicious) premature end of bracketing
    if (term.bracketed_paste) {
      if (i + 6 <= len && wcsncmp(W("\e[201~"), &data[i], 6) == 0) {
        i += 6 - 1;
        continue;
      }
    }

    wchar wc = data[i];
    if (wc == '\n')
      wc = '\r';
    if (!all && *cfg.filter_paste && isin_filter(wc))
      wc = ' ';

    if (data[i] != '\n')
      term.paste_buffer[term.paste_len++] = wc;
    else if (i == 0 || data[i - 1] != '\r')
      term.paste_buffer[term.paste_len++] = wc;
    else
      continue;

    // split bracket embedding by line
    if (bracketed_paste_split_by_line && wc == '\r' && i + 1 < len
     && (i + 2 != len || 0 != wcsncmp(&data[i], W("\r\n"), 2))
       )
    {
      size += 12;
      term.paste_buffer = renewn(term.paste_buffer, size);
      wcsncpy(&term.paste_buffer[term.paste_len], W("\e[201~\e[200~"), 12);
      term.paste_len += 12;
    }
  }

  if (term.bracketed_paste)
    child_write("\e[200~", 6);
  term_send_paste();
}

void
(term_cancel_paste)(struct term* term_p)
{
  TERM_VAR_REF(true)
  
  if (term.paste_buffer) {
    free(term.paste_buffer);
    term.paste_buffer = 0;
    if (term.bracketed_paste)
      child_write("\e[201~", 6);
  }
}

void
(term_send_paste)(struct term* term_p)
{
  TERM_VAR_REF(true)
  
  int i = term.paste_pos;
  /* We must not feed more than MAXPASTEMAX bytes into the pty in one chunk 
     or it will block on the receiving side (write() does not return).
   */
#define MAXPASTEMAX 7819
#define PASTEMAX 2222
  while (i < term.paste_len && i - term.paste_pos < PASTEMAX
         && term.paste_buffer[i++] != '\r'
        )
    ;
  if (i < term.paste_len && is_high_surrogate(term.paste_buffer[i]))
    i++;
  //printf("term_send_paste pos %d @ %d (len %d)\n", term.paste_pos, i, term.paste_len);
  child_sendw(term.paste_buffer + term.paste_pos, i - term.paste_pos);
  if (i < term.paste_len) {
    term.paste_pos = i;
    // if only part of the paste buffer has been written to the child,
    // the current strategy is to leave the rest pending for on-demand 
    // invocation of term_send_paste from child_proc within the main loop,
    // however, that causes partial loss of large paste contents;
    // worse, without the PASTEMAX limitation, if long contents without 
    // lineends is pasted, the terminal stalls (#810);
    // attempts to replace the pending strategy with looping here (to 
    // paste the whole contents) were not successful to solve the stalling
  }
  else
    term_cancel_paste();
}

void
(term_select_all)(struct term* term_p)
{
  TERM_VAR_REF(true)
  
  term.sel_start = (pos){-sblines(), 0, 0, 0, false};
  term.sel_end = (pos){term_last_nonempty_line(), term.cols, 0, 0, true};
  term.selected = true;
  term.selection_eq_clipboard = false;
  if (cfg.copy_on_select)
    term_copy();
}

#define dont_debug_user_cmd_clip

#define term_get_text(...) (term_get_text)(term_p, ##__VA_ARGS__)
static wchar *
(term_get_text)(struct term* term_p, bool all, bool screen, bool command)
{
  TERM_VAR_REF(true)
  
  pos start;
  pos end;
  bool rect = false;

  if (command) {
    int sbtop = -sblines();
    int y = term_last_nonempty_line();
    bool skipprompt = true;  // skip upper lines of multi-line prompt

    if (y < sbtop) {
      y = sbtop;
      end = (pos){y, 0, 0, 0, false};
    }
    else {
      termline * line = fetch_line(y);
      if (line->lattr & LATTR_MARKED) {
        //printf("incr %d (sbtop %d/%d rows %d)\n", y, sbtop, term.sblines, term.rows);
        if (y > sbtop) {
          y--;
          end = (pos){y, term.cols, 0, 0, false};
          release_line(line);
          line = fetch_line(y);
          if (line->lattr & LATTR_MARKED) {
            y++;
            release_line(line);
            line = fetch_line(y);
          }
        }
        else {
          end = (pos){y, 0, 0, 0, false};
        }
      }
      else {
        skipprompt = line->lattr & LATTR_UNMARKED;
        end = (pos){y, term.cols, 0, 0, false};
      }

      if (line->lattr & LATTR_UNMARKED)
        end = (pos){y, 0, 0, 0, false};
      release_line(line);
    }

    int yok = y;
    while (y-- > sbtop) {
      termline * line = fetch_line(y);
#ifdef debug_user_cmd_clip
      printf("y %d skip %d marked %X\n", y, skipprompt, line->lattr & (LATTR_UNMARKED | LATTR_MARKED));
#endif
      if (skipprompt && (line->lattr & LATTR_UNMARKED))
        end = (pos){y, 0, 0, 0, false};
      else
        skipprompt = false;
      if (line->lattr & LATTR_MARKED) {
        release_line(line);
        break;
      }
      release_line(line);
      yok = y;
    }
    start = (pos){yok, 0, 0, 0, false};
#ifdef debug_user_cmd_clip
    printf("%d:%d...%d:%d\n", start.y, start.x, end.y, end.x);
#endif
  }
  else if (screen) {
    start = (pos){term.disptop, 0, 0, 0, false};
    end = (pos){term_last_nonempty_line(), term.cols, 0, 0, false};
  }
  else if (all) {
    start = (pos){-sblines(), 0, 0, 0, false};
    end = (pos){term_last_nonempty_line(), term.cols, 0, 0, false};
  }
  else if (!term.selected) {
    return wcsdup(W(""));
  }
  else {
    start = term.sel_start;
    end = term.sel_end;
    rect = term.sel_rect;
  }

  return get_sel_str(start, end, rect, false, cfg.copy_tabs);
}

void
(term_cmd)(struct term* term_p, char * cmd)
{
  TERM_VAR_REF(true)
  
  // provide scrollback buffer
  wchar * wsel = term_get_text(true, false, false);
  char * sel = cs__wcstombs(wsel);
  free(wsel);
  setenv("FATTY_BUFFER", sel, true);
  free(sel);
  // provide current selection
  wsel = term_get_text(false, false, false);
  sel = cs__wcstombs(wsel);
  free(wsel);
  setenv("FATTY_SELECT", sel, true);
  free(sel);
  // provide current screen
  wsel = term_get_text(false, true, false);
  sel = cs__wcstombs(wsel);
  free(wsel);
  setenv("FATTY_SCREEN", sel, true);
  free(sel);
  // provide last command output
  wsel = term_get_text(false, false, true);
  sel = cs__wcstombs(wsel);
  free(wsel);
  setenv("FATTY_OUTPUT", sel, true);
  free(sel);
  // provide window title
  char * ttl = win_get_title();
  setenv("FATTY_TITLE", ttl, true);
  free(ttl);

  char * path0 = 0;
  char * path1 = 0;
  if (*cfg.user_commands_path) {
    path0 = getenv("PATH");
    path1 = cs__wcstombs(cfg.user_commands_path);
    char * ph = strstr(path1, "%s");
    if (ph && !strchr(ph + 1, '%')) {
      char * path2 = asform(path1, path0);
      free(path1);
      path1 = path2;
    }
    setenv("PATH", path1, true);
  }
  FILE * cmdf = popen(cmd, "r");
  unsetenv("FATTY_TITLE");
  unsetenv("FATTY_OUTPUT");
  unsetenv("FATTY_SCREEN");
  unsetenv("FATTY_SELECT");
  unsetenv("FATTY_BUFFER");
  if (cmdf) {
    if (term.bracketed_paste)
      child_write("\e[200~", 6);
    char line[222];
    while (fgets(line, sizeof line, cmdf)) {
      child_send(line, strlen(line));
    }
    pclose(cmdf);
    if (term.bracketed_paste)
      child_write("\e[201~", 6);
  }
  if (path0)
    setenv("PATH", path0, true);
  if (path1)
    free(path1);
}

#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include "winpriv.h"  // PADDING

#define term_create_html(...) (term_create_html)(term_p, ##__VA_ARGS__)
static char *
(term_create_html)(struct term* term_p, FILE * hf, int level)
{
  TERM_VAR_REF(true)
  
  char * hbuf = hf ? 0 : strdup("");
  size_t hbuf_len = 0;
  size_t hbuf_cap = 0;
  auto
  hprintf = [&](FILE * hf, const char * fmt, ...)
  {
    char * buf;
    va_list va;
    va_start(va, fmt);
    int len = vasprintf(&buf, fmt, va);
    va_end(va);
    if (hf)
      fprintf(hf, "%s", buf);
    else {
      if (hbuf_len + len > hbuf_cap) {
        hbuf_cap = hbuf_cap ? hbuf_cap * 5 / 4 : 5555;
        hbuf = renewn(hbuf, hbuf_cap + 1);
      }

      //strcat(hbuf, buf);
      strcpy(hbuf + hbuf_len, buf);
      hbuf_len += len;
    }
    free(buf);
  };

  pos start = term.sel_start;
  pos end = term.sel_end;
  bool rect = term.sel_rect;
  if (!term.selected) {
    start = (pos){term.disptop, 0, 0, 0, false};
    end = (pos){term.disptop + term.rows - 1, term.cols, 0, 0, false};
    rect = false;
  }

  bool enhtml = true;  // compatibility enhanced HTML

  char * font_name = cs__wcstoutf(cfg.font.name);
  colour fg_colour = win_get_colour(FG_COLOUR_I);
  colour bg_colour = win_get_colour(BG_COLOUR_I);
  colour bold_colour = win_get_colour(BOLD_COLOUR_I);
  colour blink_colour = win_get_colour(BLINK_COLOUR_I);
  hprintf(hf,
    "<head>\n"
    "  <meta name='generator' content='fatty'/>\n"
    "  <meta http-equiv='Content-Type' content='text/html; charset=UTF-8'/>\n"
    "  <title>fatty screen dump</title>\n"
    "  <link rel='stylesheet' type='text/css' href='xterm.css'/>\n"
    "  <link rel='stylesheet' type='text/css' href='fatty.css'/>\n"
    //"  <script type='text/javascript' language='JavaScript' src='emoji.js'></script>\n"
    "  <style type='text/css'>\n"
    "  #vt100 pre { font-family: inherit; margin: 0; padding: 0; }\n"
    );
  if (level >= 3)
    hprintf(hf, "  body.fatty { margin: 0; padding: 0; }\n");
  hprintf(hf, "  .super, .sub, .small { line-height: 0; font-size: 0.7em; letter-spacing: 0.3em; }\n");
  hprintf(hf, "  .super { vertical-align: super; }\n");
  hprintf(hf, "  .sub { vertical-align: sub; }\n");
  hprintf(hf, "  .double-width {display: inline-block; line-height: 1.2; transform-origin: left; transform: scale(2, 1);}\n");
  hprintf(hf, "  .double-height-top {display: inline-block; line-height: 2.4; transform-origin: left; transform: scale(2, 2);}\n");
  hprintf(hf, "  .double-height-bottom {display: none;}\n");
  hprintf(hf, "  #vt100 span {\n");
  if (level >= 2) {
    // font needed in <span> for some tools (e.g. Powerpoint)
    hprintf(hf,
      "    font-family: '%s', 'Lucida Console ', 'Consolas', monospace;\n"
                       // ? 'Lucida Sans Typewriter', 'Courier New', 'Courier'
      , font_name);
    if (cfg.underl_colour != (colour)-1)
      hprintf(hf, "    text-decoration-color: #%02X%02X%02X;\n",
              red(cfg.underl_colour), green(cfg.underl_colour), blue(cfg.underl_colour));
  }
  free(font_name);
  hprintf(hf, "  }\n");

  hprintf(hf,
    "  #vt100 {\n"
    "    border: 0px solid;\n"
    "    padding: %dpx;\n"
    , PADDING);
  if (level >= 2) {
    hprintf(hf,
      "    line-height: %d%%;\n"
      "    font-size: %dpt;\n"
      , line_scale, font_size);
  }

  if (level >= 3) {
    if (*cfg.background && !term.selected) {
      wstring wbg = cfg.background;
      bool tiled = *wbg == '*';
      if (*wbg == '*' || *wbg == '_')
        wbg++;
      char * bg = cs__wcstoutf(wbg);
      int alpha = -1;
      char * salpha = strrchr(bg, ',');
      if (salpha) {
        char tmp_char = 0;
        *salpha = 0;
        salpha++;
        sscanf(salpha, "%u%c", &alpha, &tmp_char);
      }

      if (alpha >= 0) {
        hprintf(hf, "  }\n");
        hprintf(hf, "  #vt100 pre {\n");
        hprintf(hf, "    background-color: rgba(%d, %d, %d, %.3f);\n",
                red(bg_colour), green(bg_colour), blue(bg_colour),
                (255.0 - alpha) / 255);
        hprintf(hf, "  }\n");
        hprintf(hf, "  .background {\n");
      }

      char * bgg = guardpath(bg, 4);
      if (bgg) {
        wchar * bgw = path_posix_to_win_w(bgg);
        free(bgg);
        free(bg);
        bg = cs__wcstoutf(bgw);
        free(bgw);
        char * cc = bg;
        while (*cc) {
          if (*cc == '\\')
            *cc = '/';
          cc++;
        }

        hprintf(hf, "    background-image: url('file:///%s');\n", bg);
      }

      if (!tiled) {
        hprintf(hf, "    background-attachment: no-repeat;\n");
        hprintf(hf, "    background-size: 100%% 100%%;\n");
      }

      free(bg);

      if (alpha < 0) {
        hprintf(hf, "  }\n");
        hprintf(hf, "  #vt100 pre {\n");
      }
    }
    else
    {
      hprintf(hf, "  }\n");
      hprintf(hf, "  #vt100 pre {\n");
      hprintf(hf, "    background-color: #%02X%02X%02X;\n",
              red(bg_colour), green(bg_colour), blue(bg_colour));
    }
    // add style for <pre>
    // default color needed here for some tools (e.g. Powerpoint)
    hprintf(hf, "    color: #%02X%02X%02X;\n",
            red(fg_colour), green(fg_colour), blue(fg_colour));
  }

#ifdef float_left
  // float needed here to avoid placement left of previous text (Thunderbird)
  // this cannot be reproduced anymore; dropped (#900)
  if (level >= 3) {
    hprintf(hf, "    float: left;\n");
#endif
  hprintf(hf, "  }\n");

  // add xterm-compatible style classes for some text attributes
  hprintf(hf, "  .bd { font-weight: bold }\n");
  hprintf(hf, "  .it { font-style: italic }\n");
  hprintf(hf, "  .ul { text-decoration-line: underline }\n");
  hprintf(hf, "  .st { text-decoration-line: line-through }\n");
  hprintf(hf, "  .lu { text-decoration-line: line-through underline }\n");
  if (bold_colour != (colour)-1)
    hprintf(hf, "  .bold-color { color: #%02X%02X%02X }\n",
            red(bold_colour), green(bold_colour), blue(bold_colour));
  else if (blink_colour != (colour)-1)
    hprintf(hf, "  .blink-color { color: #%02X%02X%02X }\n",
            red(blink_colour), green(blink_colour), blue(blink_colour));
  for (int i = 0; i < 16; i++) {
    colour ansii = win_get_colour((colour_i)(ANSI0 + i));
    uchar r = red(ansii), g = green(ansii), b = blue(ansii);
    hprintf(hf, "  .fg-color%d { color: #%02X%02X%02X }"
                " .bg-color%d { background-color: #%02X%02X%02X }\n",
                i, r, g, b, i, r, g, b);
  }
  colour cursor_colour = win_get_colour(CURSOR_COLOUR_I);
  hprintf(hf, "  #cursor { background-color: #%02X%02X%02X }\n",
          red(cursor_colour), green(cursor_colour), blue(cursor_colour));

  if (level >= 2) {
    for (int i = 1; i <= 10; i++)
      if (*cfg.fontfams[i].name) {
        char * fn = cs__wcstoutf(cfg.fontfams[i].name);
        hprintf(hf, "  .font%d { font-family: '%s' }\n", i, fn);
        free(fn);
      }
    if (!*cfg.fontfams[10].name)
      hprintf(hf, "  .font10 { font-family: 'F25 Blackletter Typewriter' }\n");
  }

  hprintf(hf, "  </style>\n");
  hprintf(hf, "  <script>\n");
  hprintf(hf, "  var b1 = 500; var b2 = 300;\n");
  hprintf(hf, "  function visib (tag, state, timeout) {\n");
  hprintf(hf, "    var bl = document.getElementsByName(tag);\n");
  hprintf(hf, "    var vv; if (state) vv = 'visible'; else vv = 'hidden';\n");
  hprintf(hf, "    var i;\n");
  hprintf(hf, "    for (i = 0; i < bl.length; i++) {\n");
  hprintf(hf, "      bl[i].style.visibility = vv;\n");
  hprintf(hf, "    }\n");
  hprintf(hf, "    window.setTimeout ('visib (\"' + tag + '\", ' + !state + ', ' + timeout + ')', timeout);\n");
  hprintf(hf, "  }\n");
  hprintf(hf, "  function setup () {\n");
  hprintf(hf, "    window.setTimeout ('visib (\"blink\", 0, b1)', b1);\n");
  hprintf(hf, "    window.setTimeout ('visib (\"rapid\", 0, b2)', b2);\n");
  hprintf(hf, "  }\n");
  hprintf(hf, "  </script>\n");
  hprintf(hf, "</head>\n\n");
  hprintf(hf, "<body class=fatty onload='setup();'>\n");
  //hprintf(hf, "  <table border=0 cellpadding=0 cellspacing=0><tr><td>\n");
  hprintf(hf, "  <div class=background id='vt100'>\n");
  hprintf(hf, "   <pre\n>");

  clip_workbuf * buf = get_selection(true, start, end, rect, level >= 3, false);
  int i0 = 0;
  bool odd = true;
  bool new_line = true;
  ushort lattr = LATTR_NORM;
  for (uint i = 0; i < buf->len; i++) {
    if (!buf->text[i] || buf->text[i] == '\r' || buf->text[i] == '\n'
        // buf->cattrs[i] ~!= buf->cattrs[i0] ?
        // we need to check more than termattrs_equal_fg
        // but less than termchars_equal_override
# define IGNATTR (TATTR_WIDE | TATTR_COMBINING)
        || (buf->cattrs[i].attr & ~IGNATTR) != (buf->cattrs[i0].attr & ~IGNATTR)
        || buf->cattrs[i].truefg != buf->cattrs[i0].truefg
        || buf->cattrs[i].truebg != buf->cattrs[i0].truebg
        || buf->cattrs[i].ulcolr != buf->cattrs[i0].ulcolr
       )
    {
      if (new_line) {
        wchar * nl = wcschr(&buf->text[i], '\n');
        if (nl) {
          int offset = nl - &buf->text[i];
          lattr = (ushort)buf->cattrs[i + offset].link & LATTR_MODE;
        }
        else
          lattr = LATTR_NORM;
        if (lattr)
          hprintf(hf, "<div class='double-%s'>",
                      lattr == LATTR_WIDE ? "width" :
                      lattr == LATTR_TOP ? "height-top" : "height-bottom");
        new_line = false;
      }

      // flush chunk with equal attributes
      hprintf(hf, "<span class='%s", odd ? "od" : "ev");

      cattr * ca = &buf->cattrs[i0];
      int fgi = (ca->attr & ATTR_FGMASK) >> ATTR_FGSHIFT;
      int bgi = (ca->attr & ATTR_BGMASK) >> ATTR_BGSHIFT;
      bool dim = ca->attr & ATTR_DIM;
      bool rev = ca->attr & ATTR_REVERSE;

      // colour setup preparations;
      // we could perhaps reuse apply_attr_colour here, but again 
      // the situation is specific: some terminal handling (manual bolding) 
      // is not applicable in HTML export, and we do not want to simply 
      // always retrieve a plain colour value because we want to specify 
      // colour style or class only if the respective default is overridden
      colour fg = fgi >= TRUE_COLOUR ? ca->truefg : win_get_colour((colour_i)(fgi));
      colour bg = bgi >= TRUE_COLOUR ? ca->truebg : win_get_colour((colour_i)(bgi));
      // separate ANSI values subject to BoldAsColour
      int fga = fgi >= ANSI0 ? fgi & 0xFF : 999;
      int bga = bgi >= ANSI0 ? bgi & 0xFF : 999;
      if ((ca->attr & ATTR_BOLD) && fga < 8 && term.enable_bold_colour && !rev) {
        if (bold_colour != (colour)-1)
          fg = bold_colour;
      }
      else if ((ca->attr & (ATTR_BLINK | ATTR_BLINK2)) && term.enable_blink_colour) {
        if (blink_colour != (colour)-1)
          fg = blink_colour;
      }
      if (dim) {
        fg = ((fg & 0xFEFEFEFE) >> 1)
             // dim against terminal bg (as in apply_attr_colour)
             + ((win_get_colour(BG_COLOUR_I) & 0xFEFEFEFE) >> 1);
      }
      if (rev) {
        fgi ^= bgi; fga ^= bga; fg ^= bg;
        bgi ^= fgi; bga ^= fga; bg ^= fg;
        fgi ^= bgi; fga ^= bga; fg ^= bg;
      }
      cattr ac = apply_attr_colour(*ca, ACM_TERM);
      fg = ac.truefg;
      bg = ac.truebg;

      // add marker classes
      if (ca->attr & ATTR_FRAMED)
        hprintf(hf, " emoji");  // mark emoji style

      // add subscript or superscript
      if ((ca->attr & (ATTR_SUBSCR | ATTR_SUPERSCR)) == (ATTR_SUBSCR | ATTR_SUPERSCR))
        hprintf(hf, " small");
      else if (ca->attr & ATTR_SUBSCR)
        hprintf(hf, " sub");
      else if (ca->attr & ATTR_SUPERSCR)
        hprintf(hf, " super");

      // style adding function
      bool with_style = false;
      auto add_style = [&](char * s) {
        if (!with_style) {
          hprintf(hf, "' style='%s", s);
          with_style = true;
        }
        else
          hprintf(hf, " %s", s);
      };
      auto add_color = [&](char * pre, int col) {
        colour ansii = win_get_colour((colour_i)(ANSI0 + col));
        uchar r = red(ansii), g = green(ansii), b = blue(ansii);
        add_style(const_cast<char *>(""));
        hprintf(hf, "%scolor: #%02X%02X%02X;", pre, r, g, b);
      };

      // add style classes or resolved styles;
      // explicit style= attributes instead of xterm-compatible classes
      // are used for the sake of tools that do not take styles by class
      // (Powerpoint; Word would take id= but not class=)
      if (ca->attr & ATTR_BOLD) {
        if (enhtml)
          add_style(const_cast<char *>("font-weight: bold;"));
        else
          hprintf(hf, " bd");
      }
      if (ca->attr & ATTR_ITALIC) {
        if (enhtml)
          add_style(const_cast<char *>("font-style: italic;"));
        else
          hprintf(hf, " it");
      }
      if (!enhtml) {
        if ((ca->attr & (ATTR_UNDER | ATTR_STRIKEOUT)) == (ATTR_UNDER | ATTR_STRIKEOUT))
          hprintf(hf, " lu");
        else if (ca->attr & ATTR_STRIKEOUT)
          hprintf(hf, " st");
        else if (ca->attr & UNDER_MASK)
          hprintf(hf, " ul");
      }
      int findex = (ca->attr & FONTFAM_MASK) >> ATTR_FONTFAM_SHIFT;
      if (findex > 10)
        findex = 0;
      if (findex) {
        if (enhtml) {
          if (*cfg.fontfams[findex].name || findex == 10) {
            add_style(const_cast<char *>("font-family: "));
            if (*cfg.fontfams[findex].name) {
              char * fn = cs__wcstoutf(cfg.fontfams[findex].name);
              hprintf(hf, "\"%s\";", fn);
              free(fn);
            }
            else
              hprintf(hf, "\"F25 Blackletter Typewriter\";");
          }
        }
        else
          hprintf(hf, " font%d", findex);
      }

      // catch and verify predefined colours and apply their colour classes
      if (fgi == FG_COLOUR_I) {
        if ((ca->attr & ATTR_BOLD) && term.enable_bold_colour) {
          if (fg == bold_colour) {
            if (enhtml) {
              add_style(const_cast<char *>("color: "));
              hprintf(hf, "#%02X%02X%02X;",
                      red(bold_colour), green(bold_colour), blue(bold_colour));
            }
            else
              hprintf(hf, " bold-color");
            fg = (colour)-1;
          }
        }
        else if (ca->attr & (ATTR_BLINK | ATTR_BLINK2) && term.enable_blink_colour) {
          if (fg == blink_colour) {
            if (enhtml) {
              add_style(const_cast<char *>("color: "));
              hprintf(hf, "#%02X%02X%02X;",
                      red(blink_colour), green(blink_colour), blue(blink_colour));
            }
            else
              hprintf(hf, " blink-color");
            fg = (colour)-1;
          }
        }
        else if (fg == fg_colour)
          fg = (colour)-1;
      }
      else if (fga < 8 && cfg.bold_as_colour && (ca->attr & ATTR_BOLD)
               && fg == win_get_colour((colour_i)(ANSI0 + fga + 8))
              )
      {
        if (enhtml)
          add_color(const_cast<char *>(""), fga + 8);
        else
          hprintf(hf, " fg-color%d", fga + 8);
        fg = (colour)-1;
      }
      else if (fga < 16 && fg == win_get_colour((colour_i)(ANSI0 + fga))) {
        if (enhtml)
          add_color(const_cast<char *>(""), fga);
        else
          hprintf(hf, " fg-color%d", fga);
        fg = (colour)-1;
      }
      if (bgi == BG_COLOUR_I && bg == bg_colour)
        bg = (colour)-1;
      else if (bga < 16 && bg == win_get_colour((colour_i)(ANSI0 + bga))) {
        if (enhtml)
          add_color(const_cast<char *>("background-"), bga);
        else
          hprintf(hf, " bg-color%d", bga);
        bg = (colour)-1;
      }

      // add individual styles

      // add individual colours, or fix unmatched colours
      if (fg != (colour)-1) {
        uchar r = red(fg), g = green(fg), b = blue(fg);
        add_style(const_cast<char *>(""));
        hprintf(hf, "color: #%02X%02X%02X;", r, g, b);
      }
      if (bg != (colour)-1) {
        uchar r = red(bg), g = green(bg), b = blue(bg);
        add_style(const_cast<char *>(""));
        hprintf(hf, "background-color: #%02X%02X%02X;", r, g, b);
      }

      if (enhtml && (ca->attr & (UNDER_MASK | ATTR_STRIKEOUT | ATTR_OVERL))) {
        // add explicit style= lining attributes for the sake of tools 
        // that do not take styles by class (Powerpoint)
        add_style(const_cast<char *>("text-decoration:"));
        if (ca->attr & UNDER_MASK)
          hprintf(hf, " underline");
        if (ca->attr & ATTR_STRIKEOUT)
          hprintf(hf, " line-through");
        if (ca->attr & ATTR_OVERL)
          hprintf(hf, " overline");
        hprintf(hf, ";");
      }
      else if (ca->attr & ATTR_OVERL) {
        add_style(const_cast<char *>("text-decoration-line: overline"));
        if (ca->attr & ATTR_STRIKEOUT)
          hprintf(hf, " line-through");
        if (ca->attr & ATTR_UNDER)
          hprintf(hf, " underline");
        hprintf(hf, ";");
      }
      if (ca->attr & ATTR_BROKENUND)
        if (ca->attr & ATTR_DOUBLYUND)
          add_style(const_cast<char *>("text-decoration-style: dashed;"));
        else
          add_style(const_cast<char *>("text-decoration-style: dotted;"));
      else if ((ca->attr & UNDER_MASK) == ATTR_CURLYUND)
        add_style(const_cast<char *>("text-decoration-style: wavy;"));
      else if ((ca->attr & UNDER_MASK) == ATTR_DOUBLYUND)
        add_style(const_cast<char *>("text-decoration-style: double;"));

      colour ul = (ca->attr & ATTR_ULCOLOUR) ? ca->ulcolr : cfg.underl_colour;
      if (ul != (colour)-1 && (ca->attr & (UNDER_MASK | ATTR_STRIKEOUT | ATTR_OVERL))) {
        uchar r = red(ul), g = green(ul), b = blue(ul);
        add_style(const_cast<char *>(""));
        hprintf(hf, "text-decoration-color: #%02X%02X%02X;", r, g, b);
      }

      if (ca->attr & ATTR_INVISIBLE)
        add_style(const_cast<char *>("visibility: hidden;"));
      else {
        // add JavaScript triggers
        if (ca->attr & ATTR_BLINK2)
          hprintf(hf, "' name='rapid");
        else if (ca->attr & ATTR_BLINK)
          hprintf(hf, "' name='blink");
      }

      // mark cursor position
      if (ca->attr & (TATTR_ACTCURS | TATTR_PASCURS)) {
        hprintf(hf, "' id='cursor");
        fg = win_get_colour(CURSOR_TEXT_COLOUR_I);
        // more precise cursor colour adjustments could be made...
      }

      // finish styles
      hprintf(hf, "'>");

      // retrieve chunk of text from buffer
      wchar save = buf->text[i];
      buf->text[i] = 0;
      char * s = cs__wcstoutf(&buf->text[i0]);
      buf->text[i] = save;
      // here we could:
      // * handle the chunk string by Unicode glyphs
      // * check whether each char is an emoji char or sequence
      // * check its terminal width
      // * scale width to actual (narrow or multi-cell) width

      // write chunk, apply HTML escapes
      auto hprinttext = [&](char * t) {
        if (ca->attr & ATTR_FRAMED)
          while (*t) {
            hprintf(hf, "%c", *t++);
#ifdef export_emoji_style
            // here we should, in addition to the above:
            // * check whether each char actually has an emoji presentation:
            //   (emoji_tags(emoji_idx(ch)) & EM_emoj)
            //   and only append 0xFE0F then
            if ((*t & 0xC0) != 0x80)
              hprintf(hf, "️");
#endif
          }
        else
          hprintf(hf, "%s", t);
      };
      char * s1 = strpbrk(s, "<&");
      if (s1) {
        char * s0 = s;
        do {
          if (*s0 == '<') {
            hprintf(hf, "&lt;");
            s0 ++;
          }
          else if (*s0 == '&') {
            hprintf(hf, "&amp;");
            s0 ++;
          }
          else {
            char c = s1 ? *s1 : 0;
            if (s1)
              *s1 = 0;
            hprinttext(s0);
            if (s1) {
              *s1 = c;
              s0 = s1;
            }
            else
              s0 += strlen(s0);
          }
          s1 = strpbrk(s0, "<&");
        } while (*s0);
      }
      else
        hprinttext(s);
      free(s);
      hprintf(hf, "</span>");

      // forward chunk pointer
      i0 = i;
    }

    // forward newlines
    if (buf->text[i] == '\r') {
      i++;
      i0 = i;
    }
    if (buf->text[i] == '\n') {
      i++;
      i0 = i;
      if (lattr)
        hprintf(hf, "</div>");
      if (enhtml)
        // <br> needed for HTML and for Powerpoint
        hprintf(hf, "<br%s\n>", lattr == LATTR_BOT ? " class='double-height-bottom'" : "");
      else
        hprintf(hf, "\n");
      odd = !odd;

      new_line = true;
      lattr = LATTR_NORM;
    }
  }
  destroy_clip_workbuf(buf, true);

  hprintf(hf, "</pre>\n");
  hprintf(hf, "  </div>\n");
  //hprintf(hf, "  </td></tr></table>\n");
  hprintf(hf, "</body>\n");

  return hbuf;
}

char *
(term_get_html)(struct term *term_p, int level)
{
  return term_create_html(0, level);
}

void
(term_export_html)(struct term *term_p, bool do_open)
{
  TERM_VAR_REF(true)
  
  struct timeval now;
  gettimeofday(& now, 0);
  char * htmlf = save_filename(const_cast<char *>(".html"));

  int hfd = open(htmlf, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (hfd < 0) {
    win_bell(&cfg);
    return;
  }
  FILE * hf = fdopen(hfd, "w");
  if (!hf) {
    win_bell(&cfg);
    return;
  }

  term_create_html(hf, 3);

  fclose(hf);  // implies close(hfd);

  if (do_open) {
    wchar * browse = cs__mbstowcs(htmlf);
    win_open(browse, false);  // win_open frees its argument
  }
  free(htmlf);
}

#include "print.h"

void
(print_screen)(struct term *term_p)
{
  TERM_VAR_REF(true)
  
  if (*cfg.printer == '*')
    printer_start_job(printer_get_default());
  else if (*cfg.printer)
    printer_start_job(cfg.printer);
  else
    return;

  pos start = (pos){term.disptop, 0, 0, 0, false};
  pos end = (pos){term.disptop + term.rows - 1, term.cols, 0, 0, false};
  bool rect = false;
  clip_workbuf * buf = get_selection(false, start, end, rect, false, false);
  printer_wwrite(buf->text, buf->len);
  printer_finish_job();
  destroy_clip_workbuf(buf, true);
}

}
