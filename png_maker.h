#ifndef __PNG_MAKER_H
#define __PNG_MAKER_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

/*********************************************************************************************/
// This code was taken from "https://www.programmingalgorithms.com/algorithm/hsv-to-rgb?lang=C"
struct RGB                                                                                 /**/
{                                                                                          /**/
    unsigned char R;                                                                   /**/
    unsigned char G;                                                                   /**/
    unsigned char B;                                                                   /**/
};                                                                                         /**/
                                                                                           /**/
struct HSV                                                                                 /**/
{                                                                                          /**/
    double H;                                                                          /**/
    double S;                                                                          /**/
    double V;                                                                          /**/
};                                                                                         /**/
/*********************************************************************************************/

struct RGB HSVToRGB(struct HSV hsv);
typedef struct  {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} pixel_t;

typedef struct {
    pixel_t *pixels;
    size_t width;
    size_t height;
} bitmap_t;
// Put functions here.
pixel_t * pixel_at(bitmap_t *bitmap, int x, int y);
int save_png_to_file(bitmap_t *bitmap, const char *path);

#endif /* png_maker_h */
