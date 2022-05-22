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
#include <unistd.h>

#include "map.h"

#define MAX_N_PROCESSES 10
#define MAX_LINE_SIZE 80
#define MAX_SECTION_SIZE 20
#define MSG 0
#define LOCK 1
#define OK 2

typedef struct Message {
    // the type of message: MSG->0. LOCK->1. OK->2
    int type;
    // the section to be accessed by the sender
    char section[MAX_SECTION_SIZE];
    // the LC of the sender
    int LC[MAX_N_PROCESSES];
    // the id of the sender on the LC
    char sender_id[10];

} Message;

typedef struct Process {
    char *p_id;
    int p_port;
    int p_socket;
    int arr_index;
    struct addrinfo *addr;
} Process;

// This program's logical clock
static int *LC;
// This program's process
static Process *myself;
// This program's # of processes
static int N_processes = 0;
// This program's array of processes
// static Process *process_arr[MAX_N_PROCESSES];
// This program's socket_fd
static int process_fd;
// This program's socket_address
struct sockaddr_in process_addr;
// This program's socket_address_length
socklen_t process_addr_len = sizeof(process_addr);
static map *process_map;

Process *create_Process(char *p_id, int p_port, int arr_index) {
    Process *newProcess = malloc(sizeof(Process));
    newProcess->p_id = strdup(p_id);
    newProcess->p_port = p_port;
    newProcess->arr_index = arr_index;
    const char *hostname = 0; /* localhost */
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    struct addrinfo *res = 0;
    char port[6];
    sprintf(port, "%d", p_port);
    int err = getaddrinfo(hostname, port, &hints, &res);
    if (err != 0) {
        perror("failed to resolve remote socket address");
        return NULL;
    }
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == -1) {
        perror("error on socket creation for a process");
        return NULL;
    }
    newProcess->p_socket = fd;
    newProcess->addr = res;
    return newProcess;
}

void print_tick() {
    printf("%s: TICK\n", myself->p_id);
}

void tick() {
    LC[myself->arr_index] += 1;
    print_tick();
}

void print_clock() {
    printf("%s: LC[", myself->p_id);
    for (int i = 0; i < N_processes; i++) {
        if (i != N_processes - 1) {
            printf("%d,", LC[i]);
        }

        else {
            printf("%d]\n", LC[i]);
        }
    }
}

void combine_clocks(int ext_LC[10]) {
    for (int i = 0; i < N_processes; i++) {
        int local_k = LC[i];
        int ext_k = ext_LC[i];
        if (ext_k > local_k) {
            LC[i] = ext_k;
        }
    }
}

void receive() {
    // TODO implementar receive para probar el MESSAGETO
    Message *message = malloc(sizeof(Message));
    recvfrom(process_fd, message, sizeof(Message), 0, (struct sockaddr *)&process_addr, &process_addr_len);
    combine_clocks(message->LC);
    switch (message->type) {
        case MSG: {
            printf("%s: RECEIVE(MSG, %s)\n", myself->p_id, message->sender_id);
        } break;
        case OK: {
        } break;
        case LOCK: {
        } break;
        default: {
            perror("receive msg switch");
        } break;
    }
    tick();
}

Message *create_Message(int type, char *section) {
    Message *m = NULL;
    switch (type) {
        case MSG: {
            m = malloc(sizeof(Message));
            m->type = type;
            strcpy(m->section, "MSG");
            memcpy(m->LC, LC, N_processes * sizeof(int));
            strcpy(m->sender_id, myself->p_id);
        } break;

        default:
            break;
    }
    return m;
}

