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
bool lex_is_in_expresssion() {
	return lex_process->current_expression_count > 0;
}

//token handle whitespace
static struct token* handle_whitespace() {
	struct token* last_token = lexer_last_token();
	if (last_token) {
		last_token->whitespace = true;
	}
	nextc();
	return read_next_token();
}

//token new line
struct token* token_make_newline() {
	nextc();
	return token_create(&(struct token){.type=TOKEN_TYPE_NEWLINE});
}


//token number
const char* read_number_str() {
    struct buffer* buffer = buffer_create();
    char c = peekc();

    if (c == '0') {
        buffer_write(buffer, c);
        nextc();
        c = peekc();

        if (c == 'x' || c == 'X') {
            buffer_write(buffer, c);
            nextc();
            c = peekc();
            while (isxdigit(c)) {
                buffer_write(buffer, c);
                nextc();
                c = peekc();
            }
        } else if (c == 'b' || c == 'B') {
            buffer_write(buffer, c);
            nextc();
            c = peekc();
            while (c == '0' || c == '1') {
                buffer_write(buffer, c);
                nextc();
                c = peekc();
            }
        } else {
            // Trata número como decimal com zero à esquerda, ex: 0123
            while (isdigit(c)) {
                buffer_write(buffer, c);
                nextc();
                c = peekc();
            }
        }
    } else {
        while (isdigit(c)) {
            buffer_write(buffer, c);
            nextc();
            c = peekc();
        }
    }

    buffer_write(buffer, 0x00);
    return buffer_ptr(buffer);
}
unsigned long long read_number() {
    const char* s = read_number_str();

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        return strtoull(s + 2, NULL, 16); // hexadecimal
    } else if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
        return strtoull(s + 2, NULL, 2); // binário
    }

    return strtoull(s, NULL, 10); // decimal
}
struct token* token_make_number_for_value(unsigned long number) {
	return token_create(&(struct token){.type = TOKEN_TYPE_NUMBER, .llnum = number});
}
struct token* token_make_number() {
	return token_make_number_for_value(read_number());
}

//token identifier ou keyword
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
bool token_is_keyword(struct token* token, const char* str) {
	if (token->type != TOKEN_TYPE_KEYWORD) {
		return false;
	}
	return S_EQ(token->sval, str);
}
static struct token* make_identifier_or_keyword() {
	struct buffer* buffer = buffer_create();
	char c;
	LEX_GETC_IF(buffer, c, (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
						   (c >= '0' && c <= '9') || (c == '_'));

	buffer_write(buffer, 0x00);

	if (is_keyword(buffer_ptr(buffer))) {
		return token_create(&(struct token){.type=TOKEN_TYPE_KEYWORD, .sval=buffer_ptr(buffer)});
	}

	return token_create(&(struct token){.type=TOKEN_TYPE_IDENTIFIER, .sval=buffer_ptr(buffer)});
}
struct token* read_special_token(){
	char c = peekc();
	if (isalpha(c) || c == '_'){
		return make_identifier_or_keyword();
	}

	return NULL;
};

//token symbol
static struct token* token_make_symbol() {
	char c = nextc();
	if (c == ')') {
		lex_finish_expression();
	}
	struct token* token = token_create(&(struct token){.type=TOKEN_TYPE_SYMBOL, .cval=c});

	return token;
}

//token string
static struct token *token_make_string(char start_delim, char end_delim){
	struct buffer *buf = buffer_create();
	assert(nextc() == start_delim); // verifica se o caracter inicial eh aspas duplas
	char c = nextc();

	for (; c != end_delim && c != EOF; c = nextc())
	{
		if (c == '\\')
		{ // Retira o enter do final da string (se tiver).
			continue;
		}
		buffer_write(buf, c);
	}	
	buffer_write(buf, 0x00);

