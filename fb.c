/*
 * fb.c - Triumph OS framebuffer compositor with transparency
 *
 * Boot → wallpaper. Shift+M/T open menu/terminal.
 * Menu and terminal run in a PTY, output captured and rendered
 * as pixels over a semi-transparent panel on the wallpaper.
 * Like Alacritty — real terminal, transparent background.
 */

#pragma once

#include <linux/fb.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <sys/mman.h>
#include <pty.h>
#include <pthread.h>
#include "wallpaper.h"

/* ── tuning ─────────────────────────────────────────────── */
#define PANEL_ALPHA  140   /* 0=invisible 255=opaque */
#define COL_PANEL_BG 0x060D1A

/* ── bitmap font 6x10 ───────────────────────────────────── */
static const unsigned char FONT6x10[95][10]={
{0,0,0,0,0,0,0,0,0,0},
{0x08,0x08,0x08,0x08,0x08,0,0x08,0,0,0},
{0x14,0x14,0,0,0,0,0,0,0,0},
{0x14,0x14,0x3E,0x14,0x3E,0x14,0x14,0,0,0},
{0x08,0x1E,0x28,0x1C,0x0A,0x3C,0x08,0,0,0},
{0x30,0x32,0x04,0x08,0x10,0x26,0x06,0,0,0},
{0x10,0x28,0x28,0x10,0x2A,0x24,0x1A,0,0,0},
{0x08,0x08,0,0,0,0,0,0,0,0},
{0x04,0x08,0x10,0x10,0x10,0x08,0x04,0,0,0},
{0x10,0x08,0x04,0x04,0x04,0x08,0x10,0,0,0},
{0x08,0x2A,0x1C,0x08,0x1C,0x2A,0x08,0,0,0},
{0,0x08,0x08,0x3E,0x08,0x08,0,0,0,0},
{0,0,0,0,0,0x08,0x08,0x10,0,0},
{0,0,0,0x3E,0,0,0,0,0,0},
{0,0,0,0,0,0,0x08,0,0,0},
{0,0x02,0x04,0x08,0x10,0x20,0,0,0,0},
{0x1C,0x22,0x26,0x2A,0x32,0x22,0x1C,0,0,0},
{0x08,0x18,0x08,0x08,0x08,0x08,0x1C,0,0,0},
{0x1C,0x22,0x02,0x04,0x08,0x10,0x3E,0,0,0},
{0x1C,0x22,0x02,0x0C,0x02,0x22,0x1C,0,0,0},
{0x04,0x0C,0x14,0x24,0x3E,0x04,0x04,0,0,0},
{0x3E,0x20,0x3C,0x02,0x02,0x22,0x1C,0,0,0},
{0x0C,0x10,0x20,0x3C,0x22,0x22,0x1C,0,0,0},
{0x3E,0x02,0x04,0x08,0x10,0x10,0x10,0,0,0},
{0x1C,0x22,0x22,0x1C,0x22,0x22,0x1C,0,0,0},
{0x1C,0x22,0x22,0x1E,0x02,0x04,0x18,0,0,0},
{0,0x08,0,0,0,0x08,0,0,0,0},
{0,0x08,0,0,0,0x08,0x08,0x10,0,0},
{0x04,0x08,0x10,0x20,0x10,0x08,0x04,0,0,0},
{0,0,0x3E,0,0x3E,0,0,0,0,0},
{0x10,0x08,0x04,0x02,0x04,0x08,0x10,0,0,0},
{0x1C,0x22,0x02,0x04,0x08,0,0x08,0,0,0},
{0x1C,0x22,0x2E,0x2A,0x2E,0x20,0x1C,0,0,0},
{0x08,0x14,0x22,0x22,0x3E,0x22,0x22,0,0,0},
{0x3C,0x22,0x22,0x3C,0x22,0x22,0x3C,0,0,0},
{0x1C,0x22,0x20,0x20,0x20,0x22,0x1C,0,0,0},
{0x38,0x24,0x22,0x22,0x22,0x24,0x38,0,0,0},
{0x3E,0x20,0x20,0x3C,0x20,0x20,0x3E,0,0,0},
{0x3E,0x20,0x20,0x3C,0x20,0x20,0x20,0,0,0},
{0x1C,0x22,0x20,0x2E,0x22,0x22,0x1C,0,0,0},
{0x22,0x22,0x22,0x3E,0x22,0x22,0x22,0,0,0},
{0x1C,0x08,0x08,0x08,0x08,0x08,0x1C,0,0,0},
{0x02,0x02,0x02,0x02,0x02,0x22,0x1C,0,0,0},
{0x22,0x24,0x28,0x30,0x28,0x24,0x22,0,0,0},
{0x20,0x20,0x20,0x20,0x20,0x20,0x3E,0,0,0},
{0x22,0x36,0x2A,0x2A,0x22,0x22,0x22,0,0,0},
{0x22,0x32,0x2A,0x26,0x22,0x22,0x22,0,0,0},
{0x1C,0x22,0x22,0x22,0x22,0x22,0x1C,0,0,0},
{0x3C,0x22,0x22,0x3C,0x20,0x20,0x20,0,0,0},
{0x1C,0x22,0x22,0x22,0x2A,0x24,0x1A,0,0,0},
{0x3C,0x22,0x22,0x3C,0x28,0x24,0x22,0,0,0},
{0x1C,0x22,0x20,0x1C,0x02,0x22,0x1C,0,0,0},
{0x3E,0x08,0x08,0x08,0x08,0x08,0x08,0,0,0},
{0x22,0x22,0x22,0x22,0x22,0x22,0x1C,0,0,0},
{0x22,0x22,0x22,0x14,0x14,0x08,0x08,0,0,0},
{0x22,0x22,0x22,0x2A,0x2A,0x36,0x22,0,0,0},
{0x22,0x22,0x14,0x08,0x14,0x22,0x22,0,0,0},
{0x22,0x22,0x14,0x08,0x08,0x08,0x08,0,0,0},
{0x3E,0x02,0x04,0x08,0x10,0x20,0x3E,0,0,0},
{0x1C,0x10,0x10,0x10,0x10,0x10,0x1C,0,0,0},
{0,0x20,0x10,0x08,0x04,0x02,0,0,0,0},
{0x1C,0x04,0x04,0x04,0x04,0x04,0x1C,0,0,0},
{0x08,0x14,0x22,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0x3E,0,0,0},
{0x10,0x08,0,0,0,0,0,0,0,0},
{0,0,0x1C,0x02,0x1E,0x22,0x1E,0,0,0},
{0x20,0x20,0x2C,0x32,0x22,0x32,0x2C,0,0,0},
{0,0,0x1C,0x20,0x20,0x20,0x1C,0,0,0},
{0x02,0x02,0x1A,0x26,0x22,0x26,0x1A,0,0,0},
{0,0,0x1C,0x22,0x3E,0x20,0x1C,0,0,0},
{0x0C,0x10,0x10,0x3C,0x10,0x10,0x10,0,0,0},
{0,0,0x1E,0x22,0x1E,0x02,0x1C,0,0,0},
{0x20,0x20,0x2C,0x32,0x22,0x22,0x22,0,0,0},
{0x08,0,0x18,0x08,0x08,0x08,0x1C,0,0,0},
{0x04,0,0x04,0x04,0x04,0x24,0x18,0,0,0},
{0x20,0x20,0x24,0x28,0x30,0x28,0x24,0,0,0},
{0x18,0x08,0x08,0x08,0x08,0x08,0x1C,0,0,0},
{0,0,0x36,0x2A,0x2A,0x22,0x22,0,0,0},
{0,0,0x2C,0x32,0x22,0x22,0x22,0,0,0},
{0,0,0x1C,0x22,0x22,0x22,0x1C,0,0,0},
{0,0,0x2C,0x32,0x3C,0x20,0x20,0,0,0},
{0,0,0x1A,0x26,0x1E,0x02,0x02,0,0,0},
{0,0,0x2C,0x32,0x20,0x20,0x20,0,0,0},
{0,0,0x1E,0x20,0x1C,0x02,0x3C,0,0,0},
{0x10,0x10,0x3C,0x10,0x10,0x12,0x0C,0,0,0},
{0,0,0x22,0x22,0x22,0x26,0x1A,0,0,0},
{0,0,0x22,0x22,0x14,0x14,0x08,0,0,0},
{0,0,0x22,0x22,0x2A,0x2A,0x14,0,0,0},
{0,0,0x22,0x14,0x08,0x14,0x22,0,0,0},
{0,0,0x22,0x22,0x1E,0x02,0x1C,0,0,0},
{0,0,0x3E,0x04,0x08,0x10,0x3E,0,0,0},
{0x0C,0x10,0x10,0x20,0x10,0x10,0x0C,0,0,0},
{0x08,0x08,0x08,0,0x08,0x08,0x08,0,0,0},
{0x18,0x04,0x04,0x02,0x04,0x04,0x18,0,0,0},
{0x14,0x28,0,0,0,0,0,0,0,0},
};

