#include <stdlib.h>
#include <stdio.h>
#include <interfaces.h>
int id_nombre = 0;

t_log *entrada_salida;
t_log *logger_io_generica;
t_log *logger_stdin;
t_log *logger_stdout;
t_log *logger_dialfs;

t_config *config_generica;
t_config *config_stdin;
t_config *config_stdout;
t_config *config_dialfs;

pthread_t hilo_interfaz;

sem_t conexion_generica;
sem_t conexion_io;
sem_t desconexion_io;

char *directorio_interfaces;
char *nombre_interfaz;
FILE *bloques;
FILE *bitmap;
int block_size;
int block_count;

const char *operaciones_gen[1] = {"IO_GEN_SLEEP"};
const char *operaciones_stdin[1] = {"IO_STDIN_READ"};
const char *operaciones_stdout[1] = {"IO_STDOUT_WRITE"};
const char *operaciones_dialfs[5] = {"IO_FS_CREATE", "IO_FS_DELETE", "IO_FS_TRUNCATE", "IO_FS_WRITE", "IO_FS_READ"};

TIPO_INTERFAZ get_tipo_interfaz(INTERFAZ *interfaz, char *tipo_nombre){
    TIPO_INTERFAZ tipo;
    if (!strcmp(tipo_nombre, "GENERICA"))
    { // revisar si esta bien usado el strcmp
        tipo = GENERICA;
    }
    else if (!strcmp(tipo_nombre, "STDIN"))
    {
        tipo = STDIN;
    }
    else if (!strcmp(tipo_nombre, "STDOUT"))
    {
        tipo = STDOUT;
    }
    else if (!strcmp(tipo_nombre, "DIALFS"))
    {
        tipo = DIAL_FS;
    }
    return tipo;
}

void copiar_operaciones(INTERFAZ *interfaz){
    int cantidad_operaciones;
    switch (interfaz->datos->tipo)
    {
    case GENERICA:
        cantidad_operaciones = sizeof(operaciones_gen) / sizeof(operaciones_gen[0]);
        for (int i = 0; i < cantidad_operaciones; i++)
        {
            interfaz->datos->operaciones[i] = strdup(operaciones_gen[i]);
        }
        break;
    case STDIN:
        cantidad_operaciones = sizeof(operaciones_stdin) / sizeof(operaciones_stdin[0]);
        for (int i = 0; i < cantidad_operaciones; i++)
        {
            interfaz->datos->operaciones[i] = strdup(operaciones_stdin[i]);
        }
        break;
    case STDOUT:
        cantidad_operaciones = sizeof(operaciones_stdout) / sizeof(operaciones_stdout[0]);
        for (int i = 0; i < cantidad_operaciones; i++)
        {
            interfaz->datos->operaciones[i] = strdup(operaciones_stdout[i]);
        }
        break;
    case DIAL_FS:
        cantidad_operaciones = sizeof(operaciones_dialfs) / sizeof(operaciones_dialfs[0]);
        for (int i = 0; i < cantidad_operaciones; i++)
        {
            interfaz->datos->operaciones[i] = strdup(operaciones_dialfs[i]);
        }
        break;
    default:
        break;
    }
}

SOLICITUD_INTERFAZ *asignar_espacio_a_solicitud(t_list *lista){
    SOLICITUD_INTERFAZ *nueva_interfaz = malloc(sizeof(SOLICITUD_INTERFAZ));
    nueva_interfaz = list_get(lista, 0);
    nueva_interfaz->nombre = strdup(list_get(lista, 1));
    nueva_interfaz->solicitud = strdup(list_get(lista, 2));
    nueva_interfaz->pid = strdup(list_get(lista, 3));
    nueva_interfaz->args = list_get(lista, 4);

    int argumentos = sizeof(nueva_interfaz->args) / sizeof(nueva_interfaz->args[0]);

    int j = 5;
    for (int i = 0; i < argumentos; i++)
    {
        nueva_interfaz->args[i] = strdup((char *)(list_get(lista, j)));
        j++;
    }

    return nueva_interfaz;
}

