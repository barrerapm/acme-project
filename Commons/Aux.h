#ifndef AUX_H_
#define AUX_H_

#define TYPE_HANDSHAKE 0
#define TYPE_OPEN 1
#define TYPE_CLOSE 2
#define TYPE_READ 3
#define TYPE_WRITE 4
#define TYPE_END_CONECTION 5
#define TYPE_STATE 10
#define TYPE_TRACE 11
#define TYPE_ERROR_OPEN -1
#define TYPE_ERROR_CLOSE -2
#define TYPE_ERROR_READ -3
#define TYPE_ERROR_WRITE -4
#define TYPE_ERROR_STATE -10

#define STATE_OK 1
#define STATE_SYNC 0

#define REPLY 1
#define NO_REPLY 0

#define FIRST_FILE_ENTRY 2
#define ENTRY_SIZE 32

#define FAT_ENTRY_SIZE 4

#define MAX_FILE_NAME_LENGHT 28

#define LAST_FILE_CLUSTER 0xFFFFFFF

#define LAST_LONG_ENTRY 0x41

#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME 0x0F
#define ATTR_OFFSET 11

#define FREE_USED_ENTRY 0xe5
#define FREE_ENTRY 0x00

int Aux_get_pos_in_string_vec(char** vec, int n_vec_elements, char* string);
void Aux_print_string_vec(char** vec, int n_vec_elems);
int Aux_do_command(int(*functions[])(int, char**), char** commands, int n_commands);
void Aux_string_vec_destroy(char*** vec, int size);
void Aux_get_tokens_by(const char* flag, const char* string, char*** vec_tokens, int* n_tokens);
void Aux_printf(const char* s, ...);
void Aux_print_header(const char* s1, const char* s2);
char *Aux_type_error_string(int);
char *Aux_type_string(int);
long int Aux_actual_microseconds();
char* Aux_date_string();
char* Aux_strings_concatenate(char* s1, char* s2);

#endif /* AUX_H_ */
