#include "Compiler.h"
#include "Lexer.h"
#include "Parser.h"
#include "InternalFuncs.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define CmWrite(msg, ...) CmWrite_(cm, msg, __VA_ARGS__)
#define CmWriteV(msg, va) CmWrite_(cm, msg, va)

// TODO: add all registers + 32 bit variations
typedef enum {
    CR_SP,
    CR_LR,
    CR_FP,

    CR_X0,
    CR_X1,
    CR_X2,
    CR_X3,
    CR_X4,
    CR_X5,
    CR_X6,
    CR_X7,
    CR_X8,
    CR_X9,
    CR_X10,
    CR_X11,
    CR_X12,
} RegN;

const char *RegNames[] = {
    "SP", "LR", "FP",
    "X0", "X1", "X2", "X3", "X4", "X5", "X6",
    "X7", "X8", "X9", "X10", "X11", "X12"
};

typedef struct
{
    NodeLiteral *value;
    char ref_name[64];
} CmStringLiteral;

typedef struct {
    Node *value;
    Token *name;

    int scope;
    int stack_position;

    RegN reg;

    CmStringLiteral *string_literal;
} CmVariable;


typedef struct {
    Token *name;
    int stack_index;
    int sp_size;
} CmFunc;

typedef struct {
    const char *name;
    void (*func)(Token *call, int arg_count, Node **arguments);
} CmInternalFunc;



void CmBinOp(NodeBinOp *binop, bool should_mov);
void CmCompileExpr(Node *node, RegN dest, CmFunc *func);
void CmCompileStatement(Node *statement, CmFunc *func);
void InternVarDelete_(Token *call, int arg_count, Node **args);
void CmCompileBlock(Node *node, CmFunc *cmfunc);

static Compiler *cm;

static int current_scope = 0;

static CmVariable variables[64];
static int var_index = 0;
static CmStringLiteral string_literals[64];
static int string_literal_index = 0;

static const CmInternalFunc internal_functions[] = {
    { "del", InternVarDelete_ },
};

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


int GetSPSize(int real_size)
{
    int size = 16;

    while (real_size > size) {
        size += 16;
    }

    return size;
}

static void ThrowError(Token *token, char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    if (token) {
        printf("[ERROR] [%d,%d]: ", token->file_line, token->file_col);
    }
    else {
        printf("[ERROR]: ");
    }
    vprintf(msg, ap);
    va_end(ap);

    exit(1);
}


const char *RegS(RegN reg_n)
{
    return RegNames[reg_n];
}


static void PrintVarList()
{
    printf("Vars:{");
    int i;
    for (i = 0; i < var_index; i++) {
        printf("%.*s", TKPF(variables[i].name));
        if (i < var_index - 1) {
            printf(", ");
        }
    }
    printf("}\n");
}

CmVariable *CmNewVariable(Token *name)
{
    CmVariable *var = &variables[var_index++];

    var->name = name;
    var->value = NULL;
    var->stack_position = -1;
    var->reg = CR_X8;

    return var;
}


void CmDeleteVariable(int index)
{
    memset(&variables[index], 0, sizeof(CmVariable));
    int i;
    for (i = index; i < var_index - 1; i++) {
        memcpy(&variables[i], &variables[i + 1], sizeof(CmVariable));
    }
    var_index -= 1;
}


static int MinSz_(int a, int b) {
    if (a < b) {
        return a;
    }
    return b;
}

CmVariable *CmFindVariable(Token *name, int *index)
{
    int i;
    for (i = 0; i < var_index; i++) {
        if (!strncmp(name->start, variables[i].name->start, MinSz_(LexerTokenLength(variables[i].name), LexerTokenLength(name)))) {
            if (index != NULL) {
                (*index) = i;
            }
            return &variables[i];
        }
    }
    ThrowError(name, "using undeclared variable '%.*s'\n", TKPF(name));
    return NULL;
}


void InternVarDelete_(Token *call, int arg_count, Node **args)
{
    int i;
    for (i = 0; i < arg_count; i++) {
        if (args[i]->type != NT_VAR) {
            ThrowError(call, "Invalid argument passed into del!\n");
        }

        NodeVar *arg = (NodeVar *)args[i];

        int vindex;

        if (CmFindVariable(arg->value, &vindex)) {
            CmDeleteVariable(vindex);
        }
    }
}



