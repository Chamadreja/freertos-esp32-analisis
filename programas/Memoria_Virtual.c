/*
=============== PROGRAMA SIMULACION MEMORIA VIRTUAL ===============
Sistema de simulación de Memoria Virtual con asignación fija 
y reemplazo local.

Implementa 3 algoritmos de reemplazo: FIFO, Reloj (Segunda oportunidad)
y una adaptacion práctica de LRU.

La simulación simula 3 procesos mediante hilos,
cada proceso dispone de 3 marcos en Memoria Principal.
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>  //para funcion usleep

// ==================== ESTRUCTURAS DE DATOS ====================

// Representa un marco de la memoria física
typedef struct
{
    int ocupado;        // libre=0, ocupado=1
    int proceso_id;     // ID del proceso propietario del marco
    int pagina_virtual; // Pagina virtual que esta cargada
} Marco;

// Memoria fisica completa
typedef struct
{
    Marco *marcos;         //Areglo de marcos
    int num_marcos_total; //numero total de marcos de la memoria
} MemoriaFisica;

typedef struct
{
    int presente;       // 0 = no esta, 1 = esta en memoria
    int marco_fisico;   // Marco donde esta la pagina si presente = 1
    int bit_referencia; // Bit R para algoritmo Segunda Oportunidad
    int ultimo_acceso;  // "Contador que simula tiempo" para algoritmo LRU
} EntradaPagina;

// Tabla de paginas de un proceso
typedef struct
{
    int proceso_id;        // id que identifica a que proceso corresponde la tabla
    EntradaPagina *tabla;  // Arreglo dinamico de entradas
    int num_paginas;       // cantidad de paginas virtuales del proceso
} TablaPaginas;

// Datos de cada proceso/hilo estructura implementada para simulacion de virtualizacion de memoria
typedef struct
{
    int proceso_id;
    int *direcciones_virtuales;           //Arreglo con secuencia de paginas que accedera el proceso
    int num_direcciones;                  // cantidad total de rireciones que va a acceder el procesos
    int siguiente_victima_FIFO;          // Indice circular para algoritmo FIFO
    int siguiente_victima_segunda_op;    // Indice circular para algoritmo Segunda Oportunidad
    TablaPaginas tabla_paginas;
} DatosProceso;

// ==================== VARIABLES GLOBALES ====================
MemoriaFisica memoria_fisica;
pthread_mutex_t mutex_memoria = PTHREAD_MUTEX_INITIALIZER;

int ALGORITMO_REEMPLAZO = 0;// 0 = FIFO, 1 = Segunda Oportunidad, 2 = LRU
int tiempo_global = 0; // Contador global de tiempo para LRU

int Fallos_de_paginas_totales = 0;
int accesos_total = 0;

// Asignacion estatica de marcos por proceso
int marcos_proceso0[] = {0, 1, 2};
int marcos_proceso1[] = {3, 4, 5};
int marcos_proceso2[] = {6, 7, 8};


// ==================== FUNCIONES ====================

/** 
 * @brief Inicializa la memoria principal del sistema.
 * Configura todos los marcos como libres y listos para usar.
 */
void inicializar_memoria()
{
    memoria_fisica.num_marcos_total = 9;
    memoria_fisica.marcos = malloc(memoria_fisica.num_marcos_total * sizeof(Marco));
    
    if (memoria_fisica.marcos == NULL) { printf("Error: No se pudo asignar memoria para los marcos\n"); exit(1);}
    
    for (int i = 0; i < memoria_fisica.num_marcos_total; i++){
        memoria_fisica.marcos[i].ocupado = 0;
        memoria_fisica.marcos[i].proceso_id = -1;
        memoria_fisica.marcos[i].pagina_virtual = -1;
    }
    printf("Memoria fisica inicializada (%d marcos)\n", memoria_fisica.num_marcos_total);
}

/**
 * @brief Inicializa un proceso con sus datos y tabla de paginas.
 * 
 * @param proceso: Estructura del proceso a inicializar
 * @param id: Identificador del proceso
 * @param direcciones: Arreglo con secuencia de paginas que accedera el proceso
 * @param num_dir: Cantidad de direcciones en la secuencia
 */
