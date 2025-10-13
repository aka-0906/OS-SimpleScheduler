#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define SCHEDULER_PATH "./SimpleScheduler"
#define NCPU "2"
#define TSLICE "1000"

char *trim(char *s) {
    if (!s) return NULL;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

static pid_t scheduler_pid = 0;
static int pipe_fd[2]; // pipe_fd[0] = read end for scheduler, pipe_fd[1] = write end for shell

void start_scheduler() {
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(1);
    }

    scheduler_pid = fork();
    if (scheduler_pid < 0) {
        perror("fork for scheduler");
        exit(1);
    }

    if (scheduler_pid == 0) {
        // --- CHILD: Scheduler process ---
        close(pipe_fd[1]);           // close write end
        dup2(pipe_fd[0], 3);         // scheduler reads from fd 3
        close(pipe_fd[0]);
        setenv("SCHED_PIPE_FD", "3", 1);

        execl(SCHEDULER_PATH, "SimpleScheduler", NCPU, TSLICE, (char *)NULL);
        perror("execl SimpleScheduler");
        _exit(1);
    }

    // --- PARENT: Shell process ---
    close(pipe_fd[0]);
    printf("[shell] Started SimpleScheduler (PID=%d)\n", scheduler_pid);
}

void launch_job(char *cmdline) {
    char *argv[64];
    int argc = 0;
    char *token = strtok(cmdline, " ");
    while (token && argc < 63) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    argv[argc] = NULL;
    if (argc == 0) return;

    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        return;
    }

    if (child == 0) {
        setpgid(0, 0); // give it its own group
        kill(getpid(), SIGSTOP); // stop immediately, scheduler will resume
        execvp(argv[0], argv);
        perror("execvp");
        _exit(1);
    }

    printf("[shell] Launched job (PID=%d)\n", child);

    // send child PID to scheduler
    if (write(pipe_fd[1], &child, sizeof(child)) != sizeof(child))
        perror("[shell] Failed to write PID to scheduler pipe");

    // tell scheduler that a new job has arrived
    if (kill(scheduler_pid, SIGUSR1) == -1)
        perror("[shell] Failed to signal scheduler");
}

int main() {
    start_scheduler();

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    const char *prompt = "myshell> ";

    while (1) {
        printf("%s", prompt);
        fflush(stdout);

        read = getline(&line, &len, stdin);
        if (read == -1) {
            putchar('\n');
            break;
        }

        char *cmd = trim(line);
        if (*cmd == '\0') continue;

        if (strcmp(cmd, "exit") == 0) {
            printf("[shell] Terminating scheduler...\n");
            kill(scheduler_pid, SIGTERM);
            waitpid(scheduler_pid, NULL, 0);
            break;
        }

        launch_job(cmd);
    }

    free(line);
    return 0;
}
