#include "myshell.h"

int main(void)
{
    char cmdline[MAXLINE];

    while (1) {
        /* 프롬프트 출력 */
        printf("CSE4100-SP-P2> ");
        fflush(stdout);

        /* 한 줄 입력 받기 */
        if (fgets(cmdline, MAXLINE, stdin) == NULL) {
            /* Ctrl-D 같은 EOF 입력이면 종료 */
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
    char *argv[MAXARGS];
    pid_t pid;
    int status;

    /* 원본 명령을 복사해서 파싱에 사용 */
    strcpy(buf, cmdline);

    /* 빈 줄이면 그냥 return */
    if (parse_line(buf, argv) == 0)
        return;

    if (argv[0] == NULL)
        return;

    /* builtin이면 부모 프로세스에서 처리 */
    if (builtin_command(argv))
        return;

    /* 일반 명령어는 자식 프로세스 생성 후 실행 */
    pid = Fork();
    if (pid == 0) {
        if (execvp(argv[0], argv) < 0) {
            printf("%s: Command not found.\n", argv[0]);
            fflush(stdout);
            exit(1);
        }
    }

    /* 부모는 자식이 끝날 때까지 기다림 */
    if (waitpid(pid, &status, 0) < 0) {
        if (errno != ECHILD)
            unix_error("waitpid error");
    }
}

int builtin_command(char **argv)
{
    if (argv[0] == NULL)
        return 1;

    /* exit 입력 시 쉘 종료 */
    if (!strcmp(argv[0], "exit")) {
        exit(0);
    }

    /* cd는 부모 프로세스에서 처리해야 현재 디렉토리가 바뀜 */
    if (!strcmp(argv[0], "cd")) {
        char pathbuf[MAXLINE];
        char *target;

        /* cd만 입력하면 HOME으로 이동 */
        if (argv[1] == NULL) {
            target = getenv("HOME");

            if (target == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
            } else if (chdir(target) < 0) {
                perror("cd");
            }
        } else {
            /* cd ~, cd ~/... 처리를 위해 경로 변환 */
            target = expand_tilde_path(argv[1], pathbuf, sizeof(pathbuf));

            if (target == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
            } else if (chdir(target) < 0) {
                perror("cd");
            }
        }
        return 1;
    }

    /* builtin이 아니면 0 반환 */
    return 0;
}

int parse_line(char *buf, char **argv)
{
    char *delim;
    int argc = 0;

    /* 맨 끝 개행문자 제거 */
    if (strlen(buf) > 0 && buf[strlen(buf) - 1] == '\n')
        buf[strlen(buf) - 1] = '\0';

    /* 앞쪽 공백 제거 */
    while (*buf && (*buf == ' '))
        buf++;

    /* 빈 줄이면 처리 안 함 */
    if (*buf == '\0') {
        argv[0] = NULL;
        return 0;
    }

    /* 공백 기준으로 문자열 분리 */
    while ((delim = strchr(buf, ' '))) {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;

        /* 연속된 공백 건너뛰기 */
        while (*buf && (*buf == ' '))
            buf++;
    }

    /* 마지막 토큰 추가 */
    if (*buf != '\0')
        argv[argc++] = buf;

    argv[argc] = NULL;
    return argc;
}

char *expand_tilde_path(const char *path, char *expanded, size_t size)
{
    char *home;

    if (path == NULL || expanded == NULL || size == 0)
        return NULL;

    /* ~로 시작하지 않으면 그대로 사용 */
    if (path[0] != '~') {
        strncpy(expanded, path, size - 1);
        expanded[size - 1] = '\0';
        return expanded;
    }

    home = getenv("HOME");
    if (home == NULL)
        return NULL;

    /* "~"만 있으면 HOME으로 변환 */
    if (path[1] == '\0') {
        snprintf(expanded, size, "%s", home);
        return expanded;
    }

    /* "~/..." 형태면 HOME 뒤에 이어붙임 */
    if (path[1] == '/') {
        snprintf(expanded, size, "%s%s", home, path + 1);
        return expanded;
    }

    /* "~user" 형태는 지원하지 않고 그대로 둠 */
    strncpy(expanded, path, size - 1);
    expanded[size - 1] = '\0';
    return expanded;
}