#include "vga.h"
#include "keyboard.h"
#include "serial.h"
#include "framebuffer.h"
#include "gui.h"
#include "mouse.h"
#include "ports.h"
#include "paint.h"
#include "snake.h"
#include "browser.h"
#include "settings.h"
#include "rtc.h"
#include "fs.h"
#include "files.h"
#include "winver.h"
#include "net.h"
#include "pylang.h"
#include "tinycc.h"
#include "apt.h"
#include "calcapp.h"
#include "notepad.h"
#include "settings_store.h"
#include "de_pixel.h"

int gui_mode = 0;
static unsigned long ticks;

static int shell_echo(const char *arg) {
    vga_writestring(arg ? arg : "");
    return 1;
}

static int shell_help(void) {
    vga_writestring("Available commands:\n");
    vga_writestring("  help     - Show this help\n");
    vga_writestring("  echo     - Echo text\n");
    vga_writestring("  clear    - Clear screen\n");
    vga_writestring("  uname    - Print system info\n");
    vga_writestring("  whoami   - Print current user\n");
    vga_writestring("  date     - Show current date/time\n");
    vga_writestring("  uptime   - Show system uptime\n");
    vga_writestring("  shutdown - Shut down the system\n");
    vga_writestring("  reboot   - Reboot the system\n");
    vga_writestring("  neofetch - Display system info\n");
    vga_writestring("  ps       - List running windows\n");
    vga_writestring("  meminfo  - Show memory info\n");
    vga_writestring("  about    - About minios\n");
    vga_writestring("  env      - Show environment\n");
    vga_writestring("  window   - Open a new terminal window\n");
    vga_writestring("  calc     - RPN calculator\n");
    vga_writestring("  paint    - Drawing program (GUI)\n");
    vga_writestring("  snake    - Snake game (GUI)\n");
    vga_writestring("  browser  - Web browser (GUI)\n");
    vga_writestring("  settings - System settings (GUI)\n");
    vga_writestring("  files    - File manager (GUI)\n");
    vga_writestring("  edit     - Edit a file\n");
    vga_writestring("  rm       - Delete a file\n");
    vga_writestring("  mv       - Rename a file\n");
    vga_writestring("  cat      - View a file\n");
    vga_writestring("  head     - Show first N lines of file\n");
    vga_writestring("  hexdump  - Hex dump of a file\n");
    vga_writestring("  ls       - List files\n");
    vga_writestring("  create   - Create a file (--shell for .sh)\n");
    vga_writestring("  winver   - About minios\n");
    vga_writestring("  yes      - Print y repeatedly\n");
    vga_writestring("  wc       - Word/line/char count of file\n");
    vga_writestring("  cal      - Show calendar\n");
    vga_writestring("  seq      - Print sequence of numbers\n");
    vga_writestring("  expr     - Evaluate expression\n");
    vga_writestring("  which    - Locate a command\n");
    vga_writestring("  true     - Exit with success\n");
    vga_writestring("  false    - Exit with failure\n");
    vga_writestring("  sleep    - Delay for N seconds\n");
    vga_writestring("  touch    - Create empty file\n");
    vga_writestring("  basename - Strip directory from path\n");
    vga_writestring("  dirname  - Strip filename from path\n");
    vga_writestring("  rev      - Reverse a string\n");
    vga_writestring("  uniq     - Report/filter repeated lines\n");
    vga_writestring("  sort     - Sort lines of text\n");
    vga_writestring("  hostname - Show hostname\n");
    vga_writestring("  python   - Run Python script (.py)\n");
    vga_writestring("  tcc      - Compile C source (.c)\n");
    vga_writestring("  notepad  - GUI text editor\n");
    vga_writestring("  apt      - Package manager\n");
    return 1;
}

static int shell_uname(void) {
    vga_writestring("minios v0.1 i686\n");
    return 1;
}

static int shell_whoami(void) {
    vga_writestring("root\n");
    return 1;
}

static int shell_uptime(void) {
    unsigned long up = ticks / 100;
    unsigned long days = up / 86400;
    up %= 86400;
    unsigned long hours = up / 3600;
    up %= 3600;
    unsigned long mins = up / 60;
    unsigned long secs = up % 60;

    char buf[32];
    int n = 0;
    if (days) {
        buf[n++] = '0' + days / 10;
        buf[n++] = '0' + days % 10;
        buf[n++] = 'd';
    }
    buf[n++] = '0' + hours / 10;
    buf[n++] = '0' + hours % 10;
    buf[n++] = ':';
    buf[n++] = '0' + mins / 10;
    buf[n++] = '0' + mins % 10;
    buf[n++] = ':';
    buf[n++] = '0' + secs / 10;
    buf[n++] = '0' + secs % 10;
    buf[n] = 0;

    vga_writestring("Uptime: ");
    vga_writestring(buf);
    vga_writestring("\n");
    return 1;
}

static int shell_meminfo(void) {
    vga_writestring("Memory: ~128 MB RAM\n");
    return 1;
}

static int shell_about(void) {
    vga_writestring("minios v0.1 - A minimal UNIX-like OS\n");
    vga_writestring("Built for x86, uses Multiboot, freestanding C\n");
    return 1;
}

static int shell_date(void) {
    rtc_time_t t;
    rtc_read_time(&t);
    char buf[32];
    int n = 0;
    buf[n++] = '0' + t.hours / 10;
    buf[n++] = '0' + t.hours % 10;
    buf[n++] = ':';
    buf[n++] = '0' + t.minutes / 10;
    buf[n++] = '0' + t.minutes % 10;
    buf[n++] = ':';
    buf[n++] = '0' + t.seconds / 10;
    buf[n++] = '0' + t.seconds % 10;
    buf[n] = 0;
    vga_writestring(buf);
    vga_putchar('\n');
    return 1;
}

