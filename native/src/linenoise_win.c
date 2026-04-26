/* linenoise_win.c -- Windows Console API implementation of linenoise I/O layer
 *
 * This file provides a Windows-compatible linenoise implementation using
 * Windows Console API (ReadConsoleInputA, GetConsoleScreenBufferInfo, etc.)
 * instead of POSIX termios/ioctl/read.
 *
 * The core editing logic (UTF-8 buffering, cursor positioning, history,
 * completion) is kept identical to linenoise.c; only the I/O layer is replaced.
 */

#include "linenoise.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define LINENOISE_DEFAULT_HISTORY_MAX_LEN 100
#define LINENOISE_MAX_LINE 4096

static char *unsupported_term[] = {"dumb","cons25","emacs",NULL};
static linenoiseCompletionCallback *completionCallback = NULL;
static linenoiseHintsCallback *hintsCallback = NULL;
static linenoiseFreeHintsCallback *freeHintsCallback = NULL;
/* Forward declarations */
static void disableRawMode(int fd);
static char *linenoiseNoTTY(void);
static void refreshLineWithCompletion(struct linenoiseState *ls, linenoiseCompletions *lc, int flags);
static void refreshLineWithFlags(struct linenoiseState *l, int flags);

static DWORD orig_console_mode = 0;
static int rawmode = 0;
static int mlmode = 0;
static int atexit_registered = 0;
static int history_max_len = LINENOISE_DEFAULT_HISTORY_MAX_LEN;
static int history_len = 0;
static char **history = NULL;

/* =========================== UTF-8 support ================================ */

static int utf8ByteLen(char c) {
    unsigned char uc = (unsigned char)c;
    if ((uc & 0x80) == 0)    return 1;
    if ((uc & 0xE0) == 0xC0) return 2;
    if ((uc & 0xF0) == 0xE0) return 3;
    if ((uc & 0xF8) == 0xF0) return 4;
    return 1;
}

static uint32_t utf8DecodeChar(const char *s, size_t *len) {
    unsigned char *p = (unsigned char *)s;
    uint32_t cp;
    if ((*p & 0x80) == 0) {
        *len = 1; return *p;
    } else if ((*p & 0xE0) == 0xC0) {
        *len = 2; cp = (*p & 0x1F) << 6; cp |= (p[1] & 0x3F); return cp;
    } else if ((*p & 0xF0) == 0xE0) {
        *len = 3; cp = (*p & 0x0F) << 12; cp |= (p[1] & 0x3F) << 6; cp |= (p[2] & 0x3F); return cp;
    } else if ((*p & 0xF8) == 0xF0) {
        *len = 4; cp = (*p & 0x07) << 18; cp |= (p[1] & 0x3F) << 12; cp |= (p[2] & 0x3F) << 6; cp |= (p[3] & 0x3F); return cp;
    }
    *len = 1; return *p;
}

static int isVariationSelector(uint32_t cp) { return cp == 0xFE0E || cp == 0xFE0F; }
static int isSkinToneModifier(uint32_t cp) { return cp >= 0x1F3FB && cp <= 0x1F3FF; }
static int isZWJ(uint32_t cp) { return cp == 0x200D; }
static int isRegionalIndicator(uint32_t cp) { return cp >= 0x1F1E6 && cp <= 0x1F1FF; }
static int isCombiningMark(uint32_t cp) {
    return (cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x1AB0 && cp <= 0x1AFF) ||
           (cp >= 0x1DC0 && cp <= 0x1DFF) || (cp >= 0x20D0 && cp <= 0x20FF) ||
           (cp >= 0xFE20 && cp <= 0xFE2F);
}
static int isGraphemeExtend(uint32_t cp) {
    return isVariationSelector(cp) || isSkinToneModifier(cp) || isZWJ(cp) || isCombiningMark(cp);
}

