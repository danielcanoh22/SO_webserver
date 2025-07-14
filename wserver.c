#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <limits.h>

#include "request.h"
#include "io_helper.h"

// --- Variables Globales ---
// El estado compartido del servidor, incluyendo la configuración, el búfer de
// peticiones y las primitivas de sincronización. Se inicializan en main().
char default_root[] = ".";
#define MAXBUF (8192) 

off_t get_sff_filesize_peek(int conn_fd, const char* root_dir_path_for_stat);
void *worker_routine(void *arg);

typedef struct {
    int conn_fd; // Descriptor de archivo para la conexión del cliente.
    off_t file_size_for_sff; // Tamaño del archivo solicitado (solo para SFF).
} request_entry_t;

request_entry_t *requests_buffer; // Búfer compartido para las peticiones.
int num_threads_global; // Número de hilos trabajadores.
int buffer_slots_global; // Capacidad del búfer.
char *sched_alg_global; // Algoritmo de planificación (FIFO o SFF).
char *root_dir_global; // Directorio raíz del servidor.

volatile int buffer_count_global; // Número actual de peticiones en el búfer.
int buffer_in_idx; // Índice para añadir peticiones (productor).
int buffer_out_idx; // Índice para sacar peticiones (consumidor).

pthread_mutex_t buffer_mutex_global; // Mutex para proteger el acceso al búfer.
pthread_cond_t buffer_not_full_cond; // Condición para cuando el búfer no está lleno.
pthread_cond_t buffer_not_empty_cond; // Condición para cuando el búfer no está vacío.

/**
 * @brief Inspecciona una petición para obtener el tamaño del archivo solicitado.
 * * Esta función es una ayuda para la política de planificación SFF. Utiliza
 * recv() con la bandera MSG_PEEK para leer los datos iniciales de una petición
 * sin consumirlos del socket. Parsea la URI para determinar el nombre del
 * archivo y usa stat() para obtener su tamaño.
 *
 * @param conn_fd El descriptor de archivo de la conexión.
 * @param root_dir_path_for_stat El directorio raíz del servidor.
 * @return El tamaño del archivo en bytes (off_t) en caso de éxito, o un
 * valor negativo en caso de error o si no es una petición GET válida.
 */
off_t get_sff_filesize_peek(int conn_fd, const char* root_dir_path_for_stat) {
    char peek_buf[MAXBUF];
    char method[MAXBUF], uri_from_req[MAXBUF], version[MAXBUF]; 
    char filename[MAXBUF];
    struct stat sbuf;

    (void)root_dir_path_for_stat; 

    ssize_t n = recv(conn_fd, peek_buf, MAXBUF - 1, MSG_PEEK);
    if (n <= 0) {
        return -5; 
    }
    peek_buf[n] = '\0'; 

    char *first_line_end = strstr(peek_buf, "\r\n");
    if (!first_line_end) {
        first_line_end = strchr(peek_buf, '\n'); 
        if (!first_line_end) {
            return -6;
        }
    }
    *first_line_end = '\0'; 

    if (sscanf(peek_buf, "%s %s %s", method, uri_from_req, version) != 3) {
        return -7;
    }
    
    if (strcasecmp(method, "GET") != 0) {
        return -8; 
    }

    if (strstr(uri_from_req, "..")) {
        return -2; 
    }

    char temp_uri_for_parsing[MAXBUF];
    strncpy(temp_uri_for_parsing, uri_from_req, MAXBUF-1);
    temp_uri_for_parsing[MAXBUF-1] = '\0';

    char *ptr_sff;
    if (!strstr(temp_uri_for_parsing, "cgi")) { 
        snprintf(filename, MAXBUF, ".%s", temp_uri_for_parsing);
        if (temp_uri_for_parsing[strlen(temp_uri_for_parsing)-1] == '/') {
            strncat(filename, "index.html", MAXBUF - strlen(filename) - 1);
        }
    } else { 
        ptr_sff = strchr(temp_uri_for_parsing, '?');
        if (ptr_sff) {
            *ptr_sff = '\0';
        }
        snprintf(filename, MAXBUF, ".%s", temp_uri_for_parsing);
    }

    if (stat(filename, &sbuf) < 0) {
        return -1; 
    }

    return sbuf.st_size; 
}

/**
 * @brief La rutina ejecutada por cada hilo trabajador (consumidor).
 * * En un bucle infinito, el hilo espera a que haya peticiones en el búfer.
 * Cuando hay trabajo disponible, extrae una petición según la política de
 * planificación (FIFO o SFF), la procesa llamando a request_handle(), y
 * finalmente cierra la conexión.
 *
 * @param arg El ID numérico del trabajador, pasado como un puntero.
 * @return NULL.
 */