static int shell_shutdown(void) {
    vga_writestring("Shutting down...\n");
    outw(0x2000, 0x604); // QEMU ACPI poweroff
    for (;;);
}

static int shell_neofetch(void) {
    vga_writestring("       ______  \n");
    vga_writestring("   ___/  |  |  \n");
    vga_writestring("   \\_/   | |   \n");
    vga_writestring("    /    |_|    \n");
    vga_writestring("   /______|     \n");
    vga_writestring("                \n");
    vga_writestring("  OS: minios v0.1\n");
    vga_writestring("  Arch: i686\n");
    vga_writestring("  Display: ");
    if (gui_mode) {
        vga_writestring("800x600 32bpp\n");
    } else {
        vga_writestring("text mode 80x25\n");
    }
    vga_writestring("  Shell: built-in\n");
    rtc_time_t t;
    rtc_read_time(&t);
    vga_writestring("  Time: ");
    vga_putchar('0' + t.hours / 10); vga_putchar('0' + t.hours % 10);
    vga_putchar(':');
    vga_putchar('0' + t.minutes / 10); vga_putchar('0' + t.minutes % 10);
    vga_putchar(':');
    vga_putchar('0' + t.seconds / 10); vga_putchar('0' + t.seconds % 10);
    vga_putchar('\n');
    return 1;
}

static int shell_env(void) {
    vga_writestring("USER=root\n");
    vga_writestring("SHELL=minios/sh\n");
    vga_writestring("OS=minios\n");
    vga_writestring("ARCH=i686\n");
    return 1;
}

static int shell_yes(const char *arg) {
    for (int i = 0; i < 100; i++) {
        vga_writestring(arg && *arg ? arg : "y");
        vga_putchar('\n');
    }
    return 1;
}

static int shell_ps(void) {
    if (!gui_mode) { vga_writestring("GUI mode only\n"); return 1; }
    vga_writestring("Active windows:\n");
    for (int i = 0; i < 8; i++) {
        int id = i;
        if (id < 0) continue;
        // gui_get_window_info would be needed, but we can just print from here
        vga_writestring("  Window ");
        vga_putchar('0' + i);
        vga_putchar('\n');
    }
    vga_writestring("(use 'window' to open new terminals)\n");
    return 1;
}

static int shell_hexdump(const char *arg) {
    if (!arg || !*arg) { vga_writestring("Usage: hexdump <filename>\n"); return 1; }
    uint8_t buf[4096];
    int sz = fs_read(arg, buf, 4096);
    if (sz < 0) { vga_writestring("File not found\n"); return 1; }
    char line[80];
    for (int off = 0; off < sz; off += 16) {
        int n = 0;
        line[n++] = '0' + (off >> 12) % 16; line[n++] = '0' + (off >> 8) % 16;
        line[n++] = '0' + (off >> 4) % 16; line[n++] = '0' + (off & 0xF);
        line[n++] = ':'; line[n++] = ' ';
        int remain = sz - off;
        if (remain > 16) remain = 16;
        for (int i = 0; i < 16; i++) {
            if (i < remain) {
                uint8_t b = buf[off + i];
                line[n++] = "0123456789ABCDEF"[b >> 4];
                line[n++] = "0123456789ABCDEF"[b & 0xF];
            } else {
                line[n++] = ' '; line[n++] = ' ';
            }
            line[n++] = ' ';
        }
        line[n++] = '|';
        for (int i = 0; i < remain; i++) {
            char c = buf[off + i];
            line[n++] = (c >= 32 && c < 127) ? c : '.';
        }
        line[n++] = '|';
        line[n] = 0;
        vga_writestring(line);
        vga_putchar('\n');
    }
    return 1;
}

static int shell_head(const char *arg) {
    if (!arg || !*arg) { vga_writestring("Usage: head <filename> [n]\n"); return 1; }
    char fname[32]; int fi = 0;
    while (*arg && *arg != ' ' && fi < 31) fname[fi++] = *arg++;
    fname[fi] = 0;
    while (*arg == ' ') arg++;
    int n = 10;
    if (*arg) { n = 0; while (*arg >= '0' && *arg <= '9') { n = n * 10 + (*arg - '0'); arg++; } }
    if (n < 1) n = 1;
    if (n > 1000) n = 1000;
    uint8_t buf[4096];
    int sz = fs_read(fname, buf, 4096);
    if (sz < 0) { vga_writestring("File not found\n"); return 1; }
    int lines = 0;
    for (int i = 0; i < sz && lines < n; i++) {
        vga_putchar(buf[i]);
        if (buf[i] == '\n') lines++;
    }
    if (sz > 0 && buf[sz - 1] != '\n') vga_putchar('\n');
    return 1;
}

