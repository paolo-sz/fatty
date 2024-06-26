#ifndef WINPRIV_H
#define WINPRIV_H

#include "win.h"
#include "winids.h"

#include <winbase.h>
#include <wingdi.h>
#include <winuser.h>
#include <imm.h>
#define SB_PRIOR 100
#define SB_NEXT 101

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
#ifndef WM_GETDPISCALEDSIZE
#define WM_GETDPISCALEDSIZE 0x02E4
#endif

extern HINSTANCE inst;  // The all-important instance handle
extern HWND wnd, fatty_tab_wnd;        // the main terminal window
extern char *home;
extern HIMC imc;        // the input method context
extern HWND config_wnd; // the options window
extern ATOM class_atom;

extern void clear_tabs(void);
extern void add_tab(uint tabi, HWND wndi);
// Inter-window actions
enum {
  WIN_MINIMIZE = 0,
  WIN_MAXIMIZE = -1,
  WIN_FULLSCREEN = -2,
#ifdef sanitize_min_restore_via_sync
  WIN_RESTORE = -3,
#endif
#ifdef async_horflush
  WIN_HORFLUSH = -4,
#endif
  WIN_TOP = 1,
  WIN_TITLE = 4,
  WIN_INIT_POS = 5,
#ifdef sanitize_min_restore_via_hide
  WIN_HIDE = 8,
#endif
};
// support tabbar
#define win_to_top(...) (win_to_top)(term_p, ##__VA_ARGS__)
extern void (win_to_top)(struct term* term_p, HWND top_wnd);
extern void win_post_sync_msg(HWND target, int level);
struct tabinfo {
  unsigned long tag;
  HWND wnd;
  wchar * title;
};
extern struct tabinfo * tabinfo;
extern int ntabinfo;

extern COLORREF colours[COLOUR_NUM];
extern colour brighten(colour c, colour against, bool monotone);

extern LOGFONT lfont;

extern int font_size;  // logical font size, as configured (< 0: pixel size)
extern int cell_width, cell_height;  // includes spacing
extern bool font_ambig_wide;
extern int line_scale;
extern int PADDING;
extern int OFFSET;
extern bool show_charinfo;
extern void toggle_charinfo(void);
#define toggle_vt220(...) (toggle_vt220)(term_p, ##__VA_ARGS__)
extern void (toggle_vt220)(struct term* term_p);
extern char * fontpropinfo(void);

extern bool title_settable;
extern bool support_wsl;
extern wchar * wslname;
extern wstring wsl_basepath;
extern bool report_config;
extern bool report_child_pid;
extern bool report_child_tty;

extern int ini_width, ini_height;
extern bool win_is_fullscreen;
extern bool win_is_always_on_top;
extern bool clipboard_token;
extern uint dpi;
extern int per_monitor_dpi_aware;
extern bool keep_screen_on;
extern bool force_opaque;

extern bool click_focus_token;
extern pos last_pos;
extern int lines_scrolled;
extern bool kb_input;
extern uint kb_trace;

extern char * version(void);

#define win_update_now(...) (win_update_now)(term_p, ##__VA_ARGS__)
extern void (win_update_now)(struct term* term_p);

#define fill_background(...) (fill_background)(term_p, ##__VA_ARGS__)
extern bool (fill_background)(struct term* term_p, HDC dc, RECT * boxp);
extern void win_flush_background(bool clearbg);
#define win_paint(...) (win_paint)(term_p, ##__VA_ARGS__)
extern void (win_paint)(struct term* term_p);

extern void win_init_fonts(int size, bool allfonts);
extern wstring win_get_font(uint findex);
extern void win_change_font(uint findex, wstring fn);
#define win_font_cs_reconfig(...) (win_font_cs_reconfig)(term_p, ##__VA_ARGS__)
extern void (win_font_cs_reconfig)(struct term* term_p, bool font_changed);

extern void win_keep_screen_on(bool);

#define win_update_scrollbar(...) (win_update_scrollbar)(term_p, ##__VA_ARGS__)
extern void (win_update_scrollbar)(struct term* term_p, bool inner);
#define win_set_scrollview(...) (win_set_scrollview)(term_p, ##__VA_ARGS__)
extern void (win_set_scrollview)(struct term* term_p, int pos, int len, int height);

