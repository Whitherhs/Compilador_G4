#include "compiler.h"
#include <stdlib.h>
#include <stdarg.h>

struct lex_process_functions compiler_lex_functions = {
    .next_char=compile_process_next_char,
    .peek_char=compile_process_peek_char,
    .push_char=compile_process_push_char
};

/* Begin - LAB 3 ------------------------------------------------------------*/

void compiler_error(struct compile_process* compiler, const char* msg, ...){
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    fprintf(stderr, " na linha %i, coluna %i, arquivo %s\n",compiler->pos.line, compiler->pos.col, compiler->pos.filename);
 
    exit(-1);
}

void compiler_warning(struct compile_process* compiler, const char* msg, ...){
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    fprintf(stderr, " na linha %i, coluna %i, arquivo %s\n",compiler->pos.line, compiler->pos.col, compiler->pos.filename);

    exit(-1);
}
/* End - LAB 3 --------------------------------------------------------------*/

int compile_file(const char* filename, const char* out_filename, int flags) {
    struct compile_process* process = compile_process_create(filename, out_filename, flags);
    if (!process) {
        return COMPILER_FAILED_WITH_ERRORS;
    }
    
    /* AQUI ENTRA A ANÁLISE LÉXICA */
    struct lex_process* lex_process = lex_process_create(process, &compiler_lex_functions, NULL);

    if (!lex_process) return COMPILER_FAILED_WITH_ERRORS;

    if (lex(lex_process) != LEXICAL_ANALYSIS_ALL_OK) return COMPILER_FAILED_WITH_ERRORS;
    process->token_vec = lex_process->token_vec;  /* LAB3: Adicionar */

    /* AQUI ENTRA O PARSING DO CÓDIGO */
    if (parse(process) != PARSE_ALL_OK) return COMPILER_FAILED_WITH_ERRORS;  /* LAB3: Adicionar */

    /* AQUI ENTRA A GERAÇÃO DE CÓDIGO */

    return COMPILER_FILE_COMPILED_OK;
}
