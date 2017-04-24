#ifndef TERMSEARCH_H
#define TERMSEARCH_H

extern int SEARCHBAR_HEIGHT;

extern bool win_search_visible(void);
extern void win_open_search(void);
extern void win_update_search(void);
extern void win_paint_exclude_search(HDC dc);
extern HWND win_get_search_wnd(void);
extern HWND win_get_search_edit_wnd(void);

#endif
