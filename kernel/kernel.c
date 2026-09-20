#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile unsigned short *)0xB8000)
#define VGA_ATTR 0x0F

void _start(void) {
    volatile unsigned short *vga = VGA_MEMORY;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = (unsigned short)' ' | ((unsigned short)0x07 << 8);
    }

    const char *msg = "Hello from C kernel! CraftyOS lives.";
    for (int i = 0; msg[i] != '\0'; i++) {
        vga[i] = (unsigned short)msg[i] | ((unsigned short)VGA_ATTR << 8);
    }

    for (;;) {
        __asm__ volatile ("hlt");
    }
}