static uint32_t utf8DecodePrev(const char *buf, size_t pos, size_t *cplen) {
    if (pos == 0) { *cplen = 0; return 0; }
    size_t i = pos;
    do { i--; } while (i > 0 && (pos - i) < 4 && ((unsigned char)buf[i] & 0xC0) == 0x80);
    *cplen = pos - i;
    size_t dummy;
    return utf8DecodeChar(buf + i, &dummy);
}

static int utf8GraphemeLen(const char *buf, size_t pos) {
    size_t cplen;
    uint32_t cp = utf8DecodePrev(buf, pos, &cplen);
    int gw = 1;
    while (cplen > 0 && pos >= cplen) {
        size_t prevlen;
        uint32_t prevcp = utf8DecodePrev(buf, pos - cplen, &prevlen);
        if (!isGraphemeExtend(cp) && !(isRegionalIndicator(cp) && isRegionalIndicator(prevcp))) break;
        gw++;
        cp = prevcp;
        cplen = prevlen;
    }
    return gw;
}

static int utf8GraphemeNext(const char *buf, size_t pos, size_t buflen) {
    size_t cplen;
    uint32_t cp = utf8DecodePrev(buf, pos, &cplen);
    while (pos < buflen) {
        size_t nextlen;
        uint32_t nextcp = utf8DecodeChar(buf + pos, &nextlen);
        if (!isGraphemeExtend(nextcp) && !(isRegionalIndicator(cp) && isRegionalIndicator(nextcp))) break;
        pos += nextlen;
        cp = nextcp;
    }
    return (int)pos;
}

/* ======================= Terminal/Console helpers ========================= */

/* Get console handle for output */
static HANDLE getOutputHandle(void) {
    return GetStdHandle(STD_OUTPUT_HANDLE);
}

static HANDLE getInputHandle(void) {
    return GetStdHandle(STD_INPUT_HANDLE);
}

/* Get terminal column count */
static size_t getTerminalColumns(void) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hOut = getOutputHandle();
    if (hOut == INVALID_HANDLE_VALUE) return 80;
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        return (size_t)(csbi.srWindow.Right - csbi.srWindow.Left + 1);
    }
    return 80;
}

/* Clear from cursor to end of line */
static void clearLineEnd(void) {
    HANDLE hOut = getOutputHandle();
    if (hOut == INVALID_HANDLE_VALUE) return;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        DWORD n = csbi.dwSize.X - csbi.dwCursorPosition.X;
        DWORD written;
        FillConsoleOutputCharacterA(hOut, ' ', n, csbi.dwCursorPosition, &written);
        FillConsoleOutputAttribute(hOut, csbi.wAttributes, n, csbi.dwCursorPosition, &written);
    }
}

/* Count unicode columns for a UTF-8 string (display width) */
static int utf8DisplayLen(const char *buf, size_t len) {
    int w = 0;
    size_t i = 0;
    while (i < len) {
        size_t cplen;
        uint32_t cp = utf8DecodeChar(buf + i, &cplen);
        if (!isGraphemeExtend(cp) && !isZWJ(cp)) {
            w += 1;
        }
        i += cplen;
    }
    return w;
}

/* =========================== History ===================================== */

static void freeHistory(void) {
    for (int i = 0; i < history_len; i++) {
        free(history[i]);
    }
    free(history);
    history = NULL;
}

int linenoiseHistorySetMaxLen(int len) {
    if (len < 1) return 0;
    if (history) {
        while (history_len > len) {
            free(history[history_len - 1]);
            history_len--;
        }
    }
    history_max_len = len;
    return 1;
}

int linenoiseHistoryAdd(const char *line) {
    if (!line || !line[0]) return 0;
    if (!history) {
        history = (char **)calloc(history_max_len, sizeof(char *));
        if (!history) return 0;
    }
    if (history_len == history_max_len) {
        free(history[0]);
        memmove(&history[0], &history[1], (history_max_len - 1) * sizeof(char *));
        history_len--;
    }
    history[history_len] = strdup(line);
    history_len++;
    return 1;
}