desbloquear_io *crear_solicitud_desbloqueo(char *nombre_io, char *pid){

    desbloquear_io *new_solicitude = malloc(sizeof(desbloquear_io));
    new_solicitude->nombre = strdup(nombre_io);
    new_solicitude->pid = strdup(pid);

    return new_solicitude;
}

// FUNCIONES DE ARCHIVOS FS

// TODO: VOLAR ESTA FUNCION SI NO SE USO AL TERMINAR FS
FILE* iniciar_archivo(char* nombre) {
        FILE* archivo = fopen(nombre,"r");
    if (archivo == NULL) {
        //  archivo no existe, crearlo
        archivo = fopen(nombre, "w+");
        if (archivo == NULL) {
            log_error(logger_dialfs, "Error al crear el archivo");
            return NULL;
        }
        log_info(logger_dialfs, "Archivo no existía, creado nuevo archivo.\n");
    } else {
        // El archivo existe, cerrarlo y abrirlo en modo lectura/escritura
        fclose(archivo);
        archivo = fopen(nombre, "r+");
        if (archivo == NULL) {
            log_error(logger_dialfs, "Error al abrir el archivo para lectura/escritura");
            return NULL;
        }
        log_info(logger_dialfs, "Archivo existía, abierto para lectura/escritura.\n");
    }
    return archivo;
}

FILE* inicializar_archivo_bloques(const char *filename, int block_size, int block_count) {
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Error al crear el archivo de bloques");
        exit(EXIT_FAILURE);
        return NULL;
    }
    
    char empty_block[block_size];
    memset(empty_block, 0, block_size);

    for (int i = 0; i < block_count; i++) {
        fwrite(empty_block, 1, block_size, file);
    }

    fclose(file);

    return file;
}