static int shell_calc(void) {
    if (gui_mode) { calcapp_run(); return 1; }
    vga_writestring("RPN Calculator (q=quit)\n");
    char buf[64];
    int pos = 0;
    int stack[16];
    int sp = 0;

    for (;;) {
        vga_writestring("> ");
        pos = 0;

        for (;;) {
            keyboard_poll();
            char c = keyboard_getchar();
            if (!c) continue;

            if (c == 'q') { vga_putchar('\n'); return 1; }
            if (c == '\n') { vga_putchar('\n'); break; }
            if (c >= ' ') { vga_putchar(c); if (pos < 63) buf[pos++] = c; }
        }
        buf[pos] = 0;

        if (buf[0] == 'q') return 1;
        if (buf[0] == 'c' && buf[1] == 0) { sp = 0; vga_writestring("stack cleared\n"); continue; }
        if (buf[0] == 's' && buf[1] == 0) {
            for (int i = 0; i < sp; i++) {
                vga_putchar('0' + (char)(stack[i] / 10000 % 10));
                vga_putchar('0' + (char)(stack[i] / 1000 % 10));
                vga_putchar('0' + (char)(stack[i] / 100 % 10));
                vga_putchar('0' + (char)(stack[i] / 10 % 10));
                vga_putchar('0' + (char)(stack[i] % 10));
                vga_putchar(' ');
            }
            vga_putchar('\n');
            continue;
        }

        int val = 0;
        int neg = 0;
        int i = 0;
        if (buf[i] == '-') { neg = 1; i++; }
        while (buf[i] >= '0' && buf[i] <= '9') {
            val = val * 10 + (buf[i] - '0');
            i++;
        }
        if (neg) val = -val;

        if (i > 0 && buf[i] == 0) {
            if (sp < 16) stack[sp++] = val;
            continue;
        }

        if (buf[0] == '+' && buf[1] == 0 && sp >= 2) {
            stack[sp - 2] = stack[sp - 1] + stack[sp - 2]; sp--;
            continue;
        }
        if (buf[0] == '-' && buf[1] == 0 && sp >= 2) {
            stack[sp - 2] = stack[sp - 1] - stack[sp - 2]; sp--;
            continue;
        }
        if (buf[0] == '*' && buf[1] == 0 && sp >= 2) {
            stack[sp - 2] = stack[sp - 1] * stack[sp - 2]; sp--;
            continue;
        }
        if (buf[0] == '/' && buf[1] == 0 && sp >= 2 && stack[sp - 2] != 0) {
            stack[sp - 2] = stack[sp - 1] / stack[sp - 2]; sp--;
            continue;
        }

        vga_writestring("?\n");
    }
}

static int shell_paint(void) {
    if (!gui_mode) { vga_writestring("GUI mode only\n"); return 1; }
    paint_run();
    return 1;
}

static int shell_snake(void) {
    if (!gui_mode) { vga_writestring("GUI mode only\n"); return 1; }
    snake_run();
    return 1;
}

static int shell_browser(void) {
    if (!gui_mode) { vga_writestring("GUI mode only\n"); return 1; }
    browser_run();
    return 1;
}

static int shell_settings(void) {
    if (!gui_mode) { vga_writestring("GUI mode only\n"); return 1; }
    settings_run();
    return 1;
}

static int shell_files(void) {
    if (!gui_mode) { vga_writestring("GUI mode only\n"); return 1; }
    files_run();
    return 1;
}

static int shell_notepad(void) {
    if (!gui_mode) { vga_writestring("GUI mode only\n"); return 1; }
    notepad_run();
    return 1;
}

static int shell_winver(void) {
    if (!gui_mode) { vga_writestring("GUI mode only\n"); return 1; }
    winver_run();
    return 1;
}

static int shell_ls(void) {
    char names[FS_MAX_FILES][FS_NAME_LEN];
    int n = fs_list(names, FS_MAX_FILES);
    for (int i = 0; i < n; i++) {
        vga_writestring(names[i]);
        vga_writestring("\n");
    }
    return 1;
}

static int shell_cat(const char *arg) {
    if (!arg || !*arg) { vga_writestring("Usage: cat <filename>\n"); return 1; }
    uint32_t sz = fs_size(arg);
    if (sz == 0) { vga_writestring("File empty or not found\n"); return 1; }
    char buf[4096];
    int r = fs_read(arg, (uint8_t *)buf, 4095);
    if (r < 0) { vga_writestring("File not found\n"); return 1; }
    buf[r] = 0;
    vga_writestring(buf);
    if (r > 0 && buf[r - 1] != '\n') vga_putchar('\n');
    return 1;
}

static int shell_rm(const char *arg) {
    if (!arg || !*arg) { vga_writestring("Usage: rm <filename>\n"); return 1; }
    if (fs_delete(arg)) vga_writestring("Deleted\n");
    else vga_writestring("Not found\n");
    return 1;
}

static int shell_mv(const char *arg) {
    if (!arg || !*arg) { vga_writestring("Usage: mv <old> <new>\n"); return 1; }
    char oldname[FS_NAME_LEN], newname[FS_NAME_LEN];
    int i = 0;
    while (*arg && *arg != ' ' && i < FS_NAME_LEN - 1) oldname[i++] = *arg++;
    oldname[i] = 0;
    while (*arg == ' ') arg++;
    i = 0;
    while (*arg && *arg != ' ' && i < FS_NAME_LEN - 1) newname[i++] = *arg++;
    newname[i] = 0;
    if (oldname[0] == 0 || newname[0] == 0) { vga_writestring("Usage: mv <old> <new>\n"); return 1; }
    if (fs_rename(oldname, newname)) vga_writestring("Renamed\n");
    else vga_writestring("Failed\n");
    return 1;
}

static int shell_edit(const char *arg) {
    if (!arg || !*arg) { vga_writestring("Usage: edit <filename>\n"); return 1; }
    if (!gui_mode) { vga_writestring("GUI mode only\n"); return 1; }
    if (!fs_exists(arg)) fs_create(arg);
    notepad_open(arg);
    return 1;
}

static int shell_create(const char *arg) {
    if (!arg || !*arg) { vga_writestring("Usage: create <filename> [--shell]\n"); return 1; }
    char name[FS_NAME_LEN];
    int shell_script = 0;
    int i = 0;
    while (*arg && *arg != ' ' && i < FS_NAME_LEN - 1) name[i++] = *arg++;
    name[i] = 0;
    while (*arg == ' ') arg++;
    if (arg[0] == '-' && arg[1] == '-' && arg[2] == 's' && arg[3] == 'h' && arg[4] == 'e' && arg[5] == 'l' && arg[6] == 'l' && arg[7] == 0)
        shell_script = 1;
    if (!name[0]) { vga_writestring("Usage: create <filename> [--shell]\n"); return 1; }
    if (fs_exists(name)) { vga_writestring("File already exists\n"); return 1; }
    if (shell_script) {
        const char *content = "#!/bin/sh\n";
        int len = 0;
        while (content[len]) len++;
        if (fs_write(name, (const uint8_t *)content, len))
            vga_writestring("Created shell script\n");
        else
            vga_writestring("Failed\n");
    } else {
        if (fs_create(name)) vga_writestring("Created\n");
        else vga_writestring("Failed\n");
    }
    return 1;
}

