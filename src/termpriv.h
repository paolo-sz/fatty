#ifndef TERMPRIV_H
#define TERMPRIV_H

/*
 * Internal terminal functions, types and structs.
 */

#include "term.h"

#define incpos(p) ((p).x == term.cols ? ((p).x = 0, (p).y++, 1) : ((p).x++, 0))
#define decpos(p) ((p).x == 0 ? ((p).x = term.cols, (p).y--, 1) : ((p).x--, 0))

#define poslt(p1,p2) ((p1).y < (p2).y || ((p1).y == (p2).y && (p1).x < (p2).x))
#define posle(p1,p2) ((p1).y < (p2).y || ((p1).y == (p2).y && (p1).x <= (p2).x))
#define poseq(p1,p2) ((p1).y == (p2).y && (p1).x == (p2).x)
#define posdiff(p1,p2) (((p1).y - (p2).y) * (term.cols + 1) + (p1).x - (p2).x)

/* Product-order comparisons for rectangular block selection. */
#define posPlt(p1,p2) ((p1).y <= (p2).y && (p1).x < (p2).x)
#define posPle(p1,p2) ((p1).y <= (p2).y && (p1).x <= (p2).x)


#define term_print_finish(...) (term_print_finish)(term_p, ##__VA_ARGS__)
extern void (term_print_finish)(struct term* term_p);

#define term_schedule_cblink(...) (term_schedule_cblink)(term_p, ##__VA_ARGS__)
extern void (term_schedule_cblink)(struct term* term_p);
#define term_schedule_vbell(...) (term_schedule_vbell)(term_p, ##__VA_ARGS__)
extern void (term_schedule_vbell)(struct term* term_p, int already_started, int startpoint);

#define term_switch_screen(...) (term_switch_screen)(term_p, ##__VA_ARGS__)
extern void (term_switch_screen)(struct term* term_p, bool to_alt, bool reset);
#define term_check_boundary(...) (term_check_boundary)(term_p, ##__VA_ARGS__)
extern void (term_check_boundary)(struct term* term_p, int x, int y);
#define term_do_scroll(...) (term_do_scroll)(term_p, ##__VA_ARGS__)
extern void (term_do_scroll)(struct term* term_p, int topline, int botline, int lines, bool sb);
#define term_erase(...) (term_erase)(term_p, ##__VA_ARGS__)
extern void (term_erase)(struct term* term_p, bool selective, bool line_only, bool from_begin, bool to_end);
#define term_last_nonempty_line(...) (term_last_nonempty_line)(term_p, ##__VA_ARGS__)
extern int (term_last_nonempty_line)(struct term* term_p);

/* Bidi paragraph support */
#define clear_wrapcontd(...) (clear_wrapcontd)(term_p, ##__VA_ARGS__)
extern void (clear_wrapcontd)(struct term* term_p, termline * line, int y);
extern ushort getparabidi(termline * line);
#define wcsline(...) (wcsline)(term_p, ##__VA_ARGS__)
extern wchar * (wcsline)(struct term* term_p, termline * line);  // for debug output

#define term_selecting(...) (term_selecting)(term_p, ##__VA_ARGS__)
static inline bool
(term_selecting)(struct term* term_p)
{
  TERM_VAR_REF
  
  return term.mouse_state < 0 && term.mouse_state >= MS_SEL_LINE;
}

#define term_update_cs(...) (term_update_cs)(term_p, ##__VA_ARGS__)
extern void (term_update_cs)(struct term* term_p);

extern int termchars_equal(termchar * a, termchar * b);
extern int termchars_equal_override(termchar * a, termchar * b, uint bchr, cattr battr);
extern int termattrs_equal_fg(cattr * a, cattr * b);

extern void copy_termchar(termline * destline, int x, termchar * src);
extern void move_termchar(termline * line, termchar * dest, termchar * src);

extern void add_cc(termline *, int col, wchar chr, cattr attr);
extern void clear_cc(termline *, int col);

extern uchar * compressline(termline *);
extern termline * decompressline(uchar *, int * bytes_used);

#define term_bidi_line(...) (term_bidi_line)(term_p, ##__VA_ARGS__)
extern termchar * (term_bidi_line)(struct term* term_p, termline *, int scr_y);

#define term_get_html(...) (term_get_html)(term_p, ##__VA_ARGS__)
extern char * (term_get_html)(struct term *term_p, int level);
#define print_screen(...) (print_screen)(term_p, ##__VA_ARGS__)
extern void (print_screen)(struct term *term_p);

extern int putlink(char * link);
extern char * geturl(int n);

#endif
