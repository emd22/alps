#include "Compiler.h"
#include "Lexer.h"
#include "Parser.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


static Compiler *cm;


Compiler CompilerInit(Node *ast, char *output_path)
{
    Compiler compiler;

    compiler.ast = ast;
    compiler.output_file = fopen(output_path, "w");

    return compiler;
}

void CompilerDestroy()
{
    fclose(cm->output_file);
}



void CmWrite_(Compiler *cm, char *msg, ...)
{
    printf("\t");
    va_list va;
    va_start(va, msg);
    vfprintf(cm->output_file, msg, va);
    vprintf(msg, va);
    va_end(va);
}

#define CmWrite(msg, ...) CmWrite_(cm, msg, __VA_ARGS__)

int GetSPSize(int real_size)
{
    int size = 16;

    while (real_size > size) {
        size += 16;
    }

    return size;
}

typedef struct {
    Token *name;
    int stack_index;
    int sp_size;
} CmFunc;

typedef enum {
    CR_SP,
    CR_W0,
    CR_W1,
    CR_W2,
    CR_W3,

    CR_W8,
    CR_W9,
} CmRegN;

typedef struct {
    Node *value;
    Token *name;

    int stack_position;

    CmRegN reg;
} CmVariable;



static CmVariable variables[64];
static int var_index = 0;

CmVariable *CmNewVariable(Token *name)
{
    CmVariable *var = &variables[var_index++];
    var->name = name;
    var->stack_position = -1;
    return var;
}

int MinSz(int a, int b) {
    if (a < b) {
        return a;
    }
    return b;
}

CmVariable *CmFindVariable(Token *name)
{
    int i;
    for (i = 0; i < var_index; i++) {
        if (!strncmp(name->start, variables[i].name->start, MinSz(LexerTokenLength(variables[i].name), LexerTokenLength(name)))) {
            return &variables[i];
        }
    }
    return NULL;
}

void CmCompileBlock(Node *node, CmFunc *cmfunc);

// TODO: determine size by type.
int GetTypeSz()
{
    return 4;
}

int CountStorageSz(NodeBlock *block)
{
    int size = 0;

    int i;
    for (i = 0; i < block->statement_count; i++) {
        Node *statement = block->statements[i];

        if (statement->type == NT_DECLARE) {
            size += GetTypeSz();
        }
    }

    return size;
}

int TokenToInt(Token *tk)
{
    char v[48];
    strncpy(v, tk->start, LexerTokenLength(tk));
    return atoi(v);
}


// Token *CmCompileExpr(Node *node, CmRegN reg)
// {
//     // char *instr = (initial ? "mov" : "")

//     if (node->type == NT_BINOP) {
//         NodeBinOp *binop =  (NodeBinOp *)node;
//         CmCompileExpr(binop->left, reg);
//         CmCompileExpr(binop->right, reg);
//     }
//     if (node->type == NT_LITERAL) {

//     }

// }


void CmBinOp(NodeBinOp *binop);

void CmSide(Node *side)
{
    if (side->type == NT_BINOP) {
        CmBinOp((NodeBinOp *)side);
    }
    else if (side->type == NT_VAR) {
        CmVariable *var = CmFindVariable(((NodeVar *)side)->value);
        CmWrite("ldr w9, [sp, #%d]\n", var->stack_position);
        CmWrite("add w8, w8, w9\n", 0);
    }
    else if (side->type == NT_LITERAL) {
        NodeLiteral *lit = (NodeLiteral  *)side;
        CmWrite("add w8, w8, #%.*s\n", TKPF(lit->token));
    }
}


void CmBinOp(NodeBinOp *binop)
{
    CmSide(binop->left);
    CmSide(binop->right);
}

void CmFuncEnd(CmFunc *func)
{
    CmWrite("add sp, sp, #%d\n", func->sp_size);
    CmWrite("ldp fp, lr, [sp], 64\n", 0);

    // if (strncmp(func->name->start, "_main", LexerTokenLength(func->name))) {
    //     CmWrite("bx lr\n", 0);
    // }
    CmWrite("ret\n", 0);
}

/**
.globl _main
b _main

test:
//sub sp, sp, #16
mov w0, #5
//add sp, sp, #16
ret

_main:
stp	x29, x30, [sp, -64]!
sub sp, sp, #16

bl test
mov w8, #5
add w8, w8, w0;
str w8, [sp, #12]
ldr w0, [sp, #12]
add sp, sp, #16
ldp	x29, x30, [sp], 64
ret


*/


