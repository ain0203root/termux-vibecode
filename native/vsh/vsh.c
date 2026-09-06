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

typedef struct {
    char *text;
    int quoted;
} Token;

static char *hist[HIST_MAX];
static size_t hist_n;
static size_t hist_pos;
static struct termios saved_tty;
static int tty_ready;
static int last_status;

static void die_oom(void) {
    fputs("vsh: out of memory\n", stderr);
    exit(1);
}

static void restore_tty(void) {
    if (tty_ready) tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty);
}

static void cleanup_history(void) {
    for (size_t i = 0; i < hist_n; i++) free(hist[i]);
}

static void setup_tty(void) {
    if (!isatty(STDIN_FILENO)) return;
    if (tcgetattr(STDIN_FILENO, &saved_tty) != 0) return;
    tty_ready = 1;
    atexit(restore_tty);
    atexit(cleanup_history);
}

static void add_history(const char *s) {
    if (!*s || (hist_n && strcmp(hist[hist_n - 1], s) == 0)) {
        hist_pos = hist_n;
        return;
    }
    if (hist_n == HIST_MAX) {
        free(hist[0]);
        memmove(hist, hist + 1, sizeof(hist[0]) * (HIST_MAX - 1));
        hist_n--;
    }
    hist[hist_n] = strdup(s);
    if (!hist[hist_n]) die_oom();
    hist_n++;
    hist_pos = hist_n;
}

static void redraw_line(const char *buf, size_t n, size_t cursor) {
    printf("\r\x1b[Kvsh> %s\x1b[%zuD", buf, n - cursor);
    fflush(stdout);
}

static int read_line(char *buf, size_t cap) {
    if (!isatty(STDIN_FILENO) || !tty_ready) {
        return fgets(buf, (int)cap, stdin) ? 0 : -1;
    }

    printf("vsh> ");
    fflush(stdout);
    struct termios t = saved_tty;
    t.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &t) != 0) return -1;

    size_t n = 0;
    size_t cursor = 0;
    hist_pos = hist_n;
    memset(buf, 0, cap);

    for (;;) {
        unsigned char c = 0;
        ssize_t got = read(STDIN_FILENO, &c, 1);
        if (got != 1) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty);
            return -1;
        }
        if (c == '\n' || c == '\r') {
            buf[n] = 0;
            putchar('\n');
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty);
            return (int)n;
        }
        if (c == 3) {
            puts("^C");
            buf[0] = 0;
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty);
            return 0;
        }
        if (c == 4) {
            if (n == 0) {
                putchar('^D');
                fflush(stdout);
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty);
                return -1;
            }
            continue;
        }
        if (c == 127 || c == 8) {
            if (cursor > 0) {
                memmove(buf + cursor - 1, buf + cursor, n - cursor);
                n--;
                cursor--;
                buf[n] = 0;
                redraw_line(buf, n, cursor);
            }
            continue;
        }
        if (c == 27) {
            unsigned char seq[2] = {0, 0};
            if (read(STDIN_FILENO, seq, 2) == 2 && seq[0] == '[') {
                if (seq[1] == 'C' && cursor < n) {
                    cursor++;
                    redraw_line(buf, n, cursor);
                } else if (seq[1] == 'D' && cursor > 0) {
                    cursor--;
                    redraw_line(buf, n, cursor);
                } else if (seq[1] == 'A' && hist_n > 0 && hist_pos > 0) {
                    hist_pos--;
                    strncpy(buf, hist[hist_pos], cap - 1);
                    buf[cap - 1] = 0;
                    n = strlen(buf);
                    cursor = n;
                    redraw_line(buf, n, cursor);
                } else if (seq[1] == 'B' && hist_n > 0) {
                    if (hist_pos + 1 < hist_n) {
                        hist_pos++;
                        strncpy(buf, hist[hist_pos], cap - 1);
                        buf[cap - 1] = 0;
                    } else {
                        hist_pos = hist_n;
                        buf[0] = 0;
                    }
                    n = strlen(buf);
                    cursor = n;
                    redraw_line(buf, n, cursor);
                }
            }
            continue;
        }
        if (isprint(c) && n + 1 < cap) {
            memmove(buf + cursor + 1, buf + cursor, n - cursor);
            buf[cursor++] = (char)c;
            n++;
            buf[n] = 0;
            redraw_line(buf, n, cursor);
        }
    }
}

static void append_bytes(char **dst, size_t *n, size_t *cap, const char *src, size_t len) {
    while (*n + len + 1 > *cap) {
        *cap *= 2;
        char *grown = realloc(*dst, *cap);
        if (!grown) {
            free(*dst);
            die_oom();
        }
        *dst = grown;
    }
    memcpy(*dst + *n, src, len);
    *n += len;
    (*dst)[*n] = 0;
}

