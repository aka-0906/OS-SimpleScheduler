#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

// ===== Function Prototypes =====
void enqueue(pid_t pid);
pid_t dequeue(void);
void set_job_first_start(pid_t pid);
void scheduler_tick(int signum);
void new_job_handler(int signum);
void job_done_handler(int signum);
void term_handler(int signum);

// ===== Process Queue =====
typedef struct ProcessNode {
    pid_t pid;
    struct ProcessNode *next;
} ProcessNode;

static ProcessNode *ready_head = NULL, *ready_tail = NULL;
static pid_t *running = NULL;
static int ncpu, tslice;
static int total = 0, done = 0;
static int pipe_fd = 3;

// ===== Queue Operations =====
void enqueue(pid_t pid) {
    ProcessNode *node = malloc(sizeof(ProcessNode));
    node->pid = pid;
    node->next = NULL;
    if (!ready_tail)
        ready_head = ready_tail = node;
    else {
        ready_tail->next = node;
        ready_tail = node;
    }
}

pid_t dequeue() {
    if (!ready_head)
        return -1;
    ProcessNode *tmp = ready_head;
    pid_t pid = tmp->pid;
    ready_head = ready_head->next;
    if (!ready_head)
        ready_tail = NULL;
    free(tmp);
    return pid;
}

// ===== Timer Tick (Round Robin) =====
void scheduler_tick(int signum) {
    for (int i = 0; i < ncpu; i++) {
        if (running[i] != 0) {
            kill(running[i], SIGSTOP);
            enqueue(running[i]);
            running[i] = 0;
        }
    }

    for (int i = 0; i < ncpu; i++) {
        if (running[i] == 0) {
            pid_t next = dequeue();
            if (next != -1) {
                running[i] = next;
                kill(next, SIGCONT);
            }
        }
    }

    (void)signum;
}

// ===== New Job Submission (SIGUSR1) =====
void new_job_handler(int signum) {
    pid_t new_pid;
    ssize_t r = read(pipe_fd, &new_pid, sizeof(new_pid));

    if (r == sizeof(new_pid)) {
        printf("[scheduler] Received new job PID=%d\n", new_pid);
        enqueue(new_pid);
        total++;  // count total jobs submitted

        for (int i = 0; i < ncpu; i++) {
            if (running[i] == 0) {
                running[i] = new_pid;
                set_job_first_start(new_pid);
                kill(new_pid, SIGCONT);
                printf("[scheduler] Started job %d\n", new_pid);
                break;
            }
        }
    } else if (r == 0) {
        fprintf(stderr, "[scheduler] Pipe closed, no more jobs.\n");
    } else {
        perror("[scheduler] Failed to read from pipe");
    }

    (void)signum;
}

// ===== Job Completion (SIGCHLD) =====
void job_done_handler(int signum) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        done++;
        for (int i = 0; i < ncpu; i++) {
            if (running[i] == pid)
                running[i] = 0;
        }
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "Job %d finished.\n", pid);
        write(STDOUT_FILENO, buf, len);
    }

    (void)signum;
}

// ===== Termination (SIGTERM) =====
void term_handler(int signum) {
    if (done >= total) {
        write(STDOUT_FILENO, "All jobs done. Exiting scheduler.\n", 34);
        exit(0);
    } else {
        write(STDOUT_FILENO, "Some jobs still running.\n", 26);
    }

    (void)signum;
}

// ===== Optional Stub: Record job's first start =====
void set_job_first_start(pid_t pid) {
    (void)pid; // placeholder (no-op)
    // could record timestamp or other info here if needed
}

// ===== Main =====
int main(int argc, char *argv[]) {
    char *env = getenv("SCHED_PIPE_FD");
    if (env)
        pipe_fd = atoi(env);

    if (argc != 3) {
        fprintf(stderr, "Usage: ./SimpleScheduler <NCPU> <TSLICE(ms)>\n");
        exit(1);
    }

    ncpu = atoi(argv[1]);
    tslice = atoi(argv[2]);
    running = calloc(ncpu, sizeof(pid_t));
    if (!running) {
        perror("calloc failed");
        exit(1);
    }

    signal(SIGUSR1, new_job_handler);
    signal(SIGCHLD, job_done_handler);
    signal(SIGALRM, scheduler_tick);
    signal(SIGTERM, term_handler);

    struct itimerval timer;
    timer.it_value.tv_sec = tslice / 1000;
    timer.it_value.tv_usec = (tslice % 1000) * 1000;
    timer.it_interval = timer.it_value;
    setitimer(ITIMER_REAL, &timer, NULL);

    printf("SimpleScheduler started. NCPU=%d, TSLICE=%dms, pipe_fd=%d\n",
           ncpu, tslice, pipe_fd);

    raise(SIGALRM); // start scheduling immediately

    while (1)
        pause();
}
