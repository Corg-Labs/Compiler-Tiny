/*
 * compiler.c — Tiny compiler: expression language → bytecode → stack VM.
 *
 * Language:
 *   let x = expr            variable assignment
 *   print expr              print a value
 *   if expr { ... } else { ... }
 *   while expr { ... }
 *   Operators: + - * / %   ==  !=  <  >  <=  >=
 *   Literals: integers
 *
 * Pipeline: Lexer → Parser (recursive descent, AST) → Bytecode compiler → VM
 *
 * Flags:  --debug   print tokens, AST, and bytecode disassembly before running
 *
 * Usage:
 *   ./compiler --debug          (REPL with debug output)
 *   ./compiler                  (plain REPL)
 *
 * Compile: gcc compiler.c -o compiler
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

/* =========================================================================
 * Colours
 * ====================================================================== */

#define RED  "\033[1;31m"
#define GRN  "\033[1;32m"
#define YEL  "\033[1;33m"
#define CYN  "\033[1;36m"
#define MAG  "\033[1;35m"
#define DIM  "\033[2m"
#define RST  "\033[0m"

/* =========================================================================
 * LEXER
 * ====================================================================== */

typedef enum {
    TK_NUM, TK_IDENT,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_EQ, TK_NEQ, TK_LT, TK_GT, TK_LEQ, TK_GEQ,
    TK_ASSIGN,
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_LET, TK_PRINT, TK_IF, TK_ELSE, TK_WHILE,
    TK_EOF, TK_ERR
} TKind;

static const char *tk_name(TKind k)
{
    switch (k) {
    case TK_NUM:    return "NUM";     case TK_IDENT:  return "IDENT";
    case TK_PLUS:   return "+";       case TK_MINUS:  return "-";
    case TK_STAR:   return "*";       case TK_SLASH:  return "/";
    case TK_PERCENT:return "%";       case TK_EQ:     return "==";
    case TK_NEQ:    return "!=";      case TK_LT:     return "<";
    case TK_GT:     return ">";       case TK_LEQ:    return "<=";
    case TK_GEQ:    return ">=";      case TK_ASSIGN: return "=";
    case TK_LPAREN: return "(";       case TK_RPAREN: return ")";
    case TK_LBRACE: return "{";       case TK_RBRACE: return "}";
    case TK_LET:    return "let";     case TK_PRINT:  return "print";
    case TK_IF:     return "if";      case TK_ELSE:   return "else";
    case TK_WHILE:  return "while";   case TK_EOF:    return "EOF";
    default:        return "ERR";
    }
}

typedef struct { TKind kind; int ival; char sval[64]; } Token;

#define MAX_TOKENS 2048

static Token  g_tokens[MAX_TOKENS];
static int    g_ntok = 0;
static int    g_tpos = 0;   /* parser cursor */

static void lex(const char *src)
{
    g_ntok = 0; g_tpos = 0;
    const char *p = src;
    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;

        Token t = {0};

        if (isdigit((unsigned char)*p)) {
            t.kind = TK_NUM; t.ival = 0;
            while (isdigit((unsigned char)*p)) t.ival = t.ival * 10 + (*p++ - '0');
        } else if (isalpha((unsigned char)*p) || *p == '_') {
            t.kind = TK_IDENT;
            int i = 0;
            while ((isalnum((unsigned char)*p) || *p == '_') && i < 63)
                t.sval[i++] = *p++;
            t.sval[i] = '\0';
            if      (!strcmp(t.sval, "let"))   t.kind = TK_LET;
            else if (!strcmp(t.sval, "print")) t.kind = TK_PRINT;
            else if (!strcmp(t.sval, "if"))    t.kind = TK_IF;
            else if (!strcmp(t.sval, "else"))  t.kind = TK_ELSE;
            else if (!strcmp(t.sval, "while")) t.kind = TK_WHILE;
        } else {
            switch (*p) {
            case '+': t.kind = TK_PLUS;   p++; break;
            case '-': t.kind = TK_MINUS;  p++; break;
            case '*': t.kind = TK_STAR;   p++; break;
            case '/': t.kind = TK_SLASH;  p++; break;
            case '%': t.kind = TK_PERCENT;p++; break;
            case '(': t.kind = TK_LPAREN; p++; break;
            case ')': t.kind = TK_RPAREN; p++; break;
            case '{': t.kind = TK_LBRACE; p++; break;
            case '}': t.kind = TK_RBRACE; p++; break;
            case '=': if (p[1]=='='){t.kind=TK_EQ;  p+=2;}
                      else          {t.kind=TK_ASSIGN;p++;}  break;
            case '!': if (p[1]=='='){t.kind=TK_NEQ; p+=2;}
                      else          {t.kind=TK_ERR;  p++;}   break;
            case '<': if (p[1]=='='){t.kind=TK_LEQ; p+=2;}
                      else          {t.kind=TK_LT;   p++;}   break;
            case '>': if (p[1]=='='){t.kind=TK_GEQ; p+=2;}
                      else          {t.kind=TK_GT;   p++;}   break;
            default:  t.kind=TK_ERR; p++; break;
            }
        }
        if (g_ntok < MAX_TOKENS) g_tokens[g_ntok++] = t;
    }
    g_tokens[g_ntok++] = (Token){TK_EOF, 0, ""};
}

