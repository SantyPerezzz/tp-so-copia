#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <utils/utils.h>
//STRUCTS
typedef struct{
    int pid;
    t_list* instrucciones;
}instrucciones_a_memoria;

typedef struct{
    char* instruccion;
}inst_pseudocodigo;

typedef struct{
    int nro_marco;
    int offset;
}direccion_fisica;

typedef struct {
    int tamanio;
    void* data;
} MARCO_MEMORIA;

typedef struct {
    MARCO_MEMORIA *marcos;
    int numero_marcos;
    int tam_marcos;
} MEMORIA;

//MEMORIA
void inicializar_memoria(MEMORIA*, int, int);
void resetear_memoria(MEMORIA*);
bool guardar_en_memoria(direccion_fisica, t_dato*, TABLA_PAGINA*);
bool verificar_marcos_disponibles(int);
bool escribir_en_memoria(char*, t_dato*, int);
void* leer_en_memoria(PAQUETE_LECTURA*);
bool reservar_memoria(TABLA_PAGINA*, int);
void asignar_marco_a_pagina(PAGINA*, int);
direccion_fisica obtener_marco_y_offset(char*);
t_config* iniciar_configuracion();

//PAGINADO
void inicializar_tabla_pagina(int);
t_list* crear_tabla_de_paginas();
void lista_tablas(TABLA_PAGINA*);
void destruir_tabla_pag_proceso(int pid);
void destruir_tabla();
void ajustar_tamanio(TABLA_PAGINA*, int);
unsigned int acceso_a_tabla_de_páginas(int, int);
int ultima_pagina_usada(t_list*);
int cantidad_de_paginas_usadas(TABLA_PAGINA*);

bool pagina_vacia(void*);
bool pagina_no_vacia(void*);
bool pagina_sin_frame(void*);
bool pagina_asociada_a_marco(int, void*);

//PSEUDOCODIGO
void enlistar_pseudocodigo(char*, t_log*, t_list*);

//CONEXIONES
void* gestionar_llegada_memoria_cpu(void*);
void* gestionar_llegada_memoria_kernel(void*);
void* gestionar_nueva_io(void*);
void* esperar_nuevo_io();
void enviar_instrucciones_a_cpu(t_instruccion*);

//PROCESOS
pcb* crear_pcb(c_proceso_data*);
void destruir_instrucciones(void*);
bool es_pid_de_tabla(int, void*);
bool son_inst_pid(int pid, void* data);
void destruir_memoria_instrucciones(int pid);
void inicializar_registroCPU(regCPU*);

//BITMAP
char* crear_bitmap();
void establecer_bit(int, bool);
void imprimir_bitmap();
bool obtener_bit(int);
void liberar_bitmap();
int buscar_marco_libre();

//INTERFACES
bool nombre_de_interfaz(char*, void*);

#endif
