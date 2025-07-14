#include "io_helper.h"
#include "request.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

#define MAXBUF (8192)

/**
 * @brief Envía una página de error HTTP formateada al cliente.
 * * Construye y envía una respuesta HTTP completa con un cuerpo en HTML que 
 * detalla un error específico. Es utilizada para notificar al cliente sobre 
 * problemas como archivos no encontrados (404) o métodos no implementados (501).
 *
 * @param fd El descriptor de archivo de la conexión con el cliente.
 * @param cause La causa específica del error.
 * @param errnum El código de estado HTTP.
 * @param shortmsg Un mensaje corto que describe el error.
 * @param longmsg Un mensaje más detallado para el cuerpo de la página.
 */
void request_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXBUF], body[MAXBUF];
    
    sprintf(body, ""
	    "<!doctype html>\r\n"
	    "<head>\r\n"
	    "  <title>OSTEP WebServer Error</title>\r\n"
	    "</head>\r\n"
	    "<body>\r\n"
	    "  <h2>%s: %s</h2>\r\n" 
	    "  <p>%s: %s</p>\r\n"
	    "</body>\r\n"
	    "</html>\r\n", errnum, shortmsg, longmsg, cause);
    
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    write_or_die(fd, buf, strlen(buf));
    
    sprintf(buf, "Content-Type: text/html\r\n");
    write_or_die(fd, buf, strlen(buf));
    
    sprintf(buf, "Content-Length: %lu\r\n\r\n", strlen(body));
    write_or_die(fd, buf, strlen(buf));
    
    write_or_die(fd, body, strlen(body));
}

/**
 * @brief Lee los encabezados de una petición HTTP y extrae el valor de Content-Length.
 * * Itera sobre todas las líneas de encabezado de una petición HTTP hasta encontrar
 * una línea vacía. Durante la iteración, busca específicamente el encabezado 
 * "Content-Length" y, si lo encuentra, almacena su valor numérico.
 *
 * @param fd El descriptor de archivo de la conexión.
 * @return El valor del Content-Length si se encuentra; de lo contrario, 0.
 */
int request_parse_headers(int fd) {
    char buf[MAXBUF];
    char key[MAXBUF];
    int len = 0;
    
    readline_or_die(fd, buf, MAXBUF);
    while (strcmp(buf, "\r\n")) {
        if (sscanf(buf, "%[^:]: %d", key, &len) == 2) {
            if (strcasecmp(key, "Content-Length") == 0) {
            }
        } else {
        }
        readline_or_die(fd, buf, MAXBUF);
    }
    return len;
}

/**
 * @brief Parsea la URI para determinar el nombre de archivo y los argumentos CGI.
 * * Versión segura que utiliza snprintf para prevenir desbordamientos de búfer.
 *
 * @param uri La URI completa de la petición (ej. "/index.html").
 * @param filename Búfer de salida para almacenar la ruta del archivo en el sistema.
 * @param cgiargs Búfer de salida para almacenar los argumentos de la query string.
 * @return 1 si la petición es para contenido estático, 0 si es para contenido dinámico (CGI).
 */
int request_parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;
    
    if (!strstr(uri, "cgi")) {
        strcpy(cgiargs, "");
        snprintf(filename, MAXBUF, ".%s", uri);
        if (uri[strlen(uri)-1] == '/') {
            strncat(filename, "index.html", MAXBUF - strlen(filename) - 1);
        }
        return 1;
    } else {
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        } else {
            strcpy(cgiargs, "");
        }
        snprintf(filename, MAXBUF, ".%s", uri);
        return 0;
    }
}

/**
 * @brief Determina el tipo MIME de un archivo basado en su extensión.
 *
 * @param filename El nombre del archivo.
 * @param filetype Búfer de salida donde se almacenará el string del tipo MIME.
 */
void request_get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html")) 
	strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif")) 
	strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg")) 
	strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".pdf"))
    strcpy(filetype, "application/pdf");
    else if (strstr(filename, ".css"))
    strcpy(filetype, "text/css");   
    else if (strstr(filename, ".js"))
    strcpy(filetype, "application/javascript");
    else 
	strcpy(filetype, "text/plain");
}

/**
 * @brief Sirve una petición de contenido dinámico (CGI) que utiliza el método POST.
 * * Crea un proceso hijo para ejecutar el script CGI. Utiliza una tubería (pipe)
 * para redirigir el cuerpo de la petición POST al stdin del proceso hijo,
 * permitiendo que el script procese los datos enviados.
 *
 * @param fd El descriptor de archivo de la conexión.
 * @param filename La ruta del script CGI a ejecutar.
 * @param cgiargs Los argumentos de la query string (si los hay).
 * @param post_data El búfer que contiene los datos del cuerpo de la petición POST.
 * @param content_length El tamaño de los datos en post_data.
 */
void request_serve_dynamic_post(int fd, char *filename, char *cgiargs, char *post_data, int content_length) {
    char buf[MAXBUF], *argv[] = { NULL };

    sprintf(buf, "HTTP/1.0 200 OK\r\nServer: OSTEP WebServer\r\n");
    write_or_die(fd, buf, strlen(buf));

    int pipe_to_cgi[2];
    if (pipe(pipe_to_cgi) < 0) {
        request_error(fd, "Pipe Error", "500", "Internal Server Error", "Failed to create pipe.");
        return;
    }

    if (fork_or_die() == 0) {
        close(pipe_to_cgi[1]);
        dup2_or_die(pipe_to_cgi[0], STDIN_FILENO);
        close(pipe_to_cgi[0]);

        setenv_or_die("QUERY_STRING", cgiargs, 1);
        
        char len_str[20];
        sprintf(len_str, "%d", content_length);
        setenv_or_die("CONTENT_LENGTH", len_str, 1);
        
        dup2_or_die(fd, STDOUT_FILENO);
        extern char **environ;
        execve_or_die(filename, argv, environ);
    } else {
        close(pipe_to_cgi[0]);
        if (post_data != NULL && content_length > 0) {
            write_or_die(pipe_to_cgi[1], post_data, content_length);
        }
        close(pipe_to_cgi[1]);
        
        wait_or_die(NULL);
    }
}

