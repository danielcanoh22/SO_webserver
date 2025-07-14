# Proyecto Final Sistemas Operativos - Servidor Web Concurrente

Este repositorio contiene la implementación de un servidor web multi-hilo, desarrollado como proyecto para el curso de Sistemas Operativos. El servidor está construido desde cero en lenguaje C y utiliza conceptos fundamentales de concurrencia y sincronización para manejar múltiples clientes de manera simultánea y eficiente.

## Estudiantes

Daniel Cano Hernández y Alejandra Díaz Hernández

## Características Principales

- **Modelo Concurrente:** Implementa un **pool de hilos** basado en el patrón **Productor-Consumidor** para manejar múltiples peticiones a la vez.
- **Políticas de Planificación:** Soporta dos algoritmos para la gestión de peticiones en cola:
  - `FIFO` (First-In, First-Out): Atiende las peticiones en el orden en que llegan.
  - `SFF` (Smallest File First): Prioriza las peticiones de archivos de menor tamaño para optimizar el tiempo de respuesta promedio.
- **Soporte HTTP:** Maneja los métodos `GET` para solicitar recursos y `POST` para enviar datos a scripts.
- **Tipos de Contenido:** Es capaz de servir tanto contenido **estático** (HTML, CSS, JS, imágenes, PDF) como **dinámico** a través de la ejecución de scripts **CGI**.
- **Sincronización Segura:** Utiliza **Mutex** y **Variables de Condición** de la librería `pthread` para garantizar un acceso seguro al búfer de peticiones y evitar condiciones de carrera.

## Arquitectura

El servidor se basa en una arquitectura Productor-Consumidor:

1.  **Hilo Maestro (Productor):** Acepta las conexiones entrantes y las coloca en un búfer compartido de tamaño fijo.
2.  **Pool de Hilos (Consumidores):** Un conjunto de hilos trabajadores espera a que haya peticiones en el búfer. Cuando una petición está disponible, un hilo la toma y la procesa por completo.
3.  **Sincronización:** El acceso al búfer está protegido por un mutex, y las variables de condición coordinan a los hilos para que esperen de forma eficiente cuando el búfer está vacío o lleno.

---

## Requisitos del Sistema

- **Sistema Operativo:** Un entorno tipo UNIX (probado en Linux).
- **Compilador:** `gcc` (GNU Compiler Collection).
- **Herramientas de Build:** `make`.
- **Librerías:** `pthread` (POSIX Threads), que es estándar en la mayoría de los sistemas UNIX.

---

## Compilación y Ejecución

Sigue estos pasos para compilar y ejecutar el servidor en local.

### 1\. Compilación

Desde la raíz del repositorio, ejecuta el siguiente comando. Este compilará el servidor (`wserver`), un cliente de prueba (`wclient`) y el script de ejemplo (`spin.cgi`).

```bash
make
```

### 2\. Preparación del Entorno

El servidor necesita un directorio desde el cual servir los archivos. En el repositorio se incluye el directorio `web_files` con algunos archivos de prueba.

```bash
# Mueve el script CGI compilado a ese directorio
mv spin.cgi web_files/
```

### 3\. Ejecución del Servidor

Ejecuta el servidor especificando los parámetros deseados.

```bash
./wserver -d web_files -p 8080 -t 8 -b 16 -s SFF
```

**Parámetros disponibles:**

- `-d <directorio>`: El directorio raíz desde donde se servirán los archivos (por defecto: `.` ).
- `-p <puerto>`: El puerto en el que escuchará el servidor (por defecto: `10000`).
- `-t <hilos>`: El número de hilos trabajadores en el pool (por defecto: `1`).
- `-b <buffers>`: El número de espacios en el búfer de peticiones (por defecto: `1`).
- `-s <algoritmo>`: La política de planificación (`FIFO` o `SFF`, por defecto: `FIFO`).

---

## Cómo Probar el Servidor

Una vez que el servidor esté en ejecución, puedes probarlo de las siguientes maneras:

### Prueba con `curl` (Métodos GET y POST)

Abre una nueva terminal para realizar las pruebas.

- **Solicitud GET:**

  ```bash
  curl http://localhost:8080/index.html
  ```

- **Solicitud POST** (enviará datos al script `spin.cgi`):

  ```bash
  curl -X POST --data "nombre=usuario&id=123" http://localhost:8080/spin.cgi
  ```

### Prueba de Concurrencia

Para probar la concurrencia, puedes usar el script de prueba.

Este script lanza múltiples clientes simultáneamente para simular una carga real en el servidor.

1. Convierte el script en un archivo ejecutable

```bash
chmod +x test_webserver.sh
```

2. Ejecuta el script

```bash
./test_webserver.sh
```

Los resultados de las peticiones después de ejecutar el script se almacenan en `client_outputs/`

### Prueba en Navegador Web

1. Ingresa a la dirección `http://localhost:8080/index.html` en un navegador.
2. Utiliza las diferentes secciones que hay en la página para probar cada caso: GET, POST y varias peticiones simultáneas al servidor.

**Nota:** Los datos ingresados al utilizar el método POST se almacenan en `web_files/log_post.txt`

---

## Estructura del Repositorio

```
.
├── Makefile                # Automatiza la compilación del proyecto.
├── README.md
├── io_helper.c             # Funciones de ayuda para entrada/salida.
├── io_helper.h
├── request.c               # Lógica para manejar peticiones HTTP.
├── request.h
├── spin.c                  # Código fuente del script CGI de prueba.
├── wclient.c               # Código fuente del cliente de prueba.
├── wserver.c               # Código fuente principal del servidor.
├── test_webserver.sh       # Script para pruebas de carga.
└── web_files/            # Directorio de ejemplo para el contenido web.
    └── spin.cgi
```
