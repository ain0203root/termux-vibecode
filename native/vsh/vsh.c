#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define MAX_LINE 4096
#define MAX_TOK 256
#define HIST_MAX 128

static char *hist[HIST_MAX];
static size_t hist_n;
static struct termios saved;

static void restore_tty(void) { tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved); }
static void setup_tty(void) {
    if (!isatty(STDIN_FILENO)) return;
    if (tcgetattr(STDIN_FILENO, &saved) != 0) return;
    atexit(restore_tty);
}

static void add_history(const char *s) {
    if (!*s || (hist_n && strcmp(hist[hist_n - 1], s) == 0)) return;
    if (hist_n == HIST_MAX) { free(hist[0]); memmove(hist, hist + 1, sizeof(hist[0]) * (HIST_MAX - 1)); hist_n--; }
    hist[hist_n++] = strdup(s);
}

static int read_line(char *buf, size_t cap) {
    if (!isatty(STDIN_FILENO)) return fgets(buf, (int)cap, stdin) ? 0 : -1;
    printf("vsh> "); fflush(stdout);
    size_t n = 0, pos = 0, cursor = 0;
    memset(buf, 0, cap);
    struct termios t = saved;
    t.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    t.c_cc[VMIN] = 1; t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
    while (1) {
        unsigned char c;
        if (read(STDIN_FILENO, &c, 1) != 1) return -1;
        if (c == '\n' || c == '\r') { buf[n] = 0; putchar('\n'); tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved); return (int)n; }
        if (c == 3) { putchar('^'); putchar('C'); putchar('\n'); buf[0] = 0; tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved); return 0; }
        if (c == 127 || c == 8) {
            if (cursor) { memmove(buf + cursor - 1, buf + cursor, n - cursor); n--; cursor--; buf[n] = 0; printf("\r\x1b[Kvsh> %s\x1b[%zuD", buf, n - cursor); fflush(stdout); }
            continue;
        }
        if (c == 27) { unsigned char seq[2]; if (read(STDIN_FILENO, seq, 2) == 2 && seq[0] == '[') {
                if (seq[1] == 'C' && cursor < n) { cursor++; printf("\x1b[C"); }
                else if (seq[1] == 'D' && cursor) { cursor--; printf("\x1b[D"); }
                fflush(stdout);
            } continue; }
        if (isprint(c) && n + 1 < cap) { memmove(buf + cursor + 1, buf + cursor, n - cursor); buf[cursor++] = (char)c; n++; buf[n] = 0; printf("\r\x1b[Kvsh> %s\x1b[%zuD", buf, n - cursor); fflush(stdout); }
    }
}

static int tokenize(char *s, char **tok) {
    int n = 0; char *p = s;
    while (*p && n < MAX_TOK - 1) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (strchr("|<>", *p)) {
            if (*p == '>' && p[1] == '>') tok[n++] = strdup(">>"), p += 2;
            else { char x[2] = {*p, 0}; tok[n++] = strdup(x); p++; }
            continue;
        }
        char quote = 0; char tmp[MAX_LINE]; size_t j = 0;
        while (*p) {
            if ((*p == '\'' || *p == '"')) { if (!quote) quote = *p; else if (quote == *p) quote = 0; else tmp[j++] = *p; p++; continue; }
            if (!quote && (isspace((unsigned char)*p) || strchr("|<>", *p))) break;
            if (j + 1 < sizeof(tmp)) tmp[j++] = *p;
            p++;
        }
        tmp[j] = 0; tok[n++] = strdup(tmp);
    }
    tok[n] = NULL; return n;
}

