#include "myshell.h"

volatile sig_atomic_t fg_pgid = 0;
job_t jobs[MAXJOBS];

static char *trim_spaces(char *s);
static void close_all_pipes(int pipes[][2], int count);
static int parse_job_spec(const char *arg, int *jid_out);
static int next_jid(void);
static void safe_copy_cmdline(char *dst, const char *src, size_t size);
static void print_single_job(const job_t *job);
static void cleanup_finished_job(job_t *job);

/*
 * Main loop of the shell.
 * - register signal handlers
 * - initialize the job list
 * - repeatedly print the prompt, read one line, and evaluate it
 */
int main(void)
{
    char cmdline[MAXLINE];

    /* Register handlers used in Phase 3 job control. */
    Signal(SIGCHLD, sigchld_handler);
    Signal(SIGINT, sigint_handler);
    Signal(SIGTSTP, sigtstp_handler);
    init_jobs();

    while (1) {
        print_job_notification();
        printf("CSE4100-SP-P2> ");
        fflush(stdout);

        if (fgets(cmdline, MAXLINE, stdin) == NULL) {
            if (feof(stdin)) {
                printf("\n");
                fflush(stdout);
                terminate_all_jobs();
                exit(0);
            }
            clearerr(stdin);
            continue;
        }

        eval(cmdline);
    }

    return 0;
}

/*
 * Evaluate one command line.
 * This function copies the raw input, checks whether it is a background job,
 * parses the pipeline, handles single built-in commands in the parent shell,
 * and then launches the job.
 */
void eval(char *cmdline)
{
    char buf[MAXLINE];
    char rawline[MAXLINE];
    char *cmd_argv_storage[MAXCMDS][MAXARGS];
    char **commands[MAXCMDS];
    char *trimmed_raw;
    int i, ncmds, bg;

    safe_copy_cmdline(rawline, cmdline, sizeof(rawline));
    if (strlen(rawline) > 0 && rawline[strlen(rawline) - 1] == '\n')
        rawline[strlen(rawline) - 1] = '\0';
    trimmed_raw = trim_spaces(rawline);

    safe_copy_cmdline(buf, cmdline, sizeof(buf));

    for (i = 0; i < MAXCMDS; i++)
        commands[i] = cmd_argv_storage[i];

    /* Detect background execution first, then split the line into pipeline commands. */
    bg = parse_bg(buf);
    ncmds = parse_pipeline(buf, commands);

    if (ncmds == 0)
        return;
    if (ncmds < 0)
        return;

    if (ncmds == 1) {
        if (commands[0][0] == NULL)
            return;

        /*
         * Only foreground single built-ins should run in the parent shell.
         * Background forms such as "cd .. &" must go through launch_job().
         */
        if (!bg && builtin_command(commands[0], 0))
            return;
    }

    launch_job(commands, ncmds, bg, trimmed_raw);
}

/*
 * Wrapper used for a single command.
 * It converts one argv array into the same launch_job() path used by pipelines.
 */
void execute_single_command(char **argv)
{
    char *commands_storage[MAXCMDS][MAXARGS];
    char **commands[MAXCMDS];
    char cmdline[MAXLINE] = "";
    int i;

    for (i = 0; i < MAXCMDS; i++)
        commands[i] = commands_storage[i];

    commands[0] = argv;

    for (i = 0; argv[i] != NULL; i++) {
        if (i > 0)
            strncat(cmdline, " ", sizeof(cmdline) - strlen(cmdline) - 1);
        strncat(cmdline, argv[i], sizeof(cmdline) - strlen(cmdline) - 1);
    }

    launch_job(commands, 1, 0, cmdline);
}

/*
 * Wrapper used for a pipeline command.
 * Phase 3 eventually sends all real execution through launch_job().
 */
void execute_pipeline(char ***commands, int ncmds)
{
    launch_job(commands, ncmds, 0, "");
}

/*
 * Handle built-in commands.
 * - exit, cd, jobs, bg, fg, kill
 * in_child tells whether the function is called inside a child process.
 */
