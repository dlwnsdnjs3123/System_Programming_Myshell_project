#include "myshell.h"

static char *trim_spaces(char *s);
static void close_all_pipes(int pipes[][2], int count);

int main(void)
{
    char cmdline[MAXLINE];

    while (1) {
        printf("CSE4100-SP-P2> ");
        fflush(stdout);

        if (fgets(cmdline, MAXLINE, stdin) == NULL) {
            if (feof(stdin)) {
                printf("\n");
                fflush(stdout);
                exit(0);
            }
            continue;
        }

        eval(cmdline);
    }

    return 0;
}

void eval(char *cmdline)
{
    char buf[MAXLINE];
    char *cmd_argv_storage[MAXCMDS][MAXARGS];
    char **commands[MAXCMDS];
    int i, ncmds;

    strcpy(buf, cmdline);

    for (i = 0; i < MAXCMDS; i++)
        commands[i] = cmd_argv_storage[i];

    ncmds = parse_pipeline(buf, commands);

    /* 빈 줄 */
    if (ncmds == 0)
        return;

    /* 문법 오류 */
    if (ncmds < 0)
        return;

    /* pipe가 없는 단일 명령이면 phase1 방식 유지 */
    if (ncmds == 1) {
        if (commands[0][0] == NULL)
            return;

        if (builtin_command(commands[0], 0))
            return;

        execute_single_command(commands[0]);
        return;
    }

    /* pipeline 실행 */
    execute_pipeline(commands, ncmds);
}

void execute_single_command(char **argv)
{
    pid_t pid;
    int status;

    pid = Fork();
    if (pid == 0) {
        if (builtin_command(argv, 1))
            exit(0);

        if (execvp(argv[0], argv) < 0) {
            printf("%s: Command not found.\n", argv[0]);
            fflush(stdout);
            exit(1);
        }
    }

    if (waitpid(pid, &status, 0) < 0) {
        if (errno != ECHILD)
            unix_error("waitpid error");
    }
}

void execute_pipeline(char ***commands, int ncmds)
{
    int pipes[MAXCMDS - 1][2];
    pid_t pids[MAXCMDS];
    int i, status;

    for (i = 0; i < ncmds - 1; i++) {
        if (pipe(pipes[i]) < 0)
            unix_error("pipe error");
    }

    for (i = 0; i < ncmds; i++) {
        pids[i] = Fork();

        if (pids[i] == 0) {
            /* 이전 command의 출력을 stdin으로 연결 */
            if (i > 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0)
                    unix_error("dup2 error");
            }

            /* 다음 command로 보낼 출력을 stdout으로 연결 */
            if (i < ncmds - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0)
                    unix_error("dup2 error");
            }

            close_all_pipes(pipes, ncmds - 1);

            /*
             * pipe 안 명령을 무조건 외부 명령이라고 단정하지 않고
             * shell에서 쓰는 명령도 Linux shell과 유사하게 동작하도록 처리
             */
            if (builtin_command(commands[i], 1))
                exit(0);

            if (execvp(commands[i][0], commands[i]) < 0) {
                printf("%s: Command not found.\n", commands[i][0]);
                fflush(stdout);
                exit(1);
            }
        }
    }

    close_all_pipes(pipes, ncmds - 1);

    /*
     * 명세서 phase2 반영:
     * parent는 마지막 command를 기다리는 방향으로 구현
     */
    if (waitpid(pids[ncmds - 1], &status, 0) < 0) {
        if (errno != ECHILD)
            unix_error("waitpid error");
    }

    /* 나머지 child도 회수해서 zombie 방지 */
    for (i = 0; i < ncmds - 1; i++) {
        if (waitpid(pids[i], &status, 0) < 0) {
            if (errno != ECHILD)
                unix_error("waitpid error");
        }
    }
}

