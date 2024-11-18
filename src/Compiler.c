#include "Compiler.h"
#include "Lexer.h"
#include "Parser.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


static Compiler *cm;

static int current_scope = 0;


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
    int i;
    for (i = 0; i < current_scope; i++) {
        printf("\t");
    }
    va_list va;
    va_start(va, msg);
    vfprintf(cm->output_file, msg, va);
    vprintf(msg, va);
    va_end(va);
}

void CmWriteV_(Compiler *cm, char *msg, va_list va)
{
    int i;
    for (i = 0; i < current_scope; i++) {
        printf("\t");
    }
    vfprintf(cm->output_file, msg, va);
    vprintf(msg, va);
}

#define CmWrite(msg, ...) CmWrite_(cm, msg, __VA_ARGS__)
#define CmWriteV(msg, va) CmWrite_(cm, msg, va)

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
    CR_LR,
    CR_FR,

    CR_W0,
    CR_W1,
    CR_W2,
    CR_W3,
    CR_W4,
    CR_W5,
    CR_W6,
    CR_W7,
    CR_W8,
    CR_W9,
    CR_W10,
    CR_W11,
    CR_W12,
} RegN;

typedef struct {
    Node *value;
    Token *name;

    int scope;
    int stack_position;

    RegN reg;
} CmVariable;


static CmVariable variables[64];
static int var_index = 0;


const char *RegS(RegN reg_n)
{
    const char *regs[] = {
        "sp", "lr", "fr",
        "w0", "w1", "w2", "w3", "w4", "w5", "w6",
        "w7", "w8", "w9", "w10", "w11", "w12"
    };
    return regs[reg_n];
}


CmVariable *CmNewVariable(Token *name)
{
    CmVariable *var = &variables[var_index++];
    var->name = name;
    var->stack_position = -1;
    var->reg = CR_W8;
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
    const int length = LexerTokenLength(tk);
    strncpy(v, tk->start, length);
    v[length] = 0;
    return atoi(v);
}


// Token *CmCompileExpr(Node *node, RegN reg)
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


void CmBinOp(NodeBinOp *binop, bool should_mov);
void CmCompileExpr(Node *node, RegN dest, CmFunc *func);
void CmCompileStatement(Node *statement, CmFunc *func);

void CmFuncCall(NodeFuncCall *call)
{
    int i;
    for (i = 0; i < call->argument_count; i++) {
        CmCompileExpr(call->arguments[i], CR_W0 + i, NULL);
    }
    CmWrite("bl %.*s\n", TKPF(call->func->value));
}

const char *ArithTypeToInstr(TokenType type)
{
    switch (type) {
        case TT_PLUS:
            return "add";
        case TT_MINUS:
            return "sub";
        case TT_STAR:
            return "mul";
        case TT_SLASH:
            return "udiv";
        default:
            break;
    }

    return "add";
}

void CmArithInst(char *instr, bool should_mov, RegN dest, RegN src)
{
    const char *dests = RegS(dest);
    const char *srcs = RegS(src);

    if (should_mov) {
        CmWrite("mov %s, %s\n", dests, srcs);
    }
    else {
        CmWrite("%s %s, %s, %s\n", instr, dests, dests, srcs);
    }
}

void CmArithInstImm(char *instr, TokenType op_type, bool should_mov, RegN dest, int imm)
{
    const char *dests = RegS(dest);

    if (should_mov) {
        CmWrite("mov %s, %d\n", dest, imm);
    }
    else {
        if (op_type == TT_STAR || op_type == TT_SLASH) {
            CmWrite("mov w10, #%d\n", imm);
            CmWrite("%s %s, %s, w10\n", instr, dests, dests);
        }
        else {
            CmWrite("%s %s, %s, #%d\n", instr, dests, dests, imm);
        }
    }
}

void CmSide(Node *side, RegN reg, TokenType op_type, bool should_mov)
{
    char *instr = (char *)ArithTypeToInstr(op_type);

    if (side->type == NT_BINOP) {
        CmBinOp((NodeBinOp *)side, false);
    }
    else if (side->type == NT_VAR) {
        CmVariable *var = CmFindVariable(((NodeVar *)side)->value);
        CmWrite("ldr w9, [sp, #%d]\n", var->stack_position);
        // CmWrite("%s %s, %s, w9\n", instr, RegS(reg), RegS(reg));
        CmArithInst(instr, should_mov, reg, CR_W9);
    }
    else if (side->type == NT_LITERAL) {
        NodeLiteral *lit = (NodeLiteral  *)side;

        CmArithInstImm(instr, op_type, should_mov, reg, TokenToInt(lit->token));
        // mul and div operators cannot use immediate values,
        // pop out to a register
        // if (op_type == TT_STAR || op_type == TT_SLASH) {
        //     CmWrite("mov w10, #%.*s\n", TKPF(lit->token));
        //     CmWrite("%s %s, %s, w10\n", instr, RegS(reg), RegS(reg));
        // }
        // else {
        //     CmWrite("%s %s, %s, #%.*s\n", instr, RegS(reg), RegS(reg), TKPF(lit->token));
        // }
    }
    else if (side->type == NT_FUNC_CALL) {
        CmWrite("str %s, [sp, -16]!\n", RegS(reg));
        // CmWrite("mov w9, w8\n", 0);
        CmFuncCall((NodeFuncCall *)side);
        CmWrite("ldr %s, [sp], 16\n", RegS(reg));
        // CmWrite("mov w8, w9\n", 0);
        // CmWrite("%s %s, %s, w0\n", instr, RegS(reg), RegS(reg));
        CmArithInst(instr, should_mov, reg, CR_W0);
    }
}

