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
#define MAX_N_PROCESSES 10
#define MAX_LINE_SIZE 80
#define MAX_SECTION_SIZE 20
#define MSG 0
#define LOCK 1
#define OK 2

typedef struct Message {
    int type;
    char section[MAX_SECTION_SIZE];
    int LC[MAX_N_PROCESSES];
} Message;

typedef struct Process {
    char *p_id;
    int p_port;
    int p_socket;
    int arr_index;
    struct addrinfo *addr;
} Process;

static int LC[MAX_N_PROCESSES];
static Process *myself;
static int N_p_arr = 0;
static Process *process_arr[MAX_N_PROCESSES];
static int process_fd;
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

Process *get_Process_in_arr(char *proc_id) {
    Process *process = NULL;
    for (int i = 0; i < MAX_N_PROCESSES; i++) {
        process = process_arr[i];
        if (strcmp(process->p_id, proc_id) == 0) break;
    }
    return process;
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
    for (int i = 0; i < N_p_arr; i++) {
        printf("%d,", LC[i]);
    }
    printf("]\n");
}

void receive() {
    // tick(process);
    // print_tick(process);
    // printf("%s: RECEIVE(MSG, %s)\n", process->p_id, process->p_id);
}

Message *create_Message(int type, char *section) {
    Message *m = NULL;
    switch (type) {
        case 0: {
            m = malloc(sizeof(Message));
            m->type = type;
            memcpy(m->LC, LC, MAX_N_PROCESSES * sizeof(int));
        } break;

        default:
            break;
    }
    return m;
}

int send_msg(char *proc_id, int type) {
    Process *p = get_Process_in_arr(proc_id);
    if (p == NULL) {
        return -1;
    }
    switch (type) {
        case MSG: {
            printf("SEND(MSG,%s)\n", proc_id);
            Message *msg = create_Message(MSG, NULL);
            if (sendto(p->p_socket, &msg, sizeof(msg), 0, p->addr->ai_addr, p->addr->ai_addrlen) < 0) {
                perror("error in sendto MSG");
                return -1;
            }
        } break;
        case LOCK: {
        } break;
        case OK: {
        } break;
        default:
            break;
    }
    return 0;
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
    struct sockaddr_in process_addr;
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
    socklen_t len = sizeof(process_addr);
    if (getsockname(process_fd, (struct sockaddr *)&process_addr, &len) == -1) {
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
    for (; fgets(line, MAX_LINE_SIZE, stdin);) {
        if (!strcmp(line, "START\n"))
            break;
        sscanf(line, "%[^:]: %d", proc, &udp_port);
        if (strcmp(proc, argv[1]) == 0) {
            myself = create_Process(proc, udp_port, N_p_arr);
            process_arr[N_p_arr] = myself;
            N_p_arr++;
        } else {
            Process *process = create_Process(proc, udp_port, N_p_arr);
            process_arr[N_p_arr] = process;
            N_p_arr++;
            fprintf(stderr, "Created new Process %s with port %d\n", process->p_id, process->p_port);
        }
    }

    // Logical clock creation
    fprintf(stderr, "LIST OF REGISTERED PROCESSES: [");
    for (int i = 0; i < N_p_arr; i++) {
        Process *p = process_arr[i];
        fprintf(stderr, " {%s : %d} ", p->p_id, p->p_port);
        LC[i] = 0;  // Clock initialization
    }
    fprintf(stderr, "]");

    /* Procesar Acciones */
    char command[MAX_LINE_SIZE], proc_id[MAX_LINE_SIZE];
    for (; fgets(line, MAX_LINE_SIZE, stdin);) {
        if (strcmp(line, "GETCLOCK") == 0) {
            print_clock();
        } else if (strcmp(line, "FINISH") == 0) {
            exit(0);
        } else if (strcmp(line, "EVENT") == 0) {
            tick();
            print_tick();
        } else if (strcmp(line, "RECIEVE") == 0) {
            receive();
            print_tick();
        } else {
            sscanf(line, "%[^:]: %s", command, proc_id);
            if (strcmp(command, "MESSAGETO")) {
                tick();
                send_msg(proc_id, MSG);
            } else if (strcmp(command, "LOCK")) {
            } else if (strcmp(command, "UNLOCK")) {
            }
        }
    }
    return 0;
}