void CmCompileStatement(Node *statement, CmFunc *func)
{
    if (statement->type == NT_LITERAL) {
        NodeLiteral *lit = (NodeLiteral *)statement;
        CmWrite("#%.*s", TKPF(lit->token));
    }
    else if (statement->type == NT_DECLARE) {
        NodeDeclare *declare = (NodeDeclare *)statement;
        NodeVar *node_var = ((NodeVar*)declare->variable);

        CmVariable *var = CmNewVariable(node_var->value);

        func->stack_index -= 4;

        if (var) {
            var->stack_position = func->stack_index;
        }
    }
    else if (statement->type == NT_ASSIGN) {

        NodeAssign *assign = (NodeAssign *)statement;

        // TODO: do not expect just a variable on lhs
        NodeVar *node_var = (NodeVar *)assign->left;

        CmVariable *var = CmFindVariable(node_var->value);

        if (var == NULL) {
            printf("Could not find variable!\n");
        }

        if (assign->right->type == NT_LITERAL) {
            CmWrite("mov w8, #%.*s\n", TKPF(((NodeLiteral *)assign->right)->token));
        }
        else {
            CmWrite("mov w8, wzr\n", 0);
            // CmCompileExpr(assign->right, CR_W8);
            if (assign->right->type == NT_BINOP) {
                CmBinOp((NodeBinOp *)assign->right);
            }
            else if (assign->right->type == NT_FUNC_CALL) {
                CmCompileStatement(assign->right, func);
                CmWrite("mov w8, w0\n", 0);
            }
            // CmCompileStatement(assign->right, func);

        }

        // save variable onto stack


        CmWrite("str w8, [sp, #%d]\n", var->stack_position);
    }
    else if (statement->type == NT_RETURN) {
        NodeReturn *ret = (NodeReturn *)statement;

        if (ret->value->type == NT_LITERAL) {
            CmWrite("mov w0, #%.*s\n", TKPF(((NodeLiteral *)ret->value)->token));
        }
        else if (ret->value->type == NT_VAR) {
            CmVariable *variable = CmFindVariable(((NodeVar *)ret->value)->value);
            CmWrite("ldr w0, [sp, #%d]\n", variable->stack_position);
        }
        CmFuncEnd(func);
    }
    else if (statement->type == NT_BINOP) {
        NodeBinOp *binop = (NodeBinOp *)statement;

        int x = 0;
        int y = 0;

        if (binop->left->type == NT_LITERAL && binop->right->type == NT_LITERAL) {
            // precalculate
            NodeLiteral *a = (NodeLiteral *)binop->left;
            NodeLiteral *b = (NodeLiteral *)binop->right;

            x = TokenToInt(a->token);
            y = TokenToInt(b->token);
        }

        if (binop->op->type == TT_PLUS) {
            if (binop->left->type == NT_LITERAL && binop->right->type == NT_LITERAL) {
                CmWrite("add w8, w8, #%d\n", x + y);
            }
        }
    }
    else if (statement->type == NT_FUNC_CALL) {
        NodeFuncCall *call = (NodeFuncCall *)statement;

        CmWrite("bl %.*s\n", TKPF(call->func->value));
    }
    else if (statement->type == NT_FUNC_DECLARE) {
        NodeFuncDeclare *nfd = (NodeFuncDeclare *)statement;


        Token *name = ((NodeVar *)nfd->declaration->variable)->value;

        CmWrite("%.*s:\n", TKPF(name));


        int storage_size = 0;
        if (nfd->block) {
            storage_size = CountStorageSz(nfd->block);
        }
        // TODO: dont assume int!
        const int arguments_size =  nfd->argument_count * 4;

        const int sp_size =  GetSPSize(storage_size + arguments_size);

        CmFunc *cmfunc = malloc(sizeof(CmFunc));
        cmfunc->stack_index = sp_size;
        cmfunc->sp_size = sp_size;
        cmfunc->name = name;

        CmWrite("stp fp, lr, [sp, -64]!\n", 0);
        CmWrite("sub sp, sp, #%d\n", sp_size);

        // CmWrite("mov fp, sp\n", 0);
        // CmWrite("sub sp, sp, #16\n", 0);

        // CmWrite("add x29, sp, #16\n", 0);

        // compile each argument's declare statements
        int i;
        for (i = 0; i < nfd->argument_count; i++) {
            CmCompileStatement((Node *)nfd->arguments[i], cmfunc);
        }

        // compile the block
        if (nfd->block) {
            CmCompileBlock((Node *)nfd->block, cmfunc);
        }


        // CmWrite("mov sp, fp\n", 0);



    }
}



void CmCompileBlock(Node *node, CmFunc *cmfunc)
{
    NodeBlock *block = (NodeBlock *)node;

    int i;
    for (i = 0; i < block->statement_count; i++) {
        CmCompileStatement(block->statements[i], cmfunc);
    }
}

void CmCompileProgram(Compiler *cm_)
{
    cm = cm_;
    CmWrite(".globl _main\n", 0);
    if (cm->ast->type == NT_BLOCK) {
        CmCompileBlock(cm->ast, NULL);
    }
}