int CmPrecalc(NodeBinOp *binop)
{
    NodeLiteral *a = (NodeLiteral *)binop->left;
    NodeLiteral *b = (NodeLiteral *)binop->right;

    int x = TokenToInt(a->token);
    int y = TokenToInt(b->token);

    switch (binop->op->type) {
        case TT_PLUS:
            return x + y;
        case TT_MINUS:
            return x - y;
        case TT_STAR:
            return x * y;
        case TT_SLASH:
            return x / y;
        default:
            break;
    }
    return 0;
}

void CmBinOp(NodeBinOp *binop, bool should_mov)
{
    if (binop->left->type == NT_LITERAL && binop->right->type == NT_LITERAL) {
        CmWrite("mov %s, #%d\n", RegS(CR_W8), CmPrecalc(binop));
        return;
    }
    // if there is a branch on the right side, swap the output order to preserve order of operations
    if (binop->right->type == NT_BINOP) {
        CmSide(binop->right, CR_W8, TT_NONE, true);
        CmSide(binop->left, CR_W8, binop->op->type, false);
    }
    else {
        CmSide(binop->left, CR_W8, TT_NONE, true);
        CmSide(binop->right, CR_W8, binop->op->type, false);
    }
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


void CmClearVariableScope(int scope)
{
    int i;
    int removed_indices = 0;
    for (i = 0; i < var_index; i++) {
        CmVariable *var = &variables[i];
        if (var->scope >= scope) {
            memset(var, 0, sizeof(CmVariable));
            removed_indices++;
        }
    }
    var_index -= removed_indices;
}


void CmCompileExpr(Node *node, RegN dest, CmFunc *func)
{
    if (node->type == NT_LITERAL) {
        CmWrite("mov %s, #%.*s\n", RegS(dest), TKPF(((NodeLiteral *)node)->token));
    }
    else {
        // CmWrite("mov w8, wzr\n", 0);
        // CmCompileExpr(assign->right, CR_W8);
        if (node->type == NT_BINOP) {
            // TODO: remove the only w8 restriction on CmBinOp
            CmBinOp((NodeBinOp *)node, true);
            if (dest != CR_W8) {
                CmWrite("mov %s, w8\n", RegS(dest));
            }
        }
        else if (node->type == NT_VAR) {
            CmVariable *variable = CmFindVariable(((NodeVar *)node)->value);
            CmWrite("ldr %s, [sp, #%d]\n", RegS(dest), variable->stack_position);
        }
        else if (node->type == NT_FUNC_CALL) {
            CmCompileStatement(node, func);
            CmWrite("mov %s, w0\n", RegS(dest));
        }
        // CmCompileStatement(assign->right, func);
    }
}

CmVariable *CmVarDeclare(Node *statement, RegN dest, CmFunc *func)
{
    NodeDeclare *declare = (NodeDeclare *)statement;
    NodeVar *node_var = ((NodeVar*)declare->variable);

    CmVariable *var = CmNewVariable(node_var->value);
    var->reg = dest;
    var->scope = current_scope;

    func->stack_index -= 4;

    if (var) {
        var->stack_position = func->stack_index;
    }

    return var;
}

void CmCompileStatement(Node *statement, CmFunc *func)
{
    if (statement->type == NT_LITERAL) {
        NodeLiteral *lit = (NodeLiteral *)statement;
        CmWrite("#%.*s", TKPF(lit->token));
    }
    else if (statement->type == NT_DECLARE) {
        CmVarDeclare(statement, CR_W8, func);
    }

    else if (statement->type == NT_ASSIGN) {
        NodeAssign *assign = (NodeAssign *)statement;

        // TODO: do not expect just a variable on lhs
        NodeVar *node_var = (NodeVar *)assign->left;

        CmVariable *var = CmFindVariable(node_var->value);

        if (var == NULL) {
            printf("Could not find variable!\n");
        }

        CmCompileExpr(assign->right, CR_W8, func);

        // save variable onto stack
        CmWrite("str w8, [sp, #%d]\n", var->stack_position);
    }
    else if (statement->type == NT_RETURN) {
        NodeReturn *ret = (NodeReturn *)statement;
        CmCompileExpr(ret->value, CR_W0, func);
        CmFuncEnd(func);
    }
    else if (statement->type == NT_FUNC_CALL) {
        CmFuncCall((NodeFuncCall *)statement);
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

        current_scope++;

        CmWrite("stp fp, lr, [sp, -64]!\n", 0);
        CmWrite("sub sp, sp, #%d\n", sp_size);

        // CmWrite("mov fp, sp\n", 0);
        // CmWrite("sub sp, sp, #16\n", 0);

        // CmWrite("add x29, sp, #16\n", 0);

        // compile each argument's declare statements
        int i;
        for (i = 0; i < nfd->argument_count; i++) {
            // CmCompileStatement((Node *)nfd->arguments[i], cmfunc);

            CmVariable *var = CmVarDeclare((Node *)nfd->arguments[i], CR_W1 + i, cmfunc);
            CmWrite("str %s, [sp, #%d]\n", RegS(CR_W0 + i), var->stack_position);
        }

        // compile the block
        if (nfd->block) {
            CmCompileBlock((Node *)nfd->block, cmfunc);
        }

        current_scope--;


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

    CmClearVariableScope(current_scope);
}

void CmCompileProgram(Compiler *cm_)
{
    cm = cm_;
    CmWrite(".globl _main\n", 0);
    if (cm->ast->type == NT_BLOCK) {
        CmCompileBlock(cm->ast, NULL);
    }
}
