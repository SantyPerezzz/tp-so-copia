#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <utils/utils.h>
//STRUCTS

typedef struct datos_a_memoria{
    void* data;
    char tipo;
}t_dato;

typedef struct{
    char* instruccion;
}inst_pseudocodigo;

typedef struct {
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
bool guardar_en_memoria(MEMORIA*, t_dato*, t_list*);
int buscar_marco_disponible();
int determinar_sizeof(t_dato*);
bool verificar_marcos_disponibles(int);

//PAGINADO
TABLA_PAGINA* inicializar_tabla_pagina();
void lista_tablas(TABLA_PAGINA*);
void destruir_pagina(void*);
void destruir_tabla(int);
void tradurcirDireccion();


//PSEUDOCODIGO
bool enlistar_pseudocodigo(char*, char*, t_log*, TABLA_PAGINA*);
void iterar_lista_e_imprimir(t_list*);

//CONEXIONES
void* gestionar_llegada_memoria_cpu(void*);
void* gestionar_llegada_memoria_kernel(void*);
void enviar_instrucciones_a_cpu(char*,char*, int);

//PROCESOS
pcb* crear_pcb(c_proceso_data);
void destruir_pcb(pcb*);
void destruir_instrucciones(void*);
bool es_pid_de_tabla(int, void*);

#endif