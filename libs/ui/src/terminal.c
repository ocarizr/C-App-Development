#include "ui_internal.h"

#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#ifndef _WIN32
static volatile sig_atomic_t stopped;

static void stop_signal(int number)
{
    (void)number;
    stopped = 1;
}

static int byte_ready(int timeout)
{
    struct pollfd descriptor = {STDIN_FILENO, POLLIN, 0};
    return poll(&descriptor, 1, timeout) > 0;
}
#endif
enum
{
    KEY_NEXT = 0x110000,
    KEY_PREVIOUS,
    KEY_PAGE_DOWN,
    KEY_PAGE_UP,
    KEY_TAB_PREVIOUS,
    KEY_TAB_NEXT,
    KEY_LEFT,
    KEY_RIGHT
};

static int read_key(void)
{
#ifdef _WIN32
    if (!_kbhit())
    {
        Sleep(100);
        return -1;
    }

    int key = _getwch();
    if (key == 0 || key == 224)
    {
        switch (_getwch())
        {
        case 15:
            return KEY_PREVIOUS;
        case 72:
            return KEY_PREVIOUS;
        case 80:
            return KEY_NEXT;
        case 75:
            return KEY_LEFT;
        case 77:
            return KEY_RIGHT;
        case 59:
            return KEY_TAB_PREVIOUS;
        case 60:
            return KEY_TAB_NEXT;
        case 73:
            return KEY_PAGE_UP;
        case 81:
            return KEY_PAGE_DOWN;
        default:
            return -1;
        }
    }

    return key;
#else
    unsigned char key;
    if (!byte_ready(100))
    {
        return -1;
    }

    if (read(STDIN_FILENO, &key, 1) != 1)
    {
        return 3;
    }

    if (key == 27 && byte_ready(60))
    {
        char sequence[16] = {0};
        size_t length = 0;
        do
        {
            if (read(STDIN_FILENO, &sequence[length], 1) != 1)
            {
                break;
            }

            char c = sequence[length++];
            if (length > 1 && ((c >= 'A' && c <= 'Z') || c == '~'))
            {
                break;
            }
        } while (length + 1 < sizeof(sequence) && byte_ready(20));
        if (strcmp(sequence, "[Z") == 0 || strcmp(sequence, "[A") == 0)
        {
            return KEY_PREVIOUS;
        }

        if (strcmp(sequence, "[B") == 0)
        {
            return KEY_NEXT;
        }

        if (strcmp(sequence, "[D") == 0)
        {
            return KEY_LEFT;
        }

        if (strcmp(sequence, "[C") == 0)
        {
            return KEY_RIGHT;
        }

        if (strcmp(sequence, "OP") == 0 || strcmp(sequence, "[11~") == 0)
        {
            return KEY_TAB_PREVIOUS;
        }

        if (strcmp(sequence, "OQ") == 0 || strcmp(sequence, "[12~") == 0)
        {
            return KEY_TAB_NEXT;
        }

        if (strcmp(sequence, "[5~") == 0)
        {
            return KEY_PAGE_UP;
        }

        if (strcmp(sequence, "[6~") == 0)
        {
            return KEY_PAGE_DOWN;
        }

        return -1;
    }

    return key;
#endif
}

static void key_text(UiDocument *doc, int key)
{
    char text[5] = {0};
#ifdef _WIN32
    uint32_t c = (uint32_t)key;
    if (c >= 0xd800 && c <= 0xdfff)
    {
        return;
    }

    if (c < 128)
    {
        text[0] = (char)c;
    }
    else if (c < 2048)
    {
        text[0] = (char)(0xc0 | (c >> 6));
        text[1] = (char)(0x80 | (c & 63));
    }
    else
    {
        text[0] = (char)(0xe0 | (c >> 12));
        text[1] = (char)(0x80 | ((c >> 6) & 63));
        text[2] = (char)(0x80 | (c & 63));
    }
#else
    text[0] = (char)key;
    int continuation = key >= 0xc2 && key <= 0xdf   ? 1
                       : key >= 0xe0 && key <= 0xef ? 2
                       : key >= 0xf0 && key <= 0xf4 ? 3
                                                    : 0;
    if (key >= 128 && continuation == 0)
    {
        return;
    }

    for (int i = 1; i <= continuation; ++i)
    {
        if (!byte_ready(100) || read(STDIN_FILENO, &text[i], 1) != 1 ||
            ((unsigned char)text[i] & 0xc0) != 0x80)
        {
            return;
        }
    }
#endif
    (void)ui_text(doc, text);
}