static void print_tokens(void)
{
    printf(CYN "=== TOKENS ===\n" RST);
    for (int i = 0; i < g_ntok; i++) {
        Token *t = &g_tokens[i];
        if (t->kind == TK_NUM)
            printf(DIM "  [%3d] " RST MAG "%-8s" RST " %d\n",   i, tk_name(t->kind), t->ival);
        else if (t->kind == TK_IDENT)
            printf(DIM "  [%3d] " RST MAG "%-8s" RST " %s\n",   i, tk_name(t->kind), t->sval);
        else
            printf(DIM "  [%3d] " RST YEL "%s\n" RST,           i, tk_name(t->kind));
    }
}

/* =========================================================================
 * AST
 * ====================================================================== */

typedef enum {
    N_NUM, N_VAR,
    N_BINOP,
    N_LET, N_PRINT,
    N_IF, N_WHILE,
    N_BLOCK
} NKind;

typedef struct Node Node;
#define MAX_STMTS 256

struct Node {
    NKind kind;
    int   ival;
    char  sval[64];
    char  op;         /* for N_BINOP: '+' '-' '*' '/' '%' and encoded cmp */
    Node *left, *right;
    /* N_BLOCK */
    Node *stmts[MAX_STMTS];
    int   nstmts;
    /* N_IF */
    Node *cond, *then_, *else_;
};

static Node *node_new(NKind k)
{
    Node *n = calloc(1, sizeof(Node));
    n->kind = k;
    return n;
}

/* Forward declarations */
static Node *parse_stmt(void);
static Node *parse_expr(void);

static Token *cur(void)  { return &g_tokens[g_tpos]; }
static Token *advance(void)
{
    Token *t = &g_tokens[g_tpos];
    if (g_tpos < g_ntok - 1) g_tpos++;
    return t;
}
static int expect(TKind k)
{
    if (cur()->kind != k) {
        fprintf(stderr, RED "Parse error: expected %s got %s\n" RST,
                tk_name(k), tk_name(cur()->kind));
        return 0;
    }
    advance();
    return 1;
}

static Node *parse_primary(void)
{
    if (cur()->kind == TK_NUM) {
        Node *n = node_new(N_NUM);
        n->ival = advance()->ival;
        return n;
    }
    if (cur()->kind == TK_IDENT) {
        Node *n = node_new(N_VAR);
        strncpy(n->sval, advance()->sval, 63);
        return n;
    }
    if (cur()->kind == TK_LPAREN) {
        advance();
        Node *e = parse_expr();
        expect(TK_RPAREN);
        return e;
    }
    fprintf(stderr, RED "Parse error: unexpected token %s\n" RST, tk_name(cur()->kind));
    return node_new(N_NUM);
}

static Node *parse_mul(void)
{
    Node *l = parse_primary();
    while (cur()->kind == TK_STAR || cur()->kind == TK_SLASH || cur()->kind == TK_PERCENT) {
        char op = (cur()->kind == TK_STAR) ? '*' : (cur()->kind == TK_SLASH) ? '/' : '%';
        advance();
        Node *n = node_new(N_BINOP);
        n->op = op; n->left = l; n->right = parse_primary();
        l = n;
    }
    return l;
}