// TODO: determine size by type, do not always assume 64 bit!.
int GetTypeSz()
{
    return 8;
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

static bool CallInternalFuncs(NodeFuncCall *call)
{
    int i;
    const int func_count = sizeof(internal_functions) / sizeof(CmInternalFunc);

    for (i = 0; i < func_count; i++) {
        Token *name = call->func->value;
        int name_len = LexerTokenLength(name);

        const CmInternalFunc *func = &internal_functions[i];

        if (name_len == strlen(func->name) && !strncmp(name->start, internal_functions[i].name, name_len)) {
            printf("Calling %s\n", func->name);
            func->func(name, call->argument_count, call->arguments);
            return true;
        }
    }
    return false;
}

void CmFuncCall(NodeFuncCall *call)
{
    if (CallInternalFuncs(call)) {
        return;
    }

    int i;
    for (i = 0; i < call->argument_count; i++) {
        CmCompileExpr(call->arguments[i], CR_X0 + i, NULL);
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
            CmWrite("mov %s, #%d\n", RegS(CR_X10), imm);
            CmWrite("%s %s, %s, %s\n", instr, dests, dests, RegS(CR_X10));
        }
        else {
            CmWrite("%s %s, %s, #%d\n", instr, dests, dests, imm);
        }
    }
}

void CmLoadStr(RegN dest, char *name)
{
    CmWrite("adrp %s, .L.%s@PAGE\n", RegS(dest), name);
    CmWrite("add %s, %s, .L.%s@PAGEOFF\n", RegS(dest), RegS(dest), name);
}

/**
    Compile a side of a BinOp tree
    @param side - the side of the tree to compile
    @param reg - the register to output to
    @param op_type - the operation that is taking place
    @param should_mov - should be true if this is the first call taking place for an operation.
        this will use mov instructions as opposed to accumulating on an existing register.
*/
void CmSide(Node *side, RegN reg, TokenType op_type, bool should_mov)
{
    char *instr = (char *)ArithTypeToInstr(op_type);

    if (side->type == NT_BINOP) {
        CmBinOp((NodeBinOp *)side, false);
    }
    else if (side->type == NT_VAR) {
        CmVariable *var = CmFindVariable(((NodeVar *)side)->value, NULL);
        if (var->string_literal != NULL) {
            CmLoadStr(reg, var->string_literal->ref_name);
        }
        else {
            CmWrite("ldr %s, [%s, #%d]\n", RegS(CR_X9), RegS(CR_SP), var->stack_position);
            // CmWrite("%s %s, %s, w9\n", instr, RegS(reg), RegS(reg));
            CmArithInst(instr, should_mov, reg, CR_X9);
        }
    }
    else if (side->type == NT_LITERAL) {
        NodeLiteral *lit = (NodeLiteral  *)side;

        if (lit->token->type == TT_STRING) {
            CmStringLiteral *string_lit = &string_literals[string_literal_index++];

            char name[16];
            sprintf(name, "Str%d", string_literal_index);

            strcpy(string_lit->ref_name, name);
            string_lit->value = lit;

            return;
        }

        CmArithInstImm(instr, op_type, should_mov, reg, TokenToInt(lit->token));
    }
    else if (side->type == NT_FUNC_CALL) {
        CmWrite("str %s, [%s, -16]!\n", RegS(reg), RegS(CR_SP));
        // CmWrite("mov w9, w8\n", 0);
        CmFuncCall((NodeFuncCall *)side);
        CmWrite("ldr %s, [%s], 16\n", RegS(reg), RegS(CR_SP));
        // CmWrite("mov w8, w9\n", 0);
        // CmWrite("%s %s, %s, w0\n", instr, RegS(reg), RegS(reg));
        CmArithInst(instr, should_mov, reg, CR_X0);
    }
}

/**
    Precalculate constants for some instructions
*/
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

/**
    Compile both sides of a binary operator. This function resolves the sides in proper order when given an unordered tree.
*/
void CmBinOp(NodeBinOp *binop, bool should_mov)
{
    if (binop->left->type == NT_LITERAL && binop->right->type == NT_LITERAL) {
        CmWrite("mov %s, #%d\n", RegS(CR_X8), CmPrecalc(binop));
        return;
    }
    // if there is a branch on the right side, swap the output order to preserve order of operations
    if (binop->right->type == NT_BINOP) {
        CmSide(binop->right, CR_X8, TT_NONE, true);
        CmSide(binop->left, CR_X8, binop->op->type, false);
    }
    else {
        CmSide(binop->left, CR_X8, TT_NONE, true);
        CmSide(binop->right, CR_X8, binop->op->type, false);
    }
}