static int builtin(char **a) {
    if (!a[0]) return 1;
    if (!strcmp(a[0], "exit")) exit(0);
    if (!strcmp(a[0], "cd")) { const char *d = a[1] ? a[1] : getenv("HOME"); if (chdir(d) < 0) perror("cd"); return 1; }
    if (!strcmp(a[0], "pwd")) { char b[4096]; if (getcwd(b, sizeof(b))) puts(b); return 1; }
    if (!strcmp(a[0], "export")) { if (a[1]) { char *eq = strchr(a[1], '='); if (eq) { *eq = 0; setenv(a[1], eq + 1, 1); } } return 1; }
    if (!strcmp(a[0], "unset")) { if (a[1]) unsetenv(a[1]); return 1; }
    if (!strcmp(a[0], "history")) { for (size_t i = 0; i < hist_n; i++) printf("%4zu  %s\n", i + 1, hist[i]); return 1; }
    if (!strcmp(a[0], "help")) { puts("vsh: cd pwd export unset history exit help"); return 1; }
    return 0;
}

static void wait_all(pid_t *pids, int n) { for (int i = 0; i < n; i++) waitpid(pids[i], NULL, 0); }

static int execute(char **tok, int ntok) {
    char **cmd[MAX_TOK]; int ncmd = 0; cmd[0] = tok;
    for (int i = 0; i < ntok; i++) if (!strcmp(tok[i], "|")) { tok[i] = NULL; cmd[++ncmd] = &tok[i + 1]; }
    ncmd++;
    pid_t pids[MAX_TOK]; int pipes[MAX_TOK][2];
    for (int i = 0; i < ncmd - 1; i++) if (pipe(pipes[i]) < 0) return 1;
    for (int c = 0; c < ncmd; c++) {
        pid_t pid = fork();
        if (pid < 0) return 1;
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            if (c) dup2(pipes[c - 1][0], STDIN_FILENO);
            if (c < ncmd - 1) dup2(pipes[c][1], STDOUT_FILENO);
            for (int j = 0; j < ncmd - 1; j++) { close(pipes[j][0]); close(pipes[j][1]); }
            char *a[MAX_TOK]; int na = 0;
            for (int i = 0; cmd[c][i] && na < MAX_TOK - 1; i++) {
                if ((!strcmp(cmd[c][i], ">") || !strcmp(cmd[c][i], ">>")) && cmd[c][i + 1]) {
                    int flags = O_WRONLY | O_CREAT | (!strcmp(cmd[c][i], ">>") ? O_APPEND : O_TRUNC);
                    int fd = open(cmd[c][++i], flags, 0644); if (fd < 0) { perror("open"); _exit(1); }
                    dup2(fd, STDOUT_FILENO); close(fd);
                } else if (!strcmp(cmd[c][i], "<") && cmd[c][i + 1]) {
                    int fd = open(cmd[c][++i], O_RDONLY); if (fd < 0) { perror("open"); _exit(1); }
                    dup2(fd, STDIN_FILENO); close(fd);
                } else a[na++] = cmd[c][i];
            }
            a[na] = NULL;
            execvp(a[0], a); perror(a[0]); _exit(127);
        }
        pids[c] = pid;
    }
    for (int i = 0; i < ncmd - 1; i++) { close(pipes[i][0]); close(pipes[i][1]); }
    wait_all(pids, ncmd);
    return 0;
}

int main(void) {
    setup_tty(); signal(SIGINT, SIG_IGN);
    char line[MAX_LINE];
    while (read_line(line, sizeof(line)) >= 0) {
        size_t n = strlen(line); if (n && line[n - 1] == '\n') line[n - 1] = 0;
        if (!*line) continue;
        add_history(line);
        char *tok[MAX_TOK]; int ntok = tokenize(line, tok);
        if (ntok == 0) continue;
        if (ntok > 0 && !strchr(tok[0], '/')) { char *tmp[MAX_TOK]; for (int i = 0; i < ntok; i++) tmp[i] = tok[i]; tmp[ntok] = NULL; if (ntok && !builtin(tmp)) execute(tok, ntok); }
        for (int i = 0; i < ntok; i++) free(tok[i]);
    }
    return 0;
}