#define FW 6
#define FH 10
#define FS 2
#define FCW (FW*FS)
#define FCH (FH*FS)

/* ── framebuffer ─────────────────────────────────────────── */
typedef struct {
    int fd, w, h, stride, bpp;
    int r_off, g_off, b_off;
    char *mem;
    size_t memsize;
    unsigned int *wp;
} FB;
static FB fb = {.fd=-1};

static int menu_open = 0;
static int term_open = 0;

/* ── pixel ops ───────────────────────────────────────────── */
static inline unsigned int fb_blend(unsigned int bg, unsigned int fg, int a){
    unsigned int rb=bg&0xFF00FF, g_=bg&0x00FF00;
    unsigned int rb2=fg&0xFF00FF, g2=fg&0x00FF00;
    return (((rb*(256-a)+rb2*a)>>8)&0xFF00FF)|(((g_*(256-a)+g2*a)>>8)&0x00FF00);
}

static inline void fb_put(int x, int y, unsigned int rgb){
    if((unsigned)x>=(unsigned)fb.w||(unsigned)y>=(unsigned)fb.h) return;
    unsigned int r=(rgb>>16)&0xff,g=(rgb>>8)&0xff,b=rgb&0xff;
    if(fb.bpp==32)
        *(unsigned int*)(fb.mem+y*fb.stride+x*4)=(r<<fb.r_off)|(g<<fb.g_off)|(b<<fb.b_off);
    else if(fb.bpp==16)
        *(unsigned short*)(fb.mem+y*fb.stride+x*2)=((r>>3)<<11)|((g>>2)<<5)|(b>>3);
}