static Node *parse_add(void)
{
    Node *l = parse_mul();
    while (cur()->kind == TK_PLUS || cur()->kind == TK_MINUS) {
        char op = (cur()->kind == TK_PLUS) ? '+' : '-';
        advance();
        Node *n = node_new(N_BINOP);
        n->op = op; n->left = l; n->right = parse_mul();
        l = n;
    }
    return l;
}

/* Comparison ops encoded: '=' → ==, '!' → !=, '<' → <, '>' → >, 'L' → <=, 'G' → >= */
static Node *parse_expr(void)
{
    Node *l = parse_add();
    TKind k = cur()->kind;
    if (k==TK_EQ||k==TK_NEQ||k==TK_LT||k==TK_GT||k==TK_LEQ||k==TK_GEQ) {
        char op = (k==TK_EQ)?'=':(k==TK_NEQ)?'!':(k==TK_LT)?'<':(k==TK_GT)?'>':(k==TK_LEQ)?'L':'G';
        advance();
        Node *n  = node_new(N_BINOP);
        n->op    = op; n->left = l; n->right = parse_add();
        return n;
    }
    return l;
}

static Node *parse_block(void)
{
    Node *b = node_new(N_BLOCK);
    if (!expect(TK_LBRACE)) return b;
    while (cur()->kind != TK_RBRACE && cur()->kind != TK_EOF) {
        if (b->nstmts < MAX_STMTS)
            b->stmts[b->nstmts++] = parse_stmt();
    }
    expect(TK_RBRACE);
    return b;
}

static Node *parse_stmt(void)
{
    if (cur()->kind == TK_LET) {
        advance();
        Node *n = node_new(N_LET);
        strncpy(n->sval, cur()->sval, 63);
        expect(TK_IDENT);
        expect(TK_ASSIGN);
        n->left = parse_expr();
        return n;
    }
    if (cur()->kind == TK_PRINT) {
        advance();
        Node *n  = node_new(N_PRINT);
        n->left  = parse_expr();
        return n;
    }
    if (cur()->kind == TK_IF) {
        advance();
        Node *n  = node_new(N_IF);
        n->cond  = parse_expr();
        n->then_ = parse_block();
        if (cur()->kind == TK_ELSE) { advance(); n->else_ = parse_block(); }
        return n;
    }
    if (cur()->kind == TK_WHILE) {
        advance();
        Node *n  = node_new(N_WHILE);
        n->cond  = parse_expr();
        n->then_ = parse_block();
        return n;
    }
    if (cur()->kind == TK_LBRACE) return parse_block();
    return parse_expr();
}

static void ast_print(Node *n, int depth)
{
    if (!n) return;
    for (int i = 0; i < depth; i++) printf("  ");
    switch (n->kind) {
    case N_NUM:   printf(MAG "NUM(%d)\n" RST, n->ival); break;
    case N_VAR:   printf(YEL "VAR(%s)\n" RST, n->sval); break;
    case N_BINOP: printf(GRN "BINOP(%c)\n" RST, n->op);
                  ast_print(n->left, depth+1); ast_print(n->right, depth+1); break;
    case N_LET:   printf(CYN "LET(%s)\n" RST, n->sval); ast_print(n->left, depth+1); break;
    case N_PRINT: printf(CYN "PRINT\n" RST); ast_print(n->left, depth+1); break;
    case N_IF:    printf(CYN "IF\n" RST);
                  ast_print(n->cond,  depth+1);
                  ast_print(n->then_, depth+1);
                  if (n->else_) ast_print(n->else_, depth+1); break;
    case N_WHILE: printf(CYN "WHILE\n" RST);
                  ast_print(n->cond,  depth+1);
                  ast_print(n->then_, depth+1); break;
    case N_BLOCK: printf(DIM "BLOCK(%d)\n" RST, n->nstmts);
                  for (int i = 0; i < n->nstmts; i++) ast_print(n->stmts[i], depth+1); break;
    }
}

/* =========================================================================
 * BYTECODE
 * ====================================================================== */

typedef enum {
    OP_PUSH,    /* push immediate int                */
    OP_LOAD,    /* load variable (operand = var idx) */
    OP_STORE,   /* store variable                    */
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LEQ, OP_GEQ,
    OP_PRINT,
    OP_JZ,      /* jump if top == 0 */
    OP_JMP,     /* unconditional jump */
    OP_POP,
    OP_HALT
} OpCode;

