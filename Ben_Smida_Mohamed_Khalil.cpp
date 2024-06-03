#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>

#define NUM_CHILDREN 4

sem_t *semaphore;
pid_t children[NUM_CHILDREN];
volatile sig_atomic_t task_signal_received = 0;

void handle_sigusr1(int sig) {
    task_signal_received = 1;
}

void handle_sigusr2(int sig) {
    printf("Père : Signal de confirmation reçu d'un enfant\n");
}

void handle_sigterm(int sig) {
    printf("Enfant %d : Signal de terminaison reçu\n", getpid());
    exit(0);
}

void child_task(int child_num) {
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGTERM, handle_sigterm);

    while (!task_signal_received) {
        pause();
    }

    printf("Enfant %d : Début de la tâche\n", child_num);
    sleep(2);
    printf("Enfant %d : Tâche terminée\n", child_num);

    kill(getppid(), SIGUSR2);
    exit(0);
}

int main() {
    semaphore = sem_open("semaphore", O_CREAT | O_EXCL, 0644, 0);
    if (semaphore == SEM_FAILED) {
        perror("Échec de sem_open");
        exit(EXIT_FAILURE);
    }

    signal(SIGUSR2, handle_sigusr2);

    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("Échec de fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            child_task(i);
        } else {
            children[i] = pid;
        }
    }

    for (int i = 0; i < NUM_CHILDREN; i++) {
        sem_post(semaphore);
    }

    for (int i = 0; i < NUM_CHILDREN; i++) {
        kill(children[i], SIGUSR1);
    }

    for (int i = 0; i < NUM_CHILDREN; i++) {
        wait(NULL);
    }

    if (sem_unlink("semaphore") == -1) {
        perror("Échec de sem_unlink");
        exit(EXIT_FAILURE);
    }

    printf("Père : Tous les enfants ont terminé leurs tâches\n");

    return 0;
}
