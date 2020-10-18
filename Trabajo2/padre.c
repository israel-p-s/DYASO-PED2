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
    if(semctl(idSem, 0, SETALL, valor) == -1) {
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

int main(int argc, char *argv[]) {

    //VARIABLES
    key_t clave;
    int mensajes;
    int lista;
    int numProcesos = strtol(argv[1], NULL, 10);
    int sem;
    int barrera[2];

    //ACCIONES PREVIAS
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

    //Convertir la barrera a char para que se pueda pasar a los hijos
    char barrera0[10], barrera1[10];
    sprintf(barrera0, "%d", barrera[0]);
    sprintf(barrera1, "%d", barrera[1]);

    //Crear N hijos
    for(int i = 0; i < numProcesos; i++) {
        int hijo;
        hijo = fork();
        if(hijo < 0) {
            perror("Error al crear hijo");
            exit(1);
        } else if(hijo == 0) {
            char *info = {argv[0], argv[1], barrera0, barrera1, NULL};
            execvp("./Trabajo2/HIJO", info);
            exit(0);
        }
    }

    //Esperamos 100 milisegundos por hijo creado para que todo esté listo para el combate
    for(int i = 0; i < numProcesos; i++) {
        usleep(100000);
    }

    


}