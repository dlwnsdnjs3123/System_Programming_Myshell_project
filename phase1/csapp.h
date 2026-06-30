#ifndef CSAPP_H
#define CSAPP_H

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXLINE 8192

typedef void handler_t(int);

void unix_error(const char *msg);
pid_t Fork(void);
handler_t *Signal(int signum, handler_t *handler);
void Sigemptyset(sigset_t *set);
void Sigaddset(sigset_t *set, int signum);
void Sigfillset(sigset_t *set);
void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
void Sigsuspend(const sigset_t *mask);

#endif
