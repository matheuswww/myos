typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

typedef struct {
    u16 width;
    u16 height;
    u8  bpp;
    u32 pitch;
    u32 physical_buffer;
} __attribute__((packed)) vbe_info_t;

void draw_rect(u32* fb, u32 screen_w, u32 x, u32 y, u32 w, u32 h, u32 color) {
    for (u32 i = y; i < y + h; i++) {
        for (u32 j = x; j < x + w; j++) {
            fb[i * screen_w + j] = color;
        }
    }
}

void kernel_main(void) {
vbe_info_t* vbe = (vbe_info_t*)0x00100000;

u32 w = vbe->width;
u32 h = vbe->height;
u32* fb = (u32*)0x00100000;

for (u32 i = 0; i < (w * h); i++) {
    fb[i] = 0x00000000;
}

u32 rect_size = 100;
u32 start_x = (w / 2) - (rect_size / 2);
u32 start_y = (h / 2) - (rect_size / 2);

draw_rect(fb, w, start_x, start_y, rect_size, rect_size, 0x0000FF);

while(1);
}