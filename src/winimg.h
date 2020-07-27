#ifndef WINIMG_H
#define WINIMG_H

#include "config.h"

extern bool winimg_new(imglist * * ppimg, char * id,
                       unsigned char * pixels, uint len,
                       int top, int left, int width, int height,
                       int pixelwidth, int pixelheight, bool preserveAR,
                       int crop_x, int crop_y, int crop_w, int crop_h);
extern void winimg_destroy(imglist * img);
extern void winimg_lazyinit(imglist * img);
#define winimgs_paint(...) (winimgs_paint)(term_p, ##__VA_ARGS__)
extern void (winimgs_paint)(struct term* term_p);
#define winimgs_clear(...) (winimgs_clear)(term_p, ##__VA_ARGS__)
extern void (winimgs_clear)(struct term* term_p);

extern void win_emoji_show(int x, int y, wchar * efn, void * * bufpoi, int * buflen, int elen, ushort lattr);

extern void save_img(HDC, int x, int y, int w, int h, wstring fn);

#endif