static int shell_python(const char *arg) {
    if (!arg || !*arg) { vga_writestring("Usage: python <file.py>\n"); return 1; }
    uint32_t sz = fs_size(arg);
    if (sz == 0) { vga_writestring("File not found\n"); return 1; }
    char buf[4096];
    int r = fs_read(arg, (uint8_t *)buf, 4095);
    if (r < 0) { vga_writestring("File not found\n"); return 1; }
    buf[r] = 0;
    py_run(buf);
    return 1;
}

static int shell_tcc(const char *arg) {
    if (!arg || !*arg) { vga_writestring("Usage: tcc <source.c>\n"); return 1; }
    char fname[64]; int fi = 0;
    while (*arg && *arg != ' ' && fi < 63) fname[fi++] = *arg++;
    fname[fi] = 0;
    uint32_t sz = fs_size(fname);
    if (sz == 0) { vga_writestring("File not found\n"); return 1; }
    char buf[4096];
    int r = fs_read(fname, (uint8_t *)buf, 4095);
    if (r < 0) { vga_writestring("Read failed\n"); return 1; }
    buf[r] = 0;
    // Generate output filename: strip .c, add .bin
    char outname[64]; int oi = 0;
    int flen = 0; while (fname[flen]) flen++;
    int dot = flen - 1;
    while (dot >= 0 && fname[dot] != '.') dot--;
    if (dot > 0) {
        for (int i = 0; i < dot && i < 63; i++) outname[oi++] = fname[i];
        outname[oi] = 0;
    } else {
        int i; for (i = 0; fname[i] && i < 63; i++) outname[i] = fname[i];
        outname[i] = 0;
    }
    oi = 0; while (outname[oi]) oi++;
    outname[oi++] = '.'; outname[oi++] = 'b'; outname[oi++] = 'i'; outname[oi++] = 'n'; outname[oi] = 0;
    vga_writestring("Compiling "); vga_writestring(fname); vga_writestring("...\n");
    int size = tcc_build(buf, outname);
    if (size > 0) {
        vga_writestring("Output: "); vga_writestring(outname);
        vga_writestring(" ("); vga_putchar('0' + (char)(size / 1000 % 10));
        vga_putchar('0' + (char)(size / 100 % 10));
        vga_putchar('0' + (char)(size / 10 % 10));
        vga_putchar('0' + (char)(size % 10));
        vga_writestring(" bytes)\n");
    } else {
        vga_writestring("Compilation failed\n");
    }
    return 1;
}

static int shell_apt(const char *arg) {
    while (*arg == ' ') arg++;
    if (!*arg) { vga_writestring("Usage: apt <update|list|install|remove|download>\n"); return 1; }
    char cmd[32]; int ci = 0;
    while (*arg && *arg != ' ' && ci < 31) cmd[ci++] = *arg++;
    cmd[ci] = 0;
    while (*arg == ' ') arg++;
    if (cmd[0]=='u' && cmd[1]=='p' && cmd[2]=='d' && cmd[3]=='a' && cmd[4]=='t' && cmd[5]=='e' && cmd[6]==0) {
        apt_update();
    } else if (cmd[0]=='l' && cmd[1]=='i' && cmd[2]=='s' && cmd[3]=='t' && cmd[4]==0) {
        apt_list();
    } else if (cmd[0]=='i' && cmd[1]=='n' && cmd[2]=='s' && cmd[3]=='t' && cmd[4]=='a' && cmd[5]=='l' && cmd[6]=='l' && cmd[7]==0) {
        apt_install(arg);
    } else if (cmd[0]=='r' && cmd[1]=='e' && cmd[2]=='m' && cmd[3]=='o' && cmd[4]=='v' && cmd[5]=='e' && cmd[6]==0) {
        apt_remove(arg);
    } else if (cmd[0]=='d' && cmd[1]=='o' && cmd[2]=='w' && cmd[3]=='n' && cmd[4]=='l' && cmd[5]=='o' && cmd[6]=='a' && cmd[7]=='d' && cmd[8]==0) {
        while (*arg == ' ') arg++;
        char url[256]; int ui = 0;
        while (*arg && *arg != ' ' && ui < 255) url[ui++] = *arg++;
        url[ui] = 0;
        while (*arg == ' ') arg++;
        apt_download(url, arg);
    } else {
        vga_writestring("apt: unknown command\n");
    }
    return 1;
}

static int shell_wc(const char *arg) {
    if (!arg || !*arg) { vga_writestring("Usage: wc <filename>\n"); return 1; }
    uint8_t buf[4096];
    int sz = fs_read(arg, buf, 4096);
    if (sz < 0) { vga_writestring("File not found\n"); return 1; }
    int lines = 0, words = 0, chars = sz;
    int in_word = 0;
    for (int i = 0; i < sz; i++) {
        char c = buf[i];
        if (c == '\n') lines++;
        if (c == ' ' || c == '\n' || c == '\t') { in_word = 0; }
        else if (!in_word) { words++; in_word = 1; }
    }
    // lines, words, chars
    vga_putchar('0' + (lines / 100) % 10);
    vga_putchar('0' + (lines / 10) % 10);
    vga_putchar('0' + lines % 10);
    vga_putchar(' ');
    vga_putchar('0' + (words / 100) % 10);
    vga_putchar('0' + (words / 10) % 10);
    vga_putchar('0' + words % 10);
    vga_putchar(' ');
    vga_putchar('0' + (chars / 100) % 10);
    vga_putchar('0' + (chars / 10) % 10);
    vga_putchar('0' + chars % 10);
    vga_putchar(' ');
    vga_writestring(arg);
    vga_putchar('\n');
    return 1;
}

