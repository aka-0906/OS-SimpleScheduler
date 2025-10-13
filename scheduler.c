void new_job_handler(int signum) {
    pid_t new_pid;
    ssize_t r = read(pipe_fd, &new_pid, sizeof(new_pid));

    if (r == sizeof(new_pid)) {
        printf("[scheduler] Received new job PID=%d\n", new_pid);
        enqueue(new_pid);
        total++; // was total_jobs_submitted

        for (int i = 0; i < ncpu; i++) {
            if (running[i] == 0) { // was running_processes
                running[i] = new_pid; // was running_processes
                set_job_first_start(new_pid);
                kill(new_pid, SIGCONT);
                printf("[scheduler] Started job %d\n", new_pid);
                break;
            }
        }
    } 
    else if (r == 0) {
        fprintf(stderr, "[scheduler] Pipe closed, no more jobs.\n");
    } 
    else {
        perror("[scheduler] Failed to read from pipe");
    }

    (void)signum;
}