static const char *op_name(OpCode o)
{
    switch (o) {
    case OP_PUSH:  return "PUSH";  case OP_LOAD:  return "LOAD";
    case OP_STORE: return "STORE"; case OP_ADD:   return "ADD";
    case OP_SUB:   return "SUB";   case OP_MUL:   return "MUL";
    case OP_DIV:   return "DIV";   case OP_MOD:   return "MOD";
    case OP_EQ:    return "EQ";    case OP_NEQ:   return "NEQ";
    case OP_LT:    return "LT";    case OP_GT:    return "GT";
    case OP_LEQ:   return "LEQ";   case OP_GEQ:   return "GEQ";
    case OP_PRINT: return "PRINT"; case OP_JZ:    return "JZ";
    case OP_JMP:   return "JMP";   case OP_POP:   return "POP";
    case OP_HALT:  return "HALT";  default:       return "?";
    }
}

#define MAX_CODE   4096
#define MAX_VARS   256

typedef struct {
    int    code[MAX_CODE];
    int    ncode;
    char   var_names[MAX_VARS][64];
    int    nvars;
} Chunk;

static void chunk_init(Chunk *c) { memset(c, 0, sizeof(*c)); }

static int emit(Chunk *c, int v)
{
    if (c->ncode >= MAX_CODE) { fprintf(stderr, RED "Code overflow\n" RST); return 0; }
    c->code[c->ncode++] = v;
    return c->ncode - 1;
}

static int var_index(Chunk *c, const char *name)
{
    for (int i = 0; i < c->nvars; i++)
        if (!strcmp(c->var_names[i], name)) return i;
    if (c->nvars >= MAX_VARS) return -1;
    strncpy(c->var_names[c->nvars], name, 63);
    return c->nvars++;
}

static void compile_node(Chunk *c, Node *n)
{
    if (!n) return;
    switch (n->kind) {
    case N_NUM:
        emit(c, OP_PUSH); emit(c, n->ival);
        break;
    case N_VAR: {
        int idx = var_index(c, n->sval);
        emit(c, OP_LOAD); emit(c, idx);
        break;
    }
    case N_BINOP:
        compile_node(c, n->left);
        compile_node(c, n->right);
        switch (n->op) {
        case '+': emit(c, OP_ADD); break; case '-': emit(c, OP_SUB); break;
        case '*': emit(c, OP_MUL); break; case '/': emit(c, OP_DIV); break;
        case '%': emit(c, OP_MOD); break; case '=': emit(c, OP_EQ);  break;
        case '!': emit(c, OP_NEQ); break; case '<': emit(c, OP_LT);  break;
        case '>': emit(c, OP_GT);  break; case 'L': emit(c, OP_LEQ); break;
        case 'G': emit(c, OP_GEQ); break;
        }
        break;
    case N_LET: {
        compile_node(c, n->left);
        int idx = var_index(c, n->sval);
        emit(c, OP_STORE); emit(c, idx);
        break;
    }
    case N_PRINT:
        compile_node(c, n->left);
        emit(c, OP_PRINT);
        break;
    case N_IF: {
        compile_node(c, n->cond);
        int jz_pos = emit(c, OP_JZ); emit(c, 0);   /* placeholder */
        compile_node(c, n->then_);
        if (n->else_) {
            int jmp_pos = emit(c, OP_JMP); emit(c, 0);
            c->code[jz_pos + 1] = c->ncode;
            compile_node(c, n->else_);
            c->code[jmp_pos + 1] = c->ncode;
        } else {
            c->code[jz_pos + 1] = c->ncode;
        }
        break;
    }
    case N_WHILE: {
        int loop_start = c->ncode;
        compile_node(c, n->cond);
        int jz_pos = emit(c, OP_JZ); emit(c, 0);
        compile_node(c, n->then_);
        emit(c, OP_JMP); emit(c, loop_start);
        c->code[jz_pos + 1] = c->ncode;
        break;
    }
    case N_BLOCK:
        for (int i = 0; i < n->nstmts; i++)
            compile_node(c, n->stmts[i]);
        break;
    }
}

