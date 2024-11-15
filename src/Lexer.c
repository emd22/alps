#include "Lexer.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <ctype.h>

#define TOKEN_BUFFER_START 64

#define SFLEX_IS_WHITESPACE(ch) (ch == ' ' || ch == '\t' || ch == '\n')
#define SFLEX_IS_STRING(ch, flags) ((flags & SFLEX_USE_STRINGS) && (ch == '"' || ch == '\''))
#define SFLEX_IS_SPECIAL(buf, ch) (strchr(buf, ch) != NULL)


static LexerToken *LexTokenGetNext(Lexer *inst) {
    if (inst->token_amt+1 > inst->token_buffer_size) {
        inst->token_buffer_size *= 2;
        inst->tokens = (LexerToken *)realloc(inst->tokens, inst->token_buffer_size);
    }
    return &inst->tokens[inst->token_amt++];
}

const char *LexerTokenTypeStr(TokenType type) {
    switch (type) {
        case TT_NONE:
            return "NONE";
        case TT_IDENTIFIER:
            return "IDENTIFIER";
        case TT_LPAREN:
            return "LPAREN";
        case TT_RPAREN:
            return "RPAREN";
        case TT_NUMBER:
            return "NUMBER";
        case TT_SEMICOLON:
            return "SEMICOLON";
        case TT_COLON:
            return "COLON";
        case TT_PERIOD:
            return "PERIOD";
        case TT_COMMA:
            return "COMMA";
        case TT_LBRACE:
            return "LBRACE";
        case TT_RBRACE:
            return "RBRACE";
        case TT_EQUALS:
            return "EQUALS";
        case TT_KEYWORD:
            return "KEYWORD";
        case TT_TYPE:
            return "TYPE";

        case TT_PLUS:
            return "PLUS";
        case TT_MINUS:
            return "MINUS";
        case TT_STAR:
            return "STAR";
        case TT_SLASH:
            return "SLASH";

        default:
            return "UNKNOWN";
    }
    return "UNKNOWN";
}

bool LexerApplySingleCharType(LexerToken *token)
{
    char ch = token->start[0];
    switch (ch) {
    case ';':
        token->type = TT_SEMICOLON;
        break;
    case ':':
        token->type = TT_COLON;
        break;
    case ',':
        token->type = TT_COMMA;
        break;
    case '.':
        token->type = TT_PERIOD;
        break;
    case '{':
        token->type = TT_LBRACE;
        break;
    case '}':
        token->type = TT_RBRACE;
        break;
    case '(':
        token->type = TT_LPAREN;
        break;
    case ')':
        token->type = TT_RPAREN;
        break;
    case '=':
        token->type = TT_EQUALS;
        break;

    case '+':
        token->type = TT_PLUS;
        break;
    case '-':
        token->type = TT_MINUS;
        break;
    case '*':
        token->type = TT_STAR;
        break;
    case '/':
        token->type = TT_SLASH;
        break;
    default:
        return false;
    }
    return true;
}


static void ThrowError(const char *msg)
{
    puts("Error: ");
    printf("%s", msg);
    exit(1);
}

bool IfIsKeyword(LexerToken *token, const char *keywords[], int keyword_count)
{
    int i;
    for (i = 0; i < keyword_count; i++) {
        if (!strncmp(token->start, keywords[i], LexerTokenLength(token))) {
            return true;
        }
    }
    return false;
}

void LexerCheckKeywords(LexerToken *token)
{
    const char *keywords[] = {
        "if", "return", "for", "while",
        "struct",
    };

    if (IfIsKeyword(token, keywords, 5)) {
        token->type = TT_KEYWORD;
    }

    const char *types[] = {
        "int", "str"
    };

    if (IfIsKeyword(token, types, 2)) {
        token->type = TT_TYPE;
    }
}

void LexerSetType(LexerToken *token)
{
    int length = LexerTokenLength(token);

    token->type = TT_NONE;

    bool decimal_found = false;

    // parse numbers
    int i;
    for (i = 0; i < length; i++) {
        if (isnumber(token->start[i])) {
            token->type = TT_NUMBER;
        }
        else if (token->start[i] == '.') {
            if (decimal_found) {
                ThrowError("Invalid number format!\n");
            }
            token->type = TT_NUMBER;
            decimal_found = true;
        }
        else {
            token->type = TT_IDENTIFIER;
            break;
        }
    }

    if (token->type == TT_NUMBER) {
        return;
    }

    // parse single character tokens
    if (length == 1 && LexerApplySingleCharType(token)) {
        return;
    }

    LexerCheckKeywords(token);

    if (token->type == TT_NONE && token->end - token->start > 0 && isalpha(token->start[0])) {
        token->type = TT_IDENTIFIER;
    }
}

long LexerTokenLength(LexerToken *token)
{
    return token->end - token->start;
}

void LexerDestroy(Lexer *inst) {
    if (inst == NULL)
        return;

    if (inst->tokens != NULL)
        free(inst->tokens);

    inst->token_amt = 0;
    inst->token_buffer_size = 0;
}

Lexer LexerLex(char *data, const char *specials, int flags) {
    char ch, lastch;
    // Setup sflex structure
    Lexer inst;

    inst.token_buffer_size = TOKEN_BUFFER_START;
    inst.token_amt = 0;
    inst.tokens = (LexerToken *)malloc(sizeof(LexerToken ) * inst.token_buffer_size);
    inst.data = data;

    bool in_string = false;

    // skip initial whitespace
    while (SFLEX_IS_WHITESPACE((ch = (*data)))) {
        data++;
    }

    char *newb = data;
    LexerToken *token = NULL;

    while ((ch = *(newb))) {
        if (SFLEX_IS_STRING(ch, flags))
            in_string = !in_string;

        // first character in string
        if (newb == data) {
            // token is not initialized, create new one
            token = LexTokenGetNext(&inst);
            //
            if (!SFLEX_IS_WHITESPACE(ch)) {
                token->start = newb;
            }
        }

        // handle whitespace
        if ((SFLEX_IS_WHITESPACE(ch)) && (!in_string)) {
            if (token != NULL)
                token->end = newb;

            while (SFLEX_IS_WHITESPACE(*(newb+1)))
                newb++;

            LexerSetType(token);
            token = LexTokenGetNext(&inst);
            token->start = newb + 1;
        }
        // handle 'special' characters
        else {
            if (specials != NULL && SFLEX_IS_SPECIAL(specials, ch) && !in_string) {
                if (!SFLEX_IS_WHITESPACE(lastch) && !SFLEX_IS_SPECIAL(specials, lastch) && (token != NULL)) {
                    token->end = newb;

                    LexerSetType(token);
                    token = LexTokenGetNext(&inst);
                }
                token->start = newb;
                token->end = newb+1;

                LexerSetType(token);
                token = LexTokenGetNext(&inst);

                while (SFLEX_IS_WHITESPACE(*(newb + 1)))
                    newb++;

                token->start = newb+1;
            }
        }
        lastch = ch;
        newb++;
    }

    // chop off empty token at end
    inst.token_amt--;
    return inst;
}
