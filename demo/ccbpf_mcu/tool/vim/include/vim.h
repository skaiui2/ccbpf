#ifndef VIM_H
#define VIM_H

#define VIM_SMALL_LOAD_BUF   64
#define VIM_SMALL_SAVE_BUF   64
#define VIM_INITIAL_LINES    4
#define VIM_CMD_BUF_SIZE     32
#define VIM_SCREEN_BUF       64

void vim_main(const char *path);

#endif
