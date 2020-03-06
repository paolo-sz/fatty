#ifndef TERMSEARCH_H
#define TERMSEARCH_H

extern int SEARCHBAR_HEIGHT;

#define win_search_visible(...) (win_search_visible)(term_p, ##__VA_ARGS__)
extern bool (win_search_visible)(struct term* term_p);
#define win_open_search(...) (win_open_search)(term_p, ##__VA_ARGS__)
extern void (win_open_search)(struct term* term_p);
#define win_update_search(...) (win_update_search)(term_p, ##__VA_ARGS__)
extern void (win_update_search)(struct term* term_p);
#define win_paint_exclude_search(...) (win_paint_exclude_search)(term_p, ##__VA_ARGS__)
extern void (win_paint_exclude_search)(struct term* term_p, HDC dc);
extern HWND win_get_search_wnd(void);
extern HWND win_get_search_edit_wnd(void);

#endif