static int is_var_start(unsigned char c) {
    return isalpha(c) || c == '_';
}

static int is_var_char(unsigned char c) {
    return isalnum(c) || c == '_';
}

static void append_variable(char **dst, size_t *n, size_t *cap, const char *name, size_t len) {
    char variable[256];
    if (len >= sizeof(variable)) return;
    memcpy(variable, name, len);
    variable[len] = 0;
    const char *value = getenv(variable);
    if (!value) value = "";
    append_bytes(dst, n, cap, value, strlen(value));
}

static void append_expansion(char **dst, size_t *n, size_t *cap, const char *src, size_t *idx) {
    size_t i = *idx;
    if (src[i] != '$') return;
    if (src[i + 1] == '$') {
        char pid[32];
        snprintf(pid, sizeof(pid), "%ld", (long)getpid());
        append_bytes(dst, n, cap, pid, strlen(pid));
        *idx = i + 2;
        return;
    }
    if (src[i + 1] == '?') {
        char status[32];
        snprintf(status, sizeof(status), "%d", last_status);
        append_bytes(dst, n, cap, status, strlen(status));
        *idx = i + 2;
        return;
    }
    if (src[i + 1] == '{') {
        size_t start = i + 2;
        size_t end = start;
        while (is_var_char((unsigned char)src[end])) end++;
        if (src[end] == '}') {
            append_variable(dst, n, cap, src + start, end - start);
            *idx = end + 1;
            return;
        }
    } else if (is_var_start((unsigned char)src[i + 1])) {
        size_t start = i + 1;
        size_t end = start;
        while (is_var_char((unsigned char)src[end])) end++;
        append_variable(dst, n, cap, src + start, end - start);
        *idx = end;
        return;
    }
    append_bytes(dst, n, cap, "$", 1);
    *idx = i + 1;
}

static char *decode_word(const char *src, int *quoted) {
    size_t cap = strlen(src) + 32;
    size_t n = 0;
    char *out = malloc(cap);
    if (!out) die_oom();
    int had_quote = 0;
    char quote = 0;

    for (size_t i = 0; src[i]; ) {
        unsigned char c = (unsigned char)src[i];
        if (quote == '\'') {
            had_quote = 1;
            if (c == '\'') {
                quote = 0;
                i++;
                continue;
            }
            append_bytes(&out, &n, &cap, (const char *)&c, 1);
            i++;
            continue;
        }
        if (quote == '"') {
            had_quote = 1;
            if (c == '"') {
                quote = 0;
                i++;
                continue;
            }
            if (c == '\\' && src[i + 1]) {
                append_bytes(&out, &n, &cap, src + i + 1, 1);
                i += 2;
                continue;
            }
            if (c == '$') {
                append_expansion(&out, &n, &cap, src, &i);
                continue;
            }
            append_bytes(&out, &n, &cap, (const char *)&c, 1);
            i++;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = (char)c;
            had_quote = 1;
            i++;
            continue;
        }
        if (c == '\\' && src[i + 1]) {
            append_bytes(&out, &n, &cap, src + i + 1, 1);
            i += 2;
            continue;
        }
        if (c == '$') {
            append_expansion(&out, &n, &cap, src, &i);
            continue;
        }
        append_bytes(&out, &n, &cap, (const char *)&c, 1);
        i++;
    }
    if (quote != 0) {
        free(out);
        fprintf(stderr, "vsh: unmatched quote\n");
        return NULL;
    }
    if (quoted) *quoted = had_quote;
    return out;
}

static int tokenize(char *s, Token *tok) {
    int n = 0;
    size_t i = 0;
    while (s[i] && n < MAX_TOK - 1) {
        while (isspace((unsigned char)s[i])) i++;
        if (!s[i]) break;
        if (strchr("|<>", s[i])) {
            size_t start = i;
            if (s[i] == '>' && s[i + 1] == '>') i += 2;
            else i++;
            size_t len = i - start;
            char raw[3] = {0, 0, 0};
            memcpy(raw, s + start, len);
            tok[n].text = strdup(raw);
            tok[n].quoted = 0;
            if (!tok[n].text) die_oom();
            n++;
            continue;
        }
        size_t start = i;
        char quote = 0;
        while (s[i]) {
            unsigned char c = (unsigned char)s[i];
            if (quote == '\'') {
                if (c == '\'') quote = 0;
                i++;
                continue;
            }
            if (quote == '"') {
                if (c == '"') quote = 0;
                else if (c == '\\' && s[i + 1]) i++;
                i++;
                continue;
            }
            if (c == '\'' || c == '"') {
                quote = (char)c;
                i++;
                continue;
            }
            if (c == '\\' && s[i + 1]) {
                i += 2;
                continue;
            }
            if (isspace(c) || strchr("|<>", c)) break;
            i++;
        }
        if (quote != 0) {
            fprintf(stderr, "vsh: unmatched quote\n");
            return -1;
        }
        size_t len = i - start;
        char *raw = malloc(len + 1);
        if (!raw) die_oom();
        memcpy(raw, s + start, len);
        raw[len] = 0;
        tok[n].quoted = 0;
        tok[n].text = decode_word(raw, &tok[n].quoted);
        free(raw);
        if (!tok[n].text) return -1;
        n++;
    }
    tok[n].text = NULL;
    return n;
}

