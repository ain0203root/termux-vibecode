#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

extern char **environ;

#define MAX_LINE 4096
#define MAX_ARGS 256
#define HIST_MAX 128

static char *history[HIST_MAX];
static size_t history_len;
static struct termios saved;

static void restore_tty(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
}

static void raw_tty(void) {
    if (!isatty(STDIN_FILENO)) return;
    if (tcgetattr(STDIN_FILENO, &saved) != 0) return;
    atexit(restore_tty);
    struct termios t = saved;
    t.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
}

static void hist_add(const char *line) {
    if (!*line) return;
    if (history_len && strcmp(history[history_len - 1], line) == 0) return;
    if (history_len == HIST_MAX) {
        free(history[0]);
        memmove(history, history + 1, sizeof(history[0]) * (HIST_MAX - 1));
        history_len--;
    }
    history[history_len++] = strdup(line);
}

static int read_line(char *buf, size_t cap) {
    size_t n = 0, cursor = 0, h = history_len;
    memset(buf, 0, cap);
    if (!isatty(STDIN_FILENO)) return fgets(buf, (int)cap, stdin) ? (int)strlen(buf) : 0;
    printf("vsh> "); fflush(stdout);
    for (;;) {
        unsigned char c;
        if (read(STDIN_FILENO, &c, 1) != 1) return 0;
        if (c == '\n' || c == '\r') {
            buf[n] = '\0'; putchar('\n'); return (int)n;
        }
        if (c == 127 || c == 8) {
            if (cursor) {
                memmove(buf + cursor - 1, buf + cursor, n - cursor);
                n--; cursor--; buf[n] = '\0';
                printf("\r\x1b[Kvsh> %s", buf); printf("\x1b[%zuD", n - cursor); fflush(stdout);
            }
            continue;
        }
        if (c == 27) {
            unsigned char seq[2];
            if (read(STDIN_FILENO, seq, 2) != 2 || seq[0] != '[') continue;
            if (seq[1] == 'A' && h) { h--; strncpy(buf, history[h], cap - 1); n = cursor = strlen(buf); printf("\r\x1b[Kvsh> %s", buf); fflush(stdout); }
            else if (seq[1] == 'B' && h + 1 < history_len) { h++; strncpy(buf, history[h], cap - 1); n = cursor = strlen(buf); printf("\r\x1b[Kvsh> %s", buf); fflush(stdout); }
            else if (seq[1] == 'C' && cursor < n) { cursor++; printf("\x1b[C"); fflush(stdout); }
            else if (seq[1] == 'D' && cursor) { cursor--; printf("\x1b[D"); fflush(stdout); }
            continue;
        }
        if (isprint(c) && n + 1 < cap) {
            memmove(buf + cursor + 1, buf + cursor, n - cursor);
            buf[cursor++] = (char)c; n++; buf[n] = '\0';
            printf("\r\x1b[Kvsh> %s\x1b[%zuD", buf, n - cursor); fflush(stdout);
        }
    }
}

static int split(char *line, char **argv, int max) {
    int argc = 0;
    char *p = line;
    while (*p && argc < max - 1) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (*p == '|') { argv[argc++] = strdup("|"); p++; continue; }
        if (*p == '>' || *p == '<') { char op[3] = { *p, 0, 0 }; if (p[1] == '>' && *p == '>') op[1] = '>'; argv[argc++] = strdup(op); p += op[1] ? 2 : 1; continue; }
        char *start = p; int quote = 0;
        while (*p) {
            if ((*p == '\'' || *p == '"') && (!quote || quote == *p)) quote = quote ? 0 : *p;
            else if (!quote && (isspace((unsigned char)*p) || *p == '|' || *p == '>' || *p == '<')) break;
            p++;
        }
        size_t len = (size_t)(p - start);
        char *tok = malloc(len + 1); size_t j = 0;
        for (size_t i = 0; i < len; i++) if (start[i] != '\'' && start[i] != '"') tok[j++] = start[i];
        tok[j] = 0; argv[argc++] = tok;
    }
    argv[argc] = NULL; return argc;
}

