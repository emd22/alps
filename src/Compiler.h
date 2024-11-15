#ifndef CML_COMPILER_H
#define CML_COMPILER_H

#include "Parser.h"

#include <stdio.h>


typedef struct {
    // Parser parser;
    Node *ast;
    FILE *output_file;
} Compiler;


Compiler CompilerInit(Node *ast, char *output_path);
void CmCompileProgram(Compiler *cm_);
void CompilerDestroy();

#endif