FILE* inicializar_bitmap(const char *nombre_archivo, int block_count) {
    int bitmap_size = block_count / 8;
    FILE *file = fopen(nombre_archivo, "wb");
    if (file == NULL) {
        perror("Error al crear el archivo");
        exit(EXIT_FAILURE);
    }

    // Crear un buffer inicializado a 0
    unsigned char *buffer = (unsigned char *)calloc(bitmap_size, 1);
    if (buffer == NULL) {
        perror("Error al asignar memoria");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    fwrite(buffer, bitmap_size, 1, file);

    return file;
}

void leer_bloque(int bloque_ini, char *buffer) {

    fseek(bloques, bloque_ini * block_size, SEEK_SET);
    fread(buffer, 1, block_size, bloques);

}

void escribir_bloque(int bloque_num, const char *data) {

    fseek(bloques, bloque_num * block_size, SEEK_SET);
    fwrite(data, 1, block_size, bloques);

}

void borrar_bloque(int bloque_num) {
// TODO
}

void set_bit(int bit_index, int value) {
    int byte_index = bit_index / 8;
    int bit_position = bit_index % 8;

    // Posicionarse en el byte correcto
    fseek(bitmap, byte_index, SEEK_SET);

    // Leer el byte actual
    unsigned char byte;
    fread(&byte, 1, 1, bitmap);

    // Modificar el bit específico
    if (value) {
        byte |= (1 << bit_position);  // Poner el bit a 1
    } else {
        byte &= ~(1 << bit_position); // Poner el bit a 0
    }

    // Volver a posicionarse en el byte correcto
    fseek(bitmap, byte_index, SEEK_SET);

    // Escribir el byte modificado
    fwrite(&byte, 1, 1, bitmap);

    // Asegurarse de que los cambios se escriban en el disco
    fflush(bitmap);
}

int get_bit(int bit_index) {
    int byte_index = bit_index / 8;
    int bit_position = bit_index % 8;

    // Posicionarse en el byte correcto
    fseek(bitmap, byte_index, SEEK_SET);

    // Leer el byte actual
    unsigned char byte;
    fread(&byte, 1, 1, bitmap);

    // Obtener el valor del bit específico
    int bit_value = (byte >> bit_position) & 1;

    return bit_value;
}

char* crear_path_metadata(char* nombre_archivo) {
    char* path_retorno = string_new();
    string_append(&path_retorno, directorio_interfaces);
    string_append(&path_retorno, "/");
    string_append(&path_retorno, nombre_archivo);

    return path_retorno;
}

void crear_metadata(char *nombre_archivo, int bloque_inicial, int tamanio_archivo) {
    FILE *file = fopen(crear_path_metadata(nombre_archivo), "w");
    if (file == NULL) {
        perror("Error al crear el archivo de metadatos");
        exit(EXIT_FAILURE);
    }

    fprintf(file, "BLOQUE_INICIAL=%d\n", bloque_inicial);
    fprintf(file, "TAMANIO_ARCHIVO=%d\n", tamanio_archivo);

    fclose(file);
}

void leer_metadata(char *nombre_archivo, int *bloque_inicial, int *tamanio_archivo) {
    FILE *file = fopen(crear_path_metadata(nombre_archivo), "r");
    if (file == NULL) {
        perror("Error al abrir el archivo de metadatos");
        exit(EXIT_FAILURE);
    }

    fscanf(file, "BLOQUE_INICIAL=%d\n", bloque_inicial);
    fscanf(file, "TAMANIO_ARCHIVO=%d\n", tamanio_archivo);

    fclose(file);
}

void modificar_metadata(char *nombre_archivo, int nuevo_bloque_inicial, int nuevo_tamanio_archivo) {
    // Abre el archivo en modo lectura/escritura ("r+")
    FILE *file = fopen(crear_path_metadata(nombre_archivo), "r+");
    if (file == NULL) {
        perror("Error al abrir el archivo");
        exit(EXIT_FAILURE);
    }

    // Variables para almacenar las líneas leídas y para construir el nuevo contenido
    char linea[256];
    char nuevo_contenido[512] = "";

    // Banderas para saber si hemos encontrado las líneas que debemos modificar
    int encontrado_bloque_inicial = 0;
    int encontrado_tamanio_archivo = 0;

    // Leer el archivo línea por línea y modificar las líneas deseadas
    while (fgets(linea, sizeof(linea), file) != NULL) {
        if (strncmp(linea, "BLOQUE_INICIAL=", 15) == 0) {
            sprintf(linea, "BLOQUE_INICIAL=%d\n", nuevo_bloque_inicial);
            encontrado_bloque_inicial = 1;
        } else if (strncmp(linea, "TAMANIO_ARCHIVO=", 16) == 0) {
            sprintf(linea, "TAMANIO_ARCHIVO=%d\n", nuevo_tamanio_archivo);
            encontrado_tamanio_archivo = 1;
        }
        strcat(nuevo_contenido, linea);
    }

    // Verificar si no se encontraron las líneas y agregarlas al final si es necesario
    if (!encontrado_bloque_inicial) {
        sprintf(linea, "BLOQUE_INICIAL=%d\n", nuevo_bloque_inicial);
        strcat(nuevo_contenido, linea);
    }
    if (!encontrado_tamanio_archivo) {
        sprintf(linea, "TAMANIO_ARCHIVO=%d\n", nuevo_tamanio_archivo);
        strcat(nuevo_contenido, linea);
    }

    // Volver al inicio del archivo y truncarlo
    rewind(file);
    if (ftruncate(fileno(file), 0) != 0) {
        perror("Error al truncar el archivo");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Escribir el nuevo contenido en el archivo
    fputs(nuevo_contenido, file);

    // Cerrar el archivo
    fclose(file);
}

void borrar_metadata(char* nombre_archivo) {
    // TODO
}

// se puede hacer mas simple con un for y el get_bit(i)
int buscar_bloque_libre() {

    int bitmap_size = block_count / 8;
    unsigned char byte;
    int byte_index = 0;
    int bit_index = 0;

    while (fread(&byte, 1, 1, bitmap) == 1 && byte_index < bitmap_size) {
        for (int bit_offset = 0; bit_offset < 8; bit_offset++) {
            if ((byte & (1 << bit_offset)) == 0) {
                return bit_index;
            }
            bit_index++;
        }
        byte_index++;
    }

    return -1; // No hay bloques libres
}

int bloques_libres_a_partir_de(int bloque_num) {
    int contador = 0;
    for (bloque_num+=1 ; bloque_num < block_count; bloque_num++) {
        if(get_bit(bloque_num) == 0) {
            contador++;
        }
        else{
            return contador;
        }
    }
    return contador;
}

void compactar() {
    // dividirlo en una funcion que mueva el primer archivo despues de un bloque libre para ocupar dicho bloque
    // y otra funcion que repita esa logica hasta terminar la compactacion
}

int crear_archivo(char* nombre_archivo) {
    int bloque_inicial = buscar_bloque_libre();
    if(bloque_inicial == -1) {
        log_error(logger_dialfs, "No hay bloques libres");
        return -1;
    }
    crear_metadata(nombre_archivo, bloque_inicial, 0);
    set_bit(bloque_inicial, 1);   // modificamos el bitmap para aclarar que el bloque no esta libre

    return bloque_inicial;
}

void borrar_archivo(char* nombre_archivo) {
    int* bloque_inicial;
    int* tamanio_archivo;
    leer_metadata(crear_path_metadata(nombre_archivo), bloque_inicial, tamanio_archivo);
    int cantidad_bloques_a_borrar = *tamanio_archivo / block_size;
    for (int i = *bloque_inicial; i < (*bloque_inicial + cantidad_bloques_a_borrar) ; i++) {
        borrar_bloque(i);   // borramos los bloques de bloques.dat
        set_bit(i, 0);      // liberamos los bits del bitmap
    }
    borrar_metadata(nombre_archivo);

}

// REVISAR PORQUE NO NOS GUSTA NADA EL IF {} ELSE{ IF{} ELSE{}} PERO SI ES VALIDO DEJARLO
void TRUNCATE(char* nombre_archivo, int nuevo_tamanio) {
    int* bloque_inicial;
    int* tamanio_archivo;
    leer_metadata(nombre_archivo, bloque_inicial, tamanio_archivo);

    if(tiene_espacio_suficiente(bloque_inicial, tamanio_archivo, nuevo_tamanio)) {
        modificar_metadata(nombre_archivo, *bloque_inicial, nuevo_tamanio);
        asignar_espacio_en_bitmap(bloque_inicial, tamanio_archivo);
    } else {
        compactar();
        leer_metadata(nombre_archivo, bloque_inicial, tamanio_archivo);
        if(tiene_espacio_suficiente(bloque_inicial, tamanio_archivo, nuevo_tamanio)) {
            modificar_metadata(nombre_archivo, bloque_inicial, nuevo_tamanio);
            asignar_espacio_en_bitmap(bloque_inicial, tamanio_archivo);
        } else{
            log_error(logger_dialfs, "NO HAY ESPACIO EN EL DISCO, COMPRATE UNO MAS GRANDE RATON");
        }
    }
}

bool tiene_espacio_suficiente(int bloque_inicial, int tamanio_archivo, int nuevo_tamanio) {
    int bloques_disponibles = bloques_libres_contiguos(bloque_inicial, tamanio_archivo);
    int tamanio_disponible = bloques_disponibles * block_size;
    return tamanio_disponible >= nuevo_tamanio;
}
    
void asignar_espacio_en_bitmap(bloque_inicial, tamanio_archivo) {
    int bloques_a_asignar = tamanio_archivo / block_size;
    for(int i= bloque_inicial; i <= bloques_a_asignar; i++) {
        if(i>block_count) {
            log_error(logger_dialfs, "FLACO ESTAS ASIGNANDO MAS BLOQUES DE LOS QUE HAY");
            exit (-32);
        }
        set_bit(i, 1);
    }
}

int bloques_libres_contiguos(int bloque_inicial, int tamanio_archivo) {
    int bloques_ocupados = tamanio_archivo / block_size;
    int ultimo_bloque = bloque_inicial + bloques_ocupados - 1; // verificar si esta bien el -1
    int bloques_libres = bloques_libres_a_partir_de(ultimo_bloque);
    return bloques_ocupados + bloques_libres;
}

// FUNCIONES I/O

void peticion_IO_GEN(SOLICITUD_INTERFAZ *interfaz_solicitada, INTERFAZ* io){
    log_info(logger_io_generica, "PID: %s - Operacion: %s", interfaz_solicitada->pid, interfaz_solicitada->solicitud);

    log_info(logger_io_generica, "Ingreso de Proceso PID: %s a IO_GENERICA: %s\n", interfaz_solicitada->pid, interfaz_solicitada->nombre);
    int tiempo_a_esperar = atoi(interfaz_solicitada->args[0]);

    sleep(tiempo_a_esperar);

    log_info(logger_io_generica, "Tiempo cumplido. Enviando mensaje a Kernel");
}

void peticion_STDIN(SOLICITUD_INTERFAZ *interfaz_solicitada, INTERFAZ* io){
    log_info(logger_stdin, "PID: %s - Operacion: %s", interfaz_solicitada->pid, interfaz_solicitada->solicitud);

    char* registro_direccion = interfaz_solicitada->args[0];
    char* registro_tamanio = interfaz_solicitada->args[1];
    
    // TODO: implementar console in 
    char* dato_a_escribir = readline("Ingrese dato a escribir en memoria: ");

    if((strlen(dato_a_escribir) + 1) <= atoi(registro_tamanio)){
        // Reservo memoria para los datos q vamos a enviar en el char**
        PAQUETE_ESCRITURA* paquete_escribir = malloc(sizeof(PAQUETE_ESCRITURA));
        paquete_escribir->pid = interfaz_solicitada->pid;
        paquete_escribir->direccion_fisica = registro_direccion;
        paquete_escribir->dato = malloc(sizeof(t_dato));
        paquete_escribir->dato->data = strdup(dato_a_escribir);
        paquete_escribir->dato->tamanio = strlen(dato_a_escribir) + 1;

        paquete_escribir_memoria(io->sockets->conexion_memoria, paquete_escribir);

        // Libero datos**
        free(paquete_escribir->dato);
        paquete_escribir->dato = NULL;
        free(paquete_escribir);
        paquete_escribir = NULL;
    } else {
        // EXPLOTA TODO: 
        log_error(logger_stdin, "Dato muy grande para el tamanio solicitado."); 
    }
    free(dato_a_escribir);
}

void peticion_STDOUT(SOLICITUD_INTERFAZ *interfaz_solicitada, INTERFAZ *io ){
    log_info(logger_stdout, "PID: %s - Operacion: %s", interfaz_solicitada->pid, interfaz_solicitada->solicitud);

    char* registro_direccion = interfaz_solicitada->args[0];
    char* registro_tamanio = interfaz_solicitada->args[1];

    // Tamaño del char** para reservar memoria
    int tamanio_datos = strlen(registro_direccion) + strlen(registro_tamanio) + strlen(interfaz_solicitada->pid) + 3; 
    // Reservo memoria para los datos q vamos a enviar en el char**
    char** datos = malloc(tamanio_datos);
    datos[0] = strdup(registro_direccion);
    datos[1] = strdup(registro_tamanio);
    datos[2] = strdup(interfaz_solicitada->pid);

    paquete_io_memoria(io->sockets->conexion_memoria, datos, IO_STDOUT);

    // Recibir el dato de la direccion de memoria
    int cod_op = recibir_operacion(io->sockets->conexion_memoria);

    if(cod_op != RESPUESTA_LEER_MEMORIA){ /* ERROR OPERACION INVALIDA */ exit(-32); }

    t_list* lista = recibir_paquete(io->sockets->conexion_memoria, logger_stdout);
    
    char* leido = list_get(lista, 0);
    // Mostrar dato leido de memoria
    printf("Dato leido: %s", leido);

    free(leido);
    leido = NULL;
    // Libero datos**
    liberar_memoria(datos, 3);

}

void peticion_DIAL_FS(SOLICITUD_INTERFAZ *interfaz_solicitada, INTERFAZ *io, FILE* bloques, FILE* bitmap){

    //TODO: LOGICA PRINCIPAL DE LAS INSTRUCCIONES DE ENTRADA/SALIDA DIALFS

    /*switch (interfaz_solicitada->solicitud){

    case "IO_FS_CREATE":
        int block_size = config_get_int_value(config, "BLOCK_SIZE");
        crear_archivo(bloques, bitmap, block_size);
        break;
    
    case "IO_FS_DELETE":
        break;

    default:
        break;
    }
    */
}

void recibir_peticiones_interfaz(INTERFAZ* interfaz, int cliente_fd, t_log* logger, FILE* bloques, FILE* bitmap){

    SOLICITUD_INTERFAZ *solicitud;
    t_list *lista;
    desbloquear_io* aux;

    while (1) {
        
        int cod_op = recibir_operacion(cliente_fd);

        switch (cod_op) {

        case IO_GENERICA:
            lista = recibir_paquete(cliente_fd, logger);
            solicitud = asignar_espacio_a_solicitud(lista);
            peticion_IO_GEN(solicitud, interfaz);

            aux = crear_solicitud_desbloqueo(solicitud->nombre, solicitud->pid);
            paqueteDeDesbloqueo(interfaz->sockets->conexion_kernel, aux);
            eliminar_io_solicitada(solicitud);
            break;

        case IO_STDIN:
            lista = recibir_paquete(interfaz->sockets->conexion_kernel, logger);
            solicitud = asignar_espacio_a_solicitud(lista);
            peticion_STDIN(solicitud, interfaz);

            aux = crear_solicitud_desbloqueo(solicitud->nombre, solicitud->pid);
            paqueteDeDesbloqueo(interfaz->sockets->conexion_kernel, aux);
            eliminar_io_solicitada(solicitud);
            break;

        case IO_STDOUT:
            lista = recibir_paquete(interfaz->sockets->conexion_kernel, logger);
            solicitud = asignar_espacio_a_solicitud(lista);
            peticion_STDOUT(solicitud, interfaz);

            aux = crear_solicitud_desbloqueo(solicitud->nombre, solicitud->pid);
            paqueteDeDesbloqueo(interfaz->sockets->conexion_kernel, aux);
            eliminar_io_solicitada(solicitud);
            break;

        case IO_DIALFS:
            lista = recibir_paquete(interfaz->sockets->conexion_kernel, logger);
            solicitud = asignar_espacio_a_solicitud(lista);
            peticion_DIAL_FS(solicitud, interfaz, bloques, bitmap);

            aux = crear_solicitud_desbloqueo(solicitud->nombre, solicitud->pid);
            paqueteDeDesbloqueo(interfaz->sockets->conexion_kernel, aux);
            eliminar_io_solicitada(solicitud);
            break;

        case DESCONECTAR_IO:
            lista = recibir_paquete(interfaz->sockets->conexion_kernel, logger);
            sem_post(&desconexion_io);
            break;

        default:
            return;
        }
    }
}

void *correr_interfaz(INTERFAZ* interfaz){

    // TOMA DATOS DE KERNEL DE EL CONFIG
    char *ip_kernel = config_get_string_value(interfaz->configuration, "IP_KERNEL");
    char *puerto_kernel = config_get_string_value(interfaz->configuration, "PUERTO_KERNEL");

    // CREA LA CONEXION CON KERNEL  
    interfaz->sockets->conexion_kernel = crear_conexion(ip_kernel, puerto_kernel);
    log_info(entrada_salida, "La interfaz %s está conectandose a kernel", interfaz->sockets->nombre);
    paquete_nueva_IO(interfaz->sockets->conexion_kernel, interfaz); // ENVIA PAQUETE A KERNEL
    log_warning(entrada_salida, "Conexion creada con Kernel -  PUERTO: %s  -  IP: %s\n", puerto_kernel, ip_kernel);

    if(interfaz->datos->tipo != GENERICA){

        // TOMA DATOS DE MEMORIA DE EL CONFIG
        char *ip_memoria = config_get_string_value(interfaz->configuration, "IP_MEMORIA");
        char *puerto_memoria = config_get_string_value(interfaz->configuration, "PUERTO_MEMORIA");

        // CREA LA CONEXION CON MEMORIA salvaje
        interfaz->sockets->conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
        log_info(entrada_salida, "La interfaz %s está conectandose a memoria \n", interfaz->sockets->nombre);
        paquete_llegada_io_memoria(interfaz->sockets->conexion_memoria, interfaz->sockets); // ENVIA PAQUETE A MEMORIA
        log_warning(entrada_salida, "Conexion creada con Memoria -  PUERTO: %s  -  IP: %s\n", puerto_memoria, ip_memoria);
    }    

    // TODO: cambiar la ruta relativa a la absoluta de la carpeta donde deberian estar estos archivos
    if (interfaz->datos->tipo == DIAL_FS) {

        block_count = config_get_int_value(interfaz->configuration, "BLOCK_COUNT");
        block_size = config_get_int_value(interfaz->configuration, "BLOCK_SIZE");


        char* path_bloques = string_new();
        char* path_bitmap = string_new();
        
        string_append(&path_bloques, directorio_interfaces);
        string_append(&path_bloques, "/");
        string_append(&path_bloques, nombre_interfaz);
        string_append(&path_bloques, "_bloques.dat");
        log_info(logger_dialfs, path_bloques);
        
        string_append(&path_bitmap, directorio_interfaces);
        string_append(&path_bitmap, "/");
        string_append(&path_bitmap, nombre_interfaz);
        string_append(&path_bitmap, "_bitmap.dat");
        log_info(logger_dialfs, path_bitmap);

        bloques = inicializar_archivo_bloques(path_bloques, block_size, block_count);
        bitmap = inicializar_bitmap(path_bitmap, block_count);

        recibir_peticiones_interfaz(interfaz, interfaz->sockets->conexion_kernel, entrada_salida, bloques, bitmap);
    } else {
        recibir_peticiones_interfaz(interfaz, interfaz->sockets->conexion_kernel, entrada_salida, NULL, NULL);
    }   
    return NULL;
}

void iniciar_interfaz(char *nombre, t_config *config, t_log *logger){

    INTERFAZ* interfaz = malloc(sizeof(INTERFAZ));

    interfaz->configuration = config;

    interfaz->procesos_bloqueados = NULL;

    interfaz->datos = malloc(sizeof(DATOS_INTERFAZ));
    interfaz->datos->tipo = get_tipo_interfaz(interfaz, config_get_string_value(interfaz->configuration, "TIPO_INTERFAZ"));

    interfaz->sockets = malloc(sizeof(DATOS_CONEXION));
    interfaz->sockets->nombre = strdup(nombre);
    nombre_interfaz = strdup(nombre);
   
    switch (interfaz->datos->tipo)
    {
    case GENERICA:
        interfaz->datos->operaciones = malloc(sizeof(operaciones_gen));
        break;
    case STDIN:
        interfaz->datos->operaciones = malloc(sizeof(operaciones_stdin));
        break;
    case STDOUT:
        interfaz->datos->operaciones = malloc(sizeof(operaciones_stdout));
        break;
    case DIAL_FS:
        interfaz->datos->operaciones = malloc(sizeof(operaciones_dialfs));
        directorio_interfaces = strdup(config_get_string_value(config, "PATH_BASE_DIALFS"));
        break;
    default:
        break;
    }
    copiar_operaciones(interfaz); // COPIA LOS ELEMENTOS DE LA INTERFAZ EN LA INTERFAZ   
    correr_interfaz(interfaz); // EJECUTA LA INTERFAZ
}

void conectar_interfaces(){
    
    int opcion;
    printf("SELECCIONE EL TIPO DE INTERFAZ Y DELE UN NOMBRE \n");

    printf("1. Conectar interfaz Generica \n");
    printf("2. Conectar interfaz STDIN    \n");
    printf("3. Conectar interfaz STDOUT   \n");
    printf("4. Conectar interfaz DIALFS   \n");
    printf("5. Desconectar una interfaz   \n");
    printf("6. Salir \n");

    printf("Seleccione una opcion: ");
    scanf("%d", &opcion);
        
    switch (opcion) {

    case CONECTAR_GENERICA:
        printf("Conectando interfaz Generica... \n\r ");
        iniciar_configuracion();
        terminar_programa(logger_io_generica, config_generica);
        break;

    case CONECTAR_STDIN:
        printf("Conectando interfaz STDIN... \n\r ");
        iniciar_interfaz("TECLADO", config_stdin, logger_stdin);
        break;

    case CONECTAR_STDOUT:
        printf("Conectando interfaz STDOUT... \n ");
        iniciar_interfaz("MONITOR", config_stdout, logger_stdout);
        break;

    case CONECTAR_DIALFS:
        printf("Conectando interfaz DIALFS... \n ");
        iniciar_interfaz("FS", config_dialfs, logger_dialfs);
        break;

    case SALIR:
        return;

    default:
        printf("Opcion no valida. Por favor seleccione una opcion correcta \n");
        break;

    }
}

int main(int argc, char *argv[]){
    sem_init(&conexion_generica, 1, 0);
    sem_init(&desconexion_io, 1, 0);
    sem_init(&conexion_io, 1, 0);

    entrada_salida     = iniciar_logger("main.log", "io_general_log", LOG_LEVEL_INFO);
    logger_io_generica = iniciar_logger("io_generica.log", "io_generica_log", LOG_LEVEL_INFO);
    logger_stdin       = iniciar_logger("io_stdin.log", "io_stdin_log", LOG_LEVEL_INFO);
    logger_stdout      = iniciar_logger("io_stdout.log", "io_stdout_log", LOG_LEVEL_INFO);
    logger_dialfs      = iniciar_logger("io_dialfs.log", "io_dialfs_log", LOG_LEVEL_INFO);

    config_stdin       = iniciar_config("../entradasalida/configs/TECLADO.config");
    config_stdout      = iniciar_config("../entradasalida/configs/MONITOR.config");
    config_dialfs      = iniciar_config("../entradasalida/configs/FS.config");

    conectar_interfaces(); // CREA LA INTERFAZ A CONECTAR

    // LIBERA MEMORIA Y CONEXIONES
    sem_destroy(&desconexion_io);
    terminar_programa(logger_stdin, config_stdin);
    terminar_programa(logger_stdout, config_stdout);
    terminar_programa(logger_dialfs, config_dialfs);

    return 0;
}

void iniciar_configuracion(){
    t_config* configuracion;
    
    printf("1. Conectar SLP1\n");
    printf("2. Conectar ESPERA\n");
    printf("3. Conectar GENERICA\n");
    char* opcion_en_string = readline("Seleccione una opción: ");
    int opcion = atoi(opcion_en_string);
    free(opcion_en_string);

    switch (opcion)
        {
        case 1:
            log_info(logger_io_generica, "Conectando SLP1...");
            configuracion = iniciar_config("../entradasalida/configs/SLP1.config");
            iniciar_interfaz("SLP1", configuracion, logger_io_generica);
            break;
        case 2:
            log_info(logger_io_generica, "Conectando ESPERA...");
            configuracion = iniciar_config("../entradasalida/configs/ESPERA.config");
            iniciar_interfaz("ESPERA", configuracion, logger_io_generica);
            break;
        case 3:
            log_info(logger_io_generica, "Conectando GENERICA...");
            configuracion = iniciar_config("../entradasalida/configs/GENERICA.config");
            iniciar_interfaz("GENERICA", configuracion, logger_io_generica);
            break;
        default:
            log_info(logger_io_generica, "Conectando GENERICA...");
            configuracion = iniciar_config("../entradasalida/configs/GENERICA.config");
            iniciar_interfaz("GENERICA", configuracion, logger_io_generica);
            break;
        }
}