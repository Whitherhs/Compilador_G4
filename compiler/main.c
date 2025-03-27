#include <stdio.h>
#include <assert.h>
#include "helpers/vector.h"
#include "helpers/buffer.h"
#include "compiler.h"

void test_vector() {
    // Criando um vetor de inteiros
    struct vector *vec = vector_create(sizeof(int));
    assert(vec != NULL);
    printf("Vetor criado com sucesso!\n");
    // Testando vector_push()
    int val1 = 10, val2 = 20, val3 = 30;
    vector_push(vec, &val1);
    vector_push(vec, &val2);
    vector_push(vec, &val3);
    printf("Valores inseridos: %d, %d, %d\n", val1, val2, val3);
    assert(vector_count(vec) == 3);
    // Testando vector_at() e vector_peek()
    printf("Valor na posição 0: %d\n", *(int *)vector_at(vec, 0));
    assert(*(int *)vector_at(vec, 0) == 10);
    printf("Valor na posição 1: %d\n", *(int *)vector_at(vec, 1));
    assert(*(int *)vector_at(vec, 1) == 20);
    printf("Valor na posição 2: %d\n", *(int *)vector_at(vec, 2));
    assert(*(int *)vector_at(vec, 2) == 30);
    // Testando vector_set_peek_pointer() e vector_peek()
    vector_set_peek_pointer(vec, 1);
    int peek_value = *(int *)vector_peek(vec); // Armazena o valor antes de imprimir e verificar
    printf("Peek na posição 1: %d\n", peek_value);
    assert(peek_value == 20);
    // Testando vector_pop()
    vector_pop(vec);
    printf("Após pop, tamanho do vetor: %d\n", vector_count(vec));
    assert(vector_count(vec) == 2);
    printf("Último elemento após pop: %d\n", *(int *)vector_back(vec));
    assert(*(int *)vector_back(vec) == 20);
    // Liberando memória
    vector_free(vec);
    printf("Todos os testes passaram!\n");
}

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