static inline unsigned int wp_get(int x, int y){
    if(!fb.wp||(unsigned)x>=(unsigned)fb.w||(unsigned)y>=(unsigned)fb.h) return 0;
    return fb.wp[y*fb.w+x];
}

/* ── wallpaper ───────────────────────────────────────────── */
static void fb_draw_wallpaper(void){
    if(fb.fd<0||!fb.wp) return;
    if(fb.bpp==32&&fb.r_off==16&&fb.g_off==8&&fb.b_off==0)
        for(int y=0;y<fb.h;y++)
            memcpy(fb.mem+y*fb.stride,fb.wp+y*fb.w,(size_t)fb.w*4);
    else
        for(int y=0;y<fb.h;y++)
            for(int x=0;x<fb.w;x++)
                fb_put(x,y,fb.wp[y*fb.w+x]);
}

static void fb_rescale_wp(void){
    if(!fb.wp) return;
    for(int y=0;y<fb.h;y++){
        int sy=y*WP_H/fb.h;if(sy>=WP_H)sy=WP_H-1;
        for(int x=0;x<fb.w;x++){
            int sx=x*WP_W/fb.w;if(sx>=WP_W)sx=WP_W-1;
            fb.wp[y*fb.w+x]=wp_data[sy*WP_W+sx];
        }
    }
}