int linenoiseHistorySave(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) return -1;
    for (int i = 0; i < history_len; i++) {
        fprintf(fp, "%s\n", history[i]);
    }
    fclose(fp);
    return 0;
}

int linenoiseHistoryLoad(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return -1;
    char buf[LINENOISE_MAX_LINE];
    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
            buf[--len] = '\0';
        }
        if (len > 0) linenoiseHistoryAdd(buf);
    }
    fclose(fp);
    return 0;
}

/* ========================== Completions =================================== */

void linenoiseSetCompletionCallback(linenoiseCompletionCallback *cb) {
    completionCallback = cb;
}

void linenoiseSetHintsCallback(linenoiseHintsCallback *cb) {
    hintsCallback = cb;
}

void linenoiseSetFreeHintsCallback(linenoiseFreeHintsCallback *cb) {
    freeHintsCallback = cb;
}

void linenoiseAddCompletion(linenoiseCompletions *lc, const char *str) {
    lc->len++;
    lc->cvec = (char **)realloc(lc->cvec, lc->len * sizeof(char *));
    lc->cvec[lc->len - 1] = strdup(str);
}

static void freeCompletions(linenoiseCompletions *lc) {
    for (size_t i = 0; i < lc->len; i++) free(lc->cvec[i]);
    free(lc->cvec);
}

static linenoiseCompletions *getCompletions(struct linenoiseState *l) {
    linenoiseCompletions *lc = (linenoiseCompletions *)calloc(1, sizeof(*lc));
    if (!lc) return NULL;
    if (completionCallback) {
        completionCallback(l->buf, lc);
    }
    return lc;
}

/* ========================= Line Refresh =================================== */

static void refreshLineWithFlags(struct linenoiseState *l, int flags) {
    (void)flags;

    /* Build output buffer using abuf pattern (like Unix linenoise) */
    char abuf[4096];
    int ablen = 0;

    /* Cursor to left edge */
    abuf[ablen++] = '\r';

    /* Print prompt + buffer */
    int plen = (int)strlen(l->prompt);
    int total = plen + l->len;
    if (ablen + total + 64 < (int)sizeof(abuf)) {
        memcpy(abuf + ablen, l->prompt, plen);
        ablen += plen;
        if (l->len > 0) {
            memcpy(abuf + ablen, l->buf, l->len);
            ablen += l->len;
        }
    }

    /* Erase to right */
    abuf[ablen++] = '\x1b';
    abuf[ablen++] = '[';
    abuf[ablen++] = '0';
    abuf[ablen++] = 'K';

    /* Position cursor: calculate display column and use ANSI CUF */
    int promptWidth = utf8DisplayLen(l->prompt, strlen(l->prompt));
    int cursorWidth = utf8DisplayLen(l->buf, l->pos);
    int col = promptWidth + cursorWidth;
    int cols = (int)getTerminalColumns();

    if (mlmode) {
        /* Multi-line: calculate row and column */
        int row = col / cols;
        int x = col % cols;
        if (row > 0) {
            ablen += snprintf(abuf + ablen, sizeof(abuf) - ablen, "\x1b[%dA", row);
        }
        if (x > 0) {
            ablen += snprintf(abuf + ablen, sizeof(abuf) - ablen, "\x1b[%dC", x);
        }
    } else {
        /* Single-line: move cursor to column */
        if (col > 0) {
            ablen += snprintf(abuf + ablen, sizeof(abuf) - ablen, "\x1b[%dC", col);
        }
    }

    /* Write all at once */
    DWORD written;
    HANDLE hOut = getOutputHandle();
    WriteFile(hOut, abuf, ablen, &written, NULL);
}

static void refreshLine(struct linenoiseState *l) {
    refreshLineWithFlags(l, 0);
}

static void refreshLineWithCompletion(struct linenoiseState *ls, linenoiseCompletions *lc, int flags) {
    (void)lc;
    refreshLineWithFlags(ls, flags);
}

