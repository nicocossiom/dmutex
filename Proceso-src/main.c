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
#include "queue.h"

#define MAX_N_PROCESSES 10
#define MAX_LINE_SIZE 80
#define MAX_SECTION_SIZE 20
#define MSG 0
#define LOCK 1
#define OK 2
#define OKUNLOCK 3

// message sent and recieved between processes
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

// A representation of a process
typedef struct Process {
    // this process'sidentifier
    char *p_id;

    // this process's port
    int p_port;

    // this process's socket
    int p_socket;

    // this process's index in the LC
    int arr_index;

    // this process's addrinfo
    struct addrinfo *addr;

} Process;

// A representation of a section
typedef struct Section {
    // this section's id (name)
    char section_id[MAX_SECTION_SIZE];

    // this section's last clock given by a process to this section
    int section_LC[MAX_N_PROCESSES];

    // this section's amount of OKs recieved
    int okeys;

    // this section's flag that indicates if some process has exlusive accces to it, if 0 yes, -1 if not
    int is_locked;

    // this section's queue of processes to be sent an OK message when unlocked
    queue *process_queue;

} Section;

// This process's logical clock
static int *LC;

// This process's process
static Process *myself;

// This process's # of processes
static int N_processes = 0;

// This process's socket_fd
static int process_fd;

// This process's socket_address
struct sockaddr_in process_addr;

// This process's socket_address_length
socklen_t process_addr_len = sizeof(process_addr);

// This process's map of all the processes connected
static map *process_map;

// This process's last section command argument, used when calling lock since map_visit doesn't accept function arguments
char g_section_placeholder[MAX_SECTION_SIZE];

// This process's map of all the sections available
static map *section_map;

// creates a proces given its id port and position in the LC
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

// +1 on this process's LC value
void tick() {
    LC[myself->arr_index] += 1;
    printf("%s: TICK\n", myself->p_id);
}

// prints this process's LC
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

// combines this process's LC with the given LC, getting the max values for each of LC's positions
void combine_LCs(int ext_LC[MAX_N_PROCESSES]) {
    for (int i = 0; i < N_processes; i++) {
        int local_k = LC[i];
        int ext_k = ext_LC[i];
        if (ext_k > local_k) {
            LC[i] = ext_k;
        }
    }
}

// creates a message given the message type and the section, it also includes the sender's id and its LC
Message *create_Message(int type, char *section) {
    Message *m = malloc(sizeof(Message));
    m->type = type;
    strcpy(m->section, section);
    memcpy(m->LC, LC, N_processes * sizeof(int));
    strcpy(m->sender_id, myself->p_id);
    return m;
}

// creates a section given the section's name and intializes its values
Section *create_Section(char *section_name) {
    Section *s = malloc(sizeof(Section));
    strcpy(s->section_id, section_name);
    memcpy(s->section_LC, LC, N_processes * sizeof(int));
    s->okeys = 0;
    s->is_locked = -1;
    s->process_queue = queue_create(0);
    return s;
}

/* func_entry_map_t that sends a lock message containing the type, section to be locked and this processe's LC,
to all each process of the process map but itself (multicast) */
void send_LOCK_msg_multicast_map(void *key, void *v) {
    Process *p = (Process *)v;
    // we send the message to everyone but ourselves
    if (p->arr_index != myself->arr_index) {
        Message *msg = create_Message(LOCK, g_section_placeholder);
        printf("%s: SEND(LOCK,%s)\n", myself->p_id, p->p_id);
        // we can use sizeof(Message) since we're not working with pointers
        if (sendto(p->p_socket, msg, sizeof(Message), 0, p->addr->ai_addr, p->addr->ai_addrlen) < 0) {
            perror("error in sendto MSG");
        }
    }
}