static int shell_cal(void) {
    vga_writestring("     May 2026\n");
    vga_writestring("Mo Tu We Th Fr Sa Su\n");
    vga_writestring("             1  2  3\n");
    vga_writestring(" 4  5  6  7  8  9 10\n");
    vga_writestring("11 12 13 14 15 16 17\n");
    vga_writestring("18 19 20 21 22 23 24\n");
    vga_writestring("25 26 27 28 29 30 31\n");
    return 1;
}

static int shell_seq(const char *arg) {
    int n = 10;
    if (arg && *arg) { n = 0; while (*arg >= '0' && *arg <= '9') { n = n * 10 + (*arg - '0'); arg++; } }
    if (n > 1000) n = 1000;
    if (n < 1) n = 1;
    for (int i = 1; i <= n; i++) {
        if (i >= 100) vga_putchar('0' + i / 100);
        if (i >= 10) vga_putchar('0' + (i / 10) % 10);
        vga_putchar('0' + i % 10);
        vga_putchar('\n');
    }
    return 1;
}

static int shell_expr(const char *arg) {
    if (!arg || !*arg) { vga_writestring("Usage: expr <a> <op> <b>\n"); return 1; }
    int a = 0, b = 0;
    while (*arg == ' ') arg++;
    while (*arg >= '0' && *arg <= '9') { a = a * 10 + (*arg - '0'); arg++; }
    while (*arg == ' ') arg++;
    char op = *arg;
    if (op) arg++;
    while (*arg == ' ') arg++;
    while (*arg >= '0' && *arg <= '9') { b = b * 10 + (*arg - '0'); arg++; }
    int r = 0;
    if (op == '+') r = a + b;
    else if (op == '-') r = a - b;
    else if (op == '*') r = a * b;
    else if (op == '/') { if (b != 0) r = a / b; else { vga_writestring("Division by zero\n"); return 1; } }
    else if (op == '%') { if (b != 0) r = a % b; else { vga_writestring("Division by zero\n"); return 1; } }
    else { vga_writestring("Unknown operator\n"); return 1; }
    if (r < 0) { vga_putchar('-'); r = -r; }
    if (r >= 10000) vga_putchar('0' + r / 10000 % 10);
    if (r >= 1000) vga_putchar('0' + r / 1000 % 10);
    if (r >= 100) vga_putchar('0' + r / 100 % 10);
    if (r >= 10) vga_putchar('0' + r / 10 % 10);
    vga_putchar('0' + r % 10);
    vga_putchar('\n');
    return 1;
}

static int shell_which(const char *arg) {
    if (!arg || !*arg) { vga_writestring("Usage: which <command>\n"); return 1; }
    // Check built-in commands
    const char *builtins[] = {
        "help","echo","clear","uname","whoami","date","uptime","shutdown","reboot",
        "neofetch","ps","meminfo","about","env","window","calc","paint","snake",
        "browser","settings","files","edit","rm","mv","cat","head","hexdump","ls",
        "create","winver","yes","wc","cal","seq","expr","which","true","false",
        "sleep","touch","basename","dirname","rev","uniq","sort","hostname",
        "python","tcc","apt",0
    };
    for (int i = 0; builtins[i]; i++) {
        const char *b = builtins[i];
        const char *a = arg;
        int match = 1;
        while (*b && *a) { if (*b != *a) { match = 0; break; } b++; a++; }
        if (match && *b == 0 && (*a == 0 || *a == ' ')) {
            vga_writestring("built-in: ");
            vga_writestring(builtins[i]);
            vga_putchar('\n');
            return 1;
        }
    }
    // Check filesystem
    if (fs_exists(arg) || fs_exists("")) {
        vga_writestring(arg);
        vga_writestring(" (file)\n");
    } else {
        vga_writestring("not found\n");
    }
    return 1;
}

static int shell_true(void) { return 1; }
static int shell_false(void) { return 0; }

static int shell_sleep(const char *arg) {
    int n = 1;
    if (arg && *arg) { n = 0; while (*arg >= '0' && *arg <= '9') { n = n * 10 + (*arg - '0'); arg++; } }
    if (n > 60) n = 60;
    if (n < 1) n = 1;
    for (volatile long i = 0; i < 5000000L * n; i++);
    return 1;
}

static int shell_touch(const char *arg) {
    if (!arg || !*arg) { vga_writestring("Usage: touch <filename>\n"); return 1; }
    if (fs_exists(arg)) { vga_writestring("exists\n"); return 1; }
    if (fs_create(arg)) vga_writestring("created\n");
    else vga_writestring("failed\n");
    return 1;
}

static int shell_basename(const char *arg) {
    if (!arg || !*arg) return 1;
    int last = 0, i = 0;
    while (arg[i]) { if (arg[i] == '/') last = i + 1; i++; }
    vga_writestring(arg + last);
    vga_putchar('\n');
    return 1;
}

static int shell_dirname(const char *arg) {
    if (!arg || !*arg) return 1;
    int last = -1, i = 0;
    while (arg[i]) { if (arg[i] == '/') last = i; i++; }
    if (last < 0) { vga_putchar('.'); vga_putchar('\n'); }
    else { for (i = 0; i < last; i++) vga_putchar(arg[i]); vga_putchar('\n'); }
    return 1;
}

static int shell_rev(const char *arg) {
    if (!arg || !*arg) return 1;
    int len = 0;
    while (arg[len]) len++;
    for (int i = len - 1; i >= 0; i--) vga_putchar(arg[i]);
    vga_putchar('\n');
    return 1;
}