static void show_wallpaper(void){
    fb_draw_wallpaper();
    write(1,"\x1b[2J\x1b[H\x1b[?25l",14);
    fflush(stdout);
}

/* ── draw transparent panel ──────────────────────────────── */
static void fb_draw_panel(int px, int py, int pw, int ph){
    for(int y=py;y<py+ph&&y<fb.h;y++)
        for(int x=px;x<px+pw&&x<fb.w;x++)
            fb_put(x,y,fb_blend(wp_get(x,y),COL_PANEL_BG,PANEL_ALPHA));
}

/* ── render character on framebuffer ─────────────────────── */
static void fb_char(int px, int py, char c, unsigned int col){
    if(c<32||c>126) return;
    const unsigned char *g=FONT6x10[(unsigned char)c-32];
    for(int row=0;row<FH;row++){
        unsigned char bits=g[row];
        for(int cb=0;cb<FW;cb++){
            if(!((bits>>(5-cb))&1)) continue;
            for(int sy=0;sy<FS;sy++)
                for(int sx=0;sx<FS;sx++)
                    fb_put(px+cb*FS+sx,py+row*FS+sy,col);
        }
    }
}

/* ── virtual screen for PTY capture ──────────────────────── */
#define VROWS 60
#define VCOLS 120

static char vscreen[VROWS][VCOLS+1];
static unsigned int vcolors[VROWS][VCOLS];
static int vcur_r=0, vcur_c=0;
static unsigned int vcur_fg=0xCCEEFF;
static pthread_mutex_t vlock=PTHREAD_MUTEX_INITIALIZER;

static void vscroll(void){
    memmove(vscreen[0],vscreen[1],(VROWS-1)*sizeof(vscreen[0]));
    memmove(vcolors[0],vcolors[1],(VROWS-1)*sizeof(vcolors[0]));
    memset(vscreen[VROWS-1],0,sizeof(vscreen[0]));
    memset(vcolors[VROWS-1],0,sizeof(vcolors[0]));
    vcur_r=VROWS-1;
}

static void vputc(char c){
    if(c=='\n'){vcur_c=0;vcur_r++;if(vcur_r>=VROWS)vscroll();return;}
    if(c=='\r'){vcur_c=0;return;}
    if(c=='\b'||c==127){if(vcur_c>0){vcur_c--;vscreen[vcur_r][vcur_c]=' ';}return;}
    if(c=='\t'){int next=(vcur_c+8)&~7;while(vcur_c<next&&vcur_c<VCOLS){vscreen[vcur_r][vcur_c]=' ';vcolors[vcur_r][vcur_c]=vcur_fg;vcur_c++;}return;}
    if(vcur_c>=VCOLS){vcur_c=0;vcur_r++;if(vcur_r>=VROWS)vscroll();}
    if(c>=32&&c<127){
        vscreen[vcur_r][vcur_c]=c;
        vcolors[vcur_r][vcur_c]=vcur_fg;
        vcur_c++;
    }
}

/* ANSI colour map (basic 8 + bright 8) */
static const unsigned int ANSI_COLS[16]={
    0x1A1A2E,0xCC4444,0x44CC44,0xCCCC44,0x4444CC,0xCC44CC,0x44CCCC,0xCCCCCC,
    0x666688,0xFF6666,0x66FF66,0xFFFF66,0x6666FF,0xFF66FF,0x66FFFF,0xFFFFFF,
};

