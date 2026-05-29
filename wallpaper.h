#pragma once
#include "wallpaper_berserk.h"
#include "wallpaper_anime.h"
#include "wallpaper_napoleon.h"

#define WP_W 1920
#define WP_H 1080
#define WP_COUNT 3

typedef struct { const char *name; const unsigned int *data; } WPEntry;
static const WPEntry wp_list[WP_COUNT] = {
    {"Berserk", wp_berserk_data},
    {"Anime", wp_anime_data},
    {"Napoleon", wp_napoleon_data},
};

static int wp_current = 0;
#define wp_data (wp_list[wp_current].data)