static int shell_uniq(const char *arg) {
    if (!arg || !*arg) { vga_writestring("Usage: uniq <filename>\n"); return 1; }
    uint8_t buf[4096];
    int sz = fs_read(arg, buf, 4096);
    if (sz < 0) { vga_writestring("File not found\n"); return 1; }
    char prev_line[256];
    int prev_len = 0;
    char cur_line[256];
    int cur_len = 0;
    for (int i = 0; i <= sz; i++) {
        char c = (i < sz) ? buf[i] : '\n';
        if (c == '\n') {
            cur_line[cur_len] = 0;
            int same = (cur_len == prev_len);
            if (same) for (int j = 0; j < cur_len; j++) if (cur_line[j] != prev_line[j]) { same = 0; break; }
            if (!same) {
                vga_writestring(cur_line);
                vga_putchar('\n');
                for (int j = 0; j < cur_len; j++) prev_line[j] = cur_line[j];
                prev_len = cur_len;
            }
            cur_len = 0;
        } else {
            if (cur_len < 255) cur_line[cur_len++] = c;
        }
    }
    return 1;
}

static int shell_sort(const char *arg) {
    if (!arg || !*arg) { vga_writestring("Usage: sort <filename>\n"); return 1; }
    uint8_t buf[4096];
    int sz = fs_read(arg, buf, 4096);
    if (sz < 0) { vga_writestring("File not found\n"); return 1; }
    char lines[200][64];
    int lc = 0;
    char line[64]; int li = 0;
    for (int i = 0; i <= sz && lc < 200; i++) {
        char c = (i < sz) ? buf[i] : '\n';
        if (c == '\n') { line[li] = 0; int j; for (j = 0; j <= li && j < 63; j++) lines[lc][j] = line[j]; lines[lc][j] = 0; lc++; li = 0; }
        else { if (li < 62) line[li++] = c; }
    }
    // Simple bubble sort
    for (int i = 0; i < lc - 1; i++)
        for (int j = 0; j < lc - i - 1; j++) {
            int cmp = 0; int k = 0;
            while (lines[j][k] && lines[j+1][k] && lines[j][k] == lines[j+1][k]) k++;
            cmp = lines[j][k] - lines[j+1][k];
            if (cmp > 0) {
                char tmp[64]; int ti;
                for (ti = 0; lines[j][ti]; ti++) tmp[ti] = lines[j][ti]; tmp[ti] = 0;
                for (ti = 0; lines[j+1][ti]; ti++) lines[j][ti] = lines[j+1][ti]; lines[j][ti] = 0;
                for (ti = 0; tmp[ti]; ti++) lines[j+1][ti] = tmp[ti]; lines[j+1][ti] = 0;
            }
        }
    for (int i = 0; i < lc; i++) { vga_writestring(lines[i]); vga_putchar('\n'); }
    return 1;
}

static int shell_hostname(void) {
    vga_writestring("minios\n");
    return 1;
}

static int shell_window(void) {
    if (!gui_mode) { vga_writestring("GUI mode only\n"); return 1; }
    static int win_count = 1;
    char title[32];
    title[0] = 'T'; title[1] = 'e'; title[2] = 'r'; title[3] = 'm'; title[4] = ' ';
    int n = win_count;
    int pos = 5;
    if (n >= 100) { title[pos++] = '0' + n / 100; n %= 100; }
    if (n >= 10) { title[pos++] = '0' + n / 10; n %= 10; }
    title[pos++] = '0' + n;
    title[pos] = 0;
    win_count++;

    int wx = 40 + win_count * 20;
    int wy = 30 + win_count * 15;
    if (wx + 640 > fb.width) wx = fb.width - 640;
    if (wy + 440 > fb.height - 22) wy = fb.height - 22 - 440;

    int id = gui_create_window(wx, wy, 640, 440, title);
    if (id < 0) { vga_writestring("Max windows\n"); return 1; }
    vga_writestring("Opened ");
    vga_writestring(title);
    vga_writestring("\n");
    gui_redraw();
    return 1;
}

static int shell_clear(void) {
    vga_clear();
    return 1;
}

static void shell_reboot(void) {
    vga_writestring("Rebooting...\n");
    uint8_t good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0xFE, 0x64);
    for (;;);
}