static void free_tokens(Token *tok, int ntok) {
    for (int i = 0; i < ntok; i++) free(tok[i].text);
}

static int valid_name(const char *s) {
    if (!s || !*s || !(isalpha((unsigned char)s[0]) || s[0] == '_')) return 0;
    for (size_t i = 1; s[i]; i++) {
        if (!is_var_char((unsigned char)s[i])) return 0;
    }
    return 1;
}

static int find_path(const char *name, char *out, size_t cap) {
    if (strchr(name, '/')) {
        if (access(name, X_OK) == 0) {
            snprintf(out, cap, "%s", name);
            return 1;
        }
        return 0;
    }
    const char *path = getenv("PATH");
    if (!path) return 0;
    char *copy = strdup(path);
    if (!copy) return 0;
    char *save = NULL;
    for (char *part = strtok_r(copy, ":", &save); part; part = strtok_r(NULL, ":", &save)) {
        char candidate[4096];
        int written = snprintf(candidate, sizeof(candidate), "%s/%s", *part ? part : ".", name);
        if (written > 0 && (size_t)written < sizeof(candidate) && access(candidate, X_OK) == 0) {
            snprintf(out, cap, "%s", candidate);
            free(copy);
            return 1;
        }
    }
    free(copy);
    return 0;
}

static int builtin(char **argv) {
    if (!argv[0]) return 0;
    if (!strcmp(argv[0], "exit")) {
        int code = argv[1] ? atoi(argv[1]) : last_status;
        exit(code & 255);
    }
    if (!strcmp(argv[0], "cd")) {
        const char *dir = argv[1] ? argv[1] : getenv("HOME");
        if (!dir) dir = ".";
        if (chdir(dir) != 0) {
            perror("cd");
            return 1;
        }
        return 0;
    }
    if (!strcmp(argv[0], "pwd")) {
        char cwd[4096];
        if (!getcwd(cwd, sizeof(cwd))) {
            perror("pwd");
            return 1;
        }
        puts(cwd);
        return 0;
    }
    if (!strcmp(argv[0], "echo")) {
        for (int i = 1; argv[i]; i++) {
            if (i > 1) putchar(' ');
            fputs(argv[i], stdout);
        }
        putchar('\n');
        return 0;
    }
    if (!strcmp(argv[0], "true")) return 0;
    if (!strcmp(argv[0], "false")) return 1;
    if (!strcmp(argv[0], ":")) return 0;
    if (!strcmp(argv[0], "export")) {
        int rc = 0;
        for (int i = 1; argv[i]; i++) {
            char *eq = strchr(argv[i], '=');
            if (!eq) {
                const char *value = getenv(argv[i]);
                if (valid_name(argv[i]) && value) {
                    if (setenv(argv[i], value, 1) != 0) rc = 1;
                } else if (!valid_name(argv[i])) {
                    fprintf(stderr, "vsh: export: invalid name: %s\n", argv[i]);
                    rc = 1;
                }
                continue;
            }
            *eq = 0;
            if (!valid_name(argv[i]) || setenv(argv[i], eq + 1, 1) != 0) {
                fprintf(stderr, "vsh: export: invalid assignment\n");
                rc = 1;
            }
            *eq = '=';
        }
        return rc;
    }
    if (!strcmp(argv[0], "unset")) {
        int rc = 0;
        for (int i = 1; argv[i]; i++) {
            if (!valid_name(argv[i]) || unsetenv(argv[i]) != 0) rc = 1;
        }
        return rc;
    }
    if (!strcmp(argv[0], "history")) {
        for (size_t i = 0; i < hist_n; i++) printf("%4zu  %s\n", i + 1, hist[i]);
        return 0;
    }
    if (!strcmp(argv[0], "which")) {
        if (!argv[1]) return 1;
        char resolved[4096];
        if (!find_path(argv[1], resolved, sizeof(resolved))) return 1;
        puts(resolved);
        return 0;
    }
    if (!strcmp(argv[0], "help")) {
        puts("vsh: cd pwd echo export unset history which true false : exit help");
        return 0;
    }
    return -1;
}

