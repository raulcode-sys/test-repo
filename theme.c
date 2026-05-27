/*
 * theme.c — Global theme + wallpaper system for Triumph OS
 */

typedef struct {
    const char *name;
    const char *desc;
    int wp_idx;
    int fg, fg2, dim, dim2, hi_fg, hi_bg, bg, accent, green, red, yellow, blue;
} Theme;

#define THEME_COUNT 9

static const Theme THEMES[THEME_COUNT] = {
    /* wallpaper-matched */
    {"berserk",     "Dark blue night sky",         0,  51, 117, 33,  67, 17,  51, 17,  51,  82, 196, 226,  39},
    {"sakura",      "Pink cherry blossom",         1, 218, 224,174, 138,  52, 218, 52, 218, 114, 203, 222, 183},
    {"emperor",     "Royal crimson & gold",        2, 196, 208,124,  95,  16, 196, 16, 196, 178, 108, 196, 214},
    /* standalone */
    {"nord",        "Arctic frost",               -1, 110, 146, 60,  66, 236, 110,236, 110, 108, 131, 179,  67},
    {"dracula",     "Vampiric purple",            -1, 141, 183,103,  60, 236, 141,236, 141, 120, 210, 228, 117},
    {"gruvbox",     "Retro warm",                 -1, 214, 223,137, 102, 235, 214,235, 214, 142, 167, 214, 109},
    {"catppuccin",  "Pastel mocha",               -1, 183, 189,146, 103, 236, 183,236, 183, 158, 210, 223, 110},
    {"solarized",   "Precise & balanced",         -1,  37, 136, 66,  60, 235,  37,235,  37,  64, 160, 136,  33},
    {"monochrome",  "Clean & minimal",            -1, 252, 249,245, 240,  16, 252, 16, 252, 252, 249, 249, 249},
};

static int g_theme_idx = 0;

static char TH_FG[24], TH_FG2[24], TH_DIM[24], TH_DIM2[24];
static char TH_HI[48], TH_BG[24], TH_ACC[24];
static char TH_GRN[24], TH_RED[24], TH_YEL[24], TH_BLU[24];

static void show_wallpaper(void);

static void theme_apply(int idx) {
    if (idx < 0 || idx >= THEME_COUNT) idx = 0;
    g_theme_idx = idx;
    const Theme *t = &THEMES[idx];

    snprintf(TH_FG,   sizeof(TH_FG),   "\x1b[38;5;%dm", t->fg);
    snprintf(TH_FG2,  sizeof(TH_FG2),  "\x1b[38;5;%dm", t->fg2);
    snprintf(TH_DIM,  sizeof(TH_DIM),  "\x1b[38;5;%dm", t->dim);
    snprintf(TH_DIM2, sizeof(TH_DIM2), "\x1b[38;5;%dm", t->dim2);
    snprintf(TH_HI,   sizeof(TH_HI),   "\x1b[48;5;%dm\x1b[38;5;%dm", t->fg, t->hi_fg);
    snprintf(TH_BG,   sizeof(TH_BG),   "\x1b[48;5;%dm", t->bg);
    snprintf(TH_ACC,  sizeof(TH_ACC),  "\x1b[38;5;%dm", t->accent);
    snprintf(TH_GRN,  sizeof(TH_GRN),  "\x1b[38;5;%dm", t->green);
    snprintf(TH_RED,  sizeof(TH_RED),  "\x1b[38;5;%dm", t->red);
    snprintf(TH_YEL,  sizeof(TH_YEL),  "\x1b[38;5;%dm", t->yellow);
    snprintf(TH_BLU,  sizeof(TH_BLU),  "\x1b[38;5;%dm", t->blue);

    strcpy(g_theme, TH_ACC);

    if (t->wp_idx >= 0 && t->wp_idx < WP_COUNT) {
        wp_current = t->wp_idx;
        fb_rescale_wp();
    }
}

static void theme_init(void) { theme_apply(0); }

static void th_w(const char *s) { write(1, s, strlen(s)); }
static void th_at(int r, int c) { char b[24]; snprintf(b,24,"\x1b[%d;%dH",r,c); th_w(b); }

