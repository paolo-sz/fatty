#ifndef WINIMG_H
#define WINIMG_H

#include "config.h"

#define winimg_new(...) (winimg_new)(term_p, ##__VA_ARGS__)
extern bool (winimg_new)(struct term* term_p, imglist * * ppimg, char * id,
                       unsigned char * pixels, uint len,
                       int left, int scrtop, int width, int height,
                       int pixelwidth, int pixelheight, bool preserveAR,
                       int crop_x, int crop_y, int crop_w, int crop_h,
                       int attr);
extern void winimg_destroy(imglist * img);
extern void winimg_lazyinit(imglist * img);
#define winimgs_paint(...) (winimgs_paint)(term_p, ##__VA_ARGS__)
extern void (winimgs_paint)(struct term* term_p);
#define winimgs_clear(...) (winimgs_clear)(term_p, ##__VA_ARGS__)
extern void (winimgs_clear)(struct term* term_p);

// override suppression of repetitive image painting
extern bool force_imgs;

#define win_emoji_show(...) (win_emoji_show)(term_p, ##__VA_ARGS__)
extern void (win_emoji_show)(struct term* term_p, int x, int y, wchar * efn, void * * bufpoi, int * buflen, int elen, ushort lattr, bool italic);

extern void save_img(HDC, int x, int y, int w, int h, wstring fn);

#endif