static int apply_redirection(char **argv, int *argc) {
    int out_argc = 0;
    for (int i = 0; i < *argc; i++) {
        if ((!strcmp(argv[i], ">") || !strcmp(argv[i], ">>")) && i + 1 < *argc) {
            int flags = O_WRONLY | O_CREAT | (!strcmp(argv[i], ">>") ? O_APPEND : O_TRUNC);
            int fd = open(argv[i + 1], flags, 0644);
            if (fd < 0) {
                perror(argv[i + 1]);
                return 1;
            }
            if (dup2(fd, STDOUT_FILENO) < 0) {
                perror("dup2");
                close(fd);
                return 1;
            }
            close(fd);
            i++;
            continue;
        }
        if (!strcmp(argv[i], "<") && i + 1 < *argc) {
            int fd = open(argv[i + 1], O_RDONLY);
            if (fd < 0) {
                perror(argv[i + 1]);
                return 1;
            }
            if (dup2(fd, STDIN_FILENO) < 0) {
                perror("dup2");
                close(fd);
                return 1;
            }
            close(fd);
            i++;
            continue;
        }
        argv[out_argc++] = argv[i];
    }
    argv[out_argc] = NULL;
    *argc = out_argc;
    if (out_argc == 0) {
        fprintf(stderr, "vsh: redirection without command\n");
        return 1;
    }
    return 0;
}

static int execute_tokens(Token *tok, int ntok) {
    if (ntok == 0) return 0;

    char **cmdv[MAX_TOK];
    int cmdc = 0;
    cmdv[0] = &tok[0].text;
    for (int i = 0; i < ntok; i++) {
        if (!strcmp(tok[i].text, "|")) {
            tok[i].text = NULL;
            if (i + 1 >= ntok || !tok[i + 1].text || !strcmp(tok[i + 1].text, "|")) {
                fprintf(stderr, "vsh: invalid pipeline\n");
                return 2;
            }
            cmdv[++cmdc] = &tok[i + 1].text;
        }
    }
    int ncmd = cmdc + 1;
    pid_t pids[MAX_TOK];
    int pipes[MAX_TOK][2];
    for (int i = 0; i < ncmd - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            return 1;
        }
    }

    if (ncmd == 1) {
        char *argv[MAX_TOK];
        int argc = 0;
        for (int i = 0; i < ntok; i++) {
            if (!tok[i].text) break;
            argv[argc++] = tok[i].text;
        }
        argv[argc] = NULL;
        int builtin_rc = builtin(argv);
        if (builtin_rc >= 0) {
            for (int i = 0; i < argc; i++) {
                if (!strcmp(argv[i], "export") && argv[i + 1]) {
                    /* restore any temporary '=' mutation performed by builtin; tokens remain owned by caller */
                }
            }
            return builtin_rc;
        }
    }

    for (int c = 0; c < ncmd; c++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            if (c > 0 && dup2(pipes[c - 1][0], STDIN_FILENO) < 0) _exit(1);
            if (c < ncmd - 1 && dup2(pipes[c][1], STDOUT_FILENO) < 0) _exit(1);
            for (int j = 0; j < ncmd - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            char *argv[MAX_TOK];
            int argc = 0;
            char *start = cmdv[c][0];
            (void)start;
            for (char **p = cmdv[c]; p < &tok[ntok].text && *p; p++) {
                if (!strcmp(*p, "|")) break;
                argv[argc++] = *p;
            }
            argv[argc] = NULL;
            if (apply_redirection(argv, &argc) != 0) _exit(1);

            int rc = builtin(argv);
            if (rc >= 0) _exit(rc & 255);
            execvp(argv[0], argv);
            int code = (errno == ENOENT) ? 127 : 126;
            fprintf(stderr, "vsh: %s: %s\n", argv[0], strerror(errno));
            _exit(code);
        }
        pids[c] = pid;
    }
    for (int i = 0; i < ncmd - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    int status = 0;
    for (int i = 0; i < ncmd; i++) {
        int st = 0;
        if (waitpid(pids[i], &st, 0) < 0) continue;
        if (i == ncmd - 1) {
            if (WIFEXITED(st)) status = WEXITSTATUS(st);
            else if (WIFSIGNALED(st)) status = 128 + WTERMSIG(st);
            else status = 1;
        }
    }
    return status;
}

int main(void) {
    setup_tty();
    signal(SIGINT, SIG_IGN);

    char line[MAX_LINE];
    while (read_line(line, sizeof(line)) >= 0) {
        size_t len = strlen(line);
        if (len && line[len - 1] == '\n') line[len - 1] = 0;
        if (!*line) continue;
        add_history(line);

        Token tok[MAX_TOK];
        memset(tok, 0, sizeof(tok));
        int ntok = tokenize(line, tok);
        if (ntok < 0) {
            last_status = 2;
        } else if (ntok > 0) {
            last_status = execute_tokens(tok, ntok);
        }
        free_tokens(tok, ntok > 0 ? ntok : 0);
    }
    return last_status;
}