int send_msg(char *proc_id, int type) {
    int map_op;
    Process *p = map_get(process_map, proc_id, &map_op);
    if (map_op == -1) {
        return -1;
    }
    switch (type) {
        case MSG: {
            // TOTEST
            printf("%s: SEND(MSG,%s)\n", myself->p_id, proc_id);
            Message *msg = create_Message(MSG, NULL);
            // we can use sizeof(Message) since we're not working with pointers
            if (sendto(p->p_socket, msg, sizeof(Message), 0, p->addr->ai_addr, p->addr->ai_addrlen) < 0) {
                perror("error in sendto MSG");
                return -1;
            }
        } break;
        case LOCK: {
            // TODO send_msg caso del LOCK
        } break;
        case OK: {
            // TODO send_msg caso del OK
        } break;
        default:
            break;
    }
    return 0;
}

void print_process(void *k, void *v) {
    Process *p = (Process *)v;
    fprintf(stderr, " {%s : %d} ", p->p_id, p->p_port);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: proceso <ID>\n");
        return 1;
    }
    /* Establece el modo buffer de entrada/salida a linea */
    setvbuf(stdout, (char *)malloc(sizeof(char) * MAX_LINE_SIZE), _IOLBF, MAX_LINE_SIZE);
    setvbuf(stdin, (char *)malloc(sizeof(char) * MAX_LINE_SIZE), _IOLBF, MAX_LINE_SIZE);
    // Socket creation
    if ((process_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
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
    if (getsockname(process_fd, (struct sockaddr *)&process_addr, &process_addr_len) == -1) {
        perror("getsockname");
        close(process_fd);
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "%s: %d\n", argv[1], ntohs(process_addr.sin_port));
    char line[MAX_LINE_SIZE], proc[MAX_LINE_SIZE];
    int udp_port;

    /*
    We are going to be given a list of the processes through stdin, 1 per line, every process gets
    the same list, hence we can use a normal array of processes with each index identifying each process.
    Once we have all of them we will intialize the clocks
    */
    process_map = map_create(key_string, 0);

    for (; fgets(line, MAX_LINE_SIZE, stdin);) {
        if (!strcmp(line, "START\n"))
            break;
        sscanf(line, "%[^:]: %d", proc, &udp_port);
        if (strcmp(proc, argv[1]) == 0) {
            myself = create_Process(proc, udp_port, N_processes);
            map_put(process_map, myself->p_id, myself);
            N_processes++;
        } else {
            Process *process = create_Process(proc, udp_port, N_processes);
            map_put(process_map, process->p_id, process);
            N_processes++;
            fprintf(stderr, "Created new Process %s with port %d\n", process->p_id, process->p_port);
        }
    }
    N_processes = map_size(process_map);
    LC = (int *)malloc(sizeof(int) * N_processes);
    // Logical clock creation
    fprintf(stderr, "LIST OF REGISTERED PROCESSES: [");
    map_visit(process_map, print_process);
    fprintf(stderr, "]\n");
    for (int i = 0; i < N_processes; i++) {
        LC[i] = 0;  // Clock initialization
    }
    // for (int i = 0; i < N_processes; i++) {
    //     Process *p = process_arr[i];
    //     fprintf(stderr, " {%s : %d} ", p->p_id, p->p_port);
    //     LC[i] = 0;  // Clock initialization
    // }

    /* Procesar Acciones */
    char command[MAX_LINE_SIZE], proc_id[MAX_LINE_SIZE];
    for (; fgets(line, MAX_LINE_SIZE, stdin);) {
        if (strcmp(line, "GETCLOCK\n") == 0) {
            print_clock();
        } else if (strcmp(line, "FINISH\n") == 0) {
            exit(0);
        } else if (strcmp(line, "EVENT\n") == 0) {
            tick();
        } else if (strcmp(line, "RECEIVE\n") == 0) {
            receive();
        } else {
            // TODO comprobar si funciona el parseo de comandos dobles
            sscanf(line, "%s %s", command, proc_id);
            if (strcmp(command, "MESSAGETO") == 0) {
                tick();
                send_msg(proc_id, MSG);
            } else if (strcmp(command, "LOCK") == 0) {
                // TODO implementar LOCL
            } else if (strcmp(command, "UNLOCK") == 0) {
                // TODO implementar UNLOCK
            }
        }
    }
    return 0;
}