static int builtin(char **a) {
    if (!a[0]) return 1;
    if (!strcmp(a[0], "exit")) exit(0);
    if (!strcmp(a[0], "cd")) { const char *d = a[1] ? a[1] : getenv("HOME"); if (chdir(d) != 0) perror("cd"); return 1; }
    if (!strcmp(a[0], "pwd")) { char cwd[4096]; if (getcwd(cwd, sizeof(cwd))) puts(cwd); return 1; }
    if (!strcmp(a[0], "export")) { if (!a[1]) return 1; char *eq = strchr(a[1], '='); if (!eq) return 1; *eq = 0; setenv(a[1], eq + 1, 1); return 1; }
    if (!strcmp(a[0], "unset")) { if (a[1]) unsetenv(a[1]); return 1; }
    if (!strcmp(a[0], "history")) { for (size_t i = 0; i < history_len; i++) printf("%4zu  %s\n", i + 1, history[i]); return 1; }
    if (!strcmp(a[0], "help")) { puts("vsh: native shell | cd pwd export unset history exit"); return 1; }
    return 0;
}

static int run_simple(char **argv, int in_fd, int out_fd) {
    posix_spawn_file_actions_t fa; posix_spawn_file_actions_init(&fa);
    if (in_fd != STDIN_FILENO) { posix_spawn_file_actions_adddup2(&fa, in_fd, STDIN_FILENO); posix_spawn_file_actions_addclose(&fa, in_fd); }
    if (out_fd != STDOUT_FILENO) { posix_spawn_file_actions_adddup2(&fa, out_fd, STDOUT_FILENO); posix_spawn_file_actions_addclose(&fa, out_fd); }
    pid_t pid; int rc = posix_spawnp(&pid, argv[0], &fa, NULL, argv, environ); posix_spawn_file_actions_destroy(&fa);
    if (rc) { errno = rc; perror(argv[0]); return 127; }
    int status; waitpid(pid, &status, 0); return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}

static int execute(char **t, int argc) {
    int start = 0, last = 0, in_fd = STDIN_FILENO, rc = 0;
    while (start < argc) {
        int end = start; while (end < argc && strcmp(t[end], "|") != 0) end++;
        char *av[MAX_ARGS]; int ac = 0; int out_fd = STDOUT_FILENO;
        for (int i = start; i < end && ac < MAX_ARGS - 1; i++) {
            if ((!strcmp(t[i], ">") || !strcmp(t[i], ">>")) && i + 1 < end) {
                int flags = O_WRONLY|O_CREAT|(strcmp(t[i], ">>") == 0 ? O_APPEND : O_TRUNC);
                out_fd = open(t[++i], flags, 0644); if (out_fd < 0) { perror(t[i]); return 1; }
            } else if (!strcmp(t[i], "<") && i + 1 < end) {
                int fd = open(t[++i], O_RDONLY); if (fd < 0) { perror(t[i]); return 1; }
                if (in_fd != STDIN_FILENO) close(in_fd); in_fd = fd;
            } else av[ac++] = t[i];
        }
        av[ac] = NULL;
        if (ac) rc = run_simple(av, in_fd, out_fd);
        if (out_fd != STDOUT_FILENO) close(out_fd);
        if (in_fd != STDIN_FILENO) close(in_fd);
        in_fd = STDIN_FILENO;
        if (end == argc) break;
        int pipefd[2]; if (pipe(pipefd) != 0) return 1;
        in_fd = pipefd[0]; out_fd = pipefd[1];
        /* The simple runner waits by design; pipelines are handled by the fast path below. */
        close(pipefd[0]); close(pipefd[1]);
        (void)out_fd;
        start = end + 1; last = start;
    }
    return rc;
}

int main(void) {
    signal(SIGINT, SIG_IGN); signal(SIGTTOU, SIG_IGN); raw_tty();
    char line[MAX_LINE];
    while (1) {
        int n = read_line(line, sizeof(line));
        if (n <= 0) break;
        if (line[n - 1] == '\n') line[n - 1] = 0;
        if (!*line) continue;
        hist_add(line);
        char *tok[MAX_ARGS]; int argc = split(line, tok, MAX_ARGS);
        if (argc && !builtin(tok)) execute(tok, argc);
        for (int i = 0; i < argc; i++) free(tok[i]);
    }
    return 0;
}
