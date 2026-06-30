#ifndef MYSHELL_H
#define MYSHELL_H

#include "csapp.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXARGS 128
#define MAXCMDS 64
#define MAXJOBS 128

/* Internal job states used by Phase 3 job control. */
#define JOB_RUNNING 1
#define JOB_STOPPED 2
#define JOB_DONE 3
#define JOB_TERMINATED 4

typedef struct job_t {
    int used;
    int jid;
    pid_t pgid;              /* process group id for the whole job */
    int state;               /* JOB_RUNNING / JOB_STOPPED / ... */
    int npids;               /* number of children in this job */
    int live_count;          /* children not fully reaped yet */
    int changed;             /* state changed since last report */
    int bg;                  /* 1 if the job currently runs in background */
    int notified;            /* whether the latest status was reported */
    int last_signal;         /* last stop/terminate signal if any */
    pid_t pids[MAXCMDS];
    char cmdline[MAXLINE];
} job_t;

extern volatile sig_atomic_t fg_pgid;
extern job_t jobs[MAXJOBS];

void eval(char *cmdline);
int builtin_command(char **argv, int in_child);
void execute_single_command(char **argv);
void execute_pipeline(char ***commands, int ncmds);
int parse_pipeline(char *buf, char ***commands);
int parse_command(char *segment, char **argv);
char *expand_tilde_path(const char *path, char *expanded, size_t size);

/* phase3 */
void init_jobs(void);
int parse_bg(char *buf);
void launch_job(char ***commands, int ncmds, int bg, const char *cmdline_for_job);
void waitfg(pid_t pgid);
void list_jobs(int cleanup_after_print);
int do_bgfgkill(char **argv, int mode, int in_child);
void terminate_all_jobs(void);
void print_job_notification(void);

/* job table helpers */
int add_job(pid_t pgid, pid_t *pids, int npids, int state, int bg, const char *cmdline);
void delete_job(job_t *job);
job_t *get_job_by_jid(int jid);
job_t *get_job_by_pgid(pid_t pgid);
job_t *get_job_by_pid(pid_t pid);
job_t *get_current_job(void);
int get_job_mark(job_t *job);
const char *job_state_string(const job_t *job);

/* signal handlers */
void sigchld_handler(int sig);
void sigint_handler(int sig);
void sigtstp_handler(int sig);

#endif
