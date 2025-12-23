# SimpleScheduler – Process Scheduler in C
Overview

SimpleScheduler is a basic round-robin process scheduler implemented in C as part of CSE 231: Operating Systems (Assignment 3).
It schedules multiple user-submitted jobs over a limited number of CPU cores using a fixed time quantum.

The project includes:

SimpleShell: accepts and manages job submissions

SimpleScheduler: a separate daemon process that handles CPU scheduling

Features:

1. Round-robin scheduling using a ready queue

2. Configurable number of CPUs (NCPU) and time slice (TSLICE)

3. Supports multiple concurrent jobs

4. Uses Unix processes, signals, and timers

5. Prints completion and waiting time on termination

Technologies:

Language: C

Platform: Linux / WSL

Concepts: Processes, signals, scheduling

Build tools: GCC, Make

Project Structure
.
├── simpleshell.c
├── simplescheduler.c
├── dummy_main.h
├── Makefile
└── README.md

Compile
make

Run
./simpleshell <NCPU> <TSLICE>


Example:

./simpleshell 2 100

Submit a Job

Inside the shell:

submit ./a.out


Note: User programs must include dummy_main.h and must not use blocking calls.

Project partners-
Arja Kaur Anand
Ritu Basumatary