int builtin_command(char **argv, int in_child)
{
    char pathbuf[MAXLINE];
    char *target;

    if (argv[0] == NULL)
        return 1;

    if (!strcmp(argv[0], "exit")) {
        if (!in_child)
            terminate_all_jobs();
        exit(0);
    }

    if (!strcmp(argv[0], "cd")) {
        if (argv[1] == NULL) {
            target = getenv("HOME");
            if (target == NULL)
                fprintf(stderr, "cd: HOME not set\n");
            else if (chdir(target) < 0)
                perror("cd");
        } else {
            target = expand_tilde_path(argv[1], pathbuf, sizeof(pathbuf));
            if (target == NULL)
                fprintf(stderr, "cd: HOME not set\n");
            else if (chdir(target) < 0)
                perror("cd");
        }
        return 1;
    }

    if (!strcmp(argv[0], "jobs")) {
        list_jobs(1);
        return 1;
    }

    if (!strcmp(argv[0], "bg"))
        return do_bgfgkill(argv, 1, in_child);

    if (!strcmp(argv[0], "fg"))
        return do_bgfgkill(argv, 2, in_child);

    if (!strcmp(argv[0], "kill"))
        return do_bgfgkill(argv, 3, in_child);

    return 0;
}

/*
 * Launch one job.
 * A job can be a single command or multiple processes connected by pipes.
 * For Phase 3, this function also creates a process group, adds the job to
 * the job table, and decides whether the parent waits or returns immediately.
 */
void launch_job(char ***commands, int ncmds, int bg, const char *cmdline_for_job)
{
    int pipes[MAXCMDS - 1][2];
    pid_t pids[MAXCMDS];
    pid_t pgid = 0;
    sigset_t mask_chld, prev;
    int i, jid;

    /* Block SIGCHLD temporarily to avoid races while the job is being created. */
    Sigemptyset(&mask_chld);
    Sigaddset(&mask_chld, SIGCHLD);
    Sigprocmask(SIG_BLOCK, &mask_chld, &prev);

    for (i = 0; i < ncmds - 1; i++) {
        if (pipe(pipes[i]) < 0)
            unix_error("pipe error");
    }

    for (i = 0; i < ncmds; i++) {
        pids[i] = fork();
        if (pids[i] < 0)
            unix_error("fork error");

        if (pids[i] == 0) {
            Signal(SIGINT, SIG_DFL);
            Signal(SIGTSTP, SIG_DFL);
            Signal(SIGCHLD, SIG_DFL);

            /* Put every child in the same process group so the shell can control the whole job. */
            if (pgid == 0)
                setpgid(0, 0);
            else
                setpgid(0, pgid);

            if (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) < 0)
                unix_error("dup2 error");
            if (i < ncmds - 1 && dup2(pipes[i][1], STDOUT_FILENO) < 0)
                unix_error("dup2 error");

            close_all_pipes(pipes, ncmds - 1);
            Sigprocmask(SIG_SETMASK, &prev, NULL);

            if (builtin_command(commands[i], 1))
                exit(0);

            execvp(commands[i][0], commands[i]);
            fprintf(stderr, "%s: Command not found.\n", commands[i][0]);
            exit(1);
        }

        if (pgid == 0)
            pgid = pids[i];
        setpgid(pids[i], pgid);
    }

    close_all_pipes(pipes, ncmds - 1);

    /* Save the new job before unblocking signals. */
    jid = add_job(pgid, pids, ncmds, JOB_RUNNING, bg, cmdline_for_job);
    if (!bg)
        fg_pgid = pgid;

    Sigprocmask(SIG_SETMASK, &prev, NULL);

    /* Background jobs return immediately. Foreground jobs are waited for. */
    if (bg) {
        printf("[%d] %d\n", jid, (int)pgid);
        fflush(stdout);
    } else {
        job_t *job;
        sigset_t mask_all, prev2;

        waitfg(pgid);

        Sigfillset(&mask_all);
        Sigprocmask(SIG_BLOCK, &mask_all, &prev2);
        job = get_job_by_pgid(pgid);
        cleanup_finished_job(job);
        Sigprocmask(SIG_SETMASK, &prev2, NULL);
    }
}