static void vwrite(const char *buf, int n){
    pthread_mutex_lock(&vlock);
    int i=0;
    while(i<n){
        if(buf[i]==0x1b && i+1<n && buf[i+1]=='['){
            /* parse ANSI CSI sequence */
            i+=2;
            int params[8]={0}; int np=0;
            while(i<n && ((buf[i]>='0'&&buf[i]<='9')||buf[i]==';')){
                if(buf[i]==';'){if(np<7)np++;i++;}
                else{params[np]=params[np]*10+(buf[i]-'0');i++;}
            }
            if(np<7)np++;
            if(i<n){
                char cmd=buf[i++];
                if(cmd=='m'){ /* SGR */
                    for(int p=0;p<np;p++){
                        int v=params[p];
                        if(v==0) vcur_fg=0xCCEEFF;
                        else if(v==1) {} /* bold - ignore for now */
                        else if(v>=30&&v<=37) vcur_fg=ANSI_COLS[v-30];
                        else if(v>=90&&v<=97) vcur_fg=ANSI_COLS[v-90+8];
                        else if(v==38&&p+2<np&&params[p+1]==5){
                            /* 256-color: approximate */
                            int c=params[p+2];
                            if(c<16) vcur_fg=ANSI_COLS[c];
                            else if(c<232){
                                c-=16;
                                int r=(c/36)*51,g=((c/6)%6)*51,b=(c%6)*51;
                                vcur_fg=(r<<16)|(g<<8)|b;
                            } else {
                                int v2=8+(c-232)*10;
                                vcur_fg=(v2<<16)|(v2<<8)|v2;
                            }
                            p+=2;
                        }
                    }
                }
                else if(cmd=='H'||cmd=='f'){
                    vcur_r=params[0]>0?params[0]-1:0;
                    vcur_c=np>1&&params[1]>0?params[1]-1:0;
                    if(vcur_r>=VROWS)vcur_r=VROWS-1;
                    if(vcur_c>=VCOLS)vcur_c=VCOLS-1;
                }
                else if(cmd=='J'){
                    if(params[0]==2){memset(vscreen,0,sizeof(vscreen));memset(vcolors,0,sizeof(vcolors));vcur_r=0;vcur_c=0;}
                }
                else if(cmd=='K'){
                    memset(vscreen[vcur_r]+vcur_c,0,VCOLS-vcur_c);
                }
                else if(cmd=='A'){int n2=params[0]?params[0]:1;vcur_r-=n2;if(vcur_r<0)vcur_r=0;}
                else if(cmd=='B'){int n2=params[0]?params[0]:1;vcur_r+=n2;if(vcur_r>=VROWS)vcur_r=VROWS-1;}
                else if(cmd=='C'){int n2=params[0]?params[0]:1;vcur_c+=n2;if(vcur_c>=VCOLS)vcur_c=VCOLS-1;}
                else if(cmd=='D'){int n2=params[0]?params[0]:1;vcur_c-=n2;if(vcur_c<0)vcur_c=0;}
            }
            continue;
        }
        vputc(buf[i++]);
    }
    pthread_mutex_unlock(&vlock);
}

/* ── render virtual screen to framebuffer ────────────────── */
static void fb_render_vscreen(int px, int py, int pw, int ph){
    /* draw transparent panel */
    fb_draw_panel(px,py,pw,ph);

    /* render text */
    int margin=8;
    int max_r=(ph-margin*2)/FCH;
    int max_c=(pw-margin*2)/FCW;
    if(max_r>VROWS)max_r=VROWS;
    if(max_c>VCOLS)max_c=VCOLS;

    pthread_mutex_lock(&vlock);
    for(int r=0;r<max_r;r++){
        for(int c=0;c<max_c;c++){
            char ch=vscreen[r][c];
            if(ch<32) continue;
            unsigned int col=vcolors[r][c];
            if(!col) col=0xCCEEFF;
            fb_char(px+margin+c*FCW, py+margin+r*FCH, ch, col);
        }
    }
    /* cursor */
    int cx=px+margin+vcur_c*FCW;
    int cy=py+margin+vcur_r*FCH;
    for(int y=cy;y<cy+FCH&&y<py+ph;y++)
        fb_put(cx,y,0x33CCFF);
    pthread_mutex_unlock(&vlock);
}

/* ── PTY management ──────────────────────────────────────── */
static int pty_master=-1;
static pid_t pty_child=-1;
static volatile int pty_active=0;

