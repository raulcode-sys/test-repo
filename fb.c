/*
 * fb.c - Triumph OS framebuffer compositor
 *
 * - Boot → wallpaper on /dev/fb0
 * - Shift+M → real menu, ESC back to wallpaper
 * - Shift+T → real terminal, Shift+T back to wallpaper
 * - External keyboards via /dev/input/event*
 * - Wallpaper keeper thread repaints every 200ms when idle
 */

#pragma once

#include <linux/fb.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <pthread.h>
#include "wallpaper.h"

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

static inline void fb_put(int x, int y, unsigned int rgb){
    if((unsigned)x>=(unsigned)fb.w||(unsigned)y>=(unsigned)fb.h) return;
    unsigned int r=(rgb>>16)&0xff,g=(rgb>>8)&0xff,b=rgb&0xff;
    if(fb.bpp==32)
        *(unsigned int*)(fb.mem+y*fb.stride+x*4)=(r<<fb.r_off)|(g<<fb.g_off)|(b<<fb.b_off);
    else if(fb.bpp==16)
        *(unsigned short*)(fb.mem+y*fb.stride+x*2)=((r>>3)<<11)|((g>>2)<<5)|(b>>3);
}

static void fb_draw_wallpaper(void){
    if(fb.fd<0||!fb.wp) return;
    if(fb.bpp==32&&fb.r_off==16&&fb.g_off==8&&fb.b_off==0){
        for(int y=0;y<fb.h;y++)
            memcpy(fb.mem+y*fb.stride,fb.wp+y*fb.w,(size_t)fb.w*4);
    } else {
        for(int y=0;y<fb.h;y++)
            for(int x=0;x<fb.w;x++)
                fb_put(x,y,fb.wp[y*fb.w+x]);
    }
}

static void show_wallpaper(void){
    fb_draw_wallpaper();
    write(1,"\x1b[2J\x1b[H\x1b[?25l",14);
    fflush(stdout);
}

static void fb_rescale_wp(void){
    if(!fb.wp) return;
    for(int y=0;y<fb.h;y++){
        int sy=y*WP_H/fb.h; if(sy>=WP_H)sy=WP_H-1;
        for(int x=0;x<fb.w;x++){
            int sx=x*WP_W/fb.w; if(sx>=WP_W)sx=WP_W-1;
            fb.wp[y*fb.w+x]=wp_data[sy*WP_W+sx];
        }
    }
}

static void fb_toggle_menu(void){
    if(fb.fd<0) return;
    if(menu_open){ show_wallpaper(); menu_open=0; return; }
    menu_open=1;
    write(1,"\x1b[?25h\x1b[2J\x1b[H",13);
    fflush(stdout);
}
static void fb_menu_post(void){
    show_wallpaper();
    menu_open=0;
}

static void fb_toggle_term(void){
    if(fb.fd<0) return;
    if(term_open){ show_wallpaper(); term_open=0; return; }
    term_open=1;
    write(1,"\x1b[?25h\x1b[2J\x1b[H",13);
    fflush(stdout);
}
static void fb_term_post(void){
    show_wallpaper();
    term_open=0;
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
        int fd=open(path,O_RDONLY|O_NONBLOCK); if(fd<0) continue;
        inp_fds[inp_nfds++]=fd;
    }
}

static void *input_thread(void *arg){
    (void)arg; inp_scan();
    struct input_event ev;
    while(1){
        fd_set fds; FD_ZERO(&fds); int mx=0;
        for(int i=0;i<inp_nfds;i++){FD_SET(inp_fds[i],&fds);if(inp_fds[i]>mx)mx=inp_fds[i];}
        struct timeval tv={5,0};
        if(select(mx+1,&fds,NULL,NULL,&tv)<=0){inp_scan();continue;}
        for(int i=0;i<inp_nfds;i++){
            if(!FD_ISSET(inp_fds[i],&fds)) continue;
            while(read(inp_fds[i],&ev,sizeof(ev))==sizeof(ev)){
                if(ev.type==EV_KEY){
                    if(ev.code==KEY_LEFTSHIFT||ev.code==KEY_RIGHTSHIFT)
                        kbd_shift=(ev.value!=0);
                    if(ev.value==1&&kbd_shift){
                        if(ev.code==KEY_M) write(0,"\x01M",2);
                        if(ev.code==KEY_T) write(0,"\x01T",2);
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

    struct fb_var_screeninfo vi; struct fb_fix_screeninfo fi;
    if(ioctl(fb.fd,FBIOGET_VSCREENINFO,&vi)<0||
       ioctl(fb.fd,FBIOGET_FSCREENINFO,&fi)<0)
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
    pthread_create(&t,NULL,input_thread,NULL); pthread_detach(t);
    pthread_create(&t,NULL,wp_keeper,NULL); pthread_detach(t);
}

static void fb_shutdown(void){
    if(fb.fd<0) return;
    write(1,"\x1b[?25h",6);
    free(fb.wp);fb.wp=NULL;
    munmap(fb.mem,fb.memsize);
    close(fb.fd);fb.fd=-1;
}
