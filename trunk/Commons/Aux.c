#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include "Aux.h"

int prints_on = -1;
int headers_on = -1;

void Aux_load_config() {
	FILE* fd;
	if((fd = fopen("./AuxConfig.txt", "rb"))==NULL){
		perror("Error al abrir AuxConfig.txt\n");
	}
	char var[255];
	fscanf(fd, "%s = %d\n", var, &prints_on);
	fscanf(fd, "%s = %d\n", var, &headers_on);
	fclose(fd);
}

// Printf que se activa segun el archivo AuxConfig.txt de cada proyecto
void Aux_printf(const char* s, ...) {
	if(prints_on == -1) {
		Aux_load_config();
	}

	if(prints_on) {
		va_list args;
		va_start(args, s);
		vprintf(s, args);
		va_end(args);
	}
}

// Cabecera para comienzo de funciones importantes
void Aux_print_header(const char* s1, const char* s2) {
	if (headers_on == -1) {
		Aux_load_config();
	}

	if (headers_on) {
		printf("____________________________________________\n\n");
		if (s2 == NULL) {
			printf("> %s\n", s1);
		} else {
			printf("> %s - %s\n", s1, s2);
		}
		printf("____________________________________________\n");
	}
}

// Retorna la posicion de un string en un vector, si no la encuentra retorna -1
int Aux_get_pos_in_string_vec(char** vec, int n_vec_elements, char* string) {
	int i;
	for (i = 0; i < n_vec_elements; i++) {
		if (!strcmp(string, vec[i])) {
			return i;
		}
	}
	return -1;
}

// Imprime todos los string de un vector de strings
void Aux_print_string_vec(char** vec, int n_vec_elems) {
	int i;
	for (i = 0; i < n_vec_elems; i++) {
		printf("%s", vec[i]);
		printf("\n");
	}
}

// Busca un comando ingresado por teclado en el vector de comandos, realizando su funcion del vector de funciones
int Aux_do_command(int(*functions[])(int, char**), char** commands, int n_commands) {
	const int MAX_LINE = 200;
	char entry[MAX_LINE];
	char **all_params = (char**) malloc(sizeof(char*));
	int n_params = 0;

	// Obtengo el comando
	fgets(entry, MAX_LINE, stdin);

	int pos = -1;
	if (strlen(entry) > 1) {
		// Obtenemos los parametros del string
		Aux_get_tokens_by(" ", entry, &all_params, &n_params);
		pos = Aux_get_pos_in_string_vec(commands, n_commands, all_params[0]);

	}
	int result = 0;
	if (pos >= 0) {
		result = functions[pos](n_params, all_params);
	} else {
		printf(">> Opcion incorrecta\n");
	}
	Aux_string_vec_destroy(&all_params, n_params);
	free(all_params);
	return result;
}

// Libera cada cadena de un vector de cadenas
void Aux_string_vec_destroy(char*** vec, int size) {
	int i;
	for (i = 0; i < size; i++)
		free((*vec)[i]);
	//free(vec);
}

// Divide una cadena en varias cadenas separadas por un flag y las devuelve en un vector
void Aux_get_tokens_by(const char* flag, const char* string, char*** vec_tokens, int* n_tokens) {
	char *save_ptr, *token;
	char* aux_string = (char*) malloc(strlen(string) + 1);
	strcpy(aux_string, string);

	// Hasta que un nombre de carpeta sea nulo, cargo el vector con cada nombre
	for (*n_tokens = 0, token = strtok_r(aux_string, flag, &save_ptr); token != NULL; token = strtok_r(NULL, flag, &save_ptr), (*n_tokens)++) {
		if (*n_tokens >= 1)
			*vec_tokens = realloc(*vec_tokens, sizeof(char*) * ((*n_tokens) + 1));

		// Salvedad para un ultimo elemento de una linea
		if (token[strlen(token) - 1] == '\n') {
			(*vec_tokens)[*n_tokens] = (char*) malloc(strlen(token));
			token[strlen(token) - 1] = '\0';
		} else {
			(*vec_tokens)[*n_tokens] = (char*) malloc(strlen(token) + 1);
		}

		strcpy((*vec_tokens)[*n_tokens], token);
	}

	free(aux_string);
}

// Dado un type, retorna su significado
char *Aux_type_string(int type) {
	switch (type) {
	case TYPE_HANDSHAKE:
		return "Se realizo el Handshake";
	case TYPE_OPEN:
		return "Se abrio el Disco";
	case TYPE_CLOSE:
		return "Se cerro el Disco";
	case TYPE_READ:
		return "Se leyo un Sector";
	case TYPE_WRITE:
		return "Se escribio un Sector";
	case TYPE_END_CONECTION:
		return "Se cerro la conexion";
	}
	return "Se realizo una operacion desconocida";
}

// Dado un type de error, retorna su significado
char *Aux_type_error_string(int type) {
	switch (type) {
	case TYPE_ERROR_OPEN:
		return "No se pudo abrir el Disco";
	case TYPE_ERROR_CLOSE:
		return "No se pudo cerrar el Disco";
	case TYPE_ERROR_READ:
		return "No se pudo leer un Sector";
	case TYPE_ERROR_WRITE:
		return "No se pudo escribir un Sector";
	}
	return "No se pudo realizar una operacion desconocida";
}

// Retorna la fecha y hora del sistema
char* Aux_date_string() {
	time_t rawtime;
	struct tm * timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	return asctime(timeinfo);
}

// Retorna los microsegundos actuales del sistema
long int Aux_actual_microseconds() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_usec;
}

// Dada dos cadenas retorna una nueva que es la concatenacion de ambas
char* Aux_strings_concatenate(char* s1, char* s2) {
	char* s3 = (char*) malloc(strlen(s1) + strlen(s2) + 1);
	strcpy(s3, s1);
	strcpy(s3 + strlen(s1), s2);
	return s3;
}