int shell_run(const char *cmd) {
    while (*cmd == ' ') cmd++;

    const char *arg = cmd;
    while (*arg && *arg != ' ') arg++;
    int cmd_len = arg - cmd;
    while (*arg == ' ') arg++;

    if (cmd_len == 4 && cmd[0] == 'h' && cmd[1] == 'e' && cmd[2] == 'l' && cmd[3] == 'p') return shell_help();
    if (cmd_len == 4 && cmd[0] == 'e' && cmd[1] == 'c' && cmd[2] == 'h' && cmd[3] == 'o') return shell_echo(arg);
    if (cmd_len == 5 && cmd[0] == 'c' && cmd[1] == 'l' && cmd[2] == 'e' && cmd[3] == 'a' && cmd[4] == 'r') return shell_clear();
    if (cmd_len == 5 && cmd[0] == 'u' && cmd[1] == 'n' && cmd[2] == 'a' && cmd[3] == 'm' && cmd[4] == 'e') return shell_uname();
    if (cmd_len == 6 && cmd[0] == 'w' && cmd[1] == 'h' && cmd[2] == 'o' && cmd[3] == 'a' && cmd[4] == 'm' && cmd[5] == 'i') return shell_whoami();
    if (cmd_len == 6 && cmd[0] == 'r' && cmd[1] == 'e' && cmd[2] == 'b' && cmd[3] == 'o' && cmd[4] == 'o' && cmd[5] == 't') { shell_reboot(); return 1; }
    if (cmd_len == 6 && cmd[0] == 'u' && cmd[1] == 'p' && cmd[2] == 't' && cmd[3] == 'i' && cmd[4] == 'm' && cmd[5] == 'e') return shell_uptime();
    if (cmd_len == 7 && cmd[0] == 'm' && cmd[1] == 'e' && cmd[2] == 'm' && cmd[3] == 'i' && cmd[4] == 'n' && cmd[5] == 'f' && cmd[6] == 'o') return shell_meminfo();
    if (cmd_len == 5 && cmd[0] == 'a' && cmd[1] == 'b' && cmd[2] == 'o' && cmd[3] == 'u' && cmd[4] == 't') return shell_about();
    if (cmd_len == 4 && cmd[0] == 'c' && cmd[1] == 'a' && cmd[2] == 'l' && cmd[3] == 'c') return shell_calc();
    if (cmd_len == 5 && cmd[0] == 'p' && cmd[1] == 'a' && cmd[2] == 'i' && cmd[3] == 'n' && cmd[4] == 't') return shell_paint();
    if (cmd_len == 5 && cmd[0] == 's' && cmd[1] == 'n' && cmd[2] == 'a' && cmd[3] == 'k' && cmd[4] == 'e') return shell_snake();
    if (cmd_len == 7 && cmd[0] == 'b' && cmd[1] == 'r' && cmd[2] == 'o' && cmd[3] == 'w' && cmd[4] == 's' && cmd[5] == 'e' && cmd[6] == 'r') return shell_browser();
    if (cmd_len == 8 && cmd[0] == 's' && cmd[1] == 'e' && cmd[2] == 't' && cmd[3] == 't' && cmd[4] == 'i' && cmd[5] == 'n' && cmd[6] == 'g' && cmd[7] == 's') return shell_settings();
    if (cmd_len == 5 && cmd[0] == 'f' && cmd[1] == 'i' && cmd[2] == 'l' && cmd[3] == 'e' && cmd[4] == 's') return shell_files();
    if (cmd_len == 7 && cmd[0] == 'n' && cmd[1] == 'o' && cmd[2] == 't' && cmd[3] == 'e' && cmd[4] == 'p' && cmd[5] == 'a' && cmd[6] == 'd') return shell_notepad();
    if (cmd_len == 2 && cmd[0] == 'l' && cmd[1] == 's') return shell_ls();
    if (cmd_len == 3 && cmd[0] == 'c' && cmd[1] == 'a' && cmd[2] == 't') return shell_cat(arg);
    if (cmd_len == 2 && cmd[0] == 'r' && cmd[1] == 'm') return shell_rm(arg);
    if (cmd_len == 2 && cmd[0] == 'm' && cmd[1] == 'v') return shell_mv(arg);
    if (cmd_len == 4 && cmd[0] == 'e' && cmd[1] == 'd' && cmd[2] == 'i' && cmd[3] == 't') return shell_edit(arg);
    if (cmd_len == 6 && cmd[0] == 'w' && cmd[1] == 'i' && cmd[2] == 'n' && cmd[3] == 'v' && cmd[4] == 'e' && cmd[5] == 'r') return shell_winver();
    if (cmd_len == 6 && cmd[0] == 'c' && cmd[1] == 'r' && cmd[2] == 'e' && cmd[3] == 'a' && cmd[4] == 't' && cmd[5] == 'e') return shell_create(arg);
    if (cmd_len == 6 && cmd[0] == 'w' && cmd[1] == 'i' && cmd[2] == 'n' && cmd[3] == 'd' && cmd[4] == 'o' && cmd[5] == 'w') return shell_window();
    if (cmd_len == 6 && cmd[0] == 'p' && cmd[1] == 'y' && cmd[2] == 't' && cmd[3] == 'h' && cmd[4] == 'o' && cmd[5] == 'n') return shell_python(arg);
    if (cmd_len == 3 && cmd[0] == 't' && cmd[1] == 'c' && cmd[2] == 'c') return shell_tcc(arg);
    if (cmd_len == 3 && cmd[0] == 'a' && cmd[1] == 'p' && cmd[2] == 't') return shell_apt(arg);
    if (cmd_len == 4 && cmd[0] == 'd' && cmd[1] == 'a' && cmd[2] == 't' && cmd[3] == 'e') return shell_date();
    if (cmd_len == 8 && cmd[0] == 's' && cmd[1] == 'h' && cmd[2] == 'u' && cmd[3] == 't' && cmd[4] == 'd' && cmd[5] == 'o' && cmd[6] == 'w' && cmd[7] == 'n') { shell_shutdown(); return 1; }
    if (cmd_len == 8 && cmd[0] == 'n' && cmd[1] == 'e' && cmd[2] == 'o' && cmd[3] == 'f' && cmd[4] == 'e' && cmd[5] == 't' && cmd[6] == 'c' && cmd[7] == 'h') return shell_neofetch();
    if (cmd_len == 2 && cmd[0] == 'p' && cmd[1] == 's') return shell_ps();
    if (cmd_len == 3 && cmd[0] == 'e' && cmd[1] == 'n' && cmd[2] == 'v') return shell_env();
    if (cmd_len == 3 && cmd[0] == 'y' && cmd[1] == 'e' && cmd[2] == 's') return shell_yes(arg);
    if (cmd_len == 7 && cmd[0] == 'h' && cmd[1] == 'e' && cmd[2] == 'x' && cmd[3] == 'd' && cmd[4] == 'u' && cmd[5] == 'm' && cmd[6] == 'p') return shell_hexdump(arg);
    if (cmd_len == 4 && cmd[0] == 'h' && cmd[1] == 'e' && cmd[2] == 'a' && cmd[3] == 'd') return shell_head(arg);
    if (cmd_len == 2 && cmd[0] == 'w' && cmd[1] == 'c') return shell_wc(arg);
    if (cmd_len == 3 && cmd[0] == 'c' && cmd[1] == 'a' && cmd[2] == 'l') return shell_cal();
    if (cmd_len == 3 && cmd[0] == 's' && cmd[1] == 'e' && cmd[2] == 'q') return shell_seq(arg);
    if (cmd_len == 4 && cmd[0] == 'e' && cmd[1] == 'x' && cmd[2] == 'p' && cmd[3] == 'r') return shell_expr(arg);
    if (cmd_len == 5 && cmd[0] == 'w' && cmd[1] == 'h' && cmd[2] == 'i' && cmd[3] == 'c' && cmd[4] == 'h') return shell_which(arg);
    if (cmd_len == 4 && cmd[0] == 't' && cmd[1] == 'r' && cmd[2] == 'u' && cmd[3] == 'e') return shell_true();
    if (cmd_len == 5 && cmd[0] == 'f' && cmd[1] == 'a' && cmd[2] == 'l' && cmd[3] == 's' && cmd[4] == 'e') return shell_false();
    if (cmd_len == 5 && cmd[0] == 's' && cmd[1] == 'l' && cmd[2] == 'e' && cmd[3] == 'e' && cmd[4] == 'p') return shell_sleep(arg);
    if (cmd_len == 5 && cmd[0] == 't' && cmd[1] == 'o' && cmd[2] == 'u' && cmd[3] == 'c' && cmd[4] == 'h') return shell_touch(arg);
    if (cmd_len == 8 && cmd[0] == 'b' && cmd[1] == 'a' && cmd[2] == 's' && cmd[3] == 'e' && cmd[4] == 'n' && cmd[5] == 'a' && cmd[6] == 'm' && cmd[7] == 'e') return shell_basename(arg);
    if (cmd_len == 7 && cmd[0] == 'd' && cmd[1] == 'i' && cmd[2] == 'r' && cmd[3] == 'n' && cmd[4] == 'a' && cmd[5] == 'm' && cmd[6] == 'e') return shell_dirname(arg);
    if (cmd_len == 3 && cmd[0] == 'r' && cmd[1] == 'e' && cmd[2] == 'v') return shell_rev(arg);
    if (cmd_len == 4 && cmd[0] == 'u' && cmd[1] == 'n' && cmd[2] == 'i' && cmd[3] == 'q') return shell_uniq(arg);
    if (cmd_len == 4 && cmd[0] == 's' && cmd[1] == 'o' && cmd[2] == 'r' && cmd[3] == 't') return shell_sort(arg);
    if (cmd_len == 8 && cmd[0] == 'h' && cmd[1] == 'o' && cmd[2] == 's' && cmd[3] == 't' && cmd[4] == 'n' && cmd[5] == 'a' && cmd[6] == 'm' && cmd[7] == 'e') return shell_hostname();

    vga_writestring("Unknown command: ");
    vga_write(cmd, cmd_len);
    vga_writestring("\n");
    return 1;
}

