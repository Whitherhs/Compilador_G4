#include <stdio.h>
#include <assert.h>
#include "helpers/vector.h"
#include "helpers/buffer.h"
#include "compiler.h"

int main() {
    printf("Compiladores - TURMA A/B - GRUPO 4\n\n");

    int res = compile_file("./teste.c","./outtest",0);
    if (res == COMPILER_FILE_COMPILED_OK){
        printf("Todos os arquivos foram compilados com sucesso!\n");
    } else if (res == COMPILER_FAILED_WITH_ERRORS){
        printf("Erro de compilação");
    } else {
        printf("Erro de compilação");
    }

    return 0;
}