void inicializar_proceso(DatosProceso *proceso, int id, int *direcciones, int num_dir)
{
    //Numeros de paginas por procesos 0 a 19 (20 paginas totales por proceso como maximo)
    int PaginasXProceso=20;

    //Se inicializa el proceso
    proceso->proceso_id = id;
    proceso->direcciones_virtuales = direcciones;
    proceso->num_direcciones = num_dir;
    proceso->siguiente_victima_FIFO = 0;
    proceso->siguiente_victima_segunda_op = 0;

    //se inicializa la tabla de pagina
    proceso->tabla_paginas.proceso_id = id;
    proceso->tabla_paginas.num_paginas = PaginasXProceso;
    proceso->tabla_paginas.tabla = malloc(PaginasXProceso * sizeof(EntradaPagina));
    
    if (proceso->tabla_paginas.tabla == NULL) {printf("Error: No se pudo asignar memoria para tabla de paginas\n");exit(1);}

    for (int i = 0; i < PaginasXProceso; i++){
        proceso->tabla_paginas.tabla[i].presente = 0;
        proceso->tabla_paginas.tabla[i].marco_fisico = -1;
        proceso->tabla_paginas.tabla[i].bit_referencia = 0; 
        proceso->tabla_paginas.tabla[i].ultimo_acceso = 0;  
    }    
    printf("Proceso %d inicializado\n", id);
}

/**
 * @brief Devuelve un puntero al array de marcos asignados a un proceso específico.
 * 
 * @param proceso_id ID del proceso (0, 1 o 2)
 * @return Puntero al array de marcos del proceso, NULL si el ID no es valido
 */
int* obtener_marcos_proceso(int proceso_id)
{
    switch(proceso_id) 
    {
        case 0: 
            return marcos_proceso0;  // Marcos 0, 1, 2
        case 1: 
            return marcos_proceso1; // Marcos 3, 4, 5
        case 2: 
            return marcos_proceso2; // Marcos 6, 7, 8
        default: 
            return NULL;
    }
}


/**
 * Busca un marco libre en la memoria principal para un proceso.
 * 
 * @param proceso_id: ID del proceso que necesita un marco libre
 * @return: Numero del marco libre encontrado, o -1 si no hay marcos libres
 */
int buscar_marco_libre(int proceso_id)
{
    int *marcos = obtener_marcos_proceso(proceso_id);
    if (marcos == NULL) 
        return -1;

    // Buscar el primer marco libre en el conjunto asignado al proceso
    for (int i = 0; i < 3; i++)
    {
        int marco = marcos[i];
        if (memoria_fisica.marcos[marco].ocupado == 0)
            return marco;// Retornar primer marco libre encontrado
    }
    return -1;// No hay marcos libres para este proceso
}

/**
 * @brief Selecciona el proximo marco victima usando FIFO circular
 * 
 * @param proceso: Proceso que necesita reemplazar una pagina
 * @return: Numero del marco víctima seleccionado
 */
int seleccionar_marco_fifo_local(DatosProceso *proceso)
{
    int proceso_id = proceso->proceso_id;

    int *marcos = obtener_marcos_proceso(proceso_id);
    if (marcos == NULL) 
        return -1;

    // Obtener el marco victima actual
    int marco_victima = marcos[proceso->siguiente_victima_FIFO];
    
    // Avanzar el puntero circular (ej: 0 -> 1 -> 2 -> 0 -> 1 -> 2...)
    proceso->siguiente_victima_FIFO = (proceso->siguiente_victima_FIFO + 1) % 3;
    
    return marco_victima;
}

/**
 * @brief Selecciona el proximo marco victima usando algoritmo Segunda Oportunidad
 * 
 * @param proceso: Proceso que necesita reemplazar una pagina
 * @return: Numero del marco víctima seleccionado
 */
int seleccionar_marco_segunda_oportunidad(DatosProceso *proceso)
{
    int proceso_id = proceso->proceso_id;

    int *marcos = obtener_marcos_proceso(proceso_id);
    if (marcos == NULL) 
        return -1;

    int intentos = 0;
    int max_intentos = 6; // Maximo 2 vueltas completas (3 marcos * 2)
    
    while (intentos < max_intentos) 
    {
        // Obtener el marco candidato actual (0,1,2)
        int marco_candidato = marcos[proceso->siguiente_victima_segunda_op];
        
        // Obtener la pagina virtual que esta en este marco
        int pagina_virtual = memoria_fisica.marcos[marco_candidato].pagina_virtual;
        
        // Verificar el bit de referencia de la pagina (en la tabla de paginas)
        // Bit R = 0: pagina  victima
        if (proceso->tabla_paginas.tabla[pagina_virtual].bit_referencia == 0) 
        {
            // Avanzar el puntero circular para la proxima vez
            proceso->siguiente_victima_segunda_op = (proceso->siguiente_victima_segunda_op + 1) % 3;
            return marco_candidato;
        } 
        else 
        {
            // Bit R = 1: dar segunda oportunidad Bit R = 0 
            proceso->tabla_paginas.tabla[pagina_virtual].bit_referencia = 0;
            
            // Avanzar al siguiente marco
            proceso->siguiente_victima_segunda_op = (proceso->siguiente_victima_segunda_op + 1) % 3;
            intentos++;
        }
    }
    
    // llega acasi todos los marcos tenian el Bit R= 1 usar el marco actual como victima
    int marco_victima = marcos[proceso->siguiente_victima_segunda_op];
    proceso->siguiente_victima_segunda_op = (proceso->siguiente_victima_segunda_op + 1) % 3;
    return marco_victima;
}