/* ========================== Edit Loop ===================================== */

static void linenoiseAtExit(void);

static int editInsert(struct linenoiseState *l, const char *text, size_t len) {
    if (l->len + len >= l->buflen) return 0;
    /* Move existing content after cursor */
    memmove(l->buf + l->pos + len, l->buf + l->pos, l->len - l->pos);
    memcpy(l->buf + l->pos, text, len);
    l->pos += len;
    l->len += len;
    l->buf[l->len] = '\0';
    return 1;
}

/* Read input from console. In raw mode, use ReadFile to read raw bytes
 * from stdin (like Unix read()). For non-raw fallback, use ReadConsoleInputA.
 * Returns number of bytes read into outBuf, or 0 on EOF/error.
 * Sets *vkCode to the virtual key code for special keys. */
static int readConsoleInput(char *outBuf, int *vkCode) {
    HANDLE hIn = getInputHandle();
    INPUT_RECORD ir;
    DWORD read;

    /* In raw mode, try ReadFile first (reads raw console input as bytes) */
    if (rawmode) {
        char ch;
        if (ReadFile(hIn, &ch, 1, &read, NULL) && read == 1) {
            *vkCode = 0;
            outBuf[0] = ch;
            return 1;
        }
    }

    /* Fallback: use ReadConsoleInputA for key events */
    while (1) {
        if (!ReadConsoleInputA(hIn, &ir, 1, &read)) return 0;
        if (read == 0) return 0;
        if (ir.EventType != KEY_EVENT) continue;
        if (!ir.Event.KeyEvent.bKeyDown) continue;

        WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
        *vkCode = vk;

        /* Special keys — return virtual key code */
        if (vk == VK_RETURN || vk == VK_BACK || vk == VK_ESCAPE ||
            vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN ||
            vk == VK_DELETE || vk == VK_HOME || vk == VK_END || vk == VK_TAB) {
            char asciiCh = ir.Event.KeyEvent.uChar.AsciiChar;
            if (asciiCh != 0) {
                outBuf[0] = asciiCh;
                return 1;
            }
            /* Return VK code in outBuf: [0, vk_low_byte] */
            outBuf[0] = 0;
            outBuf[1] = (char)(vk & 0xFF);
            return 2;
        }

        /* Regular character */
        char ch = ir.Event.KeyEvent.uChar.AsciiChar;
        if (ch != 0) {
            outBuf[0] = ch;
            return 1;
        }

        /* Unicode character (e.g., CJK) */
        WCHAR wch = ir.Event.KeyEvent.uChar.UnicodeChar;
        if (wch != 0) {
            if (wch < 0x80) {
                outBuf[0] = (char)wch;
                return 1;
            } else if (wch < 0x800) {
                outBuf[0] = (char)(0xC0 | (wch >> 6));
                outBuf[1] = (char)(0x80 | (wch & 0x3F));
                return 2;
            } else {
                outBuf[0] = (char)(0xE0 | (wch >> 12));
                outBuf[1] = (char)(0x80 | ((wch >> 6) & 0x3F));
                outBuf[2] = (char)(0x80 | (wch & 0x3F));
                return 3;
            }
        }
    }
}

static int linenoiseEditFeedRaw(struct linenoiseState *l, char *outBuf) {
    int vkCode = 0;
    int nread = readConsoleInput(outBuf, &vkCode);
    if (nread == 0) return 0; /* EOF */
    if (nread == 2 && outBuf[0] == 0) {
        /* Virtual key code */
        outBuf[0] = 0; /* signal VK mode */
        outBuf[1] = (char)(vkCode & 0xFF);
    }
    return nread;
}

static int isUnsupportedTerm(void) {
    /* On Windows, assume support — no TERM to check */
    return 0;
}

