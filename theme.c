/*
 * theme.c — Global theme system
 * Colours stored as pre-built ANSI strings so TUI files can use them.
 */

#define THEME_COUNT 9

typedef struct {
    const char *name;
    const char *desc;
    int wp_idx;
    char fg[24], fg2[24], dim[24], dim2[24];
    char hi[48], bg[24], acc[24];
    char grn[24], red[24], yel[24], blu[24];
} ThemeData;

static ThemeData g_themes[THEME_COUNT];
static int g_theme_idx = 0;

static void theme_build(int i, const char *name, const char *desc, int wp,
    int fg,int fg2,int dim,int dim2,int hfg,int hbg,int bg,
    int acc,int grn,int red,int yel,int blu){
    ThemeData *t = &g_themes[i];
    strncpy((char*)t->name, name, 0); /* trick: store pointer */
    t->name = name; t->desc = desc; t->wp_idx = wp;
    snprintf(t->fg,24,"\x1b[38;5;%dm",fg);
    snprintf(t->fg2,24,"\x1b[38;5;%dm",fg2);
    snprintf(t->dim,24,"\x1b[38;5;%dm",dim);
    snprintf(t->dim2,24,"\x1b[38;5;%dm",dim2);
    snprintf(t->hi,48,"\x1b[48;5;%dm\x1b[38;5;%dm",fg,hfg);
    snprintf(t->bg,24,"\x1b[48;5;%dm",bg);
    snprintf(t->acc,24,"\x1b[38;5;%dm",acc);
    snprintf(t->grn,24,"\x1b[38;5;%dm",grn);
    snprintf(t->red,24,"\x1b[38;5;%dm",red);
    snprintf(t->yel,24,"\x1b[38;5;%dm",yel);
    snprintf(t->blu,24,"\x1b[38;5;%dm",blu);
}

/* accessor functions — return current theme's colour string */
static const char *th_fg(void){return g_themes[g_theme_idx].fg;}
static const char *th_fg2(void){return g_themes[g_theme_idx].fg2;}
static const char *th_dim(void){return g_themes[g_theme_idx].dim;}
static const char *th_dim2(void){return g_themes[g_theme_idx].dim2;}
static const char *th_hi(void){return g_themes[g_theme_idx].hi;}
static const char *th_bg(void){return g_themes[g_theme_idx].bg;}
static const char *th_acc(void){return g_themes[g_theme_idx].acc;}
static const char *th_grn(void){return g_themes[g_theme_idx].grn;}
static const char *th_red(void){return g_themes[g_theme_idx].red;}
static const char *th_yel(void){return g_themes[g_theme_idx].yel;}
static const char *th_blu(void){return g_themes[g_theme_idx].blu;}

static void theme_apply(int idx){
    if(idx<0||idx>=THEME_COUNT)idx=0;
    g_theme_idx=idx;
    strcpy(g_theme, g_themes[idx].acc);
    if(g_themes[idx].wp_idx>=0 && g_themes[idx].wp_idx<WP_COUNT){
        wp_current=g_themes[idx].wp_idx;
        fb_rescale_wp();
    }
}

static void theme_init(void){
    theme_build(0,"berserk","Dark blue night sky",0, 51,117,33,67,17,51,17,51,82,196,226,39);
    theme_build(1,"sakura","Pink cherry blossom",1, 218,224,174,138,52,218,52,218,114,203,222,183);
    theme_build(2,"emperor","Royal crimson & gold",2, 196,208,124,95,16,196,16,196,178,108,196,214);
    theme_build(3,"nord","Arctic frost",-1, 110,146,60,66,236,110,236,110,108,131,179,67);
    theme_build(4,"dracula","Vampiric purple",-1, 141,183,103,60,236,141,236,141,120,210,228,117);
    theme_build(5,"gruvbox","Retro warm",-1, 214,223,137,102,235,214,235,214,142,167,214,109);
    theme_build(6,"catppuccin","Pastel mocha",-1, 183,189,146,103,236,183,236,183,158,210,223,110);
    theme_build(7,"solarized","Precise & balanced",-1, 37,136,66,60,235,37,235,37,64,160,136,33);
    theme_build(8,"monochrome","Clean & minimal",-1, 252,249,245,240,16,252,16,252,252,249,249,249);
    theme_apply(0);
}

/* settings TUI */
static void th_w(const char *s){write(1,s,strlen(s));}
static void th_at(int r,int c){char b[24];snprintf(b,24,"\x1b[%d;%dH",r,c);th_w(b);}

