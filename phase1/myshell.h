#ifndef MYSHELL_H
#define MYSHELL_H

#include "csapp.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAXARGS 128

/* 한 줄 명령 실행 */
void eval(char *cmdline);

/* 내장 명령어 처리 */
int builtin_command(char **argv);

/* 입력 문자열을 공백 기준으로 나누기 */
int parse_line(char *buf, char **argv);

/* ~, ~/... 를 HOME 경로로 바꿔줌 */
char *expand_tilde_path(const char *path, char *expanded, size_t size);

#endif