static void linenoiseAtExit(void) {
    if (atexit_registered) {
        disableRawMode(-1);
        freeHistory();
        atexit_registered = 0;
    }
}

void linenoiseClearScreen(void) {
    const char seq[] = "\033[2J\033[H";
    DWORD written;
    HANDLE hOut = getOutputHandle();
    WriteFile(hOut, seq, sizeof(seq) - 1, &written, NULL);
}

/* =========================== Raw Mode ===================================== */

static int enableRawMode(int fd) {
    (void)fd;
    if (rawmode) return 0;

    HANDLE hIn = getInputHandle();
    HANDLE hOut = getOutputHandle();

    /* Get current console mode */
    DWORD mode_in = 0, mode_out = 0;
    if (!GetConsoleMode(hIn, &mode_in)) return -1;
    if (!GetConsoleMode(hOut, &mode_out)) return -1;

    /* Save original mode */
    orig_console_mode = mode_in;

    /* Enable VT output processing for ANSI escape sequences */
    mode_out |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
    SetConsoleMode(hOut, mode_out);

    /* Disable line input, echo, processed input; enable window input.
     * Keep QUICK_EDIT_MODE disabled so mouse selection doesn't interfere.
     * ENABLE_EXTENDED_FLAGS is needed to properly disable quick edit. */
    DWORD new_mode_in = ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS;
    SetConsoleMode(hIn, new_mode_in);

    rawmode = 1;
    return 0;
}

static void disableRawMode(int fd) {
    (void)fd;
    if (!rawmode) return;

    HANDLE hIn = getInputHandle();
    SetConsoleMode(hIn, orig_console_mode);

    /* Reset console colors */
    HANDLE hOut = getOutputHandle();
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        WORD attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        SetConsoleTextAttribute(hOut, attr);
    }

    rawmode = 0;
}

/* ========================== Non-blocking Edit API ========================= */

int linenoiseEditStart(struct linenoiseState *l, int stdin_fd, int stdout_fd, char *buf, size_t buflen, const char *prompt) {
    /* Initialize state */
    memset(l, 0, sizeof(*l));
    l->ifd = stdin_fd;
    l->ofd = stdout_fd;
    l->buf = buf;
    l->buflen = buflen;
    l->prompt = prompt;
    l->plen = (int)strlen(prompt);
    l->cols = getTerminalColumns();
    l->oldpos = l->pos = 0;
    l->len = 0;
    l->history_index = history_len;

    buf[0] = '\0';

    if (!atexit_registered) {
        atexit(linenoiseAtExit);
        atexit_registered = 1;
    }

    if (enableRawMode(stdin_fd) == -1) return -1;

    /* Print prompt and refresh */
    refreshLine(l);
    return 0;
}