// sends the appropiate message depending on the given type MSG | LOCK | OK
int send_msg(char *proc_id, int type) {
    switch (type) {
        case MSG: {
            int map_op;
            Process *p = map_get(process_map, proc_id, &map_op);
            if (map_op == -1) {
                return -1;
            }
            printf("%s: SEND(MSG,%s)\n", myself->p_id, proc_id);
            Message *msg = create_Message(MSG, NULL);
            // we can use sizeof(Message) since we're not working with pointers
            if (sendto(p->p_socket, msg, sizeof(Message), 0, p->addr->ai_addr, p->addr->ai_addrlen) < 0) {
                perror("error in sendto MSG");
                return -1;
            }
        } break;
        case LOCK: {
            map_visit(process_map, send_LOCK_msg_multicast_map);
        } break;
        case OKUNLOCK: {
            int map_op;
            Section *section = map_get(section_map, g_section_placeholder, &map_op);
            int queue_op = 0;
            while (queue_length(section->process_queue) != 0) {
                tick();
                Process *p_to_ok = queue_pop_front(section->process_queue, &queue_op);
                Message *msg = create_Message(OK, section->section_id);
                // we can use sizeof(Message) since we're not working with pointers
                if (sendto(p_to_ok->p_socket, msg, sizeof(Message), 0, p_to_ok->addr->ai_addr, p_to_ok->addr->ai_addrlen) < 0) {
                    perror("error in sendto OK");
                    return -1;
                }
                printf("%s: SEND(OK,%s)\n", myself->p_id, p_to_ok->p_id);
            }
        } break;
        case OK: {
            int map_op;
            Process *p_to_ok = map_get(process_map, proc_id, &map_op);
            tick();
            fprintf(stdout, "%s: SEND(OK,%s)\n", myself->p_id, p_to_ok->p_id);
            Message *msg = create_Message(OK, g_section_placeholder);
            // we can use sizeof(Message) since we're not working with pointers
            if (sendto(p_to_ok->p_socket, msg, sizeof(Message), 0, p_to_ok->addr->ai_addr, p_to_ok->addr->ai_addrlen) < 0) {
                perror("error in sendto OK");
                return -1;
            }
        } break;
        default:
            break;
    }
    return 0;
}

/* creates a section if it's not in this process's map and adds it to said map.
Otherwhise it updates the given section's LC with this process's current one */
Section *section_save_if_needed_n_update_clock(char *section, int opt) {
    int map_op;
    Section *s = map_get(section_map, section, &map_op);
    if (map_op == -1) {
        // create the section since it doesnt exist (it will have the mopst up to date clock)
        s = create_Section(section);
        map_put(section_map, s->section_id, s);
    }
    if (opt == LOCK) {
        // update section's clock
        memcpy(s->section_LC, LC, N_processes * sizeof(int));
    }
    return s;
}

// function that recieves the next struct Message and acts accordingly depending on type: MSG | OK | LOCK
void receive() {
    Message *message = malloc(sizeof(Message));
    recvfrom(process_fd, message, sizeof(Message), 0, (struct sockaddr *)&process_addr, &process_addr_len);
    combine_LCs(message->LC);
    switch (message->type) {
        case MSG: {
            printf("%s: RECEIVE(MSG,%s)\n", myself->p_id, message->sender_id);
            tick();

        } break;
        case OK: {
            printf("%s: RECEIVE(OK,%s)\n", myself->p_id, message->sender_id);
            tick();
            int map_op;
            Section *section = map_get(section_map, message->section, &map_op);
            section->okeys++;
            if (section->okeys == N_processes - 1) {
                printf("%s: MUTEX(%s)\n", myself->p_id, (char *)section->section_id);
                section->is_locked = 0;
            }
        } break;
        case LOCK: {
            printf("%s: RECEIVE(LOCK,%s)\n", myself->p_id, message->sender_id);
            tick();
            Section *section = section_save_if_needed_n_update_clock(message->section, LOCK);
            strcpy(g_section_placeholder, message->section);
            // section is not locked so we have to send an okey to the sender
            if (section->is_locked == -1) {
                send_msg(message->sender_id, OK);
            } else {
                // since section is locked we need to add the sender of the lock request
                int map_op;
                Process *p = map_get(process_map, message->sender_id, &map_op);
                queue_push_back(section->process_queue, p);
            }
        } break;
        default: {
            perror("receive msg switch");
        } break;
    }
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
    section_map = map_create(key_string, 0);
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
    // LC creation
    N_processes = map_size(process_map);
    LC = (int *)malloc(sizeof(int) * N_processes);
    for (int i = 0; i < N_processes; i++) {
        LC[i] = 0;  // Clock initialization
    }
    // debug info
    fprintf(stderr, "LIST OF REGISTERED PROCESSES: [");
    map_visit(process_map, print_process);
    fprintf(stderr, "]\n");

    // command to execute
    char command[MAX_LINE_SIZE];

    // command arg
    char comm_arg[MAX_LINE_SIZE];
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
            sscanf(line, "%s %s", command, comm_arg);
            strcpy(g_section_placeholder, comm_arg);
            if (strcmp(command, "MESSAGETO") == 0) {
                tick();
                send_msg(comm_arg, MSG);
            } else if (strcmp(command, "LOCK") == 0) {
                tick();
                section_save_if_needed_n_update_clock(g_section_placeholder, -1);
                send_msg(NULL, LOCK);

            } else if (strcmp(command, "UNLOCK") == 0) {
                send_msg(NULL, OKUNLOCK);
            }
        }
    }
    return 0;
}