/**
 * @brief Sirve una petición de contenido dinámico (CGI) que utiliza el método GET.
 * * Crea un proceso hijo para ejecutar el script CGI. Pasa los argumentos de la
 * query string a través de la variable de entorno QUERY_STRING.
 *
 * @param fd El descriptor de archivo de la conexión.
 * @param filename La ruta del script CGI a ejecutar.
 * @param cgiargs Los argumentos de la query string.
 */
void request_serve_dynamic(int fd, char *filename, char *cgiargs) {
    char buf[MAXBUF], *argv[] = { NULL };
    
    sprintf(buf, ""
	    "HTTP/1.0 200 OK\r\n"
	    "Server: OSTEP WebServer\r\n");
    
    write_or_die(fd, buf, strlen(buf));
    
    if (fork_or_die() == 0) {
	setenv_or_die("QUERY_STRING", cgiargs, 1);
	dup2_or_die(fd, STDOUT_FILENO);
	extern char **environ;
	execve_or_die(filename, argv, environ);
    } else {
	wait_or_die(NULL);
    }
}

/**
 * @brief Sirve una petición de contenido estático.
 * * Lee un archivo del disco y envía su contenido al cliente. Utiliza mmap()
 * para una transferencia de archivos eficiente. Determina y envía los 
 * encabezados HTTP apropiados, incluyendo Content-Type y Content-Length.
 *
 * @param fd El descriptor de archivo de la conexión.
 * @param filename La ruta del archivo a servir.
 * @param filesize El tamaño del archivo en bytes.
 */
void request_serve_static(int fd, char *filename, int filesize) {
    int srcfd;
    char *srcp, filetype[MAXBUF], buf[MAXBUF];
    
    request_get_filetype(filename, filetype);
    srcfd = open_or_die(filename, O_RDONLY, 0);
    
    srcp = mmap_or_die(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    close_or_die(srcfd);
    
    sprintf(buf, ""
	    "HTTP/1.0 200 OK\r\n"
	    "Server: OSTEP WebServer\r\n"
	    "Content-Length: %d\r\n"
	    "Content-Type: %s\r\n\r\n", 
	    filesize, filetype);
    
    write_or_die(fd, buf, strlen(buf));
    
    write_or_die(fd, srcp, filesize);
    munmap_or_die(srcp, filesize);
}

/**
 * @brief Maneja una petición HTTP completa.
 * * Esta es la función principal para procesar una solicitud. Lee la petición,
 * la parsea, y determina si es estática o dinámica. Llama a la función 
 * apropiada (request_serve_static o request_serve_dynamic) para generar y 
 * enviar la respuesta.
 *
 * @param fd El descriptor de archivo de la conexión del cliente.
 * @param root_dir El directorio raíz del servidor.
 */
void request_handle(int fd, const char *root_dir) {
    (void)root_dir; 
    int is_static;
    struct stat sbuf;
    char buf[MAXBUF], method[MAXBUF], uri[MAXBUF], version[MAXBUF];
    char filename[MAXBUF], cgiargs[MAXBUF];
    
    readline_or_die(fd, buf, MAXBUF);
    sscanf(buf, "%s %s %s", method, uri, version);
    printf("[REQUEST FD=%d] Manejando: Method=%s URI=%s Version=%s\n", fd, method, uri, version);
    fflush(stdout);

    if (strstr(uri, "..")) {
        request_error(fd, uri, "403", "Forbidden", "Path traversal attempt detected in URI.");
        return;
    }

    if (strcasecmp(method, "GET") != 0 && strcasecmp(method, "POST") != 0) {
        request_error(fd, method, "501", "Not Implemented", "server does not implement this method");
        return;
    }
    
    int content_length = request_parse_headers(fd);
    
    char *post_buffer = NULL;
    if (strcasecmp(method, "POST") == 0) {
        if (content_length > 0) {
            post_buffer = (char*)malloc(content_length + 1);
            if (post_buffer == NULL) {
                request_error(fd, "POST", "500", "Internal Server Error", "Memory allocation failed");
                return;
            }
            read_or_die(fd, post_buffer, content_length);
            post_buffer[content_length] = '\0';
        } else {
            request_error(fd, "POST", "411", "Length Required", "POST requests require a Content-Length header");
            return;
        }
    }
    
    is_static = request_parse_uri(uri, filename, cgiargs);

    if (stat(filename, &sbuf) < 0) {
        request_error(fd, filename, "404", "Not found", "server could not find this file");
        if (post_buffer) free(post_buffer);
        return;
    }
    
    if (is_static) {
        if (strcasecmp(method, "POST") == 0) {
            request_error(fd, filename, "405", "Method Not Allowed", "POST method is not supported for static content");
            if (post_buffer) free(post_buffer);
            return;
        }
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            request_error(fd, filename, "403", "Forbidden", "server could not read this file");
            return;
        }
        request_serve_static(fd, filename, sbuf.st_size);
    } else {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            request_error(fd, filename, "403", "Forbidden", "server could not run this CGI program");
            if (post_buffer) free(post_buffer);
            return;
        }

        if (strcasecmp(method, "POST") == 0) {
            request_serve_dynamic_post(fd, filename, cgiargs, post_buffer, content_length);
        } else {
            request_serve_dynamic(fd, filename, cgiargs);
        }
    }

    if (post_buffer) {
        free(post_buffer);
    }
}
