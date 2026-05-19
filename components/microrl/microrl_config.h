/*
Microrl library config files
Autor: Eugene Samoylov aka Helius (ghelius@gmail.com)
*/
#ifndef _MICRORL_CONFIG_H_
#define _MICRORL_CONFIG_H_

#define MICRORL_LIB_VER CONFIG_MICRORL_LIB_VER

/*********** CONFIG SECTION **************/
/*
Command line length, define cmdline buffer size. Set max number of chars + 1,
because last byte of buffer need to contain '\0' - NULL terminator, and 
not use for storing inputed char.
If user input chars more then it parametrs-1, chars not added to command line.*/
#define _COMMAND_LINE_LEN CONFIG_MICRORL_COMMAND_LINE_LEN

/*
Command token number, define max token it command line, if number of token 
typed in command line exceed this value, then prints message about it and
command line not to be parced and 'execute' callback will not calls.
Token is word separate by white space, for example 3 token line:
"IRin> set mode test" */
#define _COMMAND_TOKEN_NMB CONFIG_MICRORL_COMMAND_TOKEN_NMB

/*
Define you prompt string here. You can use colors escape code, for highlight you prompt,
for example this prompt will green color (if you terminal supports color)*/
//#define _PROMPT_DEFAULT "\033[32mIRin >\033[0m "	// green color
#define _PROMPT_DEFAULT "\033[32m" CONFIG_MICRORL_PROMPT_DEFAULT "\033[0m "	// green color
//#define _PROMPT_DEFAULT "IRin > "

/*
Define prompt text (without ESC sequence, only text) prompt length, it needs because if you use
ESC sequence, it's not possible detect only text length*/
#define _PROMPT_LEN       CONFIG_MICRORL_PROMPT_LEN

/*Define it, if you wanna use completion functional, also set completion callback in you code,
now if user press TAB calls 'copmlitetion' callback. If you no need it, you can just set 
NULL to callback ptr and do not use it, but for memory saving tune, 
if you are not going to use it - disable this define.*/
#if CONFIG_MICRORL_USE_COMPLETE
#define _USE_COMPLETE 1
#endif

/*Define it, if you wanna use history. It s work's like bash history, and
set stored value to cmdline, if UP and DOWN key pressed. Using history add
memory consuming, depends from _RING_HISTORY_LEN parametr */
#if CONFIG_MICRORL_USE_HISTORY
#define _USE_HISTORY

/*
History ring buffer length, define static buffer size.
For saving memory, each entered cmdline store to history in ring buffer,
so we can not say, how many line we can store, it depends from cmdline len,
but memory using more effective. We not prefer dinamic memory allocation for
small and embedded devices. Overhead is 2 char on each saved line*/
#define _RING_HISTORY_LEN CONFIG_MICRORL_RING_HISTORY_LEN

#endif

/*
Enable Handling terminal ESC sequence. If disabling, then cursor arrow, HOME, END will not work,
use Ctrl+A(B,F,P,N,A,E,H,K,U,C) see README, but decrease code memory.*/
#if CONFIG_MICRORL_USE_ESC_SEQ
#define _USE_ESC_SEQ
#endif

/*
Use snprintf from you standard complier library, but it gives some overhead.
If not defined, use my own u16int_to_str variant, it's save about 800 byte of code size
on AVR (avr-gcc build).
Try to build with and without, and compare total code size for tune library.
*/
#if CONFIG_MICRORL_USE_LIBC_STDIO
#define _USE_LIBC_STDIO
#endif

/*
Enable 'interrupt signal' callback, if user press Ctrl+C */
#if CONFIG_MICRORL_USE_CTRL_C
#define _USE_CTLR_C
#endif

/*
Print prompt at 'microrl_init', if enable, prompt will print at startup, 
otherwise first prompt will print after first press Enter in terminal
NOTE!: Enable it, if you call 'microrl_init' after your communication subsystem 
already initialize and ready to print message */
#if CONFIG_MICRORL_ENABLE_INIT_PROMPT
#define _ENABLE_INIT_PROMPT
#endif

/*
New line symbol */

#define ENDL "\r\n" // default new line symbol, for example for Windows

#if CONFIG_MICRORL_ENDL_CR
#define _ENDL_CR
// #define ENDL "\r"
#elif CONFIG_MICRORL_ENDL_LF
#define _ENDL_LF
// #define ENDL "\n"
#elif CONFIG_MICRORL_ENDL_CRLF
#define _ENDL_CRLF
// #define ENDL "\r\n"
#elif CONFIG_MICRORL_ENDL_LFCR
#define _ENDL_LFCR
// #define ENDL "\n\r"
#else
#error "You must define new line symbol."
#endif


/********** END CONFIG SECTION ************/


#if _RING_HISTORY_LEN > 256
#error "This history implementation (ring buffer with 1 byte iterator) allow 256 byte buffer size maximum"
#endif

#endif
