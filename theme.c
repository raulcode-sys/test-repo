/* theme.c — Themes change prompt colour + wallpaper only */

#define THEME_COUNT 9

static const struct {
    const char *name, *desc;
    int wp_idx;
    const char *prompt_col;
} THEMES[THEME_COUNT] = {
    {"berserk",    "Dark blue night sky",    0, "\x1b[38;5;51m"},
    {"sakura",     "Pink cherry blossom",    1, "\x1b[38;5;218m"},
    {"emperor",    "Royal crimson & gold",   2, "\x1b[38;5;196m"},
    {"nord",       "Arctic frost",          -1, "\x1b[38;5;110m"},
    {"dracula",    "Vampiric purple",       -1, "\x1b[38;5;141m"},
    {"gruvbox",    "Retro warm",            -1, "\x1b[38;5;214m"},
    {"catppuccin", "Pastel mocha",          -1, "\x1b[38;5;183m"},
    {"solarized",  "Precise & balanced",    -1, "\x1b[38;5;37m"},
    {"monochrome", "Clean & minimal",       -1, "\x1b[38;5;252m"},
};
static int g_theme_idx = 0;

static void theme_apply(int idx) {
    if (idx<0||idx>=THEME_COUNT) idx=0;
    g_theme_idx = idx;
    strcpy(g_theme, THEMES[idx].prompt_col);
    if (THEMES[idx].wp_idx>=0 && THEMES[idx].wp_idx<WP_COUNT) {
        wp_current = THEMES[idx].wp_idx;
        fb_rescale_wp();
    }
}
static void theme_init(void) { theme_apply(0); }

static int b_settings(Cmd *c) { (void)c;
    struct termios old, raw;
    tcgetattr(0,&old); raw=old;
    raw.c_lflag &= ~(ICANON|ECHO);
    raw.c_cc[VMIN]=1; raw.c_cc[VTIME]=0;
    tcsetattr(0,TCSANOW,&raw);
    struct winsize ws; ioctl(0,TIOCGWINSZ,&ws);
    int rows=ws.ws_row?ws.ws_row:24, cols=ws.ws_col?ws.ws_col:80;
    int sel=g_theme_idx;

    while (1) {
        printf("\x1b[48;5;17m\x1b[2J");
        printf("\x1b[%d;%dH\x1b[38;5;51m\x1b[1m── SETTINGS · THEMES ──\x1b[22m",
               2, (cols-24)/2);
        printf("\x1b[4;4H\x1b[38;5;51m\x1b[1m  Theme          Description              Wallpaper\x1b[22m");
        printf("\x1b[5;2H\x1b[38;5;33m");
        for(int x=0;x<cols-4;x++) printf("─");

        for(int i=0;i<THEME_COUNT;i++){
            printf("\x1b[%d;4H",7+i);
            if(i==sel){
                printf("\x1b[48;5;51m\x1b[38;5;17m\x1b[1m");
                printf("  > %-14s %-24s %-12s",
                    THEMES[i].name, THEMES[i].desc,
                    THEMES[i].wp_idx>=0?wp_list[THEMES[i].wp_idx].name:"current");
                printf("\x1b[22m\x1b[48;5;17m");
            } else {
                printf("%s  %s %-14s \x1b[38;5;67m%-24s \x1b[38;5;33m%-12s",
                    i==g_theme_idx?"\x1b[38;5;82m":"\x1b[38;5;51m",
                    i==g_theme_idx?"*":" ",
                    THEMES[i].name, THEMES[i].desc,
                    THEMES[i].wp_idx>=0?wp_list[THEMES[i].wp_idx].name:"-");
            }
            /* colour preview */
            printf("\x1b[%d;%dH%s●\x1b[0m\x1b[48;5;17m",
                   7+i, cols-6, THEMES[i].prompt_col);
        }

        printf("\x1b[%d;2H\x1b[38;5;33m",rows-3);
        for(int x=0;x<cols-4;x++) printf("─");
        printf("\x1b[%d;4H\x1b[38;5;51mUp/Down \x1b[38;5;33mbrowse  "
               "\x1b[38;5;51mENTER \x1b[38;5;33mapply  "
               "\x1b[38;5;51mESC \x1b[38;5;33mback", rows-2);
        printf("\x1b[%d;4H\x1b[38;5;67mCurrent: \x1b[38;5;51m%s\x1b[0m",
               rows-1, THEMES[g_theme_idx].name);
        fflush(stdout);

        unsigned char k;
        if(read(0,&k,1)<=0) continue;
        if(k==27){
            struct termios cur;tcgetattr(0,&cur);
            struct termios tmp=cur;tmp.c_cc[VMIN]=0;tmp.c_cc[VTIME]=1;
            tcsetattr(0,TCSANOW,&tmp);
            unsigned char seq[3];int n=read(0,seq,3);
            tcsetattr(0,TCSANOW,&cur);
            if(n<=0) break;
            if(n>=2&&seq[0]=='['){
                if(seq[1]=='A') sel=(sel-1+THEME_COUNT)%THEME_COUNT;
                if(seq[1]=='B') sel=(sel+1)%THEME_COUNT;
            }
            continue;
        }
        if(k=='k'||k=='w') sel=(sel-1+THEME_COUNT)%THEME_COUNT;
        if(k=='j'||k=='s') sel=(sel+1)%THEME_COUNT;
        if(k=='\r'||k=='\n'){
            theme_apply(sel);
            show_wallpaper();
            printf("\x1b[%d;%dH\x1b[38;5;82m\x1b[1m  * Applied!  \x1b[22m",
                   rows/2,(cols-14)/2);
            fflush(stdout);usleep(400000);
        }
        if(k=='q'||k=='Q') break;
    }
    printf("\x1b[0m\x1b[2J\x1b[H");
    tcsetattr(0,TCSANOW,&old);
    return 0;
}

static int b_theme(Cmd *c){
    if(c->argc<2){
        printf(BLD CYN"Current: %s"RST" - %s\n",THEMES[g_theme_idx].name,THEMES[g_theme_idx].desc);
        for(int i=0;i<THEME_COUNT;i++)
            printf("  %s%-14s"RST" "GRY"%s"RST"\n",
                i==g_theme_idx?GRN:CYN,THEMES[i].name,THEMES[i].desc);
        printf(GRY"\nUsage: theme <name>  or  settings\n"RST);return 0;
    }
    for(int i=0;i<THEME_COUNT;i++){
        if(strcasecmp(c->argv[1],THEMES[i].name)==0){
            theme_apply(i);show_wallpaper();
            printf(GRN"* Theme: %s"RST"\n",THEMES[i].name);return 0;}}
    printf(RED"Unknown theme: %s"RST"\n",c->argv[1]);return 1;
}