/**
    Output the instructions for the tail of a function
*/
void CmFuncEnd(CmFunc *func)
{
    CmWrite("add %s, %s, #%d\n", RegS(CR_SP), RegS(CR_SP), func->sp_size);
    CmWrite("ldp %s, %s, [%s], 64\n", RegS(CR_FP), RegS(CR_LR), RegS(CR_SP));

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
        NodeLiteral *lit = (NodeLiteral *)node;


        if (lit->token->type == TT_STRING) {
            char strname[16];
            sprintf(strname, "Str%d", string_literal_index);

            CmStringLiteral *string_lit = &string_literals[string_literal_index++];

            strcpy(string_lit->ref_name, strname);
            string_lit->value = lit;

            CmLoadStr(dest, strname);
        }
        else {
            CmWrite("mov %s, #%.*s\n", RegS(dest), TKPF(((NodeLiteral *)node)->token));
        }
    }
    else {
        // CmWrite("mov w8, wzr\n", 0);
        // CmCompileExpr(assign->right, CR_X8);
        if (node->type == NT_BINOP) {
            // TODO: remove the only w8 restriction on CmBinOp
            CmBinOp((NodeBinOp *)node, true);
            if (dest != CR_X8) {
                CmWrite("mov %s, %s\n", RegS(dest), RegS(CR_X8));
            }
        }
        else if (node->type == NT_VAR) {
            CmVariable *variable = CmFindVariable(((NodeVar *)node)->value, NULL);
            CmWrite("ldr %s, [%s, #%d]\n", RegS(dest), RegS(CR_SP), variable->stack_position);
        }
        else if (node->type == NT_FUNC_CALL) {
            CmCompileStatement(node, func);
            CmWrite("mov %s, %s\n", RegS(dest), RegS(CR_X0));
        }
        // CmCompileStatement(assign->right, func);
    }
}

CmVariable *CmVarDeclare(Node *statement, RegN dest, CmFunc *func)
{
    NodeDeclare *declare = (NodeDeclare *)statement;
    NodeVar *node_var = ((NodeVar*)declare->variable);

    CmVariable *var = CmNewVariable(node_var->value);
    var->string_literal = NULL;
    var->reg = dest;
    var->scope = current_scope;

    func->stack_index -= GetTypeSz();

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
        CmVarDeclare(statement, CR_X8, func);
    }

    else if (statement->type == NT_ASSIGN) {
        NodeAssign *assign = (NodeAssign *)statement;

        // TODO: do not expect just a variable on lhs
        NodeVar *node_var = (NodeVar *)assign->left;

        CmVariable *var = CmFindVariable(node_var->value, NULL);

        if (var == NULL) {
            printf("Could not find variable!\n");
        }

        CmCompileExpr(assign->right, CR_X8, func);

        if (assign->right->type == NT_LITERAL) {
            NodeLiteral *lit = (NodeLiteral *)assign->right;
            if (lit->token->type == TT_STRING) {
                var->string_literal = &string_literals[string_literal_index - 1];
            }
        }

        // save variable onto stack
        CmWrite("str %s, [%s, #%d]\n", RegS(CR_X8), RegS(CR_SP), var->stack_position);
    }
    else if (statement->type == NT_RETURN) {

        NodeReturn *ret = (NodeReturn *)statement;
        CmCompileExpr(ret->value, CR_X0, func);
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

        const int arguments_size =  nfd->argument_count * GetTypeSz();

        const int sp_size =  GetSPSize(storage_size + arguments_size);

        CmFunc *cmfunc = malloc(sizeof(CmFunc));
        cmfunc->stack_index = sp_size;
        cmfunc->sp_size = sp_size;
        cmfunc->name = name;

        current_scope++;

        CmWrite("stp %s, %s, [%s, -64]!\n", RegS(CR_FP), RegS(CR_LR), RegS(CR_SP));
        CmWrite("sub %s, %s, #%d\n", RegS(CR_SP), RegS(CR_SP), sp_size);

        // compile each argument's declare statements
        int i;
        for (i = 0; i < nfd->argument_count; i++) {
            // CmCompileStatement((Node *)nfd->arguments[i], cmfunc);

            CmVariable *var = CmVarDeclare((Node *)nfd->arguments[i], CR_X1 + i, cmfunc);
            CmWrite("str %s, [%s, #%d]\n", RegS(CR_X0 + i), RegS(CR_SP), var->stack_position);
        }

        // compile the block
        if (nfd->block) {
            CmCompileBlock((Node *)nfd->block, cmfunc);
        }

        current_scope--;
    }
}

void CmExportDataSection()
{
    if (string_literal_index <= 0) {
        return;
    }
    CmWrite(".data\n", 0);
    int i;
    for (i = 0; i < string_literal_index; i++) {
        CmStringLiteral *strlit = (CmStringLiteral *)&string_literals[i];
        CmWrite(".L.%s: .asciz %.*s\n", strlit->ref_name, TKPF(strlit->value->token));
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
    CmWrite(".text\n", 0);
    CmWrite(".globl _main\n", 0);
    CmWrite(".align 2\n", 0);
    if (cm->ast->type == NT_BLOCK) {
        CmCompileBlock(cm->ast, NULL);
    }
    CmExportDataSection();
}