#define win_adapt_term_size(...) (win_adapt_term_size)(term_p, ##__VA_ARGS__)
extern void (win_adapt_term_size)(struct term* term_p, bool sync_size_with_font, bool scale_font_with_size);
#define scale_to_image_ratio(...) (scale_to_image_ratio)(term_p, ##__VA_ARGS__)
extern void (scale_to_image_ratio)(struct term* term_p);

extern void win_open_config(void);
extern void * load_library_func(string lib, string func);
extern void update_available_version(bool ok);
extern void set_dpi_auto_scaling(bool on);
#define win_update_transparency(...) (win_update_transparency)(term_p, ##__VA_ARGS__)
extern void (win_update_transparency)(struct term* term_p, int transparency, bool opaque);
#define win_tab_prefix_title(...) (win_tab_prefix_title)(term_p, ##__VA_ARGS__)
extern void (win_tab_prefix_title)(struct term* term_p, const wstring);
#define win_tab_unprefix_title(...) (win_tab_unprefix_title)(term_p, ##__VA_ARGS__)
extern void (win_tab_unprefix_title)(struct term* term_p, const wstring);
//extern void win_prefix_title(const wstring);
//extern void win_unprefix_title(const wstring);
extern void strip_title(wchar * title);
extern void win_set_icon(char * s, int icon_index);
extern char * guardpath(string path, int level);

extern void win_show_tip(int x, int y, int cols, int rows);
extern void win_destroy_tip(void);

extern void taskbar_progress(int percent);
extern HCURSOR win_get_cursor(bool appmouse);
extern void set_cursor_style(bool appmouse, wchar * style);

extern void win_init_menus(void);
#define win_update_menus(...) (win_update_menus)(term_p, ##__VA_ARGS__)
extern void (win_update_menus)(struct term *term_p, bool callback);
#define user_function(...) (user_function)(term_p, ##__VA_ARGS__)
extern void (user_function)(struct term* term_p, wstring commands, int n);

#define win_show_mouse(...) (win_show_mouse)(term_p, ##__VA_ARGS__)
extern void (win_show_mouse)(struct term* term_p);
#define win_mouse_click(...) (win_mouse_click)(term_p, ##__VA_ARGS__)
extern bool (win_mouse_click)(struct term* term_p, mouse_button, LPARAM);
#define win_mouse_release(...) (win_mouse_release)(term_p, ##__VA_ARGS__)
extern void (win_mouse_release)(struct term *term_p, mouse_button, LPARAM);
#define win_mouse_wheel(...) (win_mouse_wheel)(term_p, ##__VA_ARGS__)
extern void (win_mouse_wheel)(struct term *term_p, POINT wpos, bool horizontal, int delta);
#define win_mouse_move(...) (win_mouse_move)(term_p, ##__VA_ARGS__)
extern void (win_mouse_move)(struct term* term_p, bool nc, LPARAM);

extern mod_keys get_mods(void);
extern void win_key_reset(void);
#define provide_input(...) (provide_input)(term_p, ##__VA_ARGS__)
extern void (provide_input)(struct term* term_p, wchar);
#define win_key_down(...) (win_key_down)(term_p, ##__VA_ARGS__)
extern bool (win_key_down)(struct term* term_p, WPARAM, LPARAM);
#define win_key_up(...) (win_key_up)(term_p, ##__VA_ARGS__)
extern bool (win_key_up)(struct term *term_p, WPARAM, LPARAM);
extern void do_win_key_toggle(int vk, bool on);
#define win_csi_seq(...) (win_csi_seq)(child_p, ##__VA_ARGS__)
extern void (win_csi_seq)(struct child* child_p, char * pre, char * suf);

extern void win_led(int led, bool set);
extern bool get_scroll_lock(void);
extern void sync_scroll_lock(bool locked);

extern wchar * dewsl(wchar * wpath);
extern void shell_exec(wstring wpath);
extern void win_init_drop_target(void);

extern wstring wslicon(wchar * params);