	struct token* token = token_create(&(struct token){.type = TOKEN_TYPE_STRING, .sval = buffer_ptr(buf)});
	return token;
}

//token operator 
static const char* read_op() {
	struct buffer* buffer = buffer_create();
	char d, c = peekc();

	if (c == EOF) return NULL;
	if (c == '+' || c ==  '-' || c ==  '*' || c ==  '>' || c == '<' || c ==  '^' || c == '%' || c ==  '!' || c == '=' || c ==  '~' || c ==  '|' || c ==  '&' && c != EOF){
		d = c;
		nextc(); // Consome o operador
		buffer_write(buffer, d); // Consome e grava
		c = peekc();
		// Trata o '=='.
		if (c == d && c != EOF && d == '='){
			d = c;
			nextc(); // Consome o operador
			buffer_write(buffer, d); // Consome e grava
			c = peekc();
			// Trata o '==='.
			if (d == '=' && c == '=') {
				d = c;
				nextc(); // Consome o operador
				buffer_write(buffer, d); // Consome e grava
				c = peekc();;
			}
		}
		// Trata casos como '*=', '!=', '+=', '-=', etc.
		else if (c == '=' && c != EOF) {
			d = c;
			nextc(); // Consome o operador
			buffer_write(buffer, d); // Consome e grava
			c = peekc();
		}
		// Trata casos como '++', '--', '&&', '||', '<<', '>>'.
		else if(d == c && c != EOF && (d == '+' || d == '-' || d == '*' || d == '&' || d == '|' || d == '<' || d == '>' || d == '/')){
			d = c;
			nextc(); // Consome o operador
			buffer_write(buffer, d); // Consome e grava
			c = peekc();
			// Trata casos como '<<=' e '>>='.
			if (c == '=' && (d == '<' || d == '>') && c != EOF) {
				d = c;
				nextc(); // Consome o operador
				buffer_write(buffer, d); // Consome e grava
				c = peekc();
			}
		}
		// Trata o caso de '->'.
		else if (d == '-' && c == '>' && c != EOF) {
			d = c;
			nextc(); // Consome o operador
			buffer_write(buffer, d); // Consome e grava
			c = peekc();
		}
		// Finaliza a string.
		buffer_write(buffer, 0x00);
		return buffer_ptr(buffer);
	}
	// Trata todo o resto.
	else if (c == '(' || c ==  '[' || c ==  ',' || c ==  '.' || c ==  '?' && c != EOF){
		d = c;
		nextc(); // Consome o operador
		buffer_write(buffer, d); // Consome e grava
		c = peekc();
		// Finaliza a string.
		buffer_write(buffer, 0x00);
		return buffer_ptr(buffer);
	}
	
	// Finaliza a string.
	buffer_write(buffer, 0x00);
	return buffer_ptr(buffer);
}
static struct token* token_make_operator() {
	char c = peekc();
	// Tratar o caso do #include <abc.h>
	if (c == '<') {
		struct token* last_token = lexer_last_token();
		if (token_is_keyword(last_token, "include")) {
			return token_make_string('<', '>');
		}
	}
	if (c == '(') {
		lex_new_expression();
	}

	const char* op = read_op();
	struct token* token = token_create(&(struct token){.type=TOKEN_TYPE_OPERATOR, .sval=op});
	return token;
}

//token comment
struct token* token_make_one_line_comment() {
	struct buffer* buffer = buffer_create();
	char c = 0;
	LEX_GETC_IF(buffer, c, c != '\n' && c != EOF);
	return token_create(&(struct token){.type = TOKEN_TYPE_COMMENT, .sval=buffer_ptr(buffer)});
}
struct token* token_make_multiline_comment() {
	struct buffer* buffer = buffer_create();
	char c = 0;
	while(1) {
		LEX_GETC_IF(buffer, c, c != '*' && c != EOF);

		if (c == EOF) {
			compiler_error(lex_process->compiler, "O comentario nao foi fechado\n");
		} else if (c == '*') {
			nextc(); // Pula para o próximo caractere do arquivo.
			if (peekc() == '/') {
				// Finaliza o comentário
				nextc();
				break;
			}
		}
	}

	return token_create(&(struct token){.type = TOKEN_TYPE_COMMENT, .sval=buffer_ptr(buffer)});
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
		return token_make_operator();
	}

	return NULL;
}

// Função responsável por ler o próximo token do arquivo.
struct token *read_next_token() {
	struct token *token = NULL;
	char c = peekc();
	
	token = handle_comment();
	if (token) return token;

	switch (c) {
		case EOF:
			break;
		NUMERIC_CASE:
			token = token_make_number();
			break;
		OPERATOR_CASE:
			token = token_make_operator();
			break;
		SYMBOL_CASE:
			token = token_make_symbol();
			break;
		case '\t':
		case ' ':
			token = handle_whitespace();
			break;
		case '"':
			token = token_make_string('"', '"');
			break;
		case '\n':
			token = token_make_newline();
			break;
		default:
			token = read_special_token();
			if (!token) {
				compiler_error(lex_process->compiler, "Token Inválido!\n");
			}
			break;
	}
	return token;
}

void print_token_list(struct lex_process *process) {
	struct token *token = NULL;

	for (int i = 0; i < process->token_vec->count; i++) {
		token = vector_at(process->token_vec, i);

		switch (token->type) {	;
			case TOKEN_TYPE_IDENTIFIER:
				printf("TOKEN ID:    %s\n", token->sval);
				break;
			case TOKEN_TYPE_KEYWORD:
				printf("TOKEN KEY:   %s\n", token->sval);
				break;
			case TOKEN_TYPE_NEWLINE:
				printf("TOKEN NL:    \\n\n");
				break;
			case TOKEN_TYPE_COMMENT:
				printf("TOKEN COMMT: %s\n", token->sval);
				break;
			case TOKEN_TYPE_NUMBER:	
				printf("TOKEN NUM:   %llu\n", token->llnum);
				break;
			case TOKEN_TYPE_OPERATOR:
				printf("TOKEN OP:    %s\n", token->sval);
				break;
			case TOKEN_TYPE_STRING:
				printf("TOKEN STR:   %s\n", token->sval);
				break;
			case TOKEN_TYPE_SYMBOL:
				printf("TOKEN SYM:   %c\n", token->cval);
				break;
			default:
				break;
		}
	}
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

	print_token_list(process);

	return LEXICAL_ANALYSIS_ALL_OK;
}

/* END – LAB 2 ----------------------------------------------*/