/*
 * Split a full command line into pipeline segments.
 * Quoted strings are preserved so that '|' inside quotes is not treated as a pipe.
 */
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

    if (quote != '\0') {
        fprintf(stderr, "Unmatched quote.\n");
        return -1;
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

/*
 * Parse one command segment into argv form.
 * This function keeps quoted strings as one argument.
 */
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

        if (*p == '"' || *p == '\'') {
            char quote = *p;
            p++;
            argv[argc++] = p;

            while (*p && *p != quote)
                p++;

            if (*p != quote) {
                fprintf(stderr, "Unmatched quote.\n");
                return -1;
            }

            *p = '\0';
            p++;
        } else {
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

/*
 * Expand '~' or '~/...' into the user's HOME path.
 * '~user' is not specially handled in this project.
 */
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

/*
 * Initialize the fixed-size job table.
 */
void init_jobs(void)
{
    int i;
    for (i = 0; i < MAXJOBS; i++)
        memset(&jobs[i], 0, sizeof(job_t));
}

/*
 * Check whether the command line ends with '&'.
 * If yes, remove '&' from the buffer and return 1.
 */
int parse_bg(char *buf)
{
    char *p = buf;
    char quote = '\0';
    char *amp = NULL;
    char *q;

    while (*p) {
        if (quote != '\0') {
            if (*p == quote)
                quote = '\0';
        } else {
            if (*p == '"' || *p == '\'')
                quote = *p;
            else if (*p == '&')
                amp = p;
        }
        p++;
    }

    if (quote != '\0')
        return 0;

    if (amp == NULL)
        return 0;

    q = amp + 1;
    while (*q == ' ' || *q == '\t' || *q == '\n')
        q++;

    if (*q != '\0') {
        fprintf(stderr, "Syntax error near unexpected token '&'.\n");
        return 0;
    }

    *amp = '\0';
    return 1;
}

/*
 * Wait until the given process group is no longer the foreground job.
 * The shell sleeps with sigsuspend() and wakes up when a signal arrives.
 */
void waitfg(pid_t pgid)
{
    sigset_t empty;

    Sigemptyset(&empty);
    while (fg_pgid == pgid)
        Sigsuspend(&empty);
}

/*
 * Print all jobs in the table.
 * When cleanup_after_print is true, finished jobs are removed after printing.
 */
void list_jobs(int cleanup_after_print)
{
    sigset_t mask_all, prev;
    int i;

    Sigfillset(&mask_all);
    Sigprocmask(SIG_BLOCK, &mask_all, &prev);

    for (i = 0; i < MAXJOBS; i++) {
        if (!jobs[i].used)
            continue;
        print_single_job(&jobs[i]);
        jobs[i].changed = 0;
        jobs[i].notified = 1;
    }

    if (cleanup_after_print) {
        for (i = 0; i < MAXJOBS; i++) {
            if (jobs[i].used &&
                (jobs[i].state == JOB_DONE || jobs[i].state == JOB_TERMINATED))
                delete_job(&jobs[i]);
        }
    }

    Sigprocmask(SIG_SETMASK, &prev, NULL);
}

/*
 * Shared helper for bg / fg / kill.
 * mode: 1 = bg, 2 = fg, 3 = kill
 */
int do_bgfgkill(char **argv, int mode, int in_child)
{
    sigset_t mask_all, prev;
    job_t *job;
    pid_t pgid;
    int jid;

    Sigfillset(&mask_all);
    Sigprocmask(SIG_BLOCK, &mask_all, &prev);

    if (argv[1] == NULL) {
        job = get_current_job();
        if (job == NULL) {
            Sigprocmask(SIG_SETMASK, &prev, NULL);
            fprintf(stderr, "%s: current: no such job\n", argv[0]);
            return 1;
        }
    } else {
        if (parse_job_spec(argv[1], &jid) < 0) {
            Sigprocmask(SIG_SETMASK, &prev, NULL);
            fprintf(stderr, "%s: %s: invalid job specification\n", argv[0], argv[1]);
            return 1;
        }
        job = get_job_by_jid(jid);
        if (job == NULL) {
            Sigprocmask(SIG_SETMASK, &prev, NULL);
            fprintf(stderr, "%s: %s: no such job\n", argv[0], argv[1]);
            return 1;
        }
    }

    pgid = job->pgid;

    if (mode == 1) {
        /* Resume the stopped job in the background. */
        if (kill(-pgid, SIGCONT) < 0) {
            Sigprocmask(SIG_SETMASK, &prev, NULL);
            perror("bg");
            return 1;
        }
        job->state = JOB_RUNNING;
        job->bg = 1;
        job->changed = 0;
        job->notified = 1;
        print_single_job(job);
        Sigprocmask(SIG_SETMASK, &prev, NULL);
        return 1;
    }

    if (mode == 2) {
        /* Resume the job and bring it to the foreground. */
        if (kill(-pgid, SIGCONT) < 0) {
            Sigprocmask(SIG_SETMASK, &prev, NULL);
            perror("fg");
            return 1;
        }
        job->state = JOB_RUNNING;
        job->bg = 0;
        job->changed = 0;
        job->notified = 1;

        if (!in_child) {
            fg_pgid = pgid;
            Sigprocmask(SIG_SETMASK, &prev, NULL);
            waitfg(pgid);

            Sigprocmask(SIG_BLOCK, &mask_all, &prev);
            cleanup_finished_job(get_job_by_pgid(pgid));
            Sigprocmask(SIG_SETMASK, &prev, NULL);
        } else {
            Sigprocmask(SIG_SETMASK, &prev, NULL);
        }
        return 1;
    }

    if (mode == 3) {
        /* If the job is stopped, wake it first so SIGTERM can be delivered cleanly. */
        if (job->state == JOB_STOPPED) {
            if (kill(-pgid, SIGCONT) < 0) {
                Sigprocmask(SIG_SETMASK, &prev, NULL);
                perror("kill");
                return 1;
            }
        }

        if (kill(-pgid, SIGTERM) < 0) {
            Sigprocmask(SIG_SETMASK, &prev, NULL);
            perror("kill");
            return 1;
        }

        job->changed = 1;
        job->notified = 0;
        Sigprocmask(SIG_SETMASK, &prev, NULL);
        return 1;
    }

    Sigprocmask(SIG_SETMASK, &prev, NULL);
    return 1;
}

/*
 * Terminate every remaining job before shell exit.
 */
void terminate_all_jobs(void)
{
    sigset_t mask_all, prev;
    int i;

    Sigfillset(&mask_all);
    Sigprocmask(SIG_BLOCK, &mask_all, &prev);

    for (i = 0; i < MAXJOBS; i++) {
        if (!jobs[i].used)
            continue;
        kill(-jobs[i].pgid, SIGCONT);
        kill(-jobs[i].pgid, SIGTERM);
    }

    Sigprocmask(SIG_SETMASK, &prev, NULL);
}

/*
 * Print deferred job notifications before the next prompt.
 * Done / Terminated jobs are removed after they have been reported.
 */
void print_job_notification(void)
{
    sigset_t mask_all, prev;
    int i;

    Sigfillset(&mask_all);
    Sigprocmask(SIG_BLOCK, &mask_all, &prev);

    for (i = 0; i < MAXJOBS; i++) {
        if (!jobs[i].used || !jobs[i].changed || jobs[i].notified)
            continue;

        if (jobs[i].state != JOB_STOPPED &&
            jobs[i].state != JOB_DONE &&
            jobs[i].state != JOB_TERMINATED)
            continue;

        print_single_job(&jobs[i]);
        jobs[i].changed = 0;
        jobs[i].notified = 1;
    }

    for (i = 0; i < MAXJOBS; i++) {
        if (!jobs[i].used)
            continue;
        cleanup_finished_job(&jobs[i]);
    }

    Sigprocmask(SIG_SETMASK, &prev, NULL);
    return;
    /*
     * 사용자 요청 반영:
     * 자동 알림(Done/Terminated/Stopped)을 프롬프트 직전에 출력하지 않는다.
     * background job 생성 시에는 launch_job()에서 [jid] pgid 형태만 출력하고,
     * 이후 상태 확인은 jobs 명령으로만 하도록 한다.
     */
    return;
}

/*
 * Add a new job to the job table and initialize its metadata.
 */
int add_job(pid_t pgid, pid_t *pids, int npids, int state, int bg, const char *cmdline)
{
    int i, j;
    job_t *job = NULL;

    for (i = 0; i < MAXJOBS; i++) {
        if (!jobs[i].used) {
            job = &jobs[i];
            break;
        }
    }

    if (job == NULL) {
        fprintf(stderr, "Too many jobs.\n");
        return 0;
    }

    memset(job, 0, sizeof(job_t));
    job->used = 1;
    job->jid = next_jid();
    job->pgid = pgid;
    job->state = state;
    job->bg = bg;
    job->npids = npids;
    job->live_count = npids;
    job->changed = 0;
    job->bg = bg;
    job->notified = 0;
    job->last_signal = 0;
    safe_copy_cmdline(job->cmdline, cmdline, sizeof(job->cmdline));

    for (j = 0; j < npids; j++)
        job->pids[j] = pids[j];

    return job->jid;
}

/*
 * Remove one job from the table by clearing its slot.
 */
void delete_job(job_t *job)
{
    if (job != NULL)
        memset(job, 0, sizeof(job_t));
}

/* Return the job with the given job id. */
job_t *get_job_by_jid(int jid)
{
    int i;
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].used && jobs[i].jid == jid)
            return &jobs[i];
    }
    return NULL;
}

