#include <stdio.h>
#include <string.h>
#include "shell.h"
#include "vim.h"
#include "fs.h"
#include "comm.h"
#include "heap.h"
#include "lexer.h"
#include "parser.h"
#include "ir_lowering.h"
#include "cbpf.h"
#include "ccbpf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h"
#if SHELL_ENABLE_VIM
#include "vim.h"
#endif

#if SHELL_ENABLE_FS
#include "fs.h"
#endif

static char linebuf[SHELL_MAX_LINE];
static char path[SHELL_MAX_PATH];
static char cwd[SHELL_MAX_PATH] = "";
static char *argv_buf[SHELL_MAX_ARGS];
static char shell_abs[SHELL_MAX_PATH];

static void normalize_path(char *p)
{
    char *src = p;
    char *dst = p;

    if (*src != '/')
        return;

    while (*src) {
        if (src[0] == '/' && src[1] == '/') {
            src++;
            continue;
        }

        if (src[0] == '/' && src[1] == '.' &&
            (src[2] == '/' || src[2] == '\0')) {
            src += 2;
            continue;
        }

        if (src[0] == '/' && src[1] == '.' && src[2] == '.' &&
            (src[3] == '/' || src[3] == '\0')) {

            if (dst != p) {
                dst--;
                while (dst > p && *dst != '/')
                    dst--;
            }
            src += 3;
            continue;
        }

        *dst++ = *src++;
    }

    if (dst == p) {
        *dst++ = '/';
    }

    if (dst > p + 1 && *(dst - 1) == '/')
        dst--;

    *dst = '\0';
}

static void make_abs_path(char *out, const char *in)
{
    if (in[0] == '/') {
        snprintf(out, SHELL_MAX_PATH, "%s", in);
    } else if (cwd[0] == '\0' || (cwd[0] == '/' && cwd[1] == '\0')) {
        snprintf(out, SHELL_MAX_PATH, "/%s", in);
    } else {
        snprintf(out, SHELL_MAX_PATH, "%s/%s", cwd, in);
    }

    normalize_path(out);
}

#if SHELL_ENABLE_FS
static int fs_is_dir(const char *p)
{
    struct dirent tmp[1];
    int nread = 0;
    return fs_readdir(p, tmp, 1, &nread) == 0;
}
#endif

int shell_readline(char *buf, int max)
{
    int pos = 0;

    for (;;) {
        char c = comm_getc();

        if (c == '\r' || c == '\n') {
            comm_putc('\r');
            comm_putc('\n');
            buf[pos] = 0;
            return pos;
        }

        if (c == '\b' || c == 127) {
            if (pos > 0) {
                pos--;
                comm_write(SHELL_BACKSPACE_SEQ, SHELL_BACKSPACE_SEQ_LEN);
            }
            continue;
        }

        if (pos < max - 1) {
            buf[pos++] = c;
            comm_putc(c);
        }
    }
}

int shell_parse(char *line, char **argv, int max)
{
    int argc = 0;

    while (*line && argc < max) {
        while (*line == ' ')
            line++;
        if (!*line)
            break;

        argv[argc++] = line;

        while (*line && *line != ' ')
            line++;
        if (*line)
            *line++ = 0;
    }

    return argc;
}

#if SHELL_ENABLE_FS
int cmd_ls(int argc, char **argv)
{
    int n = 0;
    struct dirent *ents;

    if (argc > 1)
        make_abs_path(path, argv[1]);
    else
        strcpy(path, cwd);

    if (path[0] == '\0')
        strcpy(path, "/");

    ents = heap_malloc(sizeof(struct dirent) * SHELL_LS_MAX_ENTRIES);
    if (!ents) return -1;

    if (fs_readdir(path, ents, SHELL_LS_MAX_ENTRIES, &n) < 0) {
        heap_free(ents);
        return -1;
    }

    for (int i = 0; i < n; i++) {
        comm_write(ents[i].name, strlen(ents[i].name));
        comm_write("  ", 2);
    }

    comm_write("\r\n", 2);
    heap_free(ents);
    return 0;
}

