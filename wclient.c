#include "io_helper.h"

#define MAXBUF (8192)

/**
 * @brief Construye y envía una petición HTTP GET al servidor.
 * * @param fd El descriptor de archivo del socket conectado al servidor.
 * @param filename La ruta del recurso que se solicitará al servidor (ej. "/index.html").
 */
void client_send(int fd, char *filename) {
    char buf[MAXBUF];
    char hostname[MAXBUF];
    
    gethostname_or_die(hostname, MAXBUF);
    
    sprintf(buf, "GET %s HTTP/1.1\n", filename);
    sprintf(buf, "%shost: %s\n\r\n", buf, hostname);
    write_or_die(fd, buf, strlen(buf));
}

/**
 * @brief Lee la respuesta completa del servidor y la imprime en la salida estándar.
 * * Primero lee y muestra todos los encabezados de la respuesta HTTP, línea por
 * línea, hasta encontrar la línea vacía que los separa del cuerpo. Luego,
 * lee y muestra el cuerpo de la respuesta hasta que el servidor cierre la conexión.
 * * @param fd El descriptor de archivo del socket conectado al servidor.
 */
void client_print(int fd) {
    char buf[MAXBUF];  
    int n;
    
    n = readline_or_die(fd, buf, MAXBUF);
    while (strcmp(buf, "\r\n") && (n > 0)) {
	printf("Header: %s", buf);
	n = readline_or_die(fd, buf, MAXBUF);
    }
    
    n = readline_or_die(fd, buf, MAXBUF);
    while (n > 0) {
	printf("%s", buf);
	n = readline_or_die(fd, buf, MAXBUF);
    }
}

/**
 * @brief Función principal del cliente web.
 * * Este programa se conecta a un servidor web, envía una única petición GET
 * para un archivo específico y muestra la respuesta recibida en la consola.
 * * @usage ./wclient <host> <port> <filename>
 * * @param argc El número de argumentos de la línea de comandos. Debe ser 4.
 * @param argv El vector de argumentos: [nombre_programa, host, puerto, nombre_archivo].
 * * @return 0 en caso de éxito, 1 si los argumentos son incorrectos.
 */
int main(int argc, char *argv[]) {
    char *host, *filename;
    int port;
    int clientfd;
    
    if (argc != 4) {
	fprintf(stderr, "Uso: %s <host> <port> <filename>\n", argv[0]);
	exit(1);
    }
    
    host = argv[1];
    port = atoi(argv[2]);
    filename = argv[3];
    
    clientfd = open_client_fd_or_die(host, port);
    
    client_send(clientfd, filename);
    client_print(clientfd);
    
    close_or_die(clientfd);
    
    exit(0);
}