/* Return the job that owns the given process group id. */
job_t *get_job_by_pgid(pid_t pgid)
{
    int i;
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].used && jobs[i].pgid == pgid)
            return &jobs[i];
    }
    return NULL;
}

/* Return the job that contains the given child pid. */
job_t *get_job_by_pid(pid_t pid)
{
    int i, j;
    for (i = 0; i < MAXJOBS; i++) {
        if (!jobs[i].used)
            continue;
        for (j = 0; j < jobs[i].npids; j++) {
            if (jobs[i].pids[j] == pid)
                return &jobs[i];
        }
    }
    return NULL;
}

/*
 * Return the most recent job.
 * This is used when bg/fg/kill is called without an explicit operand.
 */
job_t *get_current_job(void)
{
    int i;
    job_t *best = NULL;

    for (i = 0; i < MAXJOBS; i++) {
        if (!jobs[i].used)
            continue;
        if (jobs[i].state == JOB_DONE || jobs[i].state == JOB_TERMINATED)
            continue;
        if (best == NULL || jobs[i].jid > best->jid)
            best = &jobs[i];
    }
    return best;
}

/*
 * Return '+', '-', or ' ' for jobs output.
 * The newest job gets '+', and the second newest gets '-'.
 */
int get_job_mark(job_t *job)
{
    int i;
    int highest = -1, second = -1;

    if (job == NULL || !job->used)
        return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (!jobs[i].used)
            continue;
        if (jobs[i].jid > highest) {
            second = highest;
            highest = jobs[i].jid;
        } else if (jobs[i].jid > second) {
            second = jobs[i].jid;
        }
    }

    if (job->jid == highest)
        return '+';
    if (job->jid == second)
        return '-';
    return ' ';
}

