# System_Programming_Myshell_project

This repository contains my `MyShell` implementation for a system programming course project. The project was developed in three incremental phases and focuses on process creation, command execution, pipes, background execution, signal handling, and basic job control.

The original submission report is excluded from the public repository because it contains personal course metadata.

## Project Structure

```text
.
|-- phase1
|   |-- myshell.c
|   |-- myshell.h
|   |-- csapp.c
|   |-- csapp.h
|   `-- Makefile
|-- phase2
|   |-- myshell.c
|   |-- myshell.h
|   |-- csapp.c
|   |-- csapp.h
|   `-- Makefile
`-- phase3
    |-- myshell.c
    |-- myshell.h
    |-- csapp.c
    |-- csapp.h
    `-- Makefile
```

## Phase Summary

### Phase 1: Basic Shell

- Repeatedly prints a prompt and reads one command line.
- Parses whitespace-separated command arguments.
- Handles `cd` and `exit` as built-in commands.
- Executes external commands with `fork()` and `execvp()`.
- Waits for foreground child processes with `waitpid()`.

### Phase 2: Pipes

- Splits a command line into pipeline segments using `|`.
- Creates child processes for each command in the pipeline.
- Connects pipeline stages with `pipe()` and `dup2()`.
- Supports multiple pipes such as `cat file | grep abc | sort`.
- Handles quoted arguments and pipe inputs without surrounding spaces.

### Phase 3: Background Jobs and Job Control

- Runs commands or pipelines in the background with `&`.
- Tracks jobs using a job table keyed by job id and process group id.
- Supports `jobs`, `bg`, `fg`, and `kill` built-ins.
- Uses process groups to control whole pipelines as one job.
- Handles `SIGCHLD`, `SIGINT`, and `SIGTSTP` for job status changes and foreground control.

## Build and Run

Each phase is self-contained. Build the phase you want to test:

```bash
cd phase3
make
./myshell
```

Clean generated files:

```bash
make clean
```

## Example Commands

```bash
CSE4100-SP-P2> ls
CSE4100-SP-P2> cd ..
CSE4100-SP-P2> ls | grep myshell
CSE4100-SP-P2> cat file.txt | grep -i "abc" | sort -r
CSE4100-SP-P2> sleep 10 &
CSE4100-SP-P2> jobs
CSE4100-SP-P2> fg %1
```

## Implementation Notes

- The small `csapp.c` / `csapp.h` files in each phase provide only the wrapper functions needed by this project, so the repository can be built without relying on external course files.
- The shell is intended for Linux-like environments. Job-control behavior depends on POSIX process, signal, and process-group APIs.
