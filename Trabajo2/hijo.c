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
char estado[4] = "";

int add_pid_lista(int *array_lista, int n_procesos)
{
  for(int i = 0; i < n_procesos; i++)
    if(array_lista[i] == 0) 
      { array_lista[i] = getpid(); return 0;}
  return 1;
}

int pid_aleatorio(int *array_lista, int n_procesos)
{
  int n_pid = 1 + rand()%n_procesos;	// Numero al azar entre 1 y el numero de pids.
  int count = 0;
  while(1)
  {
    for(int i = 0; i < n_procesos; i++)
    {
      if(array_lista[i] > 0 && array_lista[i] != getpid()) count++;	// Comprobamos si el pid esta vivo y no es el mismo pid.
      if(count == n_pid) return array_lista[i];				// Devolvemos el pid elegido al azar.
    }
  }
}

//Función defensa
void defensa() {
    printf("El hijo %d ha repelido un ataque \n");
    fflush(stdout);
    strcpy(estado, "OK");
}

//Función indefenso
void indefenso() {
    printf("El hijo %d ha sido emboscado mientras realizaba un ataque \n");
    fflush(stdout);
    strcpy(estado, "KO");
}

//MÉTODOS AUXILIARES PARA EL SEMÁFORO
//Inicializar el semáforo    
int init_sem(int semid, short valor) {
    short sem_array[1];
    sem_array[0] = valor;
    if(semctl(semid, 0, SETALL, sem_array) == -1) {
        perror("Error semctl");
        return -1;
    }
    return 0;
} 

//Semáforo cerrado
int wait_sem(int semid) {
    struct sembuf op[1];
    op[0].sem_num = 0;
    op[0].sem_op = -1;
    op[0].sem_flg = 0;
    if(semop(semid, op, 1) == -1) {
        perror("Error semop:");
        return -1;
    }
    return 0;
}

//Semáforo abierto
int signal_sem(int semid) {
    struct sembuf op[1];
    op[0].sem_num = 0;
    op[0].sem_op = 1;
    op[0].sem_flg = 0;

    if(semop(semid, op, 1) == -1) {
        perror("Error semop:");
        return -1;
    }
    return 0;
} 

//MÉTODO MAIN
int main(int argc, char *argv[]) {
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
        char estado[4];
    } mensaje;

    //Creamos una clave asociada al fichero ejecutable
    if((llave = ftok(argv[0], 'X')) == -1) {
        perror("ftok");
        exit(1);
    }

    printf("ftok: %d \n", llave);
    fflush(stdout);

    //Creamos una cola de mensajes
    mensajes = msgget(llave, IPC_CREAT | 0600);
    if (mensajes == -1) {
        perror("semid");
        exit(2);
    }
    int longitud = sizeof(mensaje) - sizeof(mensaje.tipo);

    //Creamos la región de memoria compartida
    lista = shmget(llave, nPIDs*sizeof(int), IPC_CREAT | 0600);
    if(lista == -1) {
        perror("shmget");
        exit(2);
    }

    //Asignamos al array el puntero a la memoria compartida
    int *array = shmat(lista, 0, 0);

    //Creamos el semáforo
    sem = semget(llave, 1, IPC_CREAT | 0600);
    if(sem == -1) {
        perror("semget");
        exit(1);
    }
    
    printf("array: %d\n  sem: %d \n", array, sem);
    fflush(stdout);

    //Recibimos los valores de la tubería
    barrera[0] = atoi(argv[2]);
    barrera[1] = atoi(argv[3]);

    //Añadimos el PID al array
    wait_sem(sem);
    printf("%d - %d \n", nPIDs, getpid());
    fflush(stdout);
    add_pid_lista(array, nPIDs);
    signal_sem(sem);

    char K;
    while(1) {

        read(barrera[0], &K, sizeof(K));
        int decision = rand() % 2;
        printf("dec %d \n", decision);
        fflush(stdout);
        //Ataca
        if(decision == 0) {
            if(signal(SIGUSR1, indefenso) == SIG_ERR) {
                perror("error al atacar");
                exit(2);
            }
            usleep(100000);
            //Elegimos al hijo al que atacaremos
            int pidAtacado = pid_aleatorio(array, nPIDs);
            printf("Atacando al proceso %d", pidAtacado);
            fflush(stdout);
            kill(pidAtacado, SIGUSR1);
            usleep(100000);
        } else if(decision == 1) {
            if(signal(SIGUSR1, defensa) == SIG_ERR) {
                perror("error al defender");
                exit(2);
            }
            usleep(200000);
        } else {
            perror("error eligiendo ataque o defensa");
            exit(2);
        }

        mensaje.tipo = 1;
        mensaje.pid = getpid();
        strcpy(mensaje.estado, estado);
        if(msgsnd(mensajes, &mensaje, longitud, 0) == -1) {
            perror("error en msgsnd");
            exit(2);
        }
    }
}