/* Convert an internal job state value into printable text. */
const char *job_state_string(const job_t *job)
{
    if (job->state == JOB_RUNNING)
        return "Running";
    if (job->state == JOB_STOPPED)
        return "Stopped";
    if (job->state == JOB_DONE)
        return "Done";
    if (job->state == JOB_TERMINATED)
        return "Terminated";
    return "Unknown";
}

/*
 * Reap child processes and update the job table.
 * This handler processes exit, stop, continue, and signal termination events.
 */
void sigchld_handler(int sig)
{
    int olderrno = errno;
    int status;
    pid_t pid;
    sigset_t mask_all, prev;
    job_t *job;

    (void)sig;
    Sigfillset(&mask_all);

    /* Reap every child whose state changed without blocking the shell. */
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        Sigprocmask(SIG_BLOCK, &mask_all, &prev);
        job = get_job_by_pid(pid);
        if (job != NULL) {
            if (WIFSTOPPED(status)) {
                job->state = JOB_STOPPED;
                job->bg = 1;
                job->last_signal = WSTOPSIG(status);
                job->changed = 1;
                job->notified = 0;
                if (fg_pgid == job->pgid)
                    fg_pgid = 0;
            } else if (WIFCONTINUED(status)) {
                job->state = JOB_RUNNING;
                job->changed = 0;
                job->notified = 1;
            } else if (WIFSIGNALED(status)) {
                if (job->live_count > 0)
                    job->live_count--;
                job->last_signal = WTERMSIG(status);
                if (job->live_count == 0) {
                    job->state = JOB_TERMINATED;
                    job->changed = 1;
                    job->notified = 0;
                    if (fg_pgid == job->pgid)
                        fg_pgid = 0;
                }
            } else if (WIFEXITED(status)) {
                if (job->live_count > 0)
                    job->live_count--;
                if (job->live_count == 0) {
                    job->state = JOB_DONE;
                    job->changed = 1;
                    job->notified = 0;
                    if (fg_pgid == job->pgid)
                        fg_pgid = 0;
                }
            }
        }
        Sigprocmask(SIG_SETMASK, &prev, NULL);
    }

    errno = olderrno;
}

