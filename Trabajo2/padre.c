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
int initSem(int semid, short valor) {
    short sem_array[1];
    sem_array[0] = valor;
    if(semctl(semid, 0, SETALL, sem_array) == -1) {
        perror("Error en initSem");
        return -1;
    }
    return 0;
}

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

    printf("   >>INICIO DEL MÉTODO MAIN DEL PADRE\n\n");
    fflush(stdout);

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
        char estado[3];
    } mensaje;
    

    //Creamos una clave asociada al fichero ejecutable
    if((llave = ftok(argv[0], 'X')) == -1) {
        perror("Error al crear la clave asociada al fichero ejecutable");
        exit(1);
    }
    printf("Creada la clave asociado al fichero con ID: %d \n", llave);
    fflush(stdout);

    //Creamos una cola de mensajes
    mensajes = msgget(llave, IPC_CREAT | 0600);
    if (mensajes == -1) {
        perror("Error al crear la cola de mensajes");
        exit(2);
    }
    //Calculamos el tamaño de cada mensaje
    int longitud = sizeof(mensaje) - sizeof(mensaje.tipo);
    printf("Creada la cola de mensajes\n");
    fflush(stdout);

    //Creamos la región de memoria compartida
    lista = shmget(llave, nPIDs*sizeof(int), IPC_CREAT | 0600);
    if(lista == -1) {
        perror("Error al crear la región de memoria compartida");
        exit(2);
    }
    //Asignamos al array el puntero a la memoria compartida
    int *array = shmat(lista, 0, 0);
    printf("Creada la región de memoria compartida lista\n");
    fflush(stdout);

    //Creamos el semáforo
    sem = semget(llave, 1, IPC_CREAT | 0600);
    if(sem == -1) {
        perror("Error al crear el semáforo sem");
        exit(1);
    }
    printf("Creado el semáforo sem\n");
    fflush(stdout);
    //Inicializar el semáforo
    initSem(sem, 1);

    //Creamos la tubería
    if(pipe(barrera) == -1) {
        perror("Error al crear la tubería");
        exit(-1);
    }
    printf("Creado la tubería barrera\n\n");
    fflush(stdout);

    //Convertimos la tubería para que se le pueda pasar como parametro a los hijos
    char barrera0[2];
    char barrera1[2];
    sprintf(barrera0, "%d", barrera[0]);
    sprintf(barrera1, "%d", barrera[1]);

    //Creación de los hijos
    for(int i = 0; i < nPIDs; i++) {
        int pidHijo;
        if((pidHijo = fork()) == -1) {
            perror("Error al ejecutar el fork");
            exit(-1);
        } else if(pidHijo == 0) {
            char *info[] = {argv[0], argv[1], barrera0, barrera1, NULL};
            execvp("./Trabajo2/HIJO", info);
            exit(0);
        }
    }

    //Para prevenir problemas, esperaremos 100ms por cada hijo, para asegurarnos de que todos han entrado en el array
    for(int i = 0; i < nPIDs; i++) {
        usleep(100000);
        printf("Empezamos en: %.1f segundos \n", (((float)nPIDs-i)/10));
        fflush(stdout);
    }

    printf("\n--------------------------------------------------------------------------------\n");
    printf("   >>HIJOS EN LA CONTIENDA\n");
    fflush(stdout);
    for(int i = 0; i < nPIDs; i++) {
        if(array[i] > 0) {
            //Imprimimos los hijos vivos
            printf("HIJO %d: \t\t\t %d \n", (i+1), array[i]);
        }
    }

    //Empezamos con los combates
    int pidsActivos = nPIDs;
    char K = 'k';
    while(pidsActivos > 1) {

        printf("\n--------------------------------------------------------------------------------\n");
        printf("Iniciando ronda de ataques\n\n");
        fflush(stdout);

        for(int i = 0; i < pidsActivos; i++) {
            //Enviamos K bytes por la barrera
            write(barrera[1], &K, sizeof(K));
        }

        for(int i = 0; i < pidsActivos; i++) {
            //Recibimos el mensaje de los hijos
            if(msgrcv(mensajes, &mensaje, longitud, 1, 0) == -1) {
                perror("Error recibiendo un mensaje");
                exit(3);
            }

            //Comprobamos si el hijo a muerto
            if(strcmp(mensaje.estado, "KO") == 0) {
                //Matamos el PID del hijo muerto
                kill(mensaje.pid, SIGTERM);
                //Esperamos a que el hijo muera
                waitpid(mensaje.pid, NULL, 0);
                //Sacamos al hijo muerto del array
                waitSem(sem);
                for(int i = 0; i < nPIDs; i ++) {
                    if(mensaje.pid == array[i]) {
                        array[i] = 0;
                    }
                }
                signalSem(sem);
                printf("  >>HIJO %d MUERTO\n", mensaje.pid);
                fflush(stdout);
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
        printf("\nFinalizando la ronda de ataques\n");
        fflush(stdout);

        //Comprobamos si existen hijos vivos
        int quedanVivos = 0;
        for(int i = 0; i < nPIDs; i++) {
            if(array[i] > 0) {
                quedanVivos = 1;
            }
        }

        if(quedanVivos == 1) {
            printf("\n  >>QUEDAN VIVOS LOS HIJOS\n"); 
            fflush(stdout);
            for(int i = 0; i < nPIDs; i++) {
                if(array[i] > 0) {
                    //Imprimimos los hijos vivos
                    printf("HIJO %d: \t\t\t %d \n", (i+1), array[i]);
                    fflush(stdout);
                }
                
            }
        }
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
                fprintf(ficheroResulado, "\n\nEl hijo %d ha ganadao\n\n", array[i]);
            }
        }
    } else {
        //Imprimimos en el fichero el empate
        fprintf(ficheroResulado, "\n\nEmpate\n\n");
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