int cmd_cat(int argc, char **argv)
{
    if (argc < 2)
        return -1;

    memset(path, 0, sizeof(path));
    make_abs_path(path, argv[1]);

    struct inode *ino;
    if (fs_open(path, 0, &ino) < 0)
        return -1;

    uint32_t off = 0;
    int r;
    char *cat_buf = heap_malloc(SHELL_CAT_BUF_SIZE);
    if (!cat_buf) {
        fs_close(ino);
        return -1;
    }

    while ((r = fs_read(ino, off, cat_buf, SHELL_CAT_BUF_SIZE)) > 0) {
        for (int i = 0; i < r; i++) {
            if (cat_buf[i] == '\n')
                comm_write("\r\n", 2);
            else
                comm_putc(cat_buf[i]);
        }
        off += (uint32_t)r;
    }

    heap_free(cat_buf);
    fs_close(ino);
    return 0;
}

int cmd_touch(int argc, char **argv)
{
    if (argc < 2)
        return -1;

    memset(path, 0, sizeof(path));
    make_abs_path(path, argv[1]);

    struct inode *ino;
    if (fs_open(path, O_CREAT, &ino) < 0)
        return -1;

    fs_close(ino);
    return 0;
}

int cmd_mkdir(int argc, char **argv)
{
    if (argc < 2)
        return -1;

    memset(path, 0, sizeof(path));
    make_abs_path(path, argv[1]);

    struct inode *ino;
    if (fs_mkdir(path, &ino) < 0)
        return -1;

    return 0;
}

int cmd_cd(int argc, char **argv)
{
    if (argc < 2)
        return -1;

    memset(path, 0, sizeof(path));
    make_abs_path(path, argv[1]);

    if (!fs_is_dir(path))
        return -1;

    strcpy(cwd, path);
    return 0;
}

int cmd_sync(int argc, char **argv)
{
    if (argc != 1)
        return -1;

    fs_sync();
    return 0;
}

int cmd_rm(int argc, char **argv)
{
    if (argc < 2)
        return -1;

    memset(path, 0, sizeof(path));
    make_abs_path(path, argv[1]);

    if (fs_unlink(path) < 0) {
        printf("rm failed\n");
        return -1;
    }

    return 0;
}
#endif

#if SHELL_ENABLE_VIM
int cmd_vim(int argc, char **argv)
{
    if (argc < 2)
        return -1;

    memset(shell_abs, 0, sizeof(shell_abs));
    make_abs_path(shell_abs, argv[1]);

    vim_main(shell_abs);
    return 0;
}
#endif

#if SHELL_ENABLE_COMPILER

static char *load_text_file_heap_fs(const char *path)
{
    struct inode *ino;
    if (fs_open(path, O_RDONLY, &ino) != 0)
        return NULL;

    uint32_t size = fs_get_size(ino);
    char *buf = heap_malloc(size + 1);
    if (!buf) {
        fs_close(ino);
        return NULL;
    }

    int r = fs_read(ino, 0, buf, size);
    fs_close(ino);
    if (r != (int)size) {
        heap_free(buf);
        return NULL;
    }

    buf[size] = 0;
    return buf;
}