static void disassemble(Chunk *c)
{
    printf(CYN "=== BYTECODE ===\n" RST);
    for (int ip = 0; ip < c->ncode; ) {
        printf(DIM "  %04d  " RST, ip);
        OpCode op = (OpCode)c->code[ip++];
        switch (op) {
        case OP_PUSH:  printf(YEL "%-8s" RST " %d\n",  op_name(op), c->code[ip++]); break;
        case OP_LOAD:
        case OP_STORE: printf(YEL "%-8s" RST " [%d] %s\n",
                              op_name(op), c->code[ip], c->var_names[c->code[ip]]);
                       ip++; break;
        case OP_JZ:
        case OP_JMP:   printf(YEL "%-8s" RST " → %d\n", op_name(op), c->code[ip++]); break;
        default:       printf(GRN "%s\n" RST, op_name(op)); break;
        }
    }
}

/* =========================================================================
 * VIRTUAL MACHINE
 * ====================================================================== */

#define STACK_MAX 1024

typedef struct {
    int stack[STACK_MAX];
    int sp;
    int vars[MAX_VARS];
} VM;

static void vm_run(VM *vm, Chunk *c)
{
    vm->sp = 0;
    int ip = 0;
#define PUSH(v) (vm->stack[vm->sp++] = (v))
#define POP()   (vm->stack[--vm->sp])
    while (ip < c->ncode) {
        OpCode op = (OpCode)c->code[ip++];
        int a, b;
        switch (op) {
        case OP_PUSH:  PUSH(c->code[ip++]); break;
        case OP_LOAD:  PUSH(vm->vars[c->code[ip++]]); break;
        case OP_STORE: vm->vars[c->code[ip++]] = POP(); break;
        case OP_ADD:   b=POP(); a=POP(); PUSH(a+b); break;
        case OP_SUB:   b=POP(); a=POP(); PUSH(a-b); break;
        case OP_MUL:   b=POP(); a=POP(); PUSH(a*b); break;
        case OP_DIV:   b=POP(); a=POP(); PUSH(b?a/b:0); break;
        case OP_MOD:   b=POP(); a=POP(); PUSH(b?a%b:0); break;
        case OP_EQ:    b=POP(); a=POP(); PUSH(a==b); break;
        case OP_NEQ:   b=POP(); a=POP(); PUSH(a!=b); break;
        case OP_LT:    b=POP(); a=POP(); PUSH(a<b);  break;
        case OP_GT:    b=POP(); a=POP(); PUSH(a>b);  break;
        case OP_LEQ:   b=POP(); a=POP(); PUSH(a<=b); break;
        case OP_GEQ:   b=POP(); a=POP(); PUSH(a>=b); break;
        case OP_PRINT: printf(GRN "%d\n" RST, POP()); break;
        case OP_JZ:    a=POP(); if (!a) ip=c->code[ip]; else ip++; break;
        case OP_JMP:   ip=c->code[ip]; break;
        case OP_POP:   POP(); break;
        case OP_HALT:  return;
        }
    }
#undef PUSH
#undef POP
}

/* =========================================================================
 * Run one source string end-to-end
 * ====================================================================== */

static VM g_vm = {0};   /* persistent across REPL lines (shared variables) */

static void run_source(const char *src, int debug)
{
    lex(src);
    if (debug) print_tokens();

    Node *tree = parse_stmt();
    if (debug) { printf(CYN "=== AST ===\n" RST); ast_print(tree, 0); }

    Chunk chunk;
    chunk_init(&chunk);
    compile_node(&chunk, tree);
    emit(&chunk, OP_HALT);
    if (debug) disassemble(&chunk);

    vm_run(&g_vm, &chunk);
    free(tree);
}

/* =========================================================================
 * main
 * ====================================================================== */

int main(int argc, char **argv)
{
    int debug = 0;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--debug")) debug = 1;

    printf(CYN "compiler — tiny language REPL%s\n" RST,
           debug ? YEL " [debug mode]" RST : "");
    printf("Statements: let x = expr | print expr | if expr { } else { } | while expr { }\n");
    printf("Type 'quit' to exit.\n\n");

    char line[1024];
    while (1) {
        printf(YEL ">> " RST);
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        /* Strip newline */
        line[strcspn(line, "\n")] = '\0';
        if (!strcmp(line, "quit") || !strcmp(line, "q")) break;
        if (!*line) continue;
        run_source(line, debug);
    }
    printf("Bye.\n");
    return 0;
}
