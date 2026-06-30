#include "csapp.h"

void unix_error(const char *msg)
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

pid_t Fork(void)
{
    pid_t pid = fork();

    if (pid < 0)
        unix_error("fork error");
    return pid;
}

handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("signal error");
    return old_action.sa_handler;
}

void Sigemptyset(sigset_t *set)
{
    if (sigemptyset(set) < 0)
        unix_error("sigemptyset error");
}

void Sigaddset(sigset_t *set, int signum)
{
    if (sigaddset(set, signum) < 0)
        unix_error("sigaddset error");
}

void Sigfillset(sigset_t *set)
{
    if (sigfillset(set) < 0)
        unix_error("sigfillset error");
}

void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    if (sigprocmask(how, set, oldset) < 0)
        unix_error("sigprocmask error");
}

void Sigsuspend(const sigset_t *mask)
{
    if (sigsuspend(mask) < 0 && errno != EINTR)
        unix_error("sigsuspend error");
}
