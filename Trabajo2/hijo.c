//este archivo es el fichero fuente que al compilarse produce el ejecutable HIJO
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//VARIABLES GLOBALES
char estado[4] = "";

//Función defensa
void defensa() {
    printf("El hijo %d ha repelido un ataque.\n", getpid());
    fflush(stdout);
    strcpy(estado, "OK");
}

//Función indefenso
void indefenso() {
    printf("El hijo %d ha sido emboscado mientras realizaba un ataque.\n", getpid());
    fflush(stdout);
    strcpy(estado, "KO");
}

//Función para elegir el pid del hijo atacado
int hijoAtacado(int *arrayLista, int numProcesos) {
    int res = 1 + (rand()%numProcesos);
    for(int i = 0; i < numProcesos; i++) {
        if(arrayLista[i] != getpid() && arrayLista[i] == hijoAtacado) {
            return hijoAtacado;
        }
    }
}

//FUNCIONES PARA EL SEMÁFORO
//Iniciar semáforo
void iniciarSem(int idSem, int valor) {
    if(semctl(idSem, 0, SETVAL, valor) == -1) {
        perror("Error al iniciar el semáforo");
        exit(1);
    }
}

//Abrir semáforo
void abrirSem(int idSem) {
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = 1;
    op.sem_flg = 0;

    if(semop(idSem, &op, 1) == -1) {
        perror("Error al abrir el semáforo");
        exit(1);
    }
}

//Cerrar semáforo
void cerrarSem(int idSem) {
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = -1;
    op.sem_flg = 0;

    if(semop(idSem, &op, 1) == -1) {
        perror("Error al cerrar el semáforo");
        exit(1);
    }
}

//MÉTODO MAIN
void main(int argc, char *argv[]) {
    
    //VARIABLES
    key_t clave;
    int mensajes;
    int lista;
    int numProcesos = atoi(argv[1]);
    int sem;
    int barrera[4];

    srand(getpid());

    //Crear clave asociada al ejecutable
    clave = ftok(argv[0], 'X');
    if(clave == -1) {
        perror("Error al crear la clave");
        exit(1);
    }

    //Crear cola de mensajes
    mensajes = msgget(clave, 0600 | IPC_CREAT);
    if (mensajes == -1) {
        perror("Error al crear la cola de mensajes");
        exit(1);
    }

    //Crear la estructura de mensaje que será enviado
    struct {
        int pid;
        char estado[2];
    } mensaje;

    //Crear región de memoria compartida
    lista = shmget(clave, numProcesos*sizeof(int), IPC_CREAT | 0600);
    if (lista == -1) {
        perror("Error al crear la región de memoria compartida");
        exit(1);
    }

    //Crear array para la memoria compartida
    int *arrayLista = (int *)shmat (lista, (char *)0, 0);

    //Crear semáforo
    sem = semget(clave, 1, IPC_CREAT | 0600);
    if(sem == -1) {
        perror("Error al crear el semáforo");
        exit(1);
    }

    //Iniciar semáforo
    iniciarSem(sem, 1);

    //Iniciar barrera
    barrera[0] = atoi(argv[2]);
    barrera[1] = atoi(argv[3]);

    //Añadimos el pid del hijo en la memoria compartida usando el semáforo
    cerrarSem(sem);
    for(int i = 0; i < numProcesos; i++){
        if(arrayLista[i] == 0) {
            arrayLista[i] = getpid();
        }
    }
        abrirSem(sem);

    char K;
    while(1) {
        read(barrera[0], &K, sizeof(K));
        
        int decision = (rand()%2);
        if(decision == 0) {
            if(signal(SIGUSR1, indefenso) == -1) {
                perror("Error al asignar indefenso");
                exit(1);
            }
            usleep(100000);
            int pidAtacado = hijoAtacado(arrayLista, numProcesos);
            printf("El proceso %d, atacando al proceso %d\n", getpid(), pidAtacado);
            fflush(stdout);
            kill(pidAtacado, SIGUSR1);
            usleep(100000);
        } else if(decision == 1) {
            if(signal(SIGUSR1, defensa) == -1) {
                perror("Error al asignar defensa");
                exit(1);
            }
            usleep(200000);
        } else {
            perror("Error asignando ataque o defensa en un hijo");
        }
        mensaje.pid = getpid();
        strcpy(mensaje.estado, estado);
        if(msgsnd(mensajes, &mensaje, sizeof(mensaje), 0) == -1) {
            perror("Error en la comunicación con el padre");
            exit(1);
        }
    }

}