/*
 * Forward Ctrl-C (SIGINT) to the current foreground job group.
 */
void sigint_handler(int sig)
{
    pid_t pgid = fg_pgid;
    (void)sig;
    if (pgid > 0)
        kill(-pgid, SIGINT);
}

/*
 * Forward Ctrl-Z (SIGTSTP) to the current foreground job group.
 */
void sigtstp_handler(int sig)
{
    pid_t pgid = fg_pgid;
    (void)sig;
    if (pgid > 0)
        kill(-pgid, SIGTSTP);
}

/* Remove leading and trailing spaces/tabs from a string in place. */
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

/* Close every pipe descriptor created for the current pipeline. */
static void close_all_pipes(int pipes[][2], int count)
{
    int i;
    for (i = 0; i < count; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

/*
 * Parse a job operand.
 * Both %n and n are accepted, but the result is always interpreted as a job id.
 */
static int parse_job_spec(const char *arg, int *jid_out)
{
    const char *p = arg;

    if (arg == NULL || jid_out == NULL)
        return -1;

    if (*p == '%')
        p++;

    if (*p == '\0')
        return -1;

    while (*p) {
        if (*p < '0' || *p > '9')
            return -1;
        p++;
    }

    *jid_out = atoi((*arg == '%') ? arg + 1 : arg);
    if (*jid_out <= 0)
        return -1;
    return 0;
}

/* Find the next job id to assign. */
static int next_jid(void)
{
    int i;
    int maxjid = 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].used && jobs[i].jid > maxjid)
            maxjid = jobs[i].jid;
    }
    return maxjid + 1;
}

/* Safely copy a command line string into a fixed-size buffer. */
static void safe_copy_cmdline(char *dst, const char *src, size_t size)
{
    if (size == 0)
        return;
    if (src == NULL)
        src = "";
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

/* Print one job in a Linux-shell-like format. */
static void print_single_job(const job_t *job)
{
    int mark;
    const char *state;

    if (job == NULL || !job->used)
        return;

    mark = get_job_mark((job_t *)job);
    state = job_state_string(job);

    if (mark == '+' || mark == '-')
        printf("[%d]%c  %-10s %s\n", job->jid, mark, state, job->cmdline);
    else
        printf("[%d]   %-10s %s\n", job->jid, state, job->cmdline);
}

/* Remove completed jobs from the table once the caller decides cleanup is safe. */
static void cleanup_finished_job(job_t *job)
{
    if (job == NULL || !job->used)
        return;

    if (job->state == JOB_DONE || job->state == JOB_TERMINATED)
        delete_job(job);
}
