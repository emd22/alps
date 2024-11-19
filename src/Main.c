#include "Lexer.h"
#include "Parser.h"
#include "Compiler.h"

#include <stdio.h>
#include <stdlib.h>

char *LoadFile(void) {
    FILE *fp = fopen("../test.alps", "rb");
    if (fp == NULL)
        return NULL;

    fseek(fp, 0, SEEK_END);
    unsigned file_size = ftell(fp);
    rewind(fp);

    char *buffer = (char *)malloc(file_size);
    fread(buffer, 1, file_size, fp);
    fclose(fp);
    return buffer;
}

void PrintLexerTokens(Lexer *inst)
{
    int i;
    for (i = 0; i < inst->token_amt; i++) {
        printf(
            "Token: [%.*s] type: %s\n",
            (int)LexerTokenLength(&inst->tokens[i]),
            LexToken(inst->tokens[i]),
            LexerTokenTypeStr(inst->tokens[i].type)
        );
    }
}


int main() {
    char *data;
    if ((data = LoadFile()) == NULL) {
        printf("Could not load file\n");
        return 1;
    }
    Lexer inst;
    inst = LexerLex(data, "+-*/=:;,.(){}", SFLEX_USE_STRINGS);

    // PrintLexerTokens(&inst);

    printf("\n=== PARSE TREE ===\n\n");

    Parser parser = ParserInit(inst);
    Node *ast = Parse(&parser);
    ParserPrintAST(ast, 0);

    Compiler compiler;

    printf("\n=== OUTPUT ===\n\n");

    compiler = CompilerInit(ast, "test.asm");

    CmCompileProgram(&compiler);

    CompilerDestroy();

    LexerDestroy(&inst);

    return 0;
}
