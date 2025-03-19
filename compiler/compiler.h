#ifndef COMPILER_H
#define COMPILER_H

#include <stdio.h>
#include <stdlib.h>

enum
{
    COMPILER_FILE_COMPILED_OK,
    COMPILER_FAILED_WITH_ERRORS
};

struct compile_process 
{
    //como o arquivo deve ser compilado
    int flags;

    struct compile_prcess_imput_file
    {
        FILE* fp;
        const char* abs_path;
    } cfile;

    FILE* ofile;  
};

int compile_file(const char* filename, const char* out_finename, int flags);
struct compile_process* compile_process_create(const char* filename, const char* filename_out, int flags);

#endif