static int b_settings(Cmd *c) { (void)c;
    struct termios old, raw;
    tcgetattr(0,&old); raw=old;
    raw.c_lflag &= ~(ICANON|ECHO);
    raw.c_cc[VMIN]=1; raw.c_cc[VTIME]=0;
    tcsetattr(0,TCSANOW,&raw);

    struct winsize ws; ioctl(0,TIOCGWINSZ,&ws);
    int rows=ws.ws_row?ws.ws_row:24;
    int cols=ws.ws_col?ws.ws_col:80;

    int sel = g_theme_idx;
    int scroll = 0;
    int visible = rows - 10;
    if (visible < 4) visible = 4;

    while (1) {
        th_w(TH_BG); th_w("\x1b[2J");

        th_at(2, (cols-30)/2);
        th_w(TH_ACC); th_w("\x1b[1m── SETTINGS  ·  THEMES ──\x1b[22m");

        th_at(4, 4);
        th_w(TH_FG); th_w("\x1b[1m  Theme");
        th_at(4, 22); th_w("Description");
        th_at(4, cols-22); th_w("Wallpaper");
        th_at(4, cols-10); th_w("Preview\x1b[22m");

        th_at(5, 2); th_w(TH_DIM);
        for (int x=0;x<cols-4;x++) th_w("─");

        if (sel < scroll) scroll = sel;
        if (sel >= scroll + visible) scroll = sel - visible + 1;

        for (int i = 0; i < visible && scroll + i < THEME_COUNT; i++) {
            int idx = scroll + i;
            int row = 6 + i;
            th_at(row, 4);

            if (idx == sel) {
                th_w(TH_HI); th_w("\x1b[1m");
                char line[128];
                snprintf(line, sizeof(line), "  ▶ %-14s %-24s %-12s ",
                    THEMES[idx].name, THEMES[idx].desc,
                    THEMES[idx].wp_idx >= 0 ? wp_list[THEMES[idx].wp_idx].name : "current");
                th_w(line);
                th_w("\x1b[22m"); th_w(TH_BG);
            } else {
                char line[128];
                th_w(idx == g_theme_idx ? TH_GRN : TH_FG);
                snprintf(line, sizeof(line), "  %s %-14s ",
                    idx == g_theme_idx ? "✓" : " ", THEMES[idx].name);
                th_w(line);
                th_w(TH_DIM2);
                snprintf(line, sizeof(line), "%-24s ", THEMES[idx].desc);
                th_w(line);
                th_w(TH_DIM);
                snprintf(line, sizeof(line), "%-12s",
                    THEMES[idx].wp_idx >= 0 ? wp_list[THEMES[idx].wp_idx].name : "—");
                th_w(line);
            }

            th_at(row, cols-10);
            char dots[128];
            snprintf(dots, sizeof(dots),
                "\x1b[38;5;%dm● \x1b[38;5;%dm● \x1b[38;5;%dm● \x1b[38;5;%dm●",
                THEMES[idx].accent, THEMES[idx].green, THEMES[idx].yellow, THEMES[idx].red);
            th_w(dots);
        }

        th_at(rows-3, 2); th_w(TH_DIM);
        for (int x=0;x<cols-4;x++) th_w("─");
        th_at(rows-2, 4);
        th_w(TH_ACC); th_w("↑↓ "); th_w(TH_DIM); th_w("browse  ");
        th_w(TH_ACC); th_w("ENTER "); th_w(TH_DIM); th_w("apply  ");
        th_w(TH_ACC); th_w("ESC "); th_w(TH_DIM); th_w("back");
        th_at(rows-1, 4);
        th_w(TH_DIM2); th_w("Current: ");
        th_w(TH_FG); th_w(THEMES[g_theme_idx].name);

        fflush(stdout);

        unsigned char k;
        if (read(0,&k,1)<=0) continue;
        if (k==27) {
            struct termios cur; tcgetattr(0,&cur);
            struct termios tmp=cur; tmp.c_cc[VMIN]=0; tmp.c_cc[VTIME]=1;
            tcsetattr(0,TCSANOW,&tmp);
            unsigned char seq[3]; int n=read(0,seq,3);
            tcsetattr(0,TCSANOW,&cur);
            if (n<=0) break;
            if (n>=2 && seq[0]=='[') {
                if (seq[1]=='A') sel = (sel-1+THEME_COUNT)%THEME_COUNT;
                if (seq[1]=='B') sel = (sel+1)%THEME_COUNT;
            }
            continue;
        }
        if (k=='k'||k=='w') sel = (sel-1+THEME_COUNT)%THEME_COUNT;
        if (k=='j'||k=='s') sel = (sel+1)%THEME_COUNT;
        if (k=='\r'||k=='\n') {
            theme_apply(sel);
            th_at(rows/2, (cols-24)/2);
            th_w(TH_GRN); th_w("\x1b[1m  ✓ Theme applied!  \x1b[22m");
            fflush(stdout);
            usleep(500000);
            show_wallpaper();
        }
        if (k=='q'||k=='Q') break;
    }

    th_w("\x1b[0m\x1b[2J\x1b[H");
    tcsetattr(0,TCSANOW,&old);
    return 0;
}

static int b_theme(Cmd *c) {
    if (c->argc < 2) {
        printf(BLD "%sCurrent: %s" RST " — %s\n",
            TH_ACC, THEMES[g_theme_idx].name, THEMES[g_theme_idx].desc);
        printf(GRY "Wallpaper: %s\n\n" RST,
            THEMES[g_theme_idx].wp_idx >= 0 ?
            wp_list[THEMES[g_theme_idx].wp_idx].name : "(unchanged)");
        printf(GRY "Available themes:\n" RST);
        for (int i=0; i<THEME_COUNT; i++) {
            char dots[64];
            snprintf(dots, sizeof(dots),
                "\x1b[38;5;%dm●\x1b[38;5;%dm●\x1b[38;5;%dm●\x1b[38;5;%dm●",
                THEMES[i].accent, THEMES[i].green, THEMES[i].yellow, THEMES[i].red);
            printf("  %s%-14s" RST " %s%-20s" RST " %s  %s%s" RST "\n",
                i==g_theme_idx ? TH_GRN : TH_FG, THEMES[i].name,
                TH_DIM2, THEMES[i].desc, dots, TH_DIM,
                THEMES[i].wp_idx >= 0 ? wp_list[THEMES[i].wp_idx].name : "");
        }
        printf(GRY "\nUsage: theme <name>  or  settings\n" RST);
        return 0;
    }
    for (int i=0; i<THEME_COUNT; i++) {
        if (strcasecmp(c->argv[1], THEMES[i].name) == 0) {
            theme_apply(i);
            show_wallpaper();
            printf("%s✓ Theme: %s" RST "\n", TH_GRN, THEMES[i].name);
            return 0;
        }
    }
    printf(RED "Unknown theme: %s" RST "\n", c->argv[1]);
    return 1;
}
