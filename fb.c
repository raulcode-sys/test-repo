/*
 * fb.c - Triumph OS framebuffer compositor
 *
 * - Boot → wallpaper on /dev/fb0
 * - Shift+M → real menu, ESC back to wallpaper
 * - Shift+T → real terminal, Shift+T back to wallpaper
 * - Mouse cursor rendered on framebuffer via /dev/input/event*
 * - Works in QEMU (-usb -device usb-mouse) and real hardware
 * - External keyboard support via /dev/input/event*
 */

#pragma once

#include <linux/fb.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <pthread.h>
#include "wallpaper.h"

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

/* ── mouse state ─────────────────────────────────────────── */
static int mouse_x = 0;
static int mouse_y = 0;
static int mouse_visible = 1;
static pthread_mutex_t fb_lock = PTHREAD_MUTEX_INITIALIZER;

/* mouse click state */
static volatile int mouse_clicked = 0;
static volatile int mouse_click_x = 0;
static volatile int mouse_click_y = 0;

/* saved pixels behind cursor so we can restore them */
static unsigned int cur_save[CUR_W * CUR_H];
static int cur_save_x = -1, cur_save_y = -1;

/* ── pixel ops ───────────────────────────────────────────── */
static inline void fb_put(int x, int y, unsigned int rgb){
    if((unsigned)x>=(unsigned)fb.w||(unsigned)y>=(unsigned)fb.h) return;
    unsigned int r=(rgb>>16)&0xff,g=(rgb>>8)&0xff,b=rgb&0xff;
    if(fb.bpp==32)
        *(unsigned int*)(fb.mem+y*fb.stride+x*4)=(r<<fb.r_off)|(g<<fb.g_off)|(b<<fb.b_off);
    else if(fb.bpp==16)
        *(unsigned short*)(fb.mem+y*fb.stride+x*2)=((r>>3)<<11)|((g>>2)<<5)|(b>>3);
}

static inline unsigned int fb_read(int x, int y){
    if((unsigned)x>=(unsigned)fb.w||(unsigned)y>=(unsigned)fb.h) return 0;
    if(fb.bpp==32){
        unsigned int pix=*(unsigned int*)(fb.mem+y*fb.stride+x*4);
        unsigned int r=(pix>>fb.r_off)&0xff;
        unsigned int g=(pix>>fb.g_off)&0xff;
        unsigned int b=(pix>>fb.b_off)&0xff;
        return (r<<16)|(g<<8)|b;
    }
    return 0;
}

static inline unsigned int wp_get(int x, int y){
    if(!fb.wp||(unsigned)x>=(unsigned)fb.w||(unsigned)y>=(unsigned)fb.h) return 0;
    return fb.wp[y*fb.w+x];
}

/* ── cursor drawing ──────────────────────────────────────── */
static void cur_hide(void){
    if(cur_save_x<0) return;
    for(int cy=0;cy<CUR_H;cy++)
        for(int cx=0;cx<CUR_W;cx++){
            int sx=cur_save_x+cx, sy=cur_save_y+cy;
            fb_put(sx,sy,cur_save[cy*CUR_W+cx]);
        }
    cur_save_x=-1;
}

static void cur_show(int mx, int my){
    /* save pixels under cursor */
    cur_save_x=mx; cur_save_y=my;
    for(int cy=0;cy<CUR_H;cy++)
        for(int cx=0;cx<CUR_W;cx++)
            cur_save[cy*CUR_W+cx]=fb_read(mx+cx,my+cy);

    /* draw cursor */
    for(int cy=0;cy<CUR_H;cy++)
        for(int cx=0;cx<CUR_W;cx++){
            unsigned int pix=cur_data[cy*CUR_W+cx];
            unsigned int a=(pix>>24)&0xff;
            if(a==0) continue;
            unsigned int cr=(pix>>16)&0xff,cg=(pix>>8)&0xff,cb=pix&0xff;
            unsigned int rgb=(cr<<16)|(cg<<8)|cb;
            if(a>=240) fb_put(mx+cx,my+cy,rgb);
            else {
                /* alpha blend */
                unsigned int bg=cur_save[cy*CUR_W+cx];
                unsigned int br=(bg>>16)&0xff,bgg=(bg>>8)&0xff,bb=bg&0xff;
                unsigned int fr=((cr*a+br*(255-a))/255);
                unsigned int fg=((cg*a+bgg*(255-a))/255);
                unsigned int fbb=((cb*a+bb*(255-a))/255);
                fb_put(mx+cx,my+cy,(fr<<16)|(fg<<8)|fbb);
            }
        }
}

static void cur_move(int nx, int ny){
    if(nx<0)nx=0; if(ny<0)ny=0;
    if(nx>fb.w-CUR_W)nx=fb.w-CUR_W;
    if(ny>fb.h-CUR_H)ny=fb.h-CUR_H;
    pthread_mutex_lock(&fb_lock);
    cur_hide();
    mouse_x=nx; mouse_y=ny;
    if(mouse_visible) cur_show(nx,ny);
    pthread_mutex_unlock(&fb_lock);
}

/* ── draw wallpaper ──────────────────────────────────────── */
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