/* panel geometry */
static void panel_rect(int *px,int *py,int *pw,int *ph){
    int m=fb.w*3/100;
    *px=m;*py=m;*pw=fb.w-m*2;*ph=fb.h-m*2;
}

static void pty_open(const char *cmd){
    memset(vscreen,0,sizeof(vscreen));
    memset(vcolors,0,sizeof(vcolors));
    vcur_r=0;vcur_c=0;vcur_fg=0xCCEEFF;

    int px,py,pw,ph; panel_rect(&px,&py,&pw,&ph);
    int cols=(pw-16)/FCW, rows=(ph-16)/FCH;
    if(cols>VCOLS)cols=VCOLS;if(rows>VROWS)rows=VROWS;

    struct winsize ws={rows,cols,0,0};
    pty_child=forkpty(&pty_master,NULL,NULL,&ws);
    if(pty_child==0){
        setenv("TERM","xterm-256color",1);
        setenv("COLUMNS","120",1);
        setenv("LINES","60",1);
        char *av[]={"/bin/triumph",(char*)cmd,NULL};
        if(!cmd) av[1]=NULL;
        execv("/bin/triumph",av);
        execv("/bin/sh",(char*[]){"sh",NULL});
        _exit(1);
    }
    pty_active=1;
}

static void pty_close(void){
    pty_active=0;
    if(pty_child>0){kill(pty_child,SIGTERM);waitpid(pty_child,NULL,WNOHANG);pty_child=-1;}
    if(pty_master>=0){close(pty_master);pty_master=-1;}
}

/* reader thread — reads PTY output, parses ANSI, updates vscreen, redraws */
static void *pty_reader(void *arg){
    (void)arg;
    char buf[4096];
    while(pty_active){
        int n=read(pty_master,buf,sizeof(buf));
        if(n<=0) break;
        vwrite(buf,n);
        /* redraw */
        int px,py,pw,ph; panel_rect(&px,&py,&pw,&ph);
        fb_render_vscreen(px,py,pw,ph);
    }
    return NULL;
}

/* ── toggle menu ─────────────────────────────────────────── */
static void fb_toggle_menu(void){
    if(fb.fd<0) return;
    if(menu_open){
        pty_close();
        show_wallpaper();
        menu_open=0;
        return;
    }
    menu_open=1;
    pty_open("--menu");
    pthread_t t;pthread_create(&t,NULL,pty_reader,NULL);pthread_detach(t);
}
static void fb_menu_post(void){
    pty_close();
    show_wallpaper();
    menu_open=0;
}

/* ── toggle terminal ─────────────────────────────────────── */
static void fb_toggle_term(void){
    if(fb.fd<0) return;
    if(term_open){
        pty_close();
        show_wallpaper();
        term_open=0;
        return;
    }
    term_open=1;
    pty_open(NULL);
    pthread_t t;pthread_create(&t,NULL,pty_reader,NULL);pthread_detach(t);
}
static void fb_term_post(void){
    pty_close();
    show_wallpaper();
    term_open=0;
}

/* ── keyboard input forwarding ───────────────────────────── */
/* When PTY is active, keystrokes go to the PTY instead of main shell */
static int fb_key_to_pty(unsigned char c){
    if(!pty_active||pty_master<0) return 0;
    write(pty_master,&c,1);
    return 1;
}
static int fb_seq_to_pty(const char *seq, int len){
    if(!pty_active||pty_master<0) return 0;
    write(pty_master,seq,len);
    return 1;
}

/* ── external keyboard thread ────────────────────────────── */
#define MAX_INPUT 32
static int inp_fds[MAX_INPUT], inp_nfds=0;
static volatile int kbd_shift=0;

static void inp_scan(void){
    for(int i=0;i<inp_nfds;i++) close(inp_fds[i]); inp_nfds=0;
    char path[64];
    for(int i=0;i<64&&inp_nfds<MAX_INPUT;i++){
        snprintf(path,sizeof(path),"/dev/input/event%d",i);
        int fd=open(path,O_RDONLY|O_NONBLOCK);if(fd<0)continue;
        inp_fds[inp_nfds++]=fd;
    }
}