extern void native_init_frontend(void);
static int cmd_compile(int argc, char **argv)
{
    if (argc < 2)
        return -1;

    char src_path[SHELL_MAX_PATH];
    char out_path[SHELL_MAX_PATH];

    memset(src_path, 0, sizeof(src_path));
    make_abs_path(src_path, argv[1]);

    char *src = load_text_file_heap_fs(src_path);
    if (!src) {
        comm_write("load source failed\r\n", 21);
        return 0;
    }

    compiler_init(16, 4*1024, 1*1024, 5*1024);
    struct lexer lex;
    lexer_init(&lex);
    lexer_set_input_buffer(src, strlen(src));

    struct Parser *p = parser_new(&lex);
    native_init_frontend();
    parser_program(p);

    frontend_destroy(&lex);

    struct bpf_builder b;
    bpf_builder_init(&b, 4*1024);

    struct ir_mes im;
    ir_mes_get(&im);
    ir_lower_program(im.ir_head, im.label_count, &b);

    struct bpf_insn *prog = bpf_builder_data(&b);
    int prog_len = bpf_builder_count(&b);

    size_t image_len = 0;
    uint8_t *image = ccbpf_pack_memory(prog, (size_t)prog_len, &image_len);

    bpf_builder_free(&b);
    heap_free(src);

    if (!image) {
        comm_write("pack image failed\r\n", 21);
        return 0;
    }

    const char *in = argv[1];
    const char *dot = strrchr(in, '.');
    if (!dot) dot = in + strlen(in);

    int n = (int)(dot - in);
    if (n >= SHELL_MAX_PATH - 8) n = SHELL_MAX_PATH - 8;

    memcpy(out_path, in, n);
    out_path[n] = 0;
    strcat(out_path, ".ccbpf");

    char abs_out[SHELL_MAX_PATH];
    make_abs_path(abs_out, out_path);

    struct inode *ino;
    if (fs_open(abs_out, O_CREAT | O_RDWR, &ino) != 0) {
        comm_write("fs_open failed\r\n", 17);
        heap_free(image);
        return 0;
    }

    int w = fs_write(ino, 0, image, (uint32_t)image_len);
    fs_close(ino);
    fs_sync();

    heap_free(image);

    char msg[128];
    int m = snprintf(msg, sizeof(msg),
                     "compiled to %s, %d bytes\r\n",
                     abs_out, w);
    if (m > 0) comm_write(msg, m);

    return 0;
}

static int cmd_attachbpf(int argc, char **argv)
{
    if (argc < 3) return -1;

    char path[SHELL_MAX_PATH];
    make_abs_path(path, argv[2]);

    struct inode *ino;
    if (fs_open(path, O_RDONLY, &ino) != 0)
        return 0;

    uint32_t size = fs_get_size(ino);
    uint8_t *img = heap_malloc(size);
    fs_read(ino, 0, img, size);
    fs_close(ino);

    hook_attach(argv[1], img, size);

    heap_free(img);
    return 0;
}

static int cmd_detachbpf(int argc, char **argv)
{
    if (argc < 2) return -1;
    hook_detach(argv[1]);
    return 0;
}
#endif


struct cmd_entry {
    const char *name;
    int (*func)(int argc, char **argv);
};

static struct cmd_entry cmd_table[] = {

#if SHELL_ENABLE_FS
        {"ls",     cmd_ls},
        {"cat",    cmd_cat},
        {"touch",  cmd_touch},
        {"mkdir",  cmd_mkdir},
        {"cd",     cmd_cd},
        {"sync",   cmd_sync},
        {"rm", cmd_rm},
#endif

#if SHELL_ENABLE_VIM
        {"vim",    cmd_vim},
#endif

#if SHELL_ENABLE_COMPILER
        {"compile", cmd_compile},
        {"attach", cmd_attachbpf},
        {"detach", cmd_detachbpf},
#endif
        {NULL,     NULL}
};

void shell_exec(int argc, char **argv)
{
    for (int i = 0; cmd_table[i].name; i++) {
        if (strcmp(argv[0], cmd_table[i].name) == 0) {
            cmd_table[i].func(argc, argv);
            return;
        }
    }

    char buf[64];
    int n = snprintf(buf, sizeof(buf),
                     "unknown command: %s\r\n", argv[0]);
    if (n > 0)
        comm_write(buf, n);
}

void shell_main(void)
{
    comm_write(SHELL_PROMPT, (int)strlen(SHELL_PROMPT));

    int len = shell_readline(linebuf, SHELL_MAX_LINE);
    if (len <= 0)
        return;

    int argc = shell_parse(linebuf, argv_buf, SHELL_MAX_ARGS);
    if (argc == 0)
        return;

    shell_exec(argc, argv_buf);
}

void shell_on_message(const char *msg, int len)
{
    if (!msg || len <= 0)
        return;

    if (len >= SHELL_MAX_LINE)
        len = SHELL_MAX_LINE - 1;

    memcpy(linebuf, msg, (size_t)len);
    linebuf[len] = '\0';

    int argc = shell_parse(linebuf, argv_buf, SHELL_MAX_ARGS);
    if (argc == 0)
        return;

    shell_exec(argc, argv_buf);
}