void *worker_routine(void *arg) {
    long worker_id_arg = (long)arg; 
		pthread_t self_id = pthread_self();

		printf("[WORKER %ld/%lx] Hilo iniciado y listo.\n", worker_id_arg, (unsigned long)self_id);

    while (1) {
        int fd_to_process = -1;
        
        pthread_mutex_lock(&buffer_mutex_global);

        while (buffer_count_global == 0) {
						printf("[WORKER %ld/%lx] Buffer vacío. Esperando...\n", worker_id_arg, (unsigned long)self_id);
            pthread_cond_wait(&buffer_not_empty_cond, &buffer_mutex_global);
						printf("[WORKER %ld/%lx] Despertado. Buffer ya no está vacío.\n", worker_id_arg, (unsigned long)self_id);
        }

        if (strcmp(sched_alg_global, "FIFO") == 0) {
            fd_to_process = requests_buffer[buffer_out_idx].conn_fd;
        } else {
            int SFF_chosen_idx_in_buffer_array = -1; 
            off_t SFF_min_size = 0; 

            for (int i = 0; i < buffer_count_global; i++) {
                int current_item_actual_idx = (buffer_out_idx + i) % buffer_slots_global;
                off_t current_size = requests_buffer[current_item_actual_idx].file_size_for_sff;

                if (current_size >= 0) { 
                    if (SFF_chosen_idx_in_buffer_array == -1 || current_size < SFF_min_size) {
                        SFF_min_size = current_size;
                        SFF_chosen_idx_in_buffer_array = current_item_actual_idx;
                    }
                }
            }

            if (SFF_chosen_idx_in_buffer_array == -1) { 
                 if (buffer_count_global > 0) { 
                    SFF_chosen_idx_in_buffer_array = buffer_out_idx; 
                 } else {
                    pthread_mutex_unlock(&buffer_mutex_global);
                    continue; 
                 }
            }
            
            if (SFF_chosen_idx_in_buffer_array != buffer_out_idx) {
                request_entry_t temp = requests_buffer[buffer_out_idx];
                requests_buffer[buffer_out_idx] = requests_buffer[SFF_chosen_idx_in_buffer_array];
                requests_buffer[SFF_chosen_idx_in_buffer_array] = temp;
            }
            fd_to_process = requests_buffer[buffer_out_idx].conn_fd;
        }
        
        buffer_out_idx = (buffer_out_idx + 1) % buffer_slots_global;
        buffer_count_global--;

        pthread_cond_signal(&buffer_not_full_cond); 
        pthread_mutex_unlock(&buffer_mutex_global);

        if (fd_to_process != -1) {
						printf("[WORKER %ld/%lx] Procesando FD=%d...\n", worker_id_arg, (unsigned long)self_id, fd_to_process);
            request_handle(fd_to_process, root_dir_global); 

						printf("[WORKER %ld/%lx] Finalizado FD=%d. Cerrando conexión.\n", worker_id_arg, (unsigned long)self_id, fd_to_process);
            close_or_die(fd_to_process);
        }
    }
    return NULL;
}

/**
 * @brief Función principal del servidor web.
 * * Actúa como el orquestador y el productor. Inicializa el servidor,
 * parsea los argumentos de la línea de comandos, crea el pool de hilos
 * trabajadores, y entra en un bucle infinito para aceptar nuevas conexiones
 * y encolarlas en el búfer compartido.
 *
 * @param argc El número de argumentos.
 * @param argv El vector de argumentos.
 * @return 0 en caso de éxito.
 */
