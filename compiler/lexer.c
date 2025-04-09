/* BEGIN – LAB 2 ----------------------------------------------*/
#include <string.h>
#include <stdlib.h>
#include "compiler.h"
#include "helpers/vector.h"
#include "helpers/buffer.h"

#define LEX_GETC_IF(buffer, c, exp) \
    for (c = peekc(); exp; c = peekc()){ \
        buffer_write(buffer, c); \
        nextc(); \
    }

struct token* read_next_token();
static struct lex_process* lex_process;
static struct token tmp_token;
// Pegar um caractere do arquivo, sem mudar a posição de leitura.
static char peekc() {
    return lex_process->function->peek_char(lex_process);
}

static char nextc() {
    char c = lex_process->function->next_char(lex_process);
    lex_process->pos.col += 1;
    if (c == '\n') {
        lex_process->pos.line += 1;
        lex_process->pos.col = 1;
    }
    return c;
}

// Adicionar um caractere no arquivo.
static void pushc(char c) {
    lex_process->function->push_char(lex_process, c);
}

static struct pos lex_file_position() {
    return lex_process->pos;
}
struct token* token_create(struct token* _token) {
    memcpy(&tmp_token, _token, sizeof(struct token));
    tmp_token.pos = lex_file_position();

    return &tmp_token;
}

static struct token* lexer_last_token() {
    return vector_back_or_null(lex_process->token_vec);
}

static struct token* handle_whitespace() {
    struct token* last_token = lexer_last_token();
    if (last_token) {
        last_token->whitespace = true;
    }

    nextc();
    return read_next_token();
}
static struct token* handle_newline() {
    nextc();
    return token_create(&(struct token){.type = TOKEN_TYPE_NEWLINE});
}

const char* read_number_str() {
    const char* num = NULL;
    struct buffer* buffer = buffer_create();
    char c = peekc();
    LEX_GETC_IF(buffer, c, (c >= '0' && c <= '9'));

    // Finaliza a string.
    buffer_write(buffer, 0x00);

    printf("Token: %s\n", buffer->data);
    // Retorna o ponteiro para o buffer.
    return buffer_ptr(buffer);
}

unsigned long long read_number() {
    const char* s = read_number_str();
    return atoll(s);
}

struct token* token_make_number_for_value(unsigned long number) {
    return token_create(&(struct token){.type = TOKEN_TYPE_NUMBER, .llnum = number});
}
struct token* token_make_number() {
    return token_make_number_for_value(read_number());
}

const char* operadores[] = {
    "==", "!=", ">=", "<=", ">>", "<<", "&&", "||", "***",
    "=", "-", "+", "*", ">", "<", "^", "%%", "!", "~", "|", "&",
    "(", "[" 
};
int total_operadores_validos = sizeof(operadores_validos) / sizeof(operadores_validos[0]);

int operador_valido(const char* str) {
    for (int i = 0; i < total_operadores_validos; i++) {
        if (strcmp(str, operadores_validos[i]) == 0) return 1;
    }
    return 0;
}

const char* read_operator_str() {
    struct buffer* buffer = buffer_create();
    char c1 = peekc();

    if (is_eof()) return NULL;

    buffer_write(buffer, nextc());

    for (int i = 0; i < 2 && !is_eof(); i++) {
        char c2 = peekc();
        buffer_write(buffer, c2);

        buffer_write(buffer, 0x00);
        if (operador_valido(buffer->data)) {
            nextc();
        } else {
            buffer->data[buffer->length - 1] = '\0';
            buffer->length--;
            break;
        }
    }

    buffer_write(buffer, 0x00);
    printf("Token: %s\n", buffer->data);
    return buffer_ptr(buffer);
}

const char* read_operator() {
    return read_operator_str();
}

struct token* token_make_operator_for_value(const char* op) {
    return token_create(&(struct token){
        .type = TOKEN_TYPE_OPERATOR,
        .str = strdup(op)
    });
}

struct token* token_make_operator() {
    const char* op = read_operator();
    if (!op) return NULL;
    return token_make_operator_for_value(op);
}

const char* read_symbol_str() {
    const char* sym = NULL;
    struct buffer* buffer = buffer_create();
    char c = peekc();
    LEX_GETC_IF(buffer, c, (c == '{'  || c == '}'  || c == ':'  || c == ';'  || \
                            c == '#'  || c == '\\' || c == ']'  || c == ')'));
    
    // Finaliza a string.
    buffer_write(buffer, 0x00);

    printf("Token: %s\n", buffer->data);
    // Retorna o ponteiro para o buffer.
    return buffer_ptr(buffer);
}

unsigned long long read_symbol() {
    const char* s = read_symbol_str();
    return atoll(s);
}

struct token* token_make_symbol_for_value(unsigned long symbol) {
    return token_create(&(struct token){.type = TOKEN_TYPE_SYMBOL, .llnum = symbol});
}
struct token* token_make_symbol() {
    return token_make_symbol_for_value(read_symbol());
}
/*
unsigned long long read_string() {
    struct buffer* buf = buffer_create();
    char c = nextc();

    while (nextc != '"'){
        buffer_write(buf, c);
        nextc();
    }

    if (nextc != '"'){
        return NULL;
    }

    // Finaliza a string.
    printf("Token: %s\n", buf->data);
    // Retorna o ponteiro para o buffer.
    return buffer_ptr(buf);
}

struct token* token_make_string_for_value(unsigned long string) {
    return token_create(&(struct token){.type = TOKEN_TYPE_STRING, .llnum = string});
}
struct token* token_make_string() {
    return token_make_string_for_value(read_string());
}
*/

// Função responsável por ler o próximo token do arquivo.
struct token* read_next_token() {
    struct token* token = NULL;
    char c = peekc();
    switch (c)
    {
        case EOF:
            // Fim do arquivo.
            break;

        NUMERIC_CASE:
            token = token_make_number();
            break;
        
        SYMBOL_CASE:
            token = token_make_symbol();
            break;
        OPERATOR_CASE:
            token = token_make_operator();
            break;
        case '"':
            //token = token_make_string();
            break;

        case ' ':
        case '\t':
            token = handle_whitespace();
            break;
        
        case '//':
        case '\n':
            token = handle_newline();
            break;

        default:
            // compiler_error(lex_process->compiler, "Token invalido!\n");
            break;
    }
    return token;
}
int lex(struct lex_process* process) {
    process->current_expression_count = 0;
    process->parentheses_buffer = NULL;
    lex_process = process;
    process->pos.filename = process->compiler->cfile.abs_path;

    struct token* token = read_next_token();

    // Ler todos os tokens do arquivo de input
    while (token) {
        vector_push(process->token_vec, token);
        token = read_next_token();
    }

    return LEXICAL_ANALYSIS_ALL_OK;
}
/* END – LAB 2 ----------------------------------------------*/
