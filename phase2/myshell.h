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
#define MAXCMDS 64

/* 한 줄 명령 실행 */
void eval(char *cmdline);

/* 내장 명령어 처리
 * in_child = 0 : 부모 쉘에서 처리하는 단일 명령
 * in_child = 1 : pipeline 내부 child에서 처리
 */
int builtin_command(char **argv, int in_child);

/* 단일 명령 실행 */
void execute_single_command(char **argv);

/* pipeline 실행 */
void execute_pipeline(char ***commands, int ncmds);

/* pipeline 단위로 명령줄 파싱 */
int parse_pipeline(char *buf, char ***commands);

/* 하나의 command segment를 argv로 파싱 */
int parse_command(char *segment, char **argv);

/* ~, ~/... 를 HOME 경로로 확장 */
char *expand_tilde_path(const char *path, char *expanded, size_t size);

#endif