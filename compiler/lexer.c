/* BEGIN – LAB 2 ----------------------------------------------*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h> 
#include <ctype.h>
#include <assert.h>
#include "compiler.h"
#include "helpers/vector.h"
#include "helpers/buffer.h"

#define LEX_GETC_IF(buffer, c, exp) \
    for (c = peekc(); exp; c = peekc()){ \
        buffer_write(buffer, c); \
        nextc(); \
    }

#define S_EQ(str, arg) (strcmp((str), (arg)) == 0)

struct token* read_next_token();
static struct lex_process* lex_process;
static struct token tmp_token;
// Pegar um caractere do arquivo, sem mudar a posição de leitura.
static char peekc() {
    return lex_process->function->peek_char(lex_process);
}

static void lex_new_expression (){
    lex_process->current_expression_count++;
    if (lex_process->current_expression_count == 1){
        lex_process->parentheses_buffer = buffer_create();
    }
}

static void lex_finish_expression (){
    lex_process->current_expression_count--;

    if (lex_process->current_expression_count < 0){
        compiler_error(lex_process->compiler, "Voce fechou uma expressao nunca iniciada!");
    }
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
    
    printf("Uma linha foi pulada em alguma parte do codigo\n");

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

bool is_keyword(const char* str) {
    return S_EQ(str, "unsigned") ||
           S_EQ(str, "signed") ||
           S_EQ(str, "char") ||
           S_EQ(str, "short") ||
           S_EQ(str, "int") ||
           S_EQ(str, "long") ||
           S_EQ(str, "float") ||
           S_EQ(str, "double") ||
           S_EQ(str, "void") ||
           S_EQ(str, "struct") ||
           S_EQ(str, "union") ||
           S_EQ(str, "static") ||
           S_EQ(str, "__ignore_typecheck") ||
           S_EQ(str, "return") ||
           S_EQ(str, "include") ||
           S_EQ(str, "sizeof") ||
           S_EQ(str, "if") ||
           S_EQ(str, "else") ||
           S_EQ(str, "while") ||
           S_EQ(str, "for") ||
           S_EQ(str, "do") ||
           S_EQ(str, "break") ||
           S_EQ(str, "continue") ||
           S_EQ(str, "switch") ||
           S_EQ(str, "case") ||
           S_EQ(str, "default") ||
           S_EQ(str, "goto") ||
           S_EQ(str, "typedef") ||
           S_EQ(str, "const") ||
           S_EQ(str, "extern") ||
           S_EQ(str, "retrict");
}
static struct token* make_identifier_or_keyword() {
    struct buffer* buffer = buffer_create();
    char c;
    LEX_GETC_IF(buffer, c, (c >= 'a' && c <= 'z') || 
                          (c >= 'A' && c <= 'Z') || 
                          (c >= '0' && c <= '9') || 
                          (c == '_'));

    buffer_write(buffer, 0x00);
    printf("Token: %s\n", buffer->data);

    if (is_keyword(buffer_ptr(buffer))) {
        return token_create(&(struct token){.type=TOKEN_TYPE_KEYWORD, .sval=buffer_ptr(buffer)});
    }

    return token_create(&(struct token){.type=TOKEN_TYPE_IDENTIFIER, .sval=buffer_ptr(buffer)});
}

struct token* read_special_token(){
    char c = peekc();
};


const char* read_symbol_str() {
    struct buffer* buffer = buffer_create();
    char c = peekc();
    if (c != EOF && (c == '{'  || c == '}'  || c == ':'  || c == ';'  || c == '#'  || c == '\\' || c == ']'  || c == ')')){
        buffer_write(buffer, c);
        nextc();
        if (c == ')'){
            lex_finish_expression();
        }
        
        // Finaliza a string.
        buffer_write(buffer, 0x00);
    
        printf("Token: %s\n", buffer->data);
        // Retorna o ponteiro para o buffer.
        return buffer_ptr(buffer);
    }
    return NULL;
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

struct token* token_make_one_line_comment() {
    struct buffer* buffer = buffer_create();
    char c = 0;
    LEX_GETC_IF(buffer, c, c != '\n' && c != EOF);

    buffer_write(buffer, 0x00);
    return token_create(&(struct token){.type = TOKEN_TYPE_COMMENT, .sval = buffer_ptr(buffer)});
}

struct token* token_make_multiline_comment() {
    struct buffer* buffer = buffer_create();
    char c = 0;
    while (1) {
        LEX_GETC_IF(buffer, c, c != '*' && c != EOF);

        if (c == EOF) {
            compiler_error(lex_process->compiler, "O comentario nao foi fechado\n");
        } else if (c == '*') {
            nextc(); // Pula para o próximo caractere
            if (peekc() == '/') {
                nextc(); // Finaliza o comentário
                break;
            }
        }
    }
    buffer_write(buffer, 0x00);
    
    return token_create(&(struct token){.type = TOKEN_TYPE_COMMENT, .sval = buffer_ptr(buffer)});
}

struct token* handle_comment() {
    char c = peekc();
    if (c == '/') {
        nextc();
        if (peekc() == '/') {
            nextc();
            return token_make_one_line_comment();
        } else if (peekc() == '*') {
            nextc();
            return token_make_multiline_comment();
        }

        pushc('/');
        return token_make_one_line_comment();
    }

    return NULL;
}

static struct token *token_make_string(char start_delim, char end_delim)
{
    struct buffer* buf = buffer_create();
    assert(nextc() == start_delim); //verifica se o caracter inicial eh aspas duplas
    char c = nextc();

    for (; c != end_delim && c != EOF; c = nextc())
    {
        if (c =='\\')
        {//retira o enter do final da string (se tiver)
            continue;
        }
        buffer_write(buf, c);
    }

    buffer_write(buf, 0x00);
    printf("Token: %s\n", buf->data);

    struct token* token = token_create(&(struct token){.type = TOKEN_TYPE_STRING, .sval = buffer_ptr(buf)});
    return token;
}


const char* read_operator_str() {
    struct buffer* buffer = buffer_create();
    char c = peekc();
    if (c == EOF) return NULL;
    buffer_write(buffer, c);
    if (c == '('){
        lex_new_expression();
    }
    if (c == '?' || c == '^'|| c == '~'|| c == '('|| c == '['){
        nextc();
        buffer_write(buffer, 0x00);
        printf("Token: %s\n", buffer->data);
        return buffer_ptr(buffer);
    }
    char d = c;
    nextc();
    c = peekc();
    if(d == c && c != EOF){
        buffer_write(buffer, c);
        if (c == '*'){
            d = c;
            nextc();
            c = peekc();
            if (c == '*' && c != EOF){
                buffer_write(buffer, c);
                nextc();
                buffer_write(buffer, 0x00);
                printf("Token: %s\n", buffer->data);
                return buffer_ptr(buffer);
            }
        }
        printf("Token: %s\n", buffer->data);
        nextc();
        buffer_write(buffer, 0x00);
        return buffer_ptr(buffer);

    }else if((d == '+' || d == '-'|| d == '*'|| d == '/'|| d == '!'|| d == '>'|| d == '<'|| d == '%') && c == '='){
        buffer_write(buffer, c);
        nextc();
        printf("Token: %s\n", buffer->data);
        buffer_write(buffer, 0x00);
        return buffer_ptr(buffer);
    }else{
        printf("Token: %s\n", buffer->data);
        buffer_write(buffer, 0x00);
        return buffer_ptr(buffer);
    }
    return NULL;
}

struct token* token_make_operator_for_value(const char* op) {
    return token_create(&(struct token){.type = TOKEN_TYPE_OPERATOR});
}

struct token* token_make_operator() {
    const char* op = read_operator_str();
    return token_make_operator_for_value(op);
}


// Função responsável por ler o próximo token do arquivo.
struct token* read_next_token() {
    struct token* token = NULL;
    char c = peekc();
    token = handle_comment();
    if (token) return token;

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
            token = token_make_string('"','"');
            break;
        case ' ':
        case '\t':
            token = handle_whitespace();
            break;
        case '\n':
            token = handle_newline();
            break;
        default:
            compiler_error(lex_process->compiler, "Token invalido!\n");
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