static void *input_thread(void *arg){
    (void)arg; inp_scan();
    struct input_event ev;
    while(1){
        fd_set fds;FD_ZERO(&fds);int mx=0;
        for(int i=0;i<inp_nfds;i++){FD_SET(inp_fds[i],&fds);if(inp_fds[i]>mx)mx=inp_fds[i];}
        struct timeval tv={5,0};
        if(select(mx+1,&fds,NULL,NULL,&tv)<=0){inp_scan();continue;}
        for(int i=0;i<inp_nfds;i++){
            if(!FD_ISSET(inp_fds[i],&fds))continue;
            while(read(inp_fds[i],&ev,sizeof(ev))==sizeof(ev)){
                if(ev.type==EV_KEY){
                    if(ev.code==KEY_LEFTSHIFT||ev.code==KEY_RIGHTSHIFT)
                        kbd_shift=(ev.value!=0);
                    if(ev.value==1&&kbd_shift){
                        if(ev.code==KEY_M){write(0,"\x01M",2);continue;}
                        if(ev.code==KEY_T){write(0,"\x01T",2);continue;}
                    }
                }
            }
        }
    }
    return NULL;
}

/* ── wallpaper keeper ────────────────────────────────────── */
static void *wp_keeper(void *arg){
    (void)arg;
    while(1){
        usleep(200000);
        if(!menu_open&&!term_open) fb_draw_wallpaper();
    }
    return NULL;
}

/* ── fb init ─────────────────────────────────────────────── */
static int fb_init(void){
    const char *devs[]={"/dev/fb0","/dev/fb1","/dev/graphics/fb0",NULL};
    for(int i=0;devs[i];i++){fb.fd=open(devs[i],O_RDWR);if(fb.fd>=0)break;}
    if(fb.fd<0) return -1;

    struct fb_var_screeninfo vi;struct fb_fix_screeninfo fi;
    if(ioctl(fb.fd,FBIOGET_VSCREENINFO,&vi)<0||ioctl(fb.fd,FBIOGET_FSCREENINFO,&fi)<0)
        {close(fb.fd);fb.fd=-1;return -1;}

    if(vi.bits_per_pixel!=32){
        vi.bits_per_pixel=32;
        ioctl(fb.fd,FBIOPUT_VSCREENINFO,&vi);
        ioctl(fb.fd,FBIOGET_VSCREENINFO,&vi);
        ioctl(fb.fd,FBIOGET_FSCREENINFO,&fi);
    }

    fb.w=vi.xres;fb.h=vi.yres;fb.bpp=vi.bits_per_pixel;
    fb.stride=fi.line_length;fb.memsize=(size_t)fi.line_length*vi.yres;
    fb.r_off=vi.red.offset;fb.g_off=vi.green.offset;fb.b_off=vi.blue.offset;

    fb.mem=mmap(NULL,fb.memsize,PROT_READ|PROT_WRITE,MAP_SHARED,fb.fd,0);
    if(fb.mem==MAP_FAILED){close(fb.fd);fb.fd=-1;return -1;}

    fb.wp=malloc(sizeof(unsigned int)*(size_t)fb.w*fb.h);
    fb_rescale_wp();
    return 0;
}

static void fb_startup(void){
    if(fb_init()<0) return;
    show_wallpaper();
    pthread_t t;
    pthread_create(&t,NULL,input_thread,NULL);pthread_detach(t);
    pthread_create(&t,NULL,wp_keeper,NULL);pthread_detach(t);
}

static void fb_shutdown(void){
    if(fb.fd<0) return;
    pty_close();
    write(1,"\x1b[?25h",6);
    free(fb.wp);fb.wp=NULL;
    munmap(fb.mem,fb.memsize);
    close(fb.fd);fb.fd=-1;
}
