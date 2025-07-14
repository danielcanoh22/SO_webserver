#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#define MAXBUF (8192)

/**
 * @brief Obtiene el tiempo actual del sistema con precisión de microsegundos.
 *
 * @return Un valor double que representa el número de segundos desde la Época (Epoch),
 * incluyendo la fracción de microsegundos.
 */
double get_seconds() {
    struct timeval t;
    int rc = gettimeofday(&t, NULL);
    assert(rc == 0);
    return (double)((double)t.tv_sec + (double)t.tv_usec / 1e6);
}

/**
 * @brief Función principal del programa CGI de prueba.
 * * Este programa está diseñado para ser ejecutado por un servidor web. Su propósito
 * es doble:
 * 1. Simular una tarea de larga duración, "esperando" (spinning) un número de
 * segundos especificado en la query string de la URL (ej. spin.cgi?5).
 * 2. Leer y procesar datos enviados a través del método POST. Lee la cantidad
 * de bytes especificada por la variable de entorno CONTENT_LENGTH desde
 * la entrada estándar (stdin).
 *
 * El programa escribe los datos recibidos por POST en un archivo de log
 * (log_post.txt) y genera una respuesta HTML que informa sobre sus acciones.
 *
 * @param argc El contador de argumentos de la línea de comandos.
 * @param argv El vector de argumentos de la línea de comandos.
 * @return 0 al finalizar la ejecución exitosamente.
 */
int main(int argc, char *argv[]) {
    char *query_string;
    int spin_for = 0;
    if ((query_string = getenv("QUERY_STRING")) != NULL) {
        spin_for = (double)atoi(query_string);
    }

    double t1 = get_seconds();
    while ((get_seconds() - t1) < spin_for)
        sleep(1);
    double t2 = get_seconds();

    char post_data[MAXBUF];
    int content_length = 0;
    char* len_str = getenv("CONTENT_LENGTH");
    if (len_str) {
        content_length = atoi(len_str);
    }
    if (content_length > 0 && content_length < MAXBUF) {
        fread(post_data, 1, content_length, stdin);
        post_data[content_length] = '\0';
    } else {
        strcpy(post_data, "No se recibieron datos por POST.");
    }

    FILE *log_file = fopen("log_post.txt", "a");
    if (log_file != NULL) {
        time_t now = time(NULL);
        fprintf(log_file, "Fecha: %s", ctime(&now));
        fprintf(log_file, "Datos: %s\n", post_data);
        fprintf(log_file, "--------------------------------\n");
        fclose(log_file);
    }

    char content[MAXBUF];
    sprintf(content, "<h2>Peticion procesada!</h2>\r\n");
    sprintf(content, "%s<p>He esperado %.2f segundos.</p>\r\n", content, t2 - t1);
    sprintf(content, "%s<p style='color:green;'><b>¡Datos guardados exitosamente en 'log_post.txt'!</b></p>\r\n", content);
    sprintf(content, "%s<hr><h3>Datos recibidos por POST:</h3><pre>%s</pre>\r\n", content, post_data);
    
    printf("Content-Length: %lu\r\n", strlen(content));
    printf("Content-Type: text/html\r\n\r\n");
    printf("%s", content);
    fflush(stdout);
    
    exit(0);
}