/**
 * @brief Selecciona el proximo marco victima usando algoritmo LRU. 
 * Busca la pagina con el "tiempo" mas antiguo (menor valor)
 * En nuestra aproximacion usamos un contador y no un tiempo del cpu
 * y buscamos el valor mas chico para encontrar la pagina victima
 * 
 * @param proceso: Proceso que necesita reemplazar una pagina
 * @return: Numero del marco víctima seleccionado
 */
int seleccionar_marco_lru(DatosProceso *proceso)
{
    int proceso_id = proceso->proceso_id;

    int *marcos = obtener_marcos_proceso(proceso_id);
    if (marcos == NULL) 
        return -1;

    // Buscar la pagina menos recientemente usada "menor tiempo"
    int marco_lru = marcos[0];
    int tiempo_mas_viejo = tiempo_global; 
    
    for (int i = 0; i < 3; i++) 
    {
        int marco = marcos[i];
        int pagina_virtual = memoria_fisica.marcos[marco].pagina_virtual;
        int ultimo_acceso = proceso->tabla_paginas.tabla[pagina_virtual].ultimo_acceso;
        
        if (ultimo_acceso < tiempo_mas_viejo) 
        {
            tiempo_mas_viejo = ultimo_acceso;
            marco_lru = marco;  //victima
        }
    } 
    return marco_lru;
}

/**
 * Maneja un fallo de pagina cargando la pagina solicitada en memoria principal
 * Si no hay marcos libres, aplica el algoritmo de reemplazo seleccionado globalmente
 * @param proceso: Proceso que genero el fallo de página
 * @param pagina: Número de página virtual que debe cargarse
 */
void fallo_Pagina(DatosProceso *proceso, int pagina)
{
    int proceso_id = proceso->proceso_id;
    printf("[P%d] FALLO DE PAGINA --> pagina %d\n", proceso_id, pagina);

    //  Encontrar un marco libre para el proceso
    int marco = buscar_marco_libre(proceso_id);
    
    //No hay marcos libres, aplicar algoritmo de reemplazo
    if (marco == -1) 
    {
        //seleccionar marco victima segun algoritmo global
        if (ALGORITMO_REEMPLAZO == 0) 
            marco = seleccionar_marco_fifo_local(proceso);
        else if (ALGORITMO_REEMPLAZO == 1) 
            marco = seleccionar_marco_segunda_oportunidad(proceso);
        else  
            marco = seleccionar_marco_lru(proceso);

        //pagina que va a ser desalojada
        int pagina_victima = memoria_fisica.marcos[marco].pagina_virtual;

        // Actualizar tabla de paginas de la pagina victima (desalojada)
        if (pagina_victima != -1)
        {
            proceso->tabla_paginas.tabla[pagina_victima].presente = 0;      // Marco como no presente
            proceso->tabla_paginas.tabla[pagina_victima].marco_fisico = -1; // Quito asignacion de marco
            proceso->tabla_paginas.tabla[pagina_victima].bit_referencia = 0; // Resetear bit de referencia
            proceso->tabla_paginas.tabla[pagina_victima].ultimo_acceso = 0;  // Resetear timestamp LRU
        }
        printf("[P%d] Reemplazando pagina %d en marco %d\n", proceso_id, pagina_victima, marco);
    }

    // Se cargar nueva pagina en el marco seleccionado
    memoria_fisica.marcos[marco].ocupado = 1;                              // Marcar marco como ocupado
    memoria_fisica.marcos[marco].proceso_id = proceso_id;                  // Asignar al proceso
    memoria_fisica.marcos[marco].pagina_virtual = pagina;                  // Cargar página

    // Actualizar tabla de paginas del proceso
    proceso->tabla_paginas.tabla[pagina].presente = 1;                     // Marcar pagina como presente
    proceso->tabla_paginas.tabla[pagina].marco_fisico = marco;             // Asignar marco 
    proceso->tabla_paginas.tabla[pagina].bit_referencia = 1;               // Activar bit de referencia
    proceso->tabla_paginas.tabla[pagina].ultimo_acceso = ++tiempo_global;  // Actualizar timestamp LRU

    printf("[P%d] Pagina %d cargada en marco %d (tiempo: %d)\n", proceso_id, pagina, marco, tiempo_global);
    
    // Incrementar estadisticas globales
    Fallos_de_paginas_totales++;
}

/**
 * Simula el acceso a una direccion de memoria virtual
 * Verifica si la pagina esta en memoria principal o se genera fallo de pagina
 * 
 * @param proceso: Proceso que realiza el acceso a memoria
 * @param direccion: Dirección virtual a la que se quiere acceder
 */
