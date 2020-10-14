//este archivo es el fichero fuente que al compilarse produce el ejecutable PADRE

#include <sys/types.h>

int main(int argc, char *argv[]) {

    //Variables
    key_t clave;
    int idColaMensajes;

    //Crear clave asociada al fichero ejecutable
    clave = ftok((argv[0]), 'X');
    if(clave == -1) {
        perror("Error al crear la clave IPC"); 
        exit(1);
    }

    //Crear cola de mensajes
    idColaMensajes = msgget(clave, 0600 | IPC_CREAT);
    if(idColaMensajes == -1) {
        perror("Error al crear la cola de mensajes"); 
        exit(1);
    }

}