int ui_run(UiDocument *doc)
{
    /* After entering raw/alternate-screen mode, all exit paths share restoration below. */
    int result = EXIT_SUCCESS;
    int last_width = -1, last_height = -1;
    size_t revision = 0;
    bool dirty = true;
#ifdef _WIN32
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE), output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD input_mode, output_mode;
    UINT code_page = GetConsoleOutputCP();
    if (doc == NULL || !GetConsoleMode(input, &input_mode) || !GetConsoleMode(output, &output_mode))
    {
        fputs("A UI requer um terminal. Para automacao use --script arquivo.\n", stderr);
        return EXIT_FAILURE;
    }

    if (!SetConsoleMode(output, output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) ||
        !SetConsoleMode(input, input_mode & ~(DWORD)ENABLE_PROCESSED_INPUT))
    {
        SetConsoleMode(output, output_mode);
        return EXIT_FAILURE;
    }

    SetConsoleOutputCP(CP_UTF8);
#else
    struct termios original, raw;
    struct sigaction action = {0}, old_int, old_term;
    if (doc == NULL || !isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO) ||
        tcgetattr(STDIN_FILENO, &original) != 0)
    {
        fputs("A UI requer um terminal. Para automacao use --script arquivo.\n", stderr);
        return EXIT_FAILURE;
    }

    raw = original;
    raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO | ISIG);
    raw.c_iflag &= (tcflag_t) ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
    {
        return EXIT_FAILURE;
    }

    stopped = 0;
    action.sa_handler = stop_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, &old_int) != 0)
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
        return EXIT_FAILURE;
    }

    if (sigaction(SIGTERM, &action, &old_term) != 0)
    {
        sigaction(SIGINT, &old_int, NULL);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
        return EXIT_FAILURE;
    }
#endif
    fputs("\033[?1049h\033[?25l\033[2J", stdout);
    while (!doc->quit)
    {
        int width = 100, height = 30;
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(output, &info))
        {
            width = info.srWindow.Right - info.srWindow.Left + 1;
            height = info.srWindow.Bottom - info.srWindow.Top + 1;
        }
#else
        struct winsize size;
        if (stopped)
        {
            break;
        }

        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0 && size.ws_row > 0)
        {
            width = size.ws_col;
            height = size.ws_row;
        }
#endif
        width = width > 300 ? 300 : width < 10 ? 10 : width;
        height = height > 100 ? 100 : height < 4 ? 4 : height;
        if (dirty || width != last_width || height != last_height ||
            revision != mvvm_revision(doc->model))
        {
            if (!ui_render(doc, stdout, width, height, false))
            {
                result = EXIT_FAILURE;
                break;
            }

            dirty = false;
            last_width = width;
            last_height = height;
            revision = mvvm_revision(doc->model);
        }

        int key = read_key();
        if (key == -1)
        {
            continue;
        }

        dirty = true;
        if (key == 27 || key == 3 || key == 4)
        {
            break;
        }

        switch (key)
        {
        case 9:
        case KEY_NEXT:
            ui_key(doc, UI_NEXT);
            break;
        case KEY_PREVIOUS:
            ui_key(doc, UI_PREVIOUS);
            break;
        case 13:
        case 10:
            ui_key(doc, UI_ACTIVATE);
            break;
        case 8:
        case 127:
            ui_key(doc, UI_BACKSPACE);
            break;
        case 21:
            ui_key(doc, UI_CLEAR);
            break;
        case KEY_PAGE_DOWN:
            ui_key(doc, UI_PAGE_DOWN);
            break;
        case KEY_PAGE_UP:
            ui_key(doc, UI_PAGE_UP);
            break;
        case KEY_TAB_PREVIOUS:
            ui_key(doc, UI_TAB_PREVIOUS);
            break;
        case KEY_TAB_NEXT:
            ui_key(doc, UI_TAB_NEXT);
            break;
        case KEY_LEFT:
            ui_key(doc, UI_LEFT);
            break;
        case KEY_RIGHT:
            ui_key(doc, UI_RIGHT);
            break;
        default:
            if (key >= 32 && key < 0x110000)
            {
                key_text(doc, key);
            }

            break;
        }
    }

    fputs("\033[0m\033[?25h\033[?1049l", stdout);
    fflush(stdout);
#ifdef _WIN32
    SetConsoleMode(input, input_mode);
    SetConsoleMode(output, output_mode);
    SetConsoleOutputCP(code_page);
#else
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
    sigaction(SIGINT, &old_int, NULL);
    sigaction(SIGTERM, &old_term, NULL);
#endif
    return result;
}