void acceder_memoria(DatosProceso *proceso, int direccion)
{
    int pagina = direccion;// Simplificacion: cada direccion corresponde directamente a una pagina
    printf("[P%d] Acceso a pagina %d --> ", proceso->proceso_id, pagina);

    // Verificar en la tabla de paginas si la pagina esta presente en memoria principal
    if (proceso->tabla_paginas.tabla[pagina].presente)
    {
        //La pagina está en memoria principal
        int marco = proceso->tabla_paginas.tabla[pagina].marco_fisico;
        printf("PRESENTE (marco %d)\n", marco);
        
        // Actualizar información segun el algoritmo usado
        if (ALGORITMO_REEMPLAZO == 1)
            // Activar bit de referencia para Segunda Oportunidad   
            proceso->tabla_paginas.tabla[pagina].bit_referencia = 1;
        
        if (ALGORITMO_REEMPLAZO == 2) 
            //Cada vez que accedo a una pagina -> Actualizo "tiempo" para LRU
            proceso->tabla_paginas.tabla[pagina].ultimo_acceso = ++tiempo_global;
    }
    else
    {
        printf("NO PRESENTE\n");
        fallo_Pagina(proceso, pagina);
    }
    accesos_total++;    // Incrementar contador global de accesos para estadisticas
    printf("\n");
}

/**
 * Funcion que ejecuta un proceso simulando accesos secuenciales a memoria
 * Esta función se ejecuta en un hilo separado para simular concurrencia
 * 
 * @param arg: Puntero a estructura DatosProceso (casteado como void*)
 * @return: NULL (requerido por pthread)
 */
void *ejecutar_proceso(void *arg)
{
    // Castear el argumento recibido a la estructura correcta
    DatosProceso *proceso = (DatosProceso *)arg;

    // Simula ejecucion del proceso --> acceder a cada dirección en secuencia
    for (int i = 0; i < proceso->num_direcciones; i++)
    {
        // Sección critica: solo un proceso puede acceder a memoria a la vez
        pthread_mutex_lock(&mutex_memoria);
        
        // Realizar el acceso a la direccion virtual
        acceder_memoria(proceso, proceso->direcciones_virtuales[i]);
        
        // Liberar el mutex para permitir que otros procesos accedan
        pthread_mutex_unlock(&mutex_memoria);
        
        // Pausa para una correcta impresion de la salida por consola
        usleep(200000); // 0.2 segundos 
    }
    
    return NULL;
}


int main()
{
    // Mostrar algoritmo seleccionado
    char *algoritmos[3] = {"FIFO", "SEGUNDA OPORTUNIDAD", "LRU"};

    printf("=== SIMULADOR MEMORIA VIRTUAL ===\n");
    printf("Algoritmo de reemplazo: %s\n", algoritmos[ALGORITMO_REEMPLAZO]);
    printf("=====================================\n");

    inicializar_memoria();

    // Se define secuencias de acceso para cada proceso
    // Cada arreglo representa las paginas que accederá cada proceso en orden
    int sec0[] = {2, 3, 2, 1, 5, 2, 4, 5, 3, 2, 5, 2};  // Proceso 0: 12 accesos
    int sec1[] = {2, 3, 2, 1, 5, 2, 4, 5, 3, 2, 5, 2};  // Proceso 1: 12 accesos
    int sec2[] = {2, 3, 2, 1, 5, 2, 4, 5, 3, 2, 5, 2};  // Proceso 2: 12 accesos

    //Creacion y configuracion de las estructuras de los procesos
    DatosProceso procesos[3];
    inicializar_proceso(&procesos[0], 0, sec0, 12);
    inicializar_proceso(&procesos[1], 1, sec1, 12);
    inicializar_proceso(&procesos[2], 2, sec2, 12);

    //Creacion de  hilos para ejecutar procesos concurrentes
    pthread_t hilos[3];
    for (int i = 0; i < 3; i++)
        pthread_create(&hilos[i], NULL, ejecutar_proceso, &procesos[i]);
    

    //Esperar que todos los hilos terminen su ejecución
    for (int i = 0; i < 3; i++)
        pthread_join(hilos[i], NULL);
        
    
    printf("=== ESTADISTICAS FINALES ===\n");
    printf("Algoritmo usado: %s\n", algoritmos[ALGORITMO_REEMPLAZO]);
    printf("Accesos totales: %d\n", accesos_total);
    printf("Fallos de pagina: %d\n", Fallos_de_paginas_totales);
    printf("Tasa de fallos: %.1f%%\n", (float)Fallos_de_paginas_totales / accesos_total * 100);

    //Libero memoria 
    free(memoria_fisica.marcos);
    for (int i = 0; i < 3; i++)
        free(procesos[i].tabla_paginas.tabla);
    
    return 0;
}