static int b_settings(Cmd *c){(void)c;
    struct termios old,raw;
    tcgetattr(0,&old);raw=old;
    raw.c_lflag&=~(ICANON|ECHO);raw.c_cc[VMIN]=1;raw.c_cc[VTIME]=0;
    tcsetattr(0,TCSANOW,&raw);
    struct winsize ws;ioctl(0,TIOCGWINSZ,&ws);
    int rows=ws.ws_row?ws.ws_row:24,cols=ws.ws_col?ws.ws_col:80;
    int sel=g_theme_idx;
    while(1){
        th_w(th_bg());th_w("\x1b[2J");
        th_at(2,(cols-26)/2);th_w(th_acc());th_w("\x1b[1m── SETTINGS · THEMES ──\x1b[22m");
        th_at(4,4);th_w(th_fg());th_w("\x1b[1m  Theme          Description              Wallpaper    Preview\x1b[22m");
        th_at(5,2);th_w(th_dim());for(int x=0;x<cols-4;x++)th_w("─");
        for(int i=0;i<THEME_COUNT;i++){
            th_at(7+i,4);
            if(i==sel){th_w(th_hi());th_w("\x1b[1m");
                char l[128];snprintf(l,128,"  > %-14s %-24s %-12s",g_themes[i].name,g_themes[i].desc,
                    g_themes[i].wp_idx>=0?wp_list[g_themes[i].wp_idx].name:"current");
                th_w(l);th_w("\x1b[22m");th_w(th_bg());
            } else {
                th_w(i==g_theme_idx?th_grn():th_fg());
                char l[128];snprintf(l,128,"  %s %-14s ",i==g_theme_idx?"*":" ",g_themes[i].name);
                th_w(l);th_w(th_dim2());
                char d[64];snprintf(d,64,"%-24s ",g_themes[i].desc);th_w(d);
                th_w(th_dim());
                char w[16];snprintf(w,16,"%-12s",g_themes[i].wp_idx>=0?wp_list[g_themes[i].wp_idx].name:"-");
                th_w(w);
            }
            th_at(7+i,cols-10);
            char dots[128];snprintf(dots,128,"%s● %s● %s● %s●",
                g_themes[i].acc,g_themes[i].grn,g_themes[i].yel,g_themes[i].red);
            th_w(dots);
        }
        th_at(rows-3,2);th_w(th_dim());for(int x=0;x<cols-4;x++)th_w("─");
        th_at(rows-2,4);th_w(th_acc());th_w("Up/Down ");th_w(th_dim());th_w("browse  ");
        th_w(th_acc());th_w("ENTER ");th_w(th_dim());th_w("apply  ");
        th_w(th_acc());th_w("ESC ");th_w(th_dim());th_w("back");
        fflush(stdout);
        unsigned char k;if(read(0,&k,1)<=0)continue;
        if(k==27){struct termios cur;tcgetattr(0,&cur);struct termios tmp=cur;
            tmp.c_cc[VMIN]=0;tmp.c_cc[VTIME]=1;tcsetattr(0,TCSANOW,&tmp);
            unsigned char seq[3];int n=read(0,seq,3);tcsetattr(0,TCSANOW,&cur);
            if(n<=0)break;
            if(n>=2&&seq[0]=='['){if(seq[1]=='A')sel=(sel-1+THEME_COUNT)%THEME_COUNT;
                if(seq[1]=='B')sel=(sel+1)%THEME_COUNT;}
            continue;}
        if(k=='k'||k=='w')sel=(sel-1+THEME_COUNT)%THEME_COUNT;
        if(k=='j'||k=='s')sel=(sel+1)%THEME_COUNT;
        if(k=='\r'||k=='\n'){theme_apply(sel);show_wallpaper();
            th_at(rows/2,(cols-20)/2);th_w(th_grn());th_w("\x1b[1m  * Applied!  \x1b[22m");
            fflush(stdout);usleep(400000);}
        if(k=='q'||k=='Q')break;
    }
    th_w("\x1b[0m\x1b[2J\x1b[H");tcsetattr(0,TCSANOW,&old);return 0;
}

static int b_theme(Cmd *c){
    if(c->argc<2){
        printf(BLD"%sCurrent: %s"RST" - %s\n",th_acc(),g_themes[g_theme_idx].name,g_themes[g_theme_idx].desc);
        for(int i=0;i<THEME_COUNT;i++)
            printf("  %s%-14s"RST" %s%s"RST"\n",i==g_theme_idx?th_grn():th_fg(),
                g_themes[i].name,th_dim2(),g_themes[i].desc);
        printf(GRY"\nUsage: theme <name>  or  settings\n"RST);return 0;}
    for(int i=0;i<THEME_COUNT;i++){
        if(strcasecmp(c->argv[1],g_themes[i].name)==0){
            theme_apply(i);show_wallpaper();
            printf("%s* Theme: %s"RST"\n",th_grn(),g_themes[i].name);return 0;}}
    printf(RED"Unknown theme: %s"RST"\n",c->argv[1]);return 1;
}
