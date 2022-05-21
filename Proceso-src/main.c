/* DMUTEX (2009) Sistemas Operativos Distribuidos
 * Codigo de Apoyo
 *
 * ESTE CODIGO DEBE COMPLETARLO EL ALUMNO:
 *    - Para desarrollar las funciones de mensajes, reloj y
 *      gestion del bucle de tareas se recomienda la implementacion
 *      de las mismas en diferentes ficheros.
 */

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int udp_port;

int main(int argc, char *argv[]) {
    char line[80], proc[80];

    if (argc < 2) {
        fprintf(stderr, "Uso: proceso <ID>\n");
        return 1;
    }

    /* Establece el modo buffer de entrada/salida a linea */
    setvbuf(stdout, (char *)malloc(sizeof(char) * 80), _IOLBF, 80);
    setvbuf(stdin, (char *)malloc(sizeof(char) * 80), _IOLBF, 80);
    // Socket creation
    struct sockaddr_in process_addr;
    int process_fd;
    if ((process_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    // socket address configuration
    process_addr.sin_family = AF_INET;
    process_addr.sin_addr.s_addr = INADDR_ANY;
    process_addr.sin_port = 0;
    // socket binding to random available port
    if (bind(process_fd, (struct sockaddr *)&process_addr, sizeof(process_addr)) < 0) {
        perror("error in process address binding");
        close(process_fd);
        exit(EXIT_FAILURE);
    }
    // get the port which the socket is binded to
    socklen_t len = sizeof(process_addr);
    if (getsockname(process_fd, (struct sockaddr *)&process_addr, &len) == -1) {
        perror("getsockname");
        close(process_fd);
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "%s: %d\n", argv[1], ntohs(process_addr.sin_port));


    for (; fgets(line, 80, stdin);) {
        if (!strcmp(line, "START\n"))
            break;

        sscanf(line, "%[^:]: %d", proc, &udp_port);
        /* Habra que guardarlo en algun sitio */

        if (!strcmp(proc, argv[1])) { /* Este proceso soy yo */
        }
    }

    /* Inicializar Reloj */

    /* Procesar Acciones */

    return 0;
}