#define foreground_cwd(...) (foreground_cwd)(child_p, ##__VA_ARGS__)
extern char * (foreground_cwd)(struct child* child_p);

#define toggle_status_line(...) (toggle_status_line)(term_p, ##__VA_ARGS__)
extern void (toggle_status_line)(struct term *term_p);

#define win_switch(...) (win_switch)(term_p, ##__VA_ARGS__)
extern void (win_switch)(struct term *term_p, bool back, bool alternate);
extern int sync_level(void);

extern int search_monitors(int * minx, int * miny, HMONITOR lookup_mon, int get_primary, MONITORINFO *mip);

#define win_set_ime_open(...) (win_set_ime_open)(term_p, ##__VA_ARGS__)
extern void (win_set_ime_open)(struct term* term_p, bool);
#define win_set_ime(...) (win_set_ime)(term_p, ##__VA_ARGS__)
extern void (win_set_ime)(struct term* term_p, bool open);
extern bool win_get_ime(void);

extern void win_dark_mode(HWND w);

extern void show_message(char * msg, UINT type);
extern void show_info(char * msg);

void win_process_timer_message(WPARAM message);

void win_tab_set_argv(char** argv);
void win_tab_init(char* home, char* cmd, char** argv, int width, int height, char* title);
int win_tab_count();
int win_active_tab();
#define win_tab_change(...) (win_tab_change)(term_p, ##__VA_ARGS__)
void (win_tab_change)(struct term* term_p, int change);
#define win_tab_move(...) (win_tab_move)(term_p, ##__VA_ARGS__)
void (win_tab_move)(struct term* term_p, int amount);
#define win_tab_create(...) (win_tab_create)(term_p, ##__VA_ARGS__)
void (win_tab_create)(struct term* term_p);
void win_tab_delete(struct term* term_p);
bool win_term_valid(struct term* term_p);
void win_tab_clean();
#define win_tab_attention(...) (win_tab_attention)(term_p, ##__VA_ARGS__)
void (win_tab_attention)(struct term* term_p);
#define win_tab_set_title(...) (win_tab_set_title)(term_p, ##__VA_ARGS__)
void (win_tab_set_title)(struct term* term_p, wchar_t* title);
wchar_t* win_tab_get_title(unsigned int idx);

void win_tab_title_push(struct term* term_p);
wchar_t* win_tab_title_pop(struct term* term_p);
#define win_tab_save_title(...) (win_tab_save_title)(term_p, ##__VA_ARGS__)
void (win_tab_save_title)(struct term* term_p);
#define win_tab_restore_title(...) (win_tab_restore_title)(term_p, ##__VA_ARGS__)
void (win_tab_restore_title)(struct term* term_p);

void win_tab_mouse_click();
int win_tab_height();
#define win_paint_tabs(...) (win_paint_tabs)(term_p, ##__VA_ARGS__)
void (win_paint_tabs)(struct term *term_p, LPARAM lp, int width);
struct term** win_get_term_list(int* n);
#define WIN_FOR_EACH_TERM(func) \
 { \
   int term_num; \
   struct term** term_pp = win_get_term_list(&term_num); \
   for (int i = 0; i < term_num; i++) { \
     struct term* term_p = term_pp[i]; \
     func; \
   } \
 }
struct child** win_get_child_list(int* n);
#define WIN_FOR_EACH_CHILD(func) \
 { \
   int child_num; \
   struct child** child_pp = win_get_child_list(&child_num); \
   for (int i = 0; i < child_num; i++) { \
     struct child* child_p = child_pp[i]; \
     func; \
   } \
 }

bool win_should_die();
#define win_close(...) (win_close)(term_p, ##__VA_ARGS__)
extern void (win_close)(struct term *term_p);
#define win_toggle_on_top(...) (win_toggle_on_top)(term_p, ##__VA_ARGS__)
extern void (win_toggle_on_top)(struct term *term_p);
extern void win_tab_close(struct term** term_pp);
void win_tab_close_all();

void win_tab_menu();

extern char * geturl(int n);

extern unsigned long mtime(void);

#define term_save_image(...) (term_save_image)(term_p, ##__VA_ARGS__)
extern void (term_save_image)(struct term *term_p, bool do_open);

#endif
