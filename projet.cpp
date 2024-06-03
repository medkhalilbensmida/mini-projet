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

sem_t *sem_start;
pid_t children[NUM_CHILDREN];
volatile sig_atomic_t task_signal_received = 0;

void handle_sigusr1(int sig) {
    task_signal_received = 1;
}

void handle_sigusr2(int sig) {
    // Signal de confirmation du fils au père
    printf("Père : Signal de confirmation reçu d'un enfant\n");
}

void handle_sigterm(int sig) {
    printf("Enfant %d : Signal de terminaison reçu\n", getpid());
    exit(0);
}

void child_task(int child_num) {
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGTERM, handle_sigterm);

    // Attendre le signal de départ
    while (!task_signal_received) {
        pause();
    }

    // Tâche simulée par une boucle et un sleep
    printf("Enfant %d : Début de la tâche\n", child_num);
    sleep(2);
    printf("Enfant %d : Tâche terminée\n", child_num);

    // Envoyer le signal de confirmation au père
    kill(getppid(), SIGUSR2);

    // Terminer le processus fils
    exit(0);
}

int main() {
    sem_start = sem_open("/sem_start", O_CREAT | O_EXCL, 0644, 0);
    if (sem_start == SEM_FAILED) {
        perror("Échec de sem_open");
        exit(EXIT_FAILURE);
    }

    signal(SIGUSR2, handle_sigusr2);

    // Créer les processus fils
    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("Échec de fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Processus fils
            child_task(i);
        } else {
            // Processus père
            children[i] = pid;
        }
    }

    // Attendre que tous les fils soient prêts
    for (int i = 0; i < NUM_CHILDREN; i++) {
        sem_post(sem_start);
    }

    // Envoyer le signal de départ aux fils une fois qu'ils sont tous prêts
    for (int i = 0; i < NUM_CHILDREN; i++) {
        kill(children[i], SIGUSR1);
    }

    // Attendre que les fils terminent leurs tâches
    for (int i = 0; i < NUM_CHILDREN; i++) {
        wait(NULL);
    }

    // Nettoyer le sémaphore
    if (sem_unlink("/sem_start") == -1) {
        perror("Échec de sem_unlink");
        exit(EXIT_FAILURE);
    }

    printf("Père : Tous les enfants ont terminé leurs tâches\n");

    return 0;
}
