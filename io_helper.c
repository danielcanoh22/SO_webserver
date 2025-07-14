#include "io_helper.h"

/**
 * @brief Lee una línea de texto de un descriptor de archivo, terminada por '\n'.
 * * Esta función lee caracteres uno por uno desde un descriptor de archivo (fd)
 * hasta que encuentra un carácter de nueva línea ('\n'), se alcanza el final
 * del archivo (EOF), o se llena el búfer (maxlen). La línea leída se almacena
 * en 'buf' y se le añade un terminador nulo.
 *
 * @param fd El descriptor de archivo del cual se va a leer.
 * @param buf El búfer donde se almacenará la línea leída.
 * @param maxlen El tamaño máximo del búfer.
 *
 * @return El número de bytes leídos en caso de éxito, 0 si se encuentra EOF 
 * sin leer datos, o -1 en caso de error.
 */
ssize_t readline(int fd, void *buf, size_t maxlen) {
    char c;
    char *bufp = buf;
    int n;
    for (n = 0; n < maxlen - 1; n++) {
	int rc;
        if ((rc = read_or_die(fd, &c, 1)) == 1) {
            *bufp++ = c;
            if (c == '\n')
                break;
        } else if (rc == 0) {
            if (n == 1)
                return 0; /* EOF, no data read */
            else
                break;    /* EOF, some data was read */
        } else
            return -1;    /* error */
    }
    *bufp = '\0';
    return n;
}

/**
 * @brief Abre una conexión de red con un servidor y devuelve un descriptor de archivo.
 * * Esta función actúa como un cliente de red. Crea un socket, resuelve el
 * nombre de host a una dirección IP y establece una conexión con el servidor
 * en el puerto especificado.
 *
 * @param hostname El nombre de host o la dirección IP del servidor.
 * @param port El puerto en el servidor al que se desea conectar.
 *
 * @return Un descriptor de archivo para el socket de cliente en caso de éxito, 
 * o un valor negativo en caso de error.
 */
int open_client_fd(char *hostname, int port) {
    int client_fd;
    struct hostent *hp;
    struct sockaddr_in server_addr;
    
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1; 
    
    if ((hp = gethostbyname(hostname)) == NULL)
        return -2;
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char *) hp->h_addr, 
          (char *) &server_addr.sin_addr.s_addr, hp->h_length);
    server_addr.sin_port = htons(port);

    if (connect(client_fd, (sockaddr_t *) &server_addr, sizeof(server_addr)) < 0)
        return -1;
    return client_fd;
}

/**
 * @brief Crea y prepara un socket de escucha para un servidor.
 * * Esta función realiza los pasos necesarios para que un servidor acepte 
 * conexiones entrantes: crea un socket, configura la opción SO_REUSEADDR 
 * para permitir reinicios rápidos del servidor, lo enlaza (bind) a un 
 * puerto específico en todas las interfaces de red locales y lo pone en 
 * modo de escucha (listen).
 *
 * @param port El puerto en el que el servidor escuchará las conexiones.
 *
 * @return Un descriptor de archivo para el socket de escucha en caso de éxito, 
 * o -1 en caso de error.
 */
int open_listen_fd(int port) {
    int listen_fd;
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	fprintf(stderr, "socket() failed\n");
	return -1;
    }
    
    int optval = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval, sizeof(int)) < 0) {
	fprintf(stderr, "setsockopt() failed\n");
	return -1;
    }
    
    struct sockaddr_in server_addr;
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET; 
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_addr.sin_port = htons((unsigned short) port); 
    if (bind(listen_fd, (sockaddr_t *) &server_addr, sizeof(server_addr)) < 0) {
	fprintf(stderr, "bind() failed\n");
	return -1;
    }
    
    if (listen(listen_fd, 1024) < 0) {
	fprintf(stderr, "listen() failed\n");
	return -1;
    }
    return listen_fd;
}


