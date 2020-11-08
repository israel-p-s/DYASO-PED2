//este archivo es el fichero fuente que al compilarse produce el ejecutable HIJO

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

//Varible estado
char estado[3] = "";

//MÉTODOS AUXILIARES PARA EL FUNCIONAMIENTO DEL PROGRAMA
//Función defensa
void defensa() {
    printf("El hijo %d ha repelido un ataque\n", getpid());
    fflush(stdout);
    strcpy(estado, "OK");
}

//Función indefenso
void indefenso() {
    printf("El hijo %d ha sido emboscado mientras realizaba un ataque\n", getpid());
    fflush(stdout);
    strcpy(estado, "KO");
}

//Función que añade cada PID al array
void pidALista(int *array, int nPIDs) {
    for(int i = 0; i < nPIDs; i++) {
        if(array[i] == 0) { 
            array[i] = getpid();
            return;
        }
    }
    return;
}

//Función que devuelve el PID que va a ser atacado
int eligePid(int *array, int nPIDs) {
    while(1) {
        int aux = (rand()%nPIDs);	
        if(array[aux] > 0 && array[aux] != getpid()) {
            return array[aux];
        }
    }
}

///MÉTODOS AUXILIARES PARA EL SEMÁFORO
//Semáforo cerrado
int waitSem(int semid) {
    struct sembuf op[1];
    op[0].sem_num = 0;
    op[0].sem_op = -1;
    op[0].sem_flg = 0;
    if(semop(semid, op, 1) == -1) {
        perror("Error en waitSem");
        return -1;
    }
    return 0;
}

//Semáforo abierto
int signalSem(int semid) {
    struct sembuf op[1];
    op[0].sem_num = 0;
    op[0].sem_op = 1;
    op[0].sem_flg = 0;
    if(semop(semid, op, 1) == -1) {
        perror("Error en signalSem");
        return -1;
    }
    return 0;
} 

//MÉTODO MAIN
int main(int argc, char *argv[]) {

    //Semilla para generar números aleatorios
    srand(getpid());

    //VARIABLES GLOBALES
    key_t llave;
    int mensajes;
    int lista;
    int nPIDs = atoi(argv[1]);
    int sem;
    int barrera[2];

    struct {
        long tipo;
        int pid;
        char estado[3];
    } mensaje;

    //Creamos una clave asociada al fichero ejecutable
    if((llave = ftok(argv[0], 'X')) == -1) {
        perror("Error al crear la clave asociada al fichero ejecutable");
        exit(1);
    }

    //Creamos una cola de mensajes
    mensajes = msgget(llave, IPC_CREAT | 0600);
    if (mensajes == -1) {
        perror("Error al crear la cola de mensajes");
        exit(2);
    }
    int longitud = sizeof(mensaje) - sizeof(mensaje.tipo);

    //Creamos la región de memoria compartida
    lista = shmget(llave, nPIDs*sizeof(int), IPC_CREAT | 0600);
    if(lista == -1) {
        perror("Error al crear la región de memoria compartida");
        exit(2);
    }
    //Asignamos al array el puntero a la memoria compartida
    int *array = shmat(lista, 0, 0);

    //Creamos el semáforo
    sem = semget(llave, 1, IPC_CREAT | 0600);
    if(sem == -1) {
        perror("Error al crear el semáforo sem");
        exit(1);
    }

    //Recibimos los valores de la tubería
    barrera[0] = atoi(argv[2]);
    barrera[1] = atoi(argv[3]);

    //Añadimos el PID al array
    waitSem(sem);
    pidALista(array, nPIDs);
    signalSem(sem);

    char K;
    while(1) {

        read(barrera[0], &K, sizeof(K));
        int decision = rand() % 2;
        //Ataca
        if(decision == 0) {
            if(signal(SIGUSR1, indefenso) == SIG_ERR) {
                perror("Error al atacar");
                exit(2);
            }
            usleep(100000);
            //Elegimos al hijo al que atacaremos
            int pidAtacado = eligePid(array, nPIDs);
            printf("Atacando al proceso %d\n", pidAtacado);
            fflush(stdout);
            kill(pidAtacado, SIGUSR1);
            usleep(100000);
        //Defiende
        } else if(decision == 1) {
            if(signal(SIGUSR1, defensa) == SIG_ERR) {
                perror("Error al defender");
                exit(2);
            }
            usleep(200000);
        } else {
            perror("Error eligiendo ataque o defensa");
            exit(2);
        }

        //Rellenamos el mensaje de información para que sea enviado
        mensaje.tipo = 1;
        mensaje.pid = getpid();
        strcpy(mensaje.estado, estado);
        if(msgsnd(mensajes, &mensaje, longitud, 0) == -1) {
            perror("Error envindo un mensaje");
            exit(2);
        }
    }
}
