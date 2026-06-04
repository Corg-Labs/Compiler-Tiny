# Compiler-Tiny in C

A **tiny but complete compiler** implemented in a single C file. Source text flows through four distinct stages: a **lexer** that tokenises the input, a **recursive-descent parser** that builds an **AST**, a **bytecode compiler** that walks the tree, and a **stack-based virtual machine** that executes the instructions. The language supports integer variables, arithmetic (`+ - * / %`), comparisons, `if/else`, `while` loops, and `print`.

Part of the [Corg-Labs](https://github.com/Corg-Labs) collection of single-file C programs.

---

# Features
- Full four-stage pipeline: Lexer → Parser → Bytecode Compiler → VM
- `--debug` flag prints tokens, AST, and disassembled bytecode before execution
- Persistent REPL: variables survive across input lines
- Coloured terminal output using ANSI escape codes
- Operator precedence handled through layered parser functions

---

# Tutorial

## 1. Tokenisation (Lexer)

The lexer lives in `lex()`. It walks the source string character by character and fills the global `g_tokens[]` array. Each token has a kind (`TKind`), an integer value for `TK_NUM`, and a string value for `TK_IDENT`.

```c
typedef enum {
    TK_NUM, TK_IDENT,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_EQ, TK_NEQ, TK_LT, TK_GT, TK_LEQ, TK_GEQ,
    TK_ASSIGN, TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_LET, TK_PRINT, TK_IF, TK_ELSE, TK_WHILE,
    TK_EOF, TK_ERR
} TKind;
```

Identifiers are checked against the keyword list (`let`, `print`, `if`, `else`, `while`) to promote their kind. Two-character operators like `==`, `!=`, `<=`, `>=` peek one character ahead.

## 2. Abstract Syntax Tree

The parser produces `Node` structs allocated on the heap. Each node carries a `NKind` tag, optional payloads, and child pointers.

```c
struct Node {
    NKind kind;
    int   ival;
    char  sval[64];
    char  op;
    Node *left, *right;
    Node *stmts[MAX_STMTS];
    int   nstmts;
    Node *cond, *then_, *else_;
};
```

## 3. Recursive-Descent Parsing

Operator precedence is enforced by nesting: `parse_expr` calls `parse_add`, which calls `parse_mul`, which calls `parse_primary`. Each layer only consumes its own precedence tier.

```c
static Node *parse_mul(void) {
    Node *l = parse_primary();
    while (cur()->kind == TK_STAR || cur()->kind == TK_SLASH) {
        char op = ...;
        advance();
        Node *n = node_new(N_BINOP);
        n->op = op; n->left = l; n->right = parse_primary();
        l = n;
    }
    return l;
}
```

## 4. Bytecode Generation

`compile_node` emits opcodes into a flat `Chunk`. Variables are stored by name; their index becomes the operand for `OP_LOAD` / `OP_STORE`.

`if/else` uses forward-patched jumps — emit `OP_JZ` with a placeholder, compile the then-branch, backfill the jump target:

```c
int jz_pos = emit(c, OP_JZ); emit(c, 0);   /* placeholder */
compile_node(c, n->then_);
c->code[jz_pos + 1] = c->ncode;            /* patch */
```

`while` records `loop_start` before the condition, emits `OP_JMP loop_start` after the body.

## 5. Stack VM Execution

The VM is a simple `switch` over opcodes with a `stack[]` array:

```c
#define PUSH(v) (vm->stack[vm->sp++] = (v))
#define POP()   (vm->stack[--vm->sp])

case OP_ADD:   b = POP(); a = POP(); PUSH(a + b); break;
case OP_JZ:    a = POP(); if (!a) ip = c->code[ip]; else ip++; break;
case OP_PRINT: printf("%d\n", POP()); break;
```

The `VM` struct is global, so variables persist across REPL lines.

---

# Build

```
gcc compiler.c -o compiler
```

# Run

```
./compiler            # plain REPL
./compiler --debug    # prints tokens, AST, and bytecode disassembly
```

# Controls / Usage

```
>> let x = 10
>> while x > 0 { print x  let x = x - 1 }
>> quit
```

---

# Concepts Practiced
- Lexical analysis and tokenisation
- Recursive-descent parsing and operator precedence
- AST design and tree-walking
- Bytecode compilation with forward-patch jump resolution
- Stack-based virtual machine execution

# Dependencies
Standard C library only (`stdio`, `stdlib`, `string`, `ctype`). No external libraries.
