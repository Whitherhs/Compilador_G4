#include "compiler.h"

int compile_file(const char* filename, const char* out_finename, int flags) {

    struct compile_process* process = compile_process_create(filename, out_finename, flags);
    if (!process)
    {
        return COMPILER_FAILED_WITH_ERRORS;
    }
    
    /* Aqui entra a analise lexica */

    /* Aqui entra parsing do código*/

    /* Aqui entra a geração de código*/

    return COMPILER_FILE_COMPILED_OK;
    
}