int main(int argc, char *argv[]) {
    // Parseo de argumentos
    int c;
    char *root_dir_arg = default_root;
    int port = 10000;
    int num_threads_arg = 1;
    int num_buffers_arg = 1;
    char *sched_alg_arg = "FIFO";

    while ((c = getopt(argc, argv, "d:p:t:b:s:")) != -1) {
        switch (c) {
        case 'd':
            root_dir_arg = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            if (port < 0 || port > 65535) {
                fprintf(stderr, "Número de puerto invalido %d. Debe estar 0-65535.\n", port);
                exit(1);
            }
            break;
        case 't':
            num_threads_arg = atoi(optarg);
            if (num_threads_arg <= 0) {
                fprintf(stderr, "El número de hilos debe ser positivo\n");
                exit(1);
            }
            break;
        case 'b':
            num_buffers_arg = atoi(optarg);
            if (num_buffers_arg <= 0) {
                fprintf(stderr, "El número de buffers debe ser positivo\n");
                exit(1);
            }
            break;
        case 's':
            sched_alg_arg = optarg;
            if (strcmp(sched_alg_arg, "FIFO") != 0 && strcmp(sched_alg_arg, "SFF") != 0) {
                fprintf(stderr, "La política de agendamiento debe ser FIFO o SFF\n");
                exit(1);
            }
            break;
        default:
            fprintf(stderr, "Uso: wserver [-d basedir] [-p port] [-t threads] [-b buffers] [-s schedalg]\n");
            exit(1);
        }
    }

    // Inicialización del servidor
    num_threads_global = num_threads_arg;
    buffer_slots_global = num_buffers_arg;
    sched_alg_global = strdup(sched_alg_arg); 
    root_dir_global = strdup(root_dir_arg);   

    chdir_or_die(root_dir_global);

    // Asignación de memoria para el búfer y las primitivas de sincronización
    requests_buffer = (request_entry_t *)malloc(sizeof(request_entry_t) * buffer_slots_global);
    if (requests_buffer == NULL) {
        perror("No se pudo asignar el búfer de solicitudes");
        free(sched_alg_global);
        free(root_dir_global);
        exit(1);
    }
    buffer_count_global = 0;
    buffer_in_idx = 0;
    buffer_out_idx = 0;

    pthread_mutex_init(&buffer_mutex_global, NULL);
    pthread_cond_init(&buffer_not_full_cond, NULL);
    pthread_cond_init(&buffer_not_empty_cond, NULL);

    // Creación del pool de hilos trabajadores
    pthread_t *worker_threads_arr = (pthread_t *)malloc(sizeof(pthread_t) * num_threads_global);
    if (worker_threads_arr == NULL) {
        perror("No se pudo asignar la matriz de subprocesos de trabajo");
        free(requests_buffer); 
        free(sched_alg_global);
        free(root_dir_global);
        exit(1);
    }
    for (long i = 0; i < num_threads_global; i++) {
        if (pthread_create(&worker_threads_arr[i], NULL, worker_routine, (void *)i) != 0) {
            perror("No se pudo crear el hilo de trabajo");
            free(requests_buffer);
            free(worker_threads_arr); 
            free(sched_alg_global);
            free(root_dir_global);
            exit(1); 
        }
    }

    // Bucle principal del productor
    int listen_fd = open_listen_fd_or_die(port);
    printf("Servidor escuchando en el puerto %d con %d hilos, %d buffers, %s scheduling, root dir %s\n",
           port, num_threads_global, buffer_slots_global, sched_alg_global, root_dir_global);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr); 
        int conn_fd = accept_or_die(listen_fd, (sockaddr_t *)&client_addr, &client_len);
				printf("[MASTER] Conexión aceptada: FD=%d\n", conn_fd);

        // Preprocesamiento para SFF
        request_entry_t current_req_entry;
        current_req_entry.conn_fd = conn_fd;
        current_req_entry.file_size_for_sff = 0; 

        if (strcmp(sched_alg_global, "SFF") == 0) {
            off_t size = get_sff_filesize_peek(conn_fd, root_dir_global);
            current_req_entry.file_size_for_sff = size;
        }

        // Añadir al buffer
        pthread_mutex_lock(&buffer_mutex_global);
				printf("[MASTER] Intentando encolar FD=%d. Buffer actual: %d/%d\n", conn_fd, buffer_count_global, buffer_slots_global);

        // Espera si el buffer está lleno
        while (buffer_count_global == buffer_slots_global) {
						printf("[MASTER] Buffer lleno. Esperando para encolar FD=%d...\n", conn_fd);
            pthread_cond_wait(&buffer_not_full_cond, &buffer_mutex_global);
						printf("[MASTER] Despertado. Buffer ya no está lleno. Intentando encolar FD=%d de nuevo.\n", conn_fd);
        }

        requests_buffer[buffer_in_idx] = current_req_entry;
				int enqueued_at_idx = buffer_in_idx;
        buffer_in_idx = (buffer_in_idx + 1) % buffer_slots_global;
        buffer_count_global++;

				printf("[MASTER] FD=%d encolado en slot %d. Buffer ahora: %d/%d\n", conn_fd, enqueued_at_idx, buffer_count_global, buffer_slots_global);
        
        // Avisa a un trabajador que hay trabajo disponible
        pthread_cond_signal(&buffer_not_empty_cond);
        pthread_mutex_unlock(&buffer_mutex_global);
    }

    for (int i = 0; i < num_threads_global; i++) {
        pthread_join(worker_threads_arr[i], NULL); 
    }
    free(requests_buffer);
    free(worker_threads_arr);
    free(sched_alg_global);
    free(root_dir_global);
    pthread_mutex_destroy(&buffer_mutex_global);
    pthread_cond_destroy(&buffer_not_full_cond);
    pthread_cond_destroy(&buffer_not_empty_cond);
    close_or_die(listen_fd); 

    return 0;
}