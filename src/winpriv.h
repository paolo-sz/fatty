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

extern HINSTANCE inst;  // The all-important instance handle
extern HWND wnd, tab_wnd;        // the main terminal window
extern char *home;
extern HIMC imc;        // the input method context
extern HWND config_wnd; // the options window
extern ATOM class_atom;

extern void clear_tabs(void);
extern void add_tab(uint tabi, HWND wndi);

extern COLORREF colours[COLOUR_NUM];
extern colour brighten(colour c, colour against, bool monotone);

extern LOGFONT lfont;

extern int font_size;  // logical font size, as configured (< 0: pixel size)
extern int cell_width, cell_height;  // includes spacing
extern int line_scale;
extern int PADDING;
extern bool show_charinfo;
extern void toggle_charinfo(void);
extern char * fontpropinfo(void);

extern int g_render_tab_height; // current tab height in pixels
extern bool support_wsl;
extern wstring wsl_basepath;

extern bool win_is_fullscreen;
extern bool clipboard_token;
extern uint dpi;
extern int per_monitor_dpi_aware;

extern bool click_focus_token;
extern pos last_pos;

extern void win_flush_background(bool clearbg);
extern void win_paint(void);

extern void win_init_fonts(int size);
extern wstring win_get_font(uint findex);
extern void win_change_font(uint findex, wstring fn);
extern void win_font_cs_reconfig(struct term* term, bool font_changed);

extern void win_update_scrollbar(bool inner);
extern void win_adapt_term_size(bool sync_size_with_font, bool scale_font_with_size);

extern void win_open_config(void);
extern void * load_library_func(string lib, string func);
extern void update_available_version(bool ok);
extern void set_dpi_auto_scaling(bool on);
extern void win_update_transparency(bool opaque);

extern void win_show_tip(int x, int y, int cols, int rows);
extern void win_destroy_tip(void);

extern void win_init_menus(void);
extern void win_update_menus(void);

extern void win_show_mouse(void);
extern void win_mouse_click(mouse_button, LPARAM);
extern void win_mouse_release(mouse_button, LPARAM);
extern void win_mouse_wheel(WPARAM, LPARAM);
extern void win_mouse_move(bool nc, LPARAM);

extern void win_key_reset(void);
extern bool win_key_down(WPARAM, LPARAM);
extern bool win_key_up(WPARAM, LPARAM);

extern wchar * dewsl(wchar * wpath);
extern void win_init_drop_target(void);

extern wstring wslicon(wchar * params);

extern char * foreground_cwd(struct child* child);

extern void win_switch(bool back, bool alternate);
extern int search_monitors(int * minx, int * miny, HMONITOR lookup_mon, int get_primary, MONITORINFO *mip);

extern void win_set_ime_open(bool);

extern void show_message(char * msg, UINT type);

void win_process_timer_message(WPARAM message);

void win_tab_set_argv(char** argv);
void win_tab_init(char* home, char* cmd, char** argv, int width, int height, char* title);
int win_tab_count();
int win_active_tab();
void win_tab_change(int change);
void win_tab_move(int amount);
void win_tab_create();
void win_tab_clean();
void win_tab_attention(struct term* term);
void win_tab_set_title(struct term* term, wchar_t* title);
wchar_t* win_tab_get_title(unsigned int idx);

void win_tab_title_push(struct term* term);
wchar_t* win_tab_title_pop(struct term* term);
void win_tab_save_title(struct term* term);
void win_tab_restore_title(struct term* term);

void win_tab_mouse_click(int x);
int win_tab_height();
void win_paint_tabs(LPARAM lp, int width);
void win_for_each_term(void (*cb)(struct term* term));
void win_for_each_term_bool(void (*cb)(struct term* term, bool param), bool param);

bool win_should_die();

#endif