static char shell_buf[256];
static int shell_pos;

static void shell_init(void) {
    shell_pos = 0;
    vga_writestring("minios v0.1 - Minimal UNIX-like OS\n");
    vga_writestring("Type 'help' for commands.\n\n");
    vga_writestring("$ ");
}

static void shell_process(void) {
    if (shell_pos > 0) {
        shell_buf[shell_pos] = 0;
        shell_run(shell_buf);
    }
    shell_pos = 0;
    vga_writestring("$ ");
}

void shell_inject(const char *cmd) {
    vga_writestring(cmd);
    vga_putchar('\n');
    shell_run(cmd);
    vga_writestring("$ ");
}

static void shell_handle(char c) {
    if (c == '\n') {
        vga_putchar('\n');
        shell_process();
    } else if (c == '\b') {
        if (shell_pos > 0) {
            shell_pos--;
            vga_putchar('\b');
        }
    } else if (c >= ' ') {
        if (shell_pos < 255) {
            shell_buf[shell_pos++] = c;
            vga_putchar(c);
        }
    }
}

void delay(void) {
    for (volatile int i = 0; i < 5000000; i++);
}

void timer_tick(void) {
    ticks++;
}

void kernel_main(void) {
    vga_init();
    serial_init();

    vga_writestring("Booting minios...\n");
    fs_init();
    int has_net = net_init();

    if (fb_init()) {
        gui_mode = 1;

        settings_load();

        if (gui_settings.boot_anim) {
            uint32_t c1 = RGB(51, 102, 153);
            uint32_t c2 = RGB(0, 60, 180);
            uint32_t c3 = RGB(255, 255, 255);
            for (int f = 0; f < 20; f++) {
                fb_clear(c1);
                int s = 8 + f * 2;
                fb_fill_rect(400 - s, 300 - s, s * 2, s * 2, c2);
                int tx = 360 - 3 * f;
                int ty = 300 + s + 8;
                fb_draw_string(tx, ty, "minios", c3, c1);
                serial_writechar('.');
                for (volatile int d = 0; d < 2000000; d++);
            }
            serial_writechar('\n');
        }

        rtc_init();
        gui_init();
        gui_redraw();
        vga_writestring("[OK] GUI initialized\n");
    } else {
        vga_writestring("[WARN] No VBE framebuffer, using text mode\n");
    }

    int http_done = 0;

    if (gui_mode) {
        keyboard_init();
        mouse_init();

        if (gui_settings.desktop_env == 1)
            de_pixel_run();

        for (;;) {
            gui_poll();
            keyboard_poll();
            char c = keyboard_getchar();
            if (c) {
                shell_handle(c);
                gui_redraw();
            }

            if (has_net && !http_done) {
                http_done = 1;
                char httpbuf[2048];
                int got = net_http_get("10.0.2.2", 8080, "/test.txt", httpbuf, 2048);
                if (got > 0) {
                    vga_writestring("HTTP OK: ");
                    for (int i = 0; i < got && i < 120; i++) {
                        if (httpbuf[i] >= 32) vga_putchar(httpbuf[i]);
                    }
                    vga_putchar('\n');
                } else {
                    vga_writestring("HTTP test failed\n");
                }
            }
        }
    } else {
        shell_init();
        keyboard_init();

        for (;;) {
            keyboard_poll();
            char c = keyboard_getchar();
            if (c) {
                shell_handle(c);
            }
            delay();
        }
    }
}
