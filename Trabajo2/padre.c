//este archivo es el fichero fuente que al compilarse produce el ejecutable PADRE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
    int numProcesos = strtol(argv[1], NULL, 10);
    int sem;
    int barrera[4];

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
        char estado[4];
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

    //Crear tubería
    if(pipe(barrera) == -1) {
        perror("Error al crear la tubería");
        exit(1);
    }

    //Convertir la tubería a char para que se pueda pasar a los hijos
    char barrera0[10], barrera1[10];
    sprintf(barrera0, "%d", barrera[0]);
    sprintf(barrera1, "%d", barrera[1]);

    //Crear N hijos
    for(int i = 0; i < numProcesos; i++) {
        int hijo;
        hijo = fork();
        if(hijo == -1) {
            perror("Error al crear un hijo");
            exit(1);
        } else if(hijo == 0) {
            char *info[] = {argv[0], argv[1], barrera0, barrera1};
            execvp("./Trabajo2/HIJO", info);
            exit(0);
        }
    }

    //Iniciar rondas de ataque
    int hijosVivos = numProcesos;
    char K = 'k';
    while(hijosVivos > 1) {
        printf("Iniciando la rondad de ataques\n");
        fflush(stdout);
        //Avisar a los contendientes para que se preparen
        for(int i = 0; i < hijosVivos; i++) {
            write(barrera1[1], &K, sizeof(K));
        }
        for(int i = 0; i < hijosVivos; i++) {
            //Recibir mensaje de los hijos vivos
            if(msgrcv(mensajes, &mensaje, sizeof(mensaje), 1, 0) == -1) {
                perror("Error en la comunicación con un hijo");
            }
            
            //Matar si el proceso a muerto
            if(strcmp(mensaje.estado, "KO") == 0) {
                waitpid(mensaje.pid, NULL, 0);
                printf("Proceso %d FINALIZADO\n", mensaje.pid);
                fflush(stdout);

                //Sacamos el hijo muerto de la memoria compartida
                cerrarSem(sem);
                for(int i = 0; i < numProcesos; i++) {
                    if(mensaje.pid == arrayLista[i]) {
                        arrayLista[i] = 0;
                    }
                }
                abrirSem(sem);
            }
        }

        //Comprobamos cuantos hijos quedan vivos
        for(int i = 0;  i < numProcesos; i++) {
            
        }
        

        
    }
    

}