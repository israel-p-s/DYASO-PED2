//este archivo es el fichero fuente que al compilarse produce el ejecutable PADRE

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

///MÉTODOS AUXILIARES PARA EL SEMÁFORO
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

    //VARIABLES GLOBALES
    key_t llave;
    int mensajes;
    int lista;
    int nPIDs = strtol(argv[1], NULL, 10);
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

    //Inicializar el semáforo
    init_sem(sem, 1);

    //Creamos la tubería
    if(pipe(barrera) == -1) {
        perror("pipe");
        exit(-1);
    }

    printf("array: %d\n  sem: %d \n", array, sem);
    fflush(stdout);

    //Convertimos la tubería para que se le pueda pasar como parametro a los hijos
    char barrera0[10];
    char barrera1[10];
    sprintf(barrera0, "%d", barrera[0]);
    sprintf(barrera1, "%d", barrera[1]);

    //Creación de los hijos
    for(int i = 0; i < nPIDs; i++) {
        int pidHijo;
        if((pidHijo = fork()) == -1) {
            perror("fork");
            exit(-1);
        } else if(pidHijo == 0) {
            char *info[] = {argv[0], argv[1], barrera0, barrera1, NULL};
            execvp("./Trabajo2/HIJO", info);
            exit(0);
        }
    }

    int fin = 1;
    while(fin == 1)
    {
        fin = 0;
        for(int i = 0; i < nPIDs; i++)
            if(array[i] == 0) fin = 1;
                usleep(100000);
    }
    
    // Mostramos por pantalla los PIDs creados inicialmente.
    printf("*************************\n");
    printf("*     PIDs INICIALES    *\n");
    for(int i = 0; i < nPIDs; i++) printf("* pid[%d] = %d\t\t*\n", i, array[i]);
    printf("*************************\n\n");
    fflush(stdout);

    //Empezamos con los combates
    int pidsActivos = nPIDs;
    char K = 'a';
    while(pidsActivos > 1) {
        printf("Iniciando ronda de ataques \n");
        fflush(stdout);
        for(int i = 0; i < pidsActivos; i++) {
            //Enviamos K bytes por la barrera
            write(barrera[1], &K, sizeof(K));
        }

        for(int i = 0; i < pidsActivos; i++) {
            //Recibimos el mensaje de los hijos
            if(msgrcv(mensajes, &mensaje, longitud, 1, 0) == -1) {
                perror("msgrcv");
                exit(3);
            }

            //Comprobamos si el hijo a muerto
            if(strcmp(mensaje.estado, "KO") == 0) {
                //Matamos el PID del hijo muerto
                kill(mensaje.pid, SIGTERM);
                //Esperamos a que el hijo muera
                waitpid(mensaje.pid, NULL, 0);
                //Sacamos al hijo muerto del array
                wait_sem(sem);
                for(int i = 0; i < nPIDs; i ++) {
                    if(mensaje.pid == array[i]) {
                        array[i] = 0;
                    }
                }
                signal_sem(sem);
            }
        }
        //Actualizamos el número de pids activos
        int res = 0;
        for(int i = 0; i < nPIDs; i++) {
            if(array[i] > 0) {
                res++;
            }
        }
        pidsActivos = res;
        printf("Finalizando la ronda de ataques \n");
        fflush(stdout);
    } 

    //Abrimos el fichero FIFO resultado
    FILE *ficheroResulado;
    ficheroResulado = fopen("resultado", "a");

    //Comprobamos si tenemos un ganador o ha sido empate
    if(pidsActivos == 1) {
        for(int i = 0; i < nPIDs; i++) {
            if(array[i] > 0) {
                //Matamos el PID que queda
                kill(mensaje.pid, SIGTERM);
                //Esperamos a que el hijo muera
                waitpid(mensaje.pid, NULL, 0);
                //Imprimimos en el fichero al ganador
                fprintf(ficheroResulado, "El hijo %d ha ganadao", array[i]);
            } else {
                //Imprimimos en el fichero el empate
                fprintf(ficheroResulado, "Empate");
            }
        }
    }

    //Cerramos la cola de mensajes
    msgctl(mensajes, IPC_RMID, 0);

    //Cerramos el array y la memoria compartida
    shmdt(array);
    shmctl(lista, IPC_RMID, 0);

    //Cerramos el semáforo
    semctl(sem, IPC_RMID, 0);

    //Cerramos la tubería
    close(barrera[0]);
    close(barrera[1]);

    //Comprobamos que los recursos hayan sido cerrados
    system("ipcrm -a");  
    system("ipcs -qs");

    return 0;
}