int builtin_command(char **argv, int in_child)
{
    char pathbuf[MAXLINE];
    char *target;

    if (argv[0] == NULL)
        return 1;

    /* exit */
    if (!strcmp(argv[0], "exit")) {
        exit(0);
    }

    /* cd */
    if (!strcmp(argv[0], "cd")) {
        if (argv[1] == NULL) {
            target = getenv("HOME");

            if (target == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
            } else if (chdir(target) < 0) {
                perror("cd");
            }
        } else {
            target = expand_tilde_path(argv[1], pathbuf, sizeof(pathbuf));

            if (target == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
            } else if (chdir(target) < 0) {
                perror("cd");
            }
        }

        /*
         * pipeline 안에서 cd가 실행되더라도
         * 부모 shell의 cwd는 바뀌지 않는 것이 자연스럽다.
         * child 안에서는 child만 바뀌고 끝난다.
         */
        (void)in_child;
        return 1;
    }

    return 0;
}

int parse_pipeline(char *buf, char ***commands)
{
    char *p;
    char *segment_start;
    char quote = '\0';
    int ncmds = 0;

    if (strlen(buf) > 0 && buf[strlen(buf) - 1] == '\n')
        buf[strlen(buf) - 1] = '\0';

    segment_start = buf;
    p = buf;

    while (*p) {
        if (quote != '\0') {
            if (*p == quote)
                quote = '\0';
        } else {
            if (*p == '"' || *p == '\'') {
                quote = *p;
            } else if (*p == '|') {
                *p = '\0';

                segment_start = trim_spaces(segment_start);
                if (*segment_start == '\0') {
                    fprintf(stderr, "Invalid null command.\n");
                    return -1;
                }

                if (ncmds >= MAXCMDS) {
                    fprintf(stderr, "Too many piped commands.\n");
                    return -1;
                }

                if (parse_command(segment_start, commands[ncmds]) < 0)
                    return -1;

                ncmds++;
                segment_start = p + 1;
            }
        }
        p++;
    }

    segment_start = trim_spaces(segment_start);

    if (*segment_start == '\0') {
        if (ncmds == 0)
            return 0;

        fprintf(stderr, "Invalid null command.\n");
        return -1;
    }

    if (ncmds >= MAXCMDS) {
        fprintf(stderr, "Too many piped commands.\n");
        return -1;
    }

    if (parse_command(segment_start, commands[ncmds]) < 0)
        return -1;

    ncmds++;

    return ncmds;
}

int parse_command(char *segment, char **argv)
{
    char *p = segment;
    int argc = 0;

    while (*p) {
        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '\0')
            break;

        if (argc >= MAXARGS - 1) {
            fprintf(stderr, "Too many arguments.\n");
            return -1;
        }

        /* quoted token */
        if (*p == '"' || *p == '\'') {
            char quote = *p;
            p++;
            argv[argc++] = p;

            while (*p && *p != quote)
                p++;

            if (*p == quote) {
                *p = '\0';
                p++;
            }
        }
        /* normal token */
        else {
            argv[argc++] = p;

            while (*p && *p != ' ' && *p != '\t')
                p++;

            if (*p) {
                *p = '\0';
                p++;
            }
        }
    }

    argv[argc] = NULL;
    return argc;
}

char *expand_tilde_path(const char *path, char *expanded, size_t size)
{
    char *home;

    if (path == NULL || expanded == NULL || size == 0)
        return NULL;

    if (path[0] != '~') {
        strncpy(expanded, path, size - 1);
        expanded[size - 1] = '\0';
        return expanded;
    }

    home = getenv("HOME");
    if (home == NULL)
        return NULL;

    if (path[1] == '\0') {
        snprintf(expanded, size, "%s", home);
        return expanded;
    }

    if (path[1] == '/') {
        snprintf(expanded, size, "%s%s", home, path + 1);
        return expanded;
    }

    strncpy(expanded, path, size - 1);
    expanded[size - 1] = '\0';
    return expanded;
}

static char *trim_spaces(char *s)
{
    char *end;

    while (*s == ' ' || *s == '\t')
        s++;

    if (*s == '\0')
        return s;

    end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }

    return s;
}

static void close_all_pipes(int pipes[][2], int count)
{
    int i;

    for (i = 0; i < count; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}