char *linenoiseEditFeed(struct linenoiseState *l) {
    char seq[16];
    int nread;

    nread = linenoiseEditFeedRaw(l, seq);
    if (nread == 0) {
        /* EOF */
        disableRawMode(l->ifd);
        DWORD written;
        HANDLE hOut = getOutputHandle();
        WriteFile(hOut, "\n", 1, &written, NULL);
        return linenoiseNoTTY();
    }

    /* Check for virtual key code */
    if (seq[0] == 0 && nread == 2) {
        WORD vk = (WORD)(unsigned char)seq[1];

        switch (vk) {
        case VK_RETURN:
            disableRawMode(l->ifd);
            /* Use WriteFile for consistent output */
            {
                DWORD written;
                HANDLE hOut = getOutputHandle();
                WriteFile(hOut, "\r\n", 2, &written, NULL);
            }
            fflush(stdout);
            l->buf[l->len] = '\0';
            return strdup(l->buf);

        case VK_ESCAPE:
            disableRawMode(l->ifd);
            {
                DWORD written;
                HANDLE hOut = getOutputHandle();
                WriteFile(hOut, "\n", 1, &written, NULL);
            }
            fflush(stdout);
            return NULL;

        case VK_BACK:
            if (l->len > 0 && l->pos > 0) {
                size_t cplen;
                utf8DecodePrev(l->buf, l->pos, &cplen);
                memmove(l->buf + l->pos - cplen, l->buf + l->pos, l->len - l->pos + 1);
                l->pos -= cplen;
                l->len -= cplen;
                l->buf[l->len] = '\0';
                refreshLine(l);
            }
            break;

        case VK_DELETE:
            if (l->len > 0 && l->pos < l->len) {
                size_t cplen;
                utf8DecodeChar(l->buf + l->pos, &cplen);
                memmove(l->buf + l->pos, l->buf + l->pos + cplen, l->len - l->pos - cplen + 1);
                l->len -= cplen;
                l->buf[l->len] = '\0';
                refreshLine(l);
            }
            break;

        case VK_LEFT:
            if (l->pos > 0) {
                int prev = utf8GraphemeNext(l->buf, l->pos, l->pos);
                (void)prev; /* we go backward, not forward */
                size_t cplen;
                utf8DecodePrev(l->buf, l->pos, &cplen);
                l->pos -= cplen;
                refreshLine(l);
            }
            break;

        case VK_RIGHT:
            if (l->pos < l->len) {
                size_t cplen;
                utf8DecodeChar(l->buf + l->pos, &cplen);
                l->pos += cplen;
                refreshLine(l);
            }
            break;

        case VK_UP:
            if (history_len > 0) {
                if (l->history_index > 0) {
                    l->history_index--;
                    strncpy(l->buf, history[l->history_index], l->buflen - 1);
                    l->buf[l->buflen - 1] = '\0';
                    l->len = l->pos = strlen(l->buf);
                    refreshLine(l);
                }
            }
            break;

        case VK_DOWN:
            if (history_len > 0) {
                if (l->history_index < history_len) {
                    l->history_index++;
                    if (l->history_index == history_len) {
                        l->buf[0] = '\0';
                        l->len = l->pos = 0;
                    } else {
                        strncpy(l->buf, history[l->history_index], l->buflen - 1);
                        l->buf[l->buflen - 1] = '\0';
                        l->len = l->pos = strlen(l->buf);
                    }
                    refreshLine(l);
                }
            }
            break;

        case VK_HOME:
            l->pos = 0;
            refreshLine(l);
            break;

        case VK_END:
            l->pos = l->len;
            refreshLine(l);
            break;

        case VK_TAB:
            if (completionCallback) {
                linenoiseCompletions lc = {0, NULL};
                completionCallback(l->buf, &lc);
                if (lc.len == 1) {
                    /* Single completion: apply it */
                    strncpy(l->buf, lc.cvec[0], l->buflen - 1);
                    l->buf[l->buflen - 1] = '\0';
                    l->len = l->pos = strlen(l->buf);
                    refreshLine(l);
                } else if (lc.len > 1) {
                    /* Multiple: find common prefix */
                    char *common = strdup(lc.cvec[0]);
                    for (size_t i = 1; i < lc.len && common[0]; i++) {
                        size_t j = 0;
                        while (common[j] && lc.cvec[i][j] &&
                               tolower((unsigned char)common[j]) == tolower((unsigned char)lc.cvec[i][j])) {
                            j++;
                        }
                        common[j] = '\0';
                    }
                    size_t oldLen = l->len;
                    strncpy(l->buf, common, l->buflen - 1);
                    l->buf[l->buflen - 1] = '\0';
                    l->len = strlen(l->buf);
                    /* If common prefix extended the text, update position */
                    if (l->len > oldLen) {
                        l->pos = l->len;
                    }
                    free(common);
                    refreshLine(l);

                    /* Show all completions */
                    printf("\r\n");
                    for (size_t i = 0; i < lc.len; i++) {
                        printf("  %s\n", lc.cvec[i]);
                    }
                    fflush(stdout);
                    refreshLine(l);
                }
                freeCompletions(&lc);
            }
            break;
        }
        return NULL; /* continue editing */
    }

    /* Regular character */
    char c = seq[0];

    if (c == 0x01) { /* Ctrl-A -> Home */
        l->pos = 0; refreshLine(l);
    } else if (c == 0x05) { /* Ctrl-E -> End */
        l->pos = l->len; refreshLine(l);
    } else if (c == 0x02) { /* Ctrl-B -> Left */
        if (l->pos > 0) {
            size_t cplen;
            utf8DecodePrev(l->buf, l->pos, &cplen);
            l->pos -= cplen;
            refreshLine(l);
        }
    } else if (c == 0x06) { /* Ctrl-F -> Right */
        if (l->pos < l->len) {
            size_t cplen;
            utf8DecodeChar(l->buf + l->pos, &cplen);
            l->pos += cplen;
            refreshLine(l);
        }
    } else if (c == 0x08 || c == 0x7F) { /* Ctrl-H / Backspace */
        if (l->len > 0 && l->pos > 0) {
            size_t cplen;
            utf8DecodePrev(l->buf, l->pos, &cplen);
            memmove(l->buf + l->pos - cplen, l->buf + l->pos, l->len - l->pos + 1);
            l->pos -= cplen;
            l->len -= cplen;
            l->buf[l->len] = '\0';
            refreshLine(l);
        }
    } else if (c == 0x15) { /* Ctrl-U: clear whole line */
        l->buf[0] = '\0';
        l->len = l->pos = 0;
        refreshLine(l);
    } else if (c == 0x0B) { /* Ctrl-K: clear from cursor to end */
        l->buf[l->pos] = '\0';
        l->len = l->pos;
        refreshLine(l);
    } else if (c == '\0') {
        /* Ignore null bytes */
    } else if (isprint((unsigned char)c)) {
        editInsert(l, &c, 1);
        refreshLine(l);
    }

    return NULL; /* continue editing */
}

