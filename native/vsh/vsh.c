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
    for (size_t i = 0; i < hist_n; ++i) free(hist[i]);
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
        --hist_n;
    }
    hist[hist_n] = strdup(s);
    if (!hist[hist_n]) die_oom();
    ++hist_n;
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
        if (read(STDIN_FILENO, &c, 1) != 1) {
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
                fputs("^D", stdout);
                fflush(stdout);
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty);
                return -1;
            }
            continue;
        }
        if (c == 127 || c == 8) {
            if (cursor > 0) {
                memmove(buf + cursor - 1, buf + cursor, n - cursor);
                --n;
                --cursor;
                buf[n] = 0;
                redraw_line(buf, n, cursor);
            }
            continue;
        }
        if (c == 27) {
            unsigned char seq[2] = {0, 0};
            if (read(STDIN_FILENO, seq, 2) == 2 && seq[0] == '[') {
                if (seq[1] == 'C' && cursor < n) {
                    ++cursor;
                    redraw_line(buf, n, cursor);
                } else if (seq[1] == 'D' && cursor > 0) {
                    --cursor;
                    redraw_line(buf, n, cursor);
                } else if (seq[1] == 'A' && hist_n > 0 && hist_pos > 0) {
                    --hist_pos;
                    strncpy(buf, hist[hist_pos], cap - 1);
                    buf[cap - 1] = 0;
                    n = strlen(buf);
                    cursor = n;
                    redraw_line(buf, n, cursor);
                } else if (seq[1] == 'B' && hist_n > 0) {
                    if (hist_pos + 1 < hist_n) {
                        ++hist_pos;
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
            ++n;
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

static int valid_var_start(unsigned char c) {
    return isalpha(c) || c == '_';
}

static int valid_var_char(unsigned char c) {
    return isalnum(c) || c == '_';
}

static void append_var(char **dst, size_t *length, size_t *cap, const char *name, size_t len) {
    char variable[256];
    if (len >= sizeof(variable)) return;
    memcpy(variable, name, len);
    variable[len] = 0;
    const char *value = getenv(variable);
    if (!value) value = "";
    append_bytes(dst, length, cap, value, strlen(value));
}

static void append_expansion(char **dst, size_t *length, size_t *cap, const char *src, size_t *pos) {
    size_t i = *pos;
    if (src[i] != '$') return;
    if (src[i + 1] == '$') {
        char pid[32];
        snprintf(pid, sizeof(pid), "%ld", (long)getpid());
        append_bytes(dst, length, cap, pid, strlen(pid));
        *pos = i + 2;
        return;
    }
    if (src[i + 1] == '?') {
        char rc[32];
        snprintf(rc, sizeof(rc), "%d", last_status);
        append_bytes(dst, length, cap, rc, strlen(rc));
        *pos = i + 2;
        return;
    }
    if (src[i + 1] == '{') {
        size_t start = i + 2;
        size_t end = start;
        while (valid_var_char((unsigned char)src[end])) ++end;
        if (src[end] == '}') {
            append_var(dst, length, cap, src + start, end - start);
            *pos = end + 1;
            return;
        }
    } else if (valid_var_start((unsigned char)src[i + 1])) {
        size_t start = i + 1;
        size_t end = start;
        while (valid_var_char((unsigned char)src[end])) ++end;
        append_var(dst, length, cap, src + start, end - start);
        *pos = end;
        return;
    }
    append_bytes(dst, length, cap, "$", 1);
    *pos = i + 1;
}

static char *decode_word(const char *src) {
    size_t cap = strlen(src) + 32;
    size_t length = 0;
    char *out = malloc(cap);
    if (!out) die_oom();
    char quote = 0;

    for (size_t i = 0; src[i]; ) {
        unsigned char c = (unsigned char)src[i];
        if (quote == '\'') {
            if (c == '\'') {
                quote = 0;
                ++i;
                continue;
            }
            append_bytes(&out, &length, &cap, (const char *)&c, 1);
            ++i;
            continue;
        }
        if (quote == '"') {
            if (c == '"') {
                quote = 0;
                ++i;
                continue;
            }
            if (c == '\\' && src[i + 1]) {
                append_bytes(&out, &length, &cap, src + i + 1, 1);
                i += 2;
                continue;
            }
            if (c == '$') {
                append_expansion(&out, &length, &cap, src, &i);
                continue;
            }
            append_bytes(&out, &length, &cap, (const char *)&c, 1);
            ++i;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = (char)c;
            ++i;
            continue;
        }
        if (c == '\\' && src[i + 1]) {
            append_bytes(&out, &length, &cap, src + i + 1, 1);
            i += 2;
            continue;
        }
        if (c == '$') {
            append_expansion(&out, &length, &cap, src, &i);
            continue;
        }
        append_bytes(&out, &length, &cap, (const char *)&c, 1);
        ++i;
    }
    if (quote != 0) {
        free(out);
        fprintf(stderr, "vsh: unmatched quote\n");
        return NULL;
    }
    return out;
}

static int tokenize(char *line, Token *tokens) {
    size_t i = 0;
    int count = 0;
    while (line[i] && count < MAX_TOK - 1) {
        while (isspace((unsigned char)line[i])) ++i;
        if (!line[i]) break;
        if (strchr("|<>", line[i])) {
            if (line[i] == '>' && line[i + 1] == '>') {
                tokens[count].text = strdup(">>");
                i += 2;
            } else {
                char op[2] = {line[i], 0};
                tokens[count].text = strdup(op);
                ++i;
            }
            if (!tokens[count].text) die_oom();
            ++count;
            continue;
        }
        size_t start = i;
        char quote = 0;
        while (line[i]) {
            unsigned char c = (unsigned char)line[i];
            if (quote == '\'') {
                if (c == '\'') quote = 0;
                ++i;
                continue;
            }
            if (quote == '"') {
                if (c == '"') quote = 0;
                else if (c == '\\' && line[i + 1]) ++i;
                ++i;
                continue;
            }
            if (c == '\'' || c == '"') {
                quote = (char)c;
                ++i;
                continue;
            }
            if (c == '\\' && line[i + 1]) {
                i += 2;
                continue;
            }
            if (isspace(c) || strchr("|<>", c)) break;
            ++i;
        }
        if (quote != 0) {
            fprintf(stderr, "vsh: unmatched quote\n");
            return -1;
        }
        size_t len = i - start;
        char *raw = malloc(len + 1);
        if (!raw) die_oom();
        memcpy(raw, line + start, len);
        raw[len] = 0;
        tokens[count].text = decode_word(raw);
        free(raw);
        if (!tokens[count].text) return -1;
        ++count;
    }
    tokens[count].text = NULL;
    return count;
}

static void free_tokens(Token *tokens, int count) {
    for (int i = 0; i < count; ++i) free(tokens[i].text);
}

static int valid_name(const char *name) {
    if (!name || !*name || !valid_var_start((unsigned char)name[0])) return 0;
    for (size_t i = 1; name[i]; ++i) {
        if (!valid_var_char((unsigned char)name[i])) return 0;
    }
    return 1;
}

static int which_path(const char *name, char *out, size_t cap) {
    if (strchr(name, '/')) {
        if (access(name, X_OK) != 0) return 0;
        snprintf(out, cap, "%s", name);
        return 1;
    }
    const char *path = getenv("PATH");
    if (!path) return 0;
    char *copy = strdup(path);
    if (!copy) return 0;
    char *save = NULL;
    for (char *dir = strtok_r(copy, ":", &save); dir; dir = strtok_r(NULL, ":", &save)) {
        char candidate[4096];
        int n = snprintf(candidate, sizeof(candidate), "%s/%s", *dir ? dir : ".", name);
        if (n > 0 && (size_t)n < sizeof(candidate) && access(candidate, X_OK) == 0) {
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
        if (!dir || chdir(dir) != 0) {
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
        for (int i = 1; argv[i]; ++i) {
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
        for (int i = 1; argv[i]; ++i) {
            char *eq = strchr(argv[i], '=');
            if (!eq) {
                if (!valid_name(argv[i])) {
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
        for (int i = 1; argv[i]; ++i) {
            if (!valid_name(argv[i]) || unsetenv(argv[i]) != 0) rc = 1;
        }
        return rc;
    }
    if (!strcmp(argv[0], "history")) {
        for (size_t i = 0; i < hist_n; ++i) printf("%4zu  %s\n", i + 1, hist[i]);
        return 0;
    }
    if (!strcmp(argv[0], "which")) {
        if (!argv[1]) return 1;
        char resolved[4096];
        if (!which_path(argv[1], resolved, sizeof(resolved))) return 1;
        puts(resolved);
        return 0;
    }
    if (!strcmp(argv[0], "help")) {
        puts("vsh: cd pwd echo export unset history which true false : exit help");
        return 0;
    }
    return -1;
}

static int build_argv(Token *tokens, int start, int end, char **argv, int *argc) {
    int n = 0;
    for (int i = start; i < end && n < MAX_TOK - 1; ++i) argv[n++] = tokens[i].text;
    argv[n] = NULL;
    *argc = n;
    return n > 0 ? 0 : 1;
}

static int apply_redirections(char **argv, int *argc) {
    int out = 0;
    for (int i = 0; i < *argc; ++i) {
        if ((!strcmp(argv[i], ">") || !strcmp(argv[i], ">>")) && i + 1 < *argc) {
            int flags = O_WRONLY | O_CREAT | (!strcmp(argv[i], ">>") ? O_APPEND : O_TRUNC);
            int fd = open(argv[i + 1], flags, 0644);
            if (fd < 0 || dup2(fd, STDOUT_FILENO) < 0) {
                perror(argv[i + 1]);
                if (fd >= 0) close(fd);
                return 1;
            }
            close(fd);
            ++i;
            continue;
        }
        if (!strcmp(argv[i], "<") && i + 1 < *argc) {
            int fd = open(argv[i + 1], O_RDONLY);
            if (fd < 0 || dup2(fd, STDIN_FILENO) < 0) {
                perror(argv[i + 1]);
                if (fd >= 0) close(fd);
                return 1;
            }
            close(fd);
            ++i;
            continue;
        }
        argv[out++] = argv[i];
    }
    argv[out] = NULL;
    *argc = out;
    if (out == 0) {
        fputs("vsh: redirection without command\n", stderr);
        return 1;
    }
    return 0;
}

static int execute_segment(Token *tokens, int start, int end, int in_fd, int out_fd) {
    char *argv[MAX_TOK];
    int argc = 0;
    if (build_argv(tokens, start, end, argv, &argc) != 0) return 2;
    if (apply_redirections(argv, &argc) != 0) return 1;
    if (in_fd != STDIN_FILENO && dup2(in_fd, STDIN_FILENO) < 0) _exit(1);
    if (out_fd != STDOUT_FILENO && dup2(out_fd, STDOUT_FILENO) < 0) _exit(1);

    int rc = builtin(argv);
    if (rc >= 0) return rc;
    execvp(argv[0], argv);
    int code = errno == ENOENT ? 127 : 126;
    fprintf(stderr, "vsh: %s: %s\n", argv[0], strerror(errno));
    return code;
}

static int execute_tokens(Token *tokens, int count) {
    int starts[MAX_TOK];
    int ends[MAX_TOK];
    int commands = 0;
    int start = 0;
    for (int i = 0; i <= count; ++i) {
        if (i == count || !strcmp(tokens[i].text, "|")) {
            if (i == start) {
                fputs("vsh: invalid pipeline\n", stderr);
                return 2;
            }
            starts[commands] = start;
            ends[commands] = i;
            ++commands;
            start = i + 1;
        }
    }

    if (commands == 1) {
        char *argv[MAX_TOK];
        int argc = 0;
        build_argv(tokens, starts[0], ends[0], argv, &argc);
        int has_redirection = 0;
        for (int i = 0; i < argc; ++i) {
            if (!strcmp(argv[i], "<") || !strcmp(argv[i], ">") || !strcmp(argv[i], ">>")) {
                has_redirection = 1;
                break;
            }
        }
        if (!has_redirection) {
            int rc = builtin(argv);
            if (rc >= 0) return rc;
        }
    }

    int pipes[MAX_TOK][2];
    for (int i = 0; i < commands - 1; ++i) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            return 1;
        }
    }

    pid_t pids[MAX_TOK];
    for (int cmd = 0; cmd < commands; ++cmd) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            int in_fd = cmd > 0 ? pipes[cmd - 1][0] : STDIN_FILENO;
            int out_fd = cmd < commands - 1 ? pipes[cmd][1] : STDOUT_FILENO;
            int rc = execute_segment(tokens, starts[cmd], ends[cmd], in_fd, out_fd);
            for (int i = 0; i < commands - 1; ++i) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }
            fflush(NULL);
            _exit(rc & 255);
        }
        pids[cmd] = pid;
    }
    for (int i = 0; i < commands - 1; ++i) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    int status = 0;
    for (int i = 0; i < commands; ++i) {
        int st = 0;
        if (waitpid(pids[i], &st, 0) < 0) continue;
        if (i == commands - 1) {
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
        size_t length = strlen(line);
        if (length && line[length - 1] == '\n') line[length - 1] = 0;
        if (!*line) continue;
        add_history(line);

        Token tokens[MAX_TOK];
        memset(tokens, 0, sizeof(tokens));
        int count = tokenize(line, tokens);
        if (count < 0) {
            last_status = 2;
        } else if (count > 0) {
            last_status = execute_tokens(tokens, count);
        }
        free_tokens(tokens, count > 0 ? count : 0);
    }
    return last_status;
}
