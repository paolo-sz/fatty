#ifndef TEK_H
#define TEK_H

#include <wchar.h>
#include "std.h"

// https://vt100.net/docs/vt3xx-gp/chapter13.html#F13-2
enum tekmode {
  TEKMODE_OFF, TEKMODE_ALPHA, 
  TEKMODE_GRAPH0, TEKMODE_GRAPH, TEKMODE_POINT_PLOT, TEKMODE_SPECIAL_PLOT, 
  TEKMODE_INCREMENTAL_PLOT, 
  TEKMODE_GIN
};

extern enum tekmode tek_mode;
extern bool tek_bypass;

#define tek_page(...) (tek_page)(term_p, ##__VA_ARGS__)
extern void (tek_page)(struct term* term_p);
#define tek_reset(...) (tek_reset)(term_p, ##__VA_ARGS__)
extern void (tek_reset)(struct term* term_p);
#define tek_init(...) (tek_init)(term_p, ##__VA_ARGS__)
extern void (tek_init)(struct term* term_p, int glow);
extern void tek_gin(void);

extern void tek_font(short f);
extern void tek_write(wchar c, int width);
#define tek_enq(...) (tek_enq)(child_p, ##__VA_ARGS__)
extern void (tek_enq)(struct child* child_p);
extern void tek_alt(bool);
extern void tek_copy(void);
#define tek_clear(...) (tek_clear)(term_p, ##__VA_ARGS__)
extern void (tek_clear)(struct term* term_p);
extern void tek_set_font(wchar * fn);

#define tek_move_to(...) (tek_move_to)(term_p, ##__VA_ARGS__)
extern void (tek_move_to)(struct term* term_p, int y, int x);
extern void tek_move_by(int dy, int dx);
#define tek_send_address(...) (tek_send_address)(child_p, ##__VA_ARGS__)
extern void (tek_send_address)(struct child* child_p);

extern void tek_beam(bool defocused, bool write_through, char vector_style);
extern void tek_intensity(bool defocused, int intensity);

extern void tek_address(char *);
extern void tek_pen(bool on);
extern void tek_step(char c);

#define tek_paint(...) (tek_paint)(term_p, ##__VA_ARGS__)
extern void (tek_paint)(struct term* term_p);

#endif