void linenoiseEditStop(struct linenoiseState *l) {
    disableRawMode(l->ifd);
    DWORD written;
    HANDLE hOut = getOutputHandle();
    WriteFile(hOut, "\n", 1, &written, NULL);
}

void linenoiseHide(struct linenoiseState *l) {
    printf("\r");
    clearLineEnd();
    fflush(stdout);
}

void linenoiseShow(struct linenoiseState *l) {
    refreshLine(l);
}

void linenoiseSetMultiLine(int ml) {
    mlmode = ml;
}

void linenoisePrintKeyCodes(void) {
    printf(" linenoise key codes: use arrow keys, Ctrl+C to exit\n");
}

void linenoiseMaskModeEnable(void) { }
void linenoiseMaskModeDisable(void) { }

/* ========================= Blocking linenoise ============================= */

static char *linenoiseNoTTY(void) {
    return NULL;
}

char *linenoise(const char *prompt) {
    size_t buflen = LINENOISE_MAX_LINE;
    char *buf = (char *)calloc(1, buflen);
    if (!buf) return NULL;

    struct linenoiseState l;
    int stdin_fd = 0, stdout_fd = 1;

    if (isUnsupportedTerm()) {
        size_t len;
        printf("%s", prompt);
        fflush(stdout);
        if (!fgets(buf, (int)buflen, stdin)) { free(buf); return NULL; }
        len = strlen(buf);
        while (len && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
            len--;
            buf[len] = '\0';
        }
        return buf;
    }

    if (linenoiseEditStart(&l, stdin_fd, stdout_fd, buf, buflen, prompt) == -1) {
        free(buf);
        return NULL;
    }

    char *result = NULL;
    while (1) {
        result = linenoiseEditFeed(&l);
        if (result) break; /* Enter or EOF */
    }

    /* linenoiseEditFeed returns strdup(l->buf) on Enter — free the original buffer */
    free(buf);

    return result;
}

void linenoiseFree(void *ptr) {
    free(ptr);
}
