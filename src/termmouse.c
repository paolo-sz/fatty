// termmouse.c (part of FaTTY)
// Copyright 2015 Juho Peltonen
// Based on code from mintty by 2008-2023 Andy Koppe, 2017-2025 Thomas Wolff
// Based on code from PuTTY-0.60 by Simon Tatham and team.
// Licensed under the terms of the GNU General Public License v3 or later.

#include <algorithm>

using std::max;
using std::min;

extern "C" {
  
#include "termpriv.h"
#include "win.h"
#include "winpriv.h"
#include "child.h"
#include "charset.h"  // cs__utftowcs
#include "tek.h"

/*
 * Fetch the character at a particular position in a line array.
 * The reason this isn't just a simple array reference is that if the
 * character we find is UCSWIDE, then we must look one space further
 * to the left.
 */
static wchar
get_char(termline *line, int x)
{
  wchar c = line->chars[x].chr;
  if (c == UCSWIDE && x > 0)
    c = line->chars[x - 1].chr;
  return c;
}

#define sel_spread_word(...) (sel_spread_word)(term_p, ##__VA_ARGS__)
static pos
(sel_spread_word)(struct term* term_p, pos p, bool forward)
{
  TERM_VAR_REF(true)
  
  pos ret_p = p;
  termline *line = fetch_line(p.y);
static int level = 0;
static char scheme = 0;
  if (!forward) {
    level = 0;
    scheme = 0;
  }
  //printf("sel_ %d: forward %d level %d\n", p.x, forward, level);

  wchar prevc1 = 0;
  wchar prevc2 = 0;
  for (;;) {
    wchar c = get_char(line, p.x);

    // special colon and double-colon handling
    if (forward && prevc1 == ':' && prevc2 != ':' && !strchr(":/", c)) {
      // handle previous match of : if not matching :: or :/
      return ret_p;
    }
    else if (forward && prevc1 == ':' && strchr(":/", c)) {
      // handle match of :: or :/
      ret_p = p;
    }
    prevc2 = prevc1;
    prevc1 = c;

    // scheme detection state machine
    if (!forward) {
      // http://abc.xy
      //0ssss://
      if (isalnum(c)) {
        if (scheme == ':')
          scheme = 's';
        else if (scheme != 's')
          scheme = 0;
      }
      else if (c == ':') {
        if (scheme == '/')
          scheme = ':';
        else
          scheme = 0;
      }
      else if (c == '/') {
        scheme = '/';
      }
      else if (scheme == 's')  // #1209 / #1208
        break;
      else
        scheme = 0;
    }

    if (term.mouse_state != MS_OPENING && *cfg.word_chars_excl)
      if (strchr(cfg.word_chars_excl, c))
        break;
    if (iswalnum(c))
      ret_p = p;
    else if (term.mouse_state != MS_OPENING && *cfg.word_chars) {
      if (!strchr(cfg.word_chars, c))
        break;
      ret_p = p;
    }
    else if (strchr("_#%~+-", c))
      ret_p = p;
    else if (strchr(".$@/\\", c)) {
      if (!forward)
        ret_p = p;
    }
    // support URLs with parentheses (#1196)
    // distinguish opening and closing parentheses to match proper nesting
    else if (!term.mouse_state && strchr("([{", c)) {
      level ++;
      //printf("%d: %c forward %d level %d\n", p.x, c, forward, level);
      if (forward)
        ret_p = p;
    }
    else if (!term.mouse_state && strchr(")]}", c)) {
      level --;
      //printf("%d: %c forward %d level %d\n", p.x, c, forward, level);
      if (forward && level < 0)
        break;
      if (forward)
        ret_p = p;
    }
    // should we also consider *^`| as part of a URL?
    // should we strip ?!.,;: at the end?
    // what about #$%&*\^`|~ at the end?
    else if (c == ' ' && p.x > 0 && get_char(line, p.x - 1) == '\\')
      ret_p = p;
    else if (c == (forward ? '=' : ':')) {
    }
    else if (strchr("&,;?!", c)) {
    }
    else if (forward && c == ':') {
      // could set marker in case we match : but not :: or :/
      // but this is better handled at the beginning of the for loop alone;
      // note: this could be handled more easily here with some look-ahead 
      // but that is not possible as the URL may wrap into the next line
    }
    else {
      //printf("%d: %c forward %d level %d BREAK\n", p.x, c, forward, level);
      break;
    }

    if (forward) {
      p.x++;
      if (p.x >= term.cols - ((line->lattr & LATTR_WRAPPED2) != 0)) {
        if (!(line->lattr & LATTR_WRAPPED))
          break;
        p.x = 0;
        release_line(line);
        line = fetch_line(++p.y);
      }
    }
    else {
      if (p.x <= 0) {
        if (p.y <= -sblines())
          break;
        release_line(line);
        line = fetch_line(--p.y);
        if (!(line->lattr & LATTR_WRAPPED))
          break;
        p.x = term.cols - ((line->lattr & LATTR_WRAPPED2) != 0);
      }
      p.x--;
    }
  }

  //printf("%d: return\n", ret_p.x);
  release_line(line);
  return ret_p;
}

#define sel_spread_half(...) (sel_spread_half)(term_p, ##__VA_ARGS__)
/*
 * Spread the selection outwards according to the selection mode.
 */
static pos
(sel_spread_half)(struct term* term_p, pos p, bool forward)
{
  TERM_VAR_REF(true)
    
  switch (term.mouse_state) {
    when MS_SEL_CHAR: {
     /*
      * In this mode, every character is a separate unit, except
      * for runs of spaces at the end of a non-wrapping line.
      */
      termline *line = fetch_line(p.y);
      if (!(line->lattr & LATTR_WRAPPED)) {
        termchar *q = line->chars + term.cols;
        while (q > line->chars && q[-1].chr == ' ' && !q[-1].cc_next)
          q--;
        if (q == line->chars + term.cols)
          q--;
        if (p.x >= q - line->chars)
          p.x = forward ? term.cols - 1 : q - line->chars;
      }
      release_line(line);
    }
    when MS_SEL_WORD case_or MS_OPENING:
      p = sel_spread_word(p, forward);
    when MS_SEL_LINE:
      if (forward) {
        termline *line = fetch_line(p.y);
        while (line->lattr & LATTR_WRAPPED) {
          release_line(line);
          line = fetch_line(++p.y);
          p.x = 0;
        }
        int x = p.x;
        p.x = term.cols - 1;
        do {
          if (get_char(line, x) != ' ')
            p.x = x;
        } while (++x < line->cols);
        release_line(line);
      }
      else {
        p.x = 0;
        while (p.y > -sblines()) {
          termline *line = fetch_line(p.y - 1);
          bool wrapped = line->lattr & LATTR_WRAPPED;
          release_line(line);
          if (!wrapped)
            break;
          p.y--;
        }
      }
    othwise:
     /* Shouldn't happen. */
      break;
  }
  return p;
}

#define sel_spread(...) (sel_spread)(term_p, ##__VA_ARGS__)
static void
(sel_spread)(struct term* term_p)
{
  TERM_VAR_REF(true)
  
  term.sel_start = sel_spread_half(term.sel_start, false);
  term.sel_end = sel_spread_half(term.sel_end, true);
  incpos(term.sel_end);
}

#define hover_spread_empty(...) (hover_spread_empty)(term_p, ##__VA_ARGS__)
static bool
(hover_spread_empty)(struct term* term_p)
{
  TERM_VAR_REF(true)
  
  //printf("hover_spread_empty\n");
  term.hover_start = sel_spread_word(term.hover_start, false);
  term.hover_end = sel_spread_word(term.hover_end, true);
  //printf("hover_spread_empty %d..%d\n", term.hover_start.x, term.hover_end.x);
  bool eq = term.hover_start.y == term.hover_end.y && term.hover_start.x == term.hover_end.x;
  incpos(term.hover_end);
  return eq;
}

#define sel_drag(...) (sel_drag)(term_p, ##__VA_ARGS__)
static void
(sel_drag)(struct term* term_p, pos selpoint)
{
  TERM_VAR_REF(true)
  
  //printf("sel_drag %d+%d/2 (anchor %d+%d/2)\n", selpoint.x, selpoint.r, term.sel_anchor.x, term.sel_anchor.r);
  term.selected = true;
  term.selection_eq_clipboard = false;
  if (!term.sel_rect) {
   /*
    * For normal selection, we set (sel_start,sel_end) to
    * (selpoint,sel_anchor) in some order.
    */
    if (poslt(selpoint, term.sel_anchor)) {
      term.sel_start = selpoint;
      term.sel_end = term.sel_anchor;
      if (cfg.elastic_mouse && !term.mouse_mode) {
        if (selpoint.r) {
          incpos(term.sel_start);
        }
        if (!term.sel_anchor.r) {
          decpos(term.sel_end);
        }
      }
    }
    else {
      term.sel_start = term.sel_anchor;
      term.sel_end = selpoint;
      if (cfg.elastic_mouse && !term.mouse_mode) {
        if (term.sel_anchor.r) {
          incpos(term.sel_start);
        }
        if (!selpoint.r) {
          decpos(term.sel_end);
        }
      }
    }
    sel_spread();
  }
  else {
   /*
    * For rectangular selection, we may need to
    * interchange x and y coordinates (if the user has
    * dragged in the -x and +y directions, or vice versa).
    */
    term.sel_start.x = min(term.sel_anchor.x, selpoint.x);
    term.sel_end.x = 1 + max(term.sel_anchor.x, selpoint.x);
    term.sel_start.y = min(term.sel_anchor.y, selpoint.y);
    term.sel_end.y = max(term.sel_anchor.y, selpoint.y);
  }
}

#define sel_extend(...) (sel_extend)(term_p, ##__VA_ARGS__)
static void
(sel_extend)(struct term* term_p, pos selpoint)
{
  TERM_VAR_REF(true)
  
  //printf("sel_extend %d+%d/2 (anchor %d+%d/2)\n", selpoint.x, selpoint.r, term.sel_anchor.x, term.sel_anchor.r);
  if (term.selected) {
    if (!term.sel_rect) {
     /*
      * For normal selection, we extend by moving
      * whichever end of the current selection is closer
      * to the mouse.
      */
      if (posdiff(selpoint, term.sel_start) <
          posdiff(term.sel_end, term.sel_start) / 2) {
        term.sel_anchor = term.sel_end;
        decpos(term.sel_anchor);
      }
      else
        term.sel_anchor = term.sel_start;
    }
    else {
     /*
      * For rectangular selection, we have a choice of
      * _four_ places to put sel_anchor and selpoint: the
      * four corners of the selection.
      */
      term.sel_anchor.x =
        selpoint.x * 2 < term.sel_start.x + term.sel_end.x
        ? term.sel_end.x - 1
        : term.sel_start.x;
      term.sel_anchor.y =
        selpoint.y * 2 < term.sel_start.y + term.sel_end.y
        ? term.sel_end.y
        : term.sel_start.y;
    }
  }
  else
    term.sel_anchor = selpoint;
  sel_drag(selpoint);
}

typedef enum {
  MA_CLICK = 0,
  MA_MOVE = 1,
  MA_WHEEL = 2,
  MA_RELEASE = 3
} mouse_action;  // values are significant, used for calculation!

#define send_mouse_event(...) (send_mouse_event)(term_p, ##__VA_ARGS__)
static void
(send_mouse_event)(struct term* term_p, mouse_action a, mouse_button b, mod_keys mods, pos p)
{
  TERM_VAR_REF(true)
  
  if (term.mouse_mode == MM_LOCATOR) {
    // handle DECSLE: select locator events
    if ((a == MA_CLICK && term.locator_report_up)
     || (a == MA_RELEASE && term.locator_report_dn)) {
      int pe = 0;
      switch (b) {
        when MBT_LEFT:
          pe = a == MA_CLICK ? 2 : 3;
        when MBT_MIDDLE:
          pe = a == MA_CLICK ? 4 : 5;
        when MBT_RIGHT:
          pe = a == MA_CLICK ? 6 : 7;
        when MBT_4:
          pe = a == MA_CLICK ? 8 : 9;
        othwise:;
      }
      if (pe) {
        int x, y, buttons;
        win_get_locator_info(&x, &y, &buttons, term.locator_by_pixels);
        child_printf("\e[%d;%d;%d;%d;0&w", pe, buttons, y, x);
        term.locator_rectangle = false;
      }
    }
    // handle DECEFR: enable filter rectangle
    else if (a == MA_MOVE && term.locator_rectangle) {
      /* Anytime the locator is detected outside of the filter
         rectangle, an outside rectangle event is generated and the
         rectangle is disabled.
      */
      int x, y, buttons;
      win_get_locator_info(&x, &y, &buttons, term.locator_by_pixels);
      if (x < term.locator_left || x > term.locator_right
          || y < term.locator_top || y > term.locator_bottom) {
        child_printf("\e[10;%d;%d;%d;0&w", buttons, y, x);
        term.locator_rectangle = false;
      }
    }
    return;
  }

  uint x = p.x + 1, y = p.y + 1;

  if (a != MA_WHEEL) {
    if (cfg.old_xbuttons)
      switch (b) {
        when MBT_4:
          b = MBT_LEFT; mods |= MDK_ALT;
        when MBT_5:
          b = MBT_RIGHT; mods |= MDK_ALT;
        othwise:;
      }
    else
      switch (b) {
        when MBT_4:
          b = (mouse_button)129;
        when MBT_5:
          b = (mouse_button)130;
        othwise:;
      }
  }

  uint code = b ? b - 1 : 0x3;

  if (a != MA_RELEASE)
    code |= a * 0x20;
  else if (term.mouse_enc != ME_XTERM_CSI && term.mouse_enc != ME_PIXEL_CSI)
    code = 0x3;

  code |= (mods & ~cfg.click_target_mod) * 0x4;

  if (term.mouse_enc == ME_XTERM_CSI)
    child_printf("\e[<%u;%u;%u%c", code, x, y, (a == MA_RELEASE ? 'm' : 'M'));
  else if (term.mouse_enc == ME_PIXEL_CSI)
    child_printf("\e[<%u;%u;%u%c", code, p.pix + 1, p.piy + 1, (a == MA_RELEASE ? 'm' : 'M'));
  else if (term.mouse_enc == ME_URXVT_CSI)
    child_printf("\e[%u;%u;%uM", code + 0x20, x, y);
  else {
    // Xterm's hacky but traditional character offset approach.
    char buf[8] = "\e[M";
    uint len = 3;

    auto encode_coord = [&](uint c) {
      c += 0x20;
      if (term.mouse_enc != ME_UTF8)
        buf[len++] = c < 0x100 ? c : 0;
      else if (c < 0x80)
        buf[len++] = c;
      else if (c < 0x800) {
        // In extended mouse mode, positions from 96 to 2015 are encoded as a
        // two-byte UTF-8 sequence (as introduced in xterm #262.)
        buf[len++] = 0xC0 + (c >> 6);
        buf[len++] = 0x80 + (c & 0x3F);
      }
      else {
        // Xterm reports out-of-range positions as a NUL byte.
        buf[len++] = 0;
      }
    };

    buf[len++] = code + 0x20;
    encode_coord(x);
    encode_coord(y);

    child_write(buf, len);
  }
}

#define box_pos(...) (box_pos)(term_p, ##__VA_ARGS__)
static pos
(box_pos)(struct term* term_p, pos p)
{
  TERM_VAR_REF(true)
  
  p.y = min(max(0, p.y), term.rows - 1);
  p.x = min(max(0, p.x), term.cols - 1);
  // p.piy and p.pix already clipped in translate_pos()
  return p;
}

#define get_selpoint(...) (get_selpoint)(term_p, ##__VA_ARGS__)
static pos
(get_selpoint)(struct term* term_p, const pos p)
{
  TERM_VAR_REF(true)
  
  pos sp = { .y = p.y + term.disptop, .x = p.x, .piy = 0, .pix = 0, .r = p.r };
  termline *line = fetch_line(sp.y);

  // Adjust to presentational direction.
  if (line->lattr & LATTR_PRESRTL) {
    sp.x = term.cols - 1 - sp.x;
    sp.r = !sp.r;
  }

  // Adjust to double-width line display.
  if ((line->lattr & LATTR_MODE) != LATTR_NORM)
    sp.x /= 2;

 /*
  * Transform x through the bidi algorithm to find the _logical_
  * click point from the physical one.
  */
  if (term_bidi_line(line, p.y) != null) {
#ifdef debug_bidi_cache
    printf("mouse @ log %d -> vis %d\n", sp.x, term.post_bidi_cache[p.y].backward[sp.x]);
#endif
    sp.x = term.post_bidi_cache[p.y].backward[sp.x];
  }

  // Back to previous cell if current one is second half of a wide char
  if (line->chars[sp.x].chr == UCSWIDE)
    sp.x--;

  release_line(line);
  return sp;
}

#define send_keys(...) (send_keys)(term_p, ##__VA_ARGS__)
static void
(send_keys)(struct term* term_p, uint count, string code)
{
  TERM_VAR_REF(true)
  
  if (count) {
    uint len = strlen(code);
    char buf[len * count];
    char *p = buf;
    while (count--) { memcpy(p, code, len); p += len; }
    child_write(buf, sizeof buf);
  }
}

#define check_app_mouse(...) (check_app_mouse)(term_p, ##__VA_ARGS__)
static bool
(check_app_mouse)(struct term* term_p, mod_keys *mods_p)
{
  TERM_VAR_REF(true)
  
  if (term.locator_1_enabled)
    return true;
  if (!term.mouse_mode || term.show_other_screen)
    return false;
  bool override = *mods_p & cfg.click_target_mod;
  *mods_p &= (mod_keys)(~cfg.click_target_mod);
  return cfg.clicks_target_app ^ override;
}

bool
(term_mouse_click)(struct term* term_p, mouse_button b, mod_keys mods, pos p, int count)
{
  TERM_VAR_REF(true)
  
  compose_clear();

  /* (#1169) was_hovering considers the case when hovering is not directly 
     triggered via state MS_OPENING but rather overrides state MS_SEL_CHAR,
     in order to support its configuration to be applied without modifier
  */
  bool was_hovering = term.hovering;

  if (term.hovering) {
    term.hovering = false;
    win_update_term(true);
  }

  bool res = true;
  if (tek_mode == TEKMODE_GIN) {
    char c = '`';
    switch (b) {
      when MBT_LEFT: c = 'l';
      when MBT_MIDDLE: c = 'm';
      when MBT_RIGHT: c = 'r';
      when MBT_4: c = 'p';
      when MBT_5: c = 'q';
    }
    if (mods & MDK_SHIFT)
      c ^= ' ';
    c |= 0x80;
    child_send(&c, 1);
    tek_send_address();
  }
  else if (check_app_mouse(&mods)) {
    if (term.mouse_mode == MM_X10)
      mods = (mod_keys)0;
    send_mouse_event(MA_CLICK, b, mods, box_pos(p));
    term.mouse_state = (mouse_state_t)b;
  }
  else {
    // generic transformation M4/M5 -> Alt+left/right;
    // if any specific handling is designed for M4/M5, this needs to be tweaked
    bool fake_alt = false;
    switch (b) {
      when MBT_4:
        b = MBT_LEFT; mods |= MDK_ALT; fake_alt = true;
      when MBT_5:
        b = MBT_RIGHT; mods |= MDK_ALT; fake_alt = true;
      othwise:;
    }

    bool alt = mods & MDK_ALT;
    bool shift_or_ctrl = mods & (MDK_SHIFT | MDK_CTRL);
    int mca = cfg.middle_click_action;
    int rca = cfg.right_click_action;
    term.mouse_state = (mouse_state_t)0;
    if (b == MBT_RIGHT && (rca == RC_MENU || shift_or_ctrl)) {
      // disable Alt+mouse menu opening;
      // the menu would often be closed soon by auto-repeat Alt, sending
      // WM_CAPTURECHANGED, WM_UNINITMENUPOPUP, WM_MENUSELECT, WM_EXITMENULOOP
      // trying to ignore WM_CAPTURECHANGED does not help
      if (!alt || fake_alt)
        win_popup_menu(mods);
      else
        res = false;
    }
    else if (b == MBT_MIDDLE && (mods & ~MDK_SHIFT) == MDK_CTRL) {
      if (cfg.zoom_mouse)
        win_zoom_font(0, mods & MDK_SHIFT);
      else
        res = false;
    }
    else if ((b == MBT_RIGHT && rca == RC_PASTE) ||
             (b == MBT_MIDDLE && mca == MC_PASTE))
    {
      if (!alt)
        term.mouse_state = shift_or_ctrl ? MS_COPYING : MS_PASTING;
      else
        res = false;
    }
    else if ((b == MBT_RIGHT && rca == RC_ENTER) ||
             (b == MBT_MIDDLE && mca == MC_ENTER)) {
      child_send("\r", 1);
    }
    else if (b == MBT_LEFT && mods == MDK_SHIFT && rca == RC_EXTEND) {
      term.mouse_state = MS_PASTING;
    }
    else if (b == MBT_LEFT &&
             ((char)(mods & ~cfg.click_target_mod) == cfg.opening_mod || was_hovering)
            )
    {
      if (count == cfg.opening_clicks) {
        // Open word under cursor
        p = get_selpoint(box_pos(p));
        term.mouse_state = MS_OPENING;
        term.selected = true;
        term.selection_eq_clipboard = false;
        term.sel_rect = false;
        term.sel_start = term.sel_end = term.sel_anchor = p;
        sel_spread();
        win_update(true);
      }
      else
        res = false;
    }
    else if (b == MBT_MIDDLE && mca == MC_VOID) {
      // res = true; // MC_VOID explicitly ignores the click
    }
    else if ((mods & (MDK_CTRL | MDK_ALT)) != (MDK_CTRL | MDK_ALT)) {
      // Only clicks for selecting and extending should get here.
      p = get_selpoint(box_pos(p));
      term.mouse_state = (mouse_state_t)(-count);
      term.sel_rect = alt;
      if (b != MBT_LEFT || shift_or_ctrl)
        sel_extend(p);
      else if (count == 1) {
        term.selected = false;
        term.sel_anchor = p;
      }
      else {
        // Double or triple-click: select whole word or line
        term.selected = true;
        term.selection_eq_clipboard = false;
        term.sel_rect = false;
        term.sel_start = term.sel_end = term.sel_anchor = p;
        sel_spread();
      }
      win_capture_mouse();
      win_update(true);
    }
    else {
      res = false;
    }
  }

  return res;
}

#define mouse_open(...) (mouse_open)(term_p, ##__VA_ARGS__)
static void
(mouse_open)(struct term* term_p, pos p)
{
  TERM_VAR_REF(true)
  
  termline *line = fetch_line(p.y + term.disptop);
  int urli = line->chars[p.x].attr.link;
  release_line(line);
  char * url = geturl(urli);
  if (url)
    win_open(cs__utftowcs(url), true);  // win_open frees its argument
  else
    term_open();
  term.selected = false;
  term.hovering = false;
  win_update(true);
}

void
(term_mouse_release)(struct term* term_p, mouse_button b, mod_keys mods, pos p)
{
  TERM_VAR_REF(true)
  
  compose_clear();

  int state = term.mouse_state;
  //printf("term_mouse_release state %d button %d\n", state, b);

  // "Clicks place cursor" implementation.
  auto place_cursor = [&](int mode)
  {
    pos dest;
    if (mode == 2002)
      dest = get_selpoint(box_pos(p));
    else
      dest = term.selected ? term.sel_end : get_selpoint(box_pos(p));
    //printf("place_cursor p %d x %d sel %d..%d\n", p.x, term.curs.x, term.sel_start.x, term.sel_end.x);

    static bool moved_previously = false;
    static pos last_dest;

    pos orig;
    if (mode == 2003)
      orig = term.sel_start;
    else
    if (state == MS_SEL_CHAR)
      orig = (pos){y : term.curs.y, x : term.curs.x, piy : 0, pix : 0, r : false};
    else if (moved_previously)
      orig = last_dest;
    else
      return;

    bool forward = posle(orig, dest);
    pos end = forward ? dest : orig;
    p = forward ? orig : dest;
    //printf("place_cursor %d %d..%d\n", mode, p.x, end.x);

    uint count = 0;
    while (p.y != end.y) {
      termline *line = fetch_line(p.y);
      if (!(line->lattr & LATTR_WRAPPED)) {
        release_line(line);
        moved_previously = false;
        return;
      }
      int cols = term.cols - ((line->lattr & LATTR_WRAPPED2) != 0);
      for (int x = p.x; x < cols; x++) {
        if (line->chars[x].chr != UCSWIDE)
          count++;
      }
      p.y++;
      p.x = 0;
      release_line(line);
    }
    termline *line = fetch_line(p.y);
    for (int x = p.x; x < end.x; x++) {
      if (line->chars[x].chr != UCSWIDE)
        count++;
    }
    release_line(line);

    //printf(forward ? "keys +%d\n" : "keys -%d\n", count);
    if (mode == 2003) {
      struct termios attr;
      tcgetattr(0, &attr);
      unsigned char tmp_str[] = {attr.c_cc[VERASE], 0};
      send_keys(count, (char *)tmp_str);
    }
    else {
      send_keys(count, term.app_cursor_keys ? (forward ? const_cast<char *>("\eOC") : const_cast<char *>("\eOD"))
                                            : (forward ? const_cast<char *>("\e[C") : const_cast<char *>("\e[D")));
    }

    moved_previously = true;
    last_dest = dest;
  };

  term.mouse_state = (mouse_state_t)0;
  switch (state) {
    when MS_COPYING: term_copy();
    when MS_OPENING: mouse_open(p);
    when MS_PASTING: {
      // Finish selection.
      if (term.selected && cfg.copy_on_select)
        term_copy();

      // Flush any output held back during selection.
      term_flush();

      // Readline mouse mode: place cursor to mouse position before pasting
      if (term.readline_mouse_2)
        place_cursor(2002);

      // Now the pasting.
      win_paste();
    }
    when MS_SEL_CHAR case_or MS_SEL_WORD case_or MS_SEL_LINE: {
      // Open hovered link, accepting configurable modifiers
      if (state == MS_SEL_CHAR && !term.selected
          && ((char)(mods & ~cfg.click_target_mod) == cfg.opening_mod)
         )
      {
        // support the case of hovering and link opening without modifiers 
        // if so configured (#1169)
        mouse_open(p);
        term.mouse_state = (mouse_state_t)0;
        return;
      }

      // Finish selection.
      if (term.selected && cfg.copy_on_select)
        term_copy();

      // Flush any output held back during selection.
      term_flush();

      // Guard "Clicks place cursor" implementation.
      if (term.on_alt_screen || term.app_cursor_keys)
        return;

      // Readline mouse mode: place cursor to mouse position
      // Should cfg.clicks_place_cursor override DECSET mode?
      bool extenda = (b == MBT_RIGHT && cfg.right_click_action == RC_EXTEND)
                 || (b == MBT_MIDDLE && cfg.middle_click_action == MC_EXTEND);
      if (term.readline_mouse_1 && b == MBT_LEFT)
        place_cursor(2001);
      else if (term.readline_mouse_3 && extenda && state == MS_SEL_WORD) {
        place_cursor(2001);
        place_cursor(2003);
        term.selected = false;
      }
    }
    othwise:
      if (check_app_mouse(&mods)) {
        if (term.mouse_mode >= MM_VT200)
          send_mouse_event(MA_RELEASE, b, mods, box_pos(p));
      }
  }
}

static void
sel_scroll_cb(void* data)
{
  struct term* term_p = (struct term*)data;
  if (!win_term_valid(term_p)) {
    return;
  }
  TERM_VAR_REF(true)
  
  if (term_selecting() && term.sel_scroll) {
    term_scroll(0, term.sel_scroll);
    sel_drag(get_selpoint(term.sel_pos));
    win_update(true);
    win_set_timer(sel_scroll_cb, data, 125);
  }
}

void
(term_mouse_move)(struct term* term_p, mod_keys mods, pos p)
{
  TERM_VAR_REF(true)
  
  compose_clear();

  //printf("mouse_move %d+%d/2\n", p.x, p.r);
  pos bp = box_pos(p);

  if (term_selecting()) {
    //printf("term_mouse_move selecting\n");
    if (p.y < 0 || p.y >= term.rows) {
      if (!term.sel_scroll)
        win_set_timer(sel_scroll_cb, (void *)term_p, 200);
      term.sel_scroll = p.y < 0 ? p.y : p.y - term.rows + 1;
      term.sel_pos = bp;
    }
    else {
      term.sel_scroll = 0;
      if (p.x < 0 && p.y + term.disptop > term.sel_anchor.y)
        bp = (pos){.y = p.y - 1, .x = term.cols - 1, .piy = 0, .pix = 0, .r = p.r};
    }

    bool alt = mods & MDK_ALT;
    term.sel_rect = alt;
    sel_drag(get_selpoint(bp));

    win_update(true);
  }
  else if (term.mouse_state == MS_OPENING && 0 == cfg.opening_mod) {
    //printf("term_mouse_move opening mods %X\n", mods);
    // assumption: don't need to check mouse button state in this workflow

    // if hover links are configured to work without modifier, 
    // still enable text selection even over a link, after the mouse 
    // has moved from initial click;
    // not this is in opposite to the next case, which deliberately 
    // catches this case to NOT switch to selection, in order to 
    // support hover action even after tiny mouse shaking (#1039)

    term.mouse_state = MS_SEL_CHAR;
    // here we could already establish selection mode properly, 
    // as it is done above in term_mouse_click, case 
    // "Only clicks for selecting and extending should get here." -
    // we save that effort as it will be achieved anyway by the 
    // next tiny movement...
  }
  else if (term.mouse_state == MS_OPENING) {
    //printf("term_mouse_move opening mods %X\n", mods);
    // let's not clear link opening state when just moving the mouse (#1039)
    // but only after hovering out of the link area (below)
#ifdef link_opening_only_if_unmoved
    term.mouse_state = (mouse_state_t)0;
    term.selected = false;
    win_update(true);
#endif
  }
  else if (term.mouse_state > 0) {
    //printf("term_mouse_move >0\n");
    if (term.mouse_mode >= MM_BTN_EVENT)
      send_mouse_event(MA_MOVE, (mouse_button)term.mouse_state, mods, bp);
  }
  else {
    //printf("term_mouse_move any\n");
    if (term.mouse_mode == MM_ANY_EVENT)
      send_mouse_event(MA_MOVE,(mouse_button)0, mods, bp);
  }

  // hover indication
  if (!check_app_mouse(&mods) && term.has_focus &&
      ((char)(mods & ~cfg.click_target_mod) == cfg.opening_mod)
     )
  {
    //printf("term_mouse_move link\n");
    p = get_selpoint(box_pos(p));
    term.hover_start = term.hover_end = p;
    if (!hover_spread_empty()) {
      term.hovering = true;
      termline *line = fetch_line(p.y);
      term.hoverlink = line->chars[p.x].attr.link;
      release_line(line);
      win_update(true);
    }
    else if (term.hovering) {
      term.hovering = false;
      win_update_term(true);
    }
    //printf("->hovering %d (opening %d)\n", term.hovering, term.mouse_state == MS_OPENING);
    // clear link opening state after hovering out of link area
    if (!term.hovering && term.mouse_state == MS_OPENING)
      term.mouse_state = (mouse_state_t)0;
  }
}

void
(term_mouse_wheel)(struct term* term_p, bool horizontal, int delta, int lines_per_notch, mod_keys mods, pos p)
{
  TERM_VAR_REF(true)
  
  compose_clear();

  if (term.hovering) {
    term.hovering = false;
    win_update_term(true);
  }

  enum { NOTCH_DELTA = 120 };

  static int accu = 0;
  accu += delta;

  if (tek_mode == TEKMODE_GIN) {
    int step = (mods & MDK_SHIFT) ? 40 : (mods & MDK_CTRL) ? 1 : 4;
    if (horizontal ^ (mods & MDK_CTRL))
      tek_move_by(0, step * delta / NOTCH_DELTA);
    else
      tek_move_by(step * delta / NOTCH_DELTA, 0);
  }
  else if (check_app_mouse(&mods)) {
    if (strstr(cfg.suppress_wheel, "report"))
      return;
    // Send as mouse events, with one event per notch.
    int notches = accu / NOTCH_DELTA;
    if (notches) {
      accu -= NOTCH_DELTA * notches;
      mouse_button b = (mouse_button)((notches < 0) + 1);
      if (horizontal)
        b = (mouse_button)(5 - b);
      notches = abs(notches);
      do
        send_mouse_event(MA_WHEEL, b, mods, box_pos(p));
      while (--notches);
    }
  }
  else if (horizontal) {
  }
  else if (cfg.zoom_mouse && (mods & ~MDK_SHIFT) == MDK_CTRL) {
    if (strstr(cfg.suppress_wheel, "zoom"))
      return;
    if (cfg.zoom_mouse) {
      int zoom = accu / NOTCH_DELTA;
      if (zoom) {
        accu -= NOTCH_DELTA * zoom;
        win_zoom_font(zoom, mods & MDK_SHIFT);
      }
    }
  }
  else if ((mods & ~(MDK_SHIFT | MDK_CTRL)) == MDK_WIN) {
    // yes, some copy/paste here for more clarity of the conditions
    if (strstr(cfg.suppress_wheel, "zoom"))
      return;
    int zoom = accu / NOTCH_DELTA;
    if (zoom) {
      accu -= NOTCH_DELTA * zoom;
      win_zoom_font(zoom, mods & MDK_SHIFT);
    }
  }
  else if (!(mods & ~(MDK_SHIFT | MDK_CTRL | MDK_ALT))) {
    // Determine number of lines per wheel notch. -1 means page-wise scrolling.
    if (mods & MDK_SHIFT)
      lines_per_notch = -1;
    else if (mods & MDK_CTRL)
      lines_per_notch = 1;
    else if (cfg.lines_per_notch > 0)
      lines_per_notch = min(cfg.lines_per_notch, term.rows - 1);

    bool pages = lines_per_notch == -1;
    int count_per_notch = pages ? 1 : lines_per_notch;

    int count = count_per_notch * accu / NOTCH_DELTA;

    if (count) {
      accu -= count * NOTCH_DELTA / count_per_notch;

      bool alt = mods & MDK_ALT;
      bool scrollback = !term.on_alt_screen || term.show_other_screen;

      // If Alt is pressed while looking at the primary screen, consume the Alt
      // modifier and send events to the application instead.
      if (scrollback && alt)
        scrollback = alt = false;

      if (scrollback) {
        if (strstr(cfg.suppress_wheel, "scrollwin"))
          return;
        // For page-wise scrolling, scroll by one line less than window height.
        if (pages)
          count *= max(1, term.rows - 1);
        term_scroll(0, -count);
      }
      else if (term.wheel_reporting || term.wheel_reporting_xterm) {
        // Application scrolling.
        if (strstr(cfg.suppress_wheel, "scrollapp") && !term.wheel_reporting_xterm)
          return;
        bool up = count > 0;
        count = abs(count);

        if (term.app_wheel && !term.wheel_reporting_xterm) {
          // Application wheel mode: send wheel events as CSI a/b codes rather
          // than cursor key codes so they can be distinguished from key presses
          // without enabling full application mouse mode.
          // Pages are distinguished with the Shift modifier code.
          if (pages) {
            send_keys(count, alt ? (up ? const_cast<char *>("\e[1;4a") : const_cast<char *>("\e[1;4b"))
                                 : (up ? const_cast<char *>("\e[1;2a") : const_cast<char *>("\e[1;2b")));
          }
          else {
            send_keys(count, alt ? (up ? const_cast<char *>("\e[1;3a") : const_cast<char *>("\e[1;3b"))
                                 : (up ? const_cast<char *>("\eOa") : const_cast<char *>("\eOb")));
          }
        }
        else if (term.vt52_mode) {
          // No PgUp/Dn keycodes in VT52 mode, so only send cursor up/down.
          if (!pages)
            send_keys(count, up ? const_cast<char *>("\eA") : const_cast<char *>("\eB"));
        }
        else {
          // Send PgUp/Dn or cursor up/down codes.
          if (pages) {
            send_keys(count, alt ? (up ? const_cast<char *>("\e[5;3~") : const_cast<char *>("\e[6;3~"))
                                 : (up ? const_cast<char *>("\e[5~") : const_cast<char *>("\e[6~")));
          }
          else {
            send_keys(count,
              alt ^ term.alt_wheel ? (up ? const_cast<char *>("\e[1;3A") : const_cast<char *>("\e[1;3B")) :
              term.app_cursor_keys ? (up ? const_cast<char *>("\eOA") : const_cast<char *>("\eOB"))
                                   : (up ? const_cast<char *>("\e[A") : const_cast<char *>("\e[B")));
          }
        }
      }
    }
  }
}

}