static void show_wallpaper(void){
    pthread_mutex_lock(&fb_lock);
    fb_draw_wallpaper();
    cur_save_x=-1;
    if(mouse_visible) cur_show(mouse_x,mouse_y);
    pthread_mutex_unlock(&fb_lock);
    write(1,"\x1b[2J\x1b[H\x1b[?25l",14);
    fflush(stdout);
}

/* ── toggle handlers ─────────────────────────────────────── */
static void fb_toggle_menu(void){
    if(fb.fd<0) return;
    if(menu_open){ show_wallpaper(); menu_open=0; return; }
    menu_open=1;
    mouse_visible=0;
    pthread_mutex_lock(&fb_lock);
    cur_hide();
    pthread_mutex_unlock(&fb_lock);
    write(1,"\x1b[?25h\x1b[2J\x1b[H",13);
    fflush(stdout);
}
static void fb_menu_post(void){
    mouse_visible=1;
    show_wallpaper();
    menu_open=0;
}

static void fb_toggle_term(void){
    if(fb.fd<0) return;
    if(term_open){ mouse_visible=1; show_wallpaper(); term_open=0; return; }
    term_open=1;
    mouse_visible=0;
    pthread_mutex_lock(&fb_lock);
    cur_hide();
    pthread_mutex_unlock(&fb_lock);
    write(1,"\x1b[?25h\x1b[2J\x1b[H",13);
    fflush(stdout);
}
static void fb_term_post(void){
    mouse_visible=1;
    show_wallpaper();
    term_open=0;
}

/* ── input thread (keyboard + mouse) ────────────────────── */
#define MAX_INPUT 32
static int inp_fds[MAX_INPUT];
static int inp_nfds=0;
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
                /* keyboard */
                if(ev.type==EV_KEY){
                    if(ev.code==KEY_LEFTSHIFT||ev.code==KEY_RIGHTSHIFT)
                        kbd_shift=(ev.value!=0);
                    if(ev.value==1&&kbd_shift){
                        if(ev.code==KEY_M) write(0,"\x01M",2);
                        if(ev.code==KEY_T) write(0,"\x01T",2);
                    }
                }
                /* mouse relative movement */
                if(ev.type==EV_REL){
                    int nx=mouse_x, ny=mouse_y;
                    if(ev.code==REL_X) nx+=ev.value;
                    if(ev.code==REL_Y) ny+=ev.value;
                    cur_move(nx,ny);
                }
                /* mouse absolute (touchpad) */
                if(ev.type==EV_ABS){
                    if(ev.code==ABS_X){
                        int nx=ev.value*fb.w/32768;
                        cur_move(nx,mouse_y);
                    }
                    if(ev.code==ABS_Y){
                        int ny=ev.value*fb.h/32768;
                        cur_move(mouse_x,ny);
                    }
                }
                /* mouse buttons */
                if(ev.type==EV_KEY&&ev.code==BTN_LEFT&&ev.value==1){
                    mouse_clicked=1;
                    mouse_click_x=mouse_x;
                    mouse_click_y=mouse_y;
                }
                /* mouse scroll — send arrow keys to stdin for TTY scrolling */
                if(ev.type==EV_REL&&ev.code==REL_WHEEL){
                    if(ev.value>0) write(0,"\x1b[A",3); /* scroll up */
                    if(ev.value<0) write(0,"\x1b[B",3); /* scroll down */
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
        if(!menu_open&&!term_open){
            pthread_mutex_lock(&fb_lock);
            fb_draw_wallpaper();
            cur_save_x=-1;
            if(mouse_visible) cur_show(mouse_x,mouse_y);
            pthread_mutex_unlock(&fb_lock);
        }
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
    if(fb.wp)
        for(int y=0;y<fb.h;y++){
            int sy=y*WP_H/fb.h;if(sy>=WP_H)sy=WP_H-1;
            for(int x=0;x<fb.w;x++){
                int sx=x*WP_W/fb.w;if(sx>=WP_W)sx=WP_W-1;
                fb.wp[y*fb.w+x]=wp_data[sy*WP_W+sx];
            }
        }

    /* start mouse in centre */
    mouse_x=fb.w/2; mouse_y=fb.h/2;

    return 0;
}

/* ── startup / shutdown ──────────────────────────────────── */
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

/* ── cursor control API (for web browser etc) ────────────── */
static void fb_cursor_on(void){
    mouse_visible=1;
    pthread_mutex_lock(&fb_lock);
    if(cur_save_x<0) cur_show(mouse_x,mouse_y);
    pthread_mutex_unlock(&fb_lock);
}
static void fb_cursor_off(void){
    mouse_visible=0;
    pthread_mutex_lock(&fb_lock);
    cur_hide();
    pthread_mutex_unlock(&fb_lock);
}
static int fb_get_mouse_x(void){ return mouse_x; }
static int fb_get_mouse_y(void){ return mouse_y; }
static int fb_get_width(void){ return fb.w; }
static int fb_get_height(void){ return fb.h; }

static int fb_get_click(int *cx, int *cy){
    if(!mouse_clicked) return 0;
    *cx=mouse_click_x; *cy=mouse_click_y;
    mouse_clicked=0;
    return 1;
}
