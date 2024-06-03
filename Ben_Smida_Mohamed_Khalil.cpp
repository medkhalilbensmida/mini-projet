#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/wait.h>

#define NUM_CHILDREN 4
#define SEM_NAME "/sync_semaphore"
#define WORK_SEM_NAME "/work_semaphore"

pid_t child_pids[NUM_CHILDREN];
sem_t *sync_sem;
sem_t *work_sem;

void parent_handle_signal(int sig, siginfo_t *siginfo, void *context) {
    if (sig == SIGUSR2) {
        printf("Le parent a reçu une confirmation de l'enfant %d.\n", siginfo->si_pid - getpid() + 1);
    }
}

void child_handle_signal(int sig, siginfo_t *siginfo, void *context) {
    if (sig == SIGUSR1) {
        printf("L'enfant %d a reçu le signal de démarrage.\n", getpid() - getppid() + 1);

        sem_wait(work_sem);
        printf("L'enfant %d travaille...\n", getpid() - getppid() + 1);
        sleep(1);
        sem_post(work_sem);

        kill(getppid(), SIGUSR2);
    } else if (sig == SIGTERM) {
        printf("L'enfant %d se termine.\n", getpid() - getppid() + 1);
        exit(0);
    }
}

void child_process() {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = child_handle_signal;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    sem_wait(sync_sem);

    while (1) {
        pause();
    }
}

void parent_process() {
    sleep(1); 

    for (int i = 0; i < NUM_CHILDREN; i++) {
        sem_post(sync_sem);
    }

    for (int i = 0; i < NUM_CHILDREN; i++) {
        kill(child_pids[i], SIGUSR1);
    }

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = parent_handle_signal;
    sigaction(SIGUSR2, &sa, NULL);

    for (int i = 0; i < NUM_CHILDREN; i++) {
        pause();
    }

    for (int i = 0; i < NUM_CHILDREN; i++) {
        kill(child_pids[i], SIGTERM);
    }

    sem_unlink(SEM_NAME);
    sem_close(sync_sem);
    sem_unlink(WORK_SEM_NAME);
    sem_close(work_sem);
}

int main() {
    sync_sem = sem_open(SEM_NAME, O_CREAT, 0644, 0);
    if (sync_sem == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    work_sem = sem_open(WORK_SEM_NAME, O_CREAT, 0644, 1);
    if (work_sem == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            child_process();
            exit(0);
        } else {
            child_pids[i] = pid;
        }
    }

    parent_process();

    for (int i = 0; i < NUM_CHILDREN; i++) {
        waitpid(child_pids[i], NULL, 0);
    }

    return 0;
}
