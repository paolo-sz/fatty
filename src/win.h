#ifndef WIN_H
#define WIN_H

#include "std.h"
#include "term.h"

#define dont_debug_resize

#ifdef debug_resize
#define trace_resize(params)	printf params
#else
#define trace_resize(params)
#endif

extern bool icon_is_from_shortcut;
extern wstring shortcut;

extern void exit_fatty(int exit_val);
#define report_pos(...) (report_pos)(term_p, ##__VA_ARGS__)
extern void (report_pos)(struct term* term_p);
#define win_reconfig(...) (win_reconfig)(term_p, ##__VA_ARGS__)
extern void (win_reconfig)(struct term* term_p);

#define win_update(...) (win_update)(term_p, ##__VA_ARGS__)
extern void (win_update)(struct term* term_p, bool update_sel_tip);
#define win_update_term(...) (win_update_term)(term_p, ##__VA_ARGS__)
extern void (win_update_term)(struct term* term_p, bool update_sel_tip);
extern void win_schedule_update();
#define do_update(...) (do_update)(term_p, ##__VA_ARGS__)
extern void (do_update)(struct term* term_p);

#define win_text(...) (win_text)(term_p, ##__VA_ARGS__)
extern void (win_text)(struct term* term_p, int x, int y, wchar *text, int len, cattr attr, cattr *textattr, ushort lattr, bool has_rtl, bool clearpad, uchar phase);

#define win_update_mouse(...) (win_update_mouse)(term_p, ##__VA_ARGS__)
extern void (win_update_mouse)(struct term* term_p);
extern void win_capture_mouse(void);
#define win_get_locator_info(...) (win_get_locator_info)(term_p, ##__VA_ARGS__)
extern void (win_get_locator_info)(struct term *term_p, int *x, int *y, int *buttons, bool by_pixels);

#define win_bell(...) (win_bell)(term_p, ##__VA_ARGS__)
extern void (win_bell)(struct term* term_p, config *);
#define win_margin_bell(...) (win_margin_bell)(term_p, ##__VA_ARGS__)
extern void (win_margin_bell)(struct term* term_p, config *);

extern void win_set_title(wchar *wtitle);
#define win_copy_title(...) (win_copy_title)(term_p, ##__VA_ARGS__)
extern void (win_copy_title)(struct term *term_p);
extern char * win_get_title(void);
#define win_copy_text(...) (win_copy_text)(term_p, ##__VA_ARGS__)
extern void (win_copy_text)(struct term *term_p, const char *s);

#define win_get_colour(...) (win_get_colour)(term_p, ##__VA_ARGS__)
extern colour (win_get_colour)(struct term* term_p, colour_i);
extern void win_set_colour(colour_i, colour);
extern void win_reset_colours(void);
extern colour win_get_sys_colour(int colid);
extern uint colour_dist(colour a, colour b);
extern colour truecolour(cattr *, colour bg);

extern void win_invalidate_all(bool clearbg);

extern void win_set_pos(int x, int y);
#define win_set_chars(...) (win_set_chars)(term_p, ##__VA_ARGS__)
extern void (win_set_chars)(struct term* term_p, int rows, int cols);
#define win_set_pixels(...) (win_set_pixels)(term_p, ##__VA_ARGS__)
extern void (win_set_pixels)(struct term* term_p, int height, int width);
#define win_set_geom(...) (win_set_geom)(term_p, ##__VA_ARGS__)
extern void (win_set_geom)(struct term* term_p, int y, int x, int height, int width);
#define win_maximise(...) (win_maximise)(term_p, ##__VA_ARGS__)
extern void (win_maximise)(struct term* term_p, int max);
extern void win_set_zorder(bool top);
extern void win_set_iconic(bool);
extern bool win_is_iconic(void);
extern void win_get_scrpos(int *xp, int *yp, bool with_borders);
#define win_get_pixels(...) (win_get_pixels)(term_p, ##__VA_ARGS__)
extern void (win_get_pixels)(struct term* term_p, int *height_p, int *width_p, bool with_borders);
extern void win_get_screen_chars(int *rows_p, int *cols_p);
#define win_popup_menu(...) (win_popup_menu)(term_p, ##__VA_ARGS__)
extern void (win_popup_menu)(struct term *term_p, mod_keys mods);
#define win_title_menu(...) (win_title_menu)(term_p, ##__VA_ARGS__)
extern bool (win_title_menu)(struct term *term_p, bool leftbut);

#define win_zoom_font(...) (win_zoom_font)(term_p, ##__VA_ARGS__)
extern void (win_zoom_font)(struct term* term_p, int, bool sync_size_with_font);
#define win_set_font_size(...) (win_set_font_size)(term_p, ##__VA_ARGS__)
extern void (win_set_font_size)(struct term* term_p, int, bool sync_size_with_font);
extern uint win_get_font_size(void);

extern void win_check_glyphs(wchar *wcs, uint num, cattrflags attr);
extern int win_char_width(xchar, cattrflags attr);
extern wchar win_combine_chars(wchar bc, wchar cc, cattrflags attr);

#define win_open(...) (win_open)(term_p, ##__VA_ARGS__)
extern void (win_open)(struct term *term_p, wstring path, bool adjust_dir);
#define win_copy(...) (win_copy)(term_p, ##__VA_ARGS__)
extern void (win_copy)(struct term *term_p, const wchar *data, cattr *cattrs, int len);
#define win_copy_as(...) (win_copy_as)(term_p, ##__VA_ARGS__)
extern void (win_copy_as)(struct term *term_p, const wchar *data, cattr *cattrs, int len, char what);
#define win_paste(...) (win_paste)(term_p, ##__VA_ARGS__)
extern void (win_paste)(struct term *term_p);
#define win_paste_path(...) (win_paste_path)(term_p, ##__VA_ARGS__)
extern void (win_paste_path)(struct term *term_p);

extern void win_set_timer(void (*cb)(void*), void* data, uint ticks);

extern bool print_opterror(FILE * stream, string msg, bool utf8params, string p1, string p2);
extern void win_show_about(void);
extern void win_show_error(char * msg);
extern void win_show_warning(char * msg);
extern int message_box(HWND parwnd, char * wtext, char * wcaption, int type, wstring ok);
extern int message_box_w(HWND parwnd, wchar * wtext, wchar * wcaption, int type, wstring ok);

extern bool win_is_glass_available(void);

extern void win_clear_images(void);

extern int get_tick_count(void);
extern int cursor_blink_ticks(void);

#define win_linedraw_char(...) (win_linedraw_char)(term_p, ##__VA_ARGS__)
extern wchar (win_linedraw_char)(struct term* term_p, int i);

extern struct term* win_active_terminal();
#define is_active_terminal(...) (is_active_terminal)(term_p, ##__VA_ARGS__)
extern bool (is_active_terminal)(struct term* term_p);

typedef enum {
  ACM_TERM = 1,        /* actual terminal rendering */
  ACM_RTF_PALETTE = 2, /* winclip - rtf palette setup stage */
  ACM_RTF_GEN = 4,     /* winclip - rtf generation stage */
  ACM_SIMPLE = 8,      /* simplified (bold, [rvideo,] dim, invisible) */
  ACM_VBELL_BG = 16,   /* visual-bell background highlight */
} attr_colour_mode;

#define apply_attr_colour(...) (apply_attr_colour) (term_p, ##__VA_ARGS__)
extern cattr (apply_attr_colour)(struct term* term_p, cattr a, attr_colour_mode mode);

#endif
