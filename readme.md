# ParserX

<div align="center">

## A Web-Based Mini Compiler and Grammar Analysis Tool

Visualize the complete compilation pipeline including Lexical Analysis, Syntax Analysis, AST Generation, Symbol Table Construction, Semantic Validation, Compiler Optimization, and Grammar Analysis through an interactive web interface.

![Python](https://img.shields.io/badge/Python-Flask-blue)
![C](https://img.shields.io/badge/C-Compiler%20Core-green)
![HTML](https://img.shields.io/badge/Frontend-HTML%2FCSS%2FJS-orange)
![JSON](https://img.shields.io/badge/Data-JSON-yellow)
![License](https://img.shields.io/badge/License-Educational-red)

</div>

---

# 📖 Table of Contents

- Overview
- Features
- Project Architecture
- Compiler Workflow
- Technology Stack
- Supported Language Features
- Compiler Modules
- Grammar Analysis Lab
- Optimization Techniques
- Error Handling
- Project Structure
- Installation Guide
- Running the Project
- Sample Programs
- Sample Grammars
- Output Examples
- Educational Concepts Covered
- Future Enhancements
- Learning Outcomes
- Author

---

# 📌 Overview

ParserX is a full-stack educational compiler project developed to demonstrate the internal working of a compiler through a modern web-based interface.

The system allows users to write programs in a custom language called **TinyLang** and observe how source code is transformed through various compiler phases.

Unlike traditional compiler projects that only produce output, ParserX visualizes every important stage of compilation, making it highly useful for students studying:

- Compiler Design
- Formal Languages and Automata Theory
- Parsing Techniques
- Context-Free Grammars
- Lexical Analysis
- Syntax Analysis
- Code Optimization

The project also includes a dedicated Grammar Analysis Lab that supports grammar validation, FIRST/FOLLOW computation, LL(1) parse table generation, conflict detection, and predictive parsing analysis.

---

# 🚀 Features

## Compiler Module

### Lexical Analysis
- Keyword recognition
- Identifier detection
- Integer tokenization
- Float tokenization
- String tokenization
- Operator detection
- Delimiter recognition
- Token stream generation

### Syntax Analysis
- Grammar-based parsing
- Syntax validation
- Statement parsing
- Expression parsing
- Error reporting

### Abstract Syntax Tree (AST)
- Tree generation
- Expression hierarchy
- Assignment nodes
- Conditional nodes
- Loop nodes

### Symbol Table Generation
- Variable tracking
- Datatype storage
- Scope tracking
- Identifier lookup

### Semantic Validation
- Type mismatch detection
- Duplicate declaration detection
- Undeclared variable detection
- Expression compatibility checking

### Compiler Optimization
- Constant Folding
- Constant Propagation
- Dead Code Elimination
- Expression Simplification

### Structured Output
- JSON-based communication
- Frontend visualization
- Easy debugging

---

## Grammar Analysis Lab

### Grammar Processing
- CFG parsing
- Production rule analysis

### FIRST Set Computation
- Automatic FIRST calculation

### FOLLOW Set Computation
- Automatic FOLLOW calculation

### Parse Table Generation
- LL(1) parse table construction

### Conflict Detection
- Parse table conflict identification

### Left Recursion Detection
- Immediate recursion detection
- Recursive grammar validation

### Predictive Parsing Support
- Parse simulation
- Stack-based tracing

---

# 🏗️ Project Architecture

```text
+---------------------------------------------------+
|                    Frontend                       |
|                HTML + CSS + JS                    |
+-------------------------+-------------------------+
                          |
                          | HTTP Requests
                          v
+-------------------------+-------------------------+
|                  Flask Backend                    |
|                      app.py                       |
+-------------------------+-------------------------+
                          |
                          | subprocess.run()
                          v
+-------------------------+-------------------------+
|                TinyLang Compiler                  |
|                    (C Core)                       |
+-------------------------+-------------------------+
                          |
       +------------------+------------------+
       |                                     |
       v                                     v
+--------------+                  +------------------+
| Compiler Core|                  | Grammar Analysis |
+--------------+                  +------------------+
```

---

# ⚙️ Complete Compiler Workflow

```text
Source Code
     │
     ▼
Lexical Analysis
     │
     ▼
Token Stream
     │
     ▼
Syntax Analysis
     │
     ▼
Abstract Syntax Tree
     │
     ▼
Symbol Table
     │
     ▼
Semantic Validation
     │
     ▼
Optimization
     │
     ▼
JSON Generation
     │
     ▼
Frontend Visualization
```

---

# 🛠 Technology Stack

## Frontend

### HTML5
Used for:
- Code editor
- Grammar editor
- Output sections
- Interactive controls

### CSS3
Used for:
- UI styling
- Responsive design
- Modern dashboard layout

### JavaScript
Used for:
- API requests
- Dynamic rendering
- Compiler execution
- Grammar analysis requests

---

## Backend

### Python Flask

Used for:

- API endpoints
- Request handling
- Temporary file management
- Compiler execution
- JSON processing

Main Routes:

```python
/
```

Loads frontend.

```python
/compile
```

Runs compiler.

```python
/grammar-analyze
```

Runs grammar analysis.

---

## Compiler Core

### C Language

Used for:

- Lexical analysis
- Parsing
- AST generation
- Symbol table generation
- Optimization
- JSON output generation

---

## Data Exchange

### JSON

Used for communication between:

```text
Frontend
   ↕
Flask Backend
   ↕
Compiler Core
```

Example:

```json
{
  "status": "success",
  "tokens": [],
  "ast": {},
  "symbol_table": [],
  "optimizer": {}
}
```

---

# 📚 Supported TinyLang Features

## Data Types

```c
int
float
string
```

---

## Variable Declaration

```c
int age = 21;
float salary = 5000.50;
string name = "Dhruv";
```

---

## Assignment Statements

```c
age = 22;
salary = 5500.75;
```

---

## Arithmetic Operations

```c
int sum = a + b;
int diff = a - b;
int mul = a * b;
int div = a / b;
```

---

## Relational Operators

```c
>
<
>=
<=
==
!=
```

---

## Conditional Statements

```c
if (age > 18) {
    print(age);
}
```

---

## If Else Statements

```c
if (marks >= 40) {
    print(1);
}
else {
    print(0);
}
```

---

## While Loop

```c
int i = 0;

while (i < 5) {
    print(i);
    i = i + 1;
}
```

---

## Print Statement

```c
print("Hello World");
print(age);
```

---

# 🧩 Compiler Modules

## 1. Lexer

File:

```text
lexer.c
lexer.h
```

Responsibilities:

- Convert characters into tokens
- Detect keywords
- Detect identifiers
- Detect numbers
- Detect strings
- Detect operators

---

## 2. Parser

File:

```text
parser.c
parser.h
```

Responsibilities:

- Validate syntax
- Apply grammar rules
- Parse statements
- Parse expressions

---

## 3. Parse Table

File:

```text
parse_table.c
parse_table.h
```

Responsibilities:

- Store LL(1) parsing decisions
- Grammar rule lookup

---

## 4. AST

File:

```text
ast.c
ast.h
```

Responsibilities:

- Build Abstract Syntax Tree
- Represent program hierarchy

---

## 5. Symbol Table

File:

```text
symbol_table.c
symbol_table.h
```

Responsibilities:

- Store identifiers
- Store datatypes
- Perform lookups

---

## 6. Optimizer

File:

```text
optimizer.c
optimizer.h
```

Responsibilities:

- Constant Folding
- Constant Propagation
- Dead Code Removal

---

## 7. JSON Output

File:

```text
json_output.c
json_output.h
```

Responsibilities:

- Convert compiler data into JSON

---

# 🚀 Optimization Techniques

## Constant Folding

Before:

```c
int x = 2 + 3;
```

After:

```c
int x = 5;
```

---

## Constant Propagation

Before:

```c
int a = 5;
int b = a + 2;
```

After:

```c
int b = 5 + 2;
```

---

## Dead Code Elimination

Before:

```c
int x = 5;
x = 10;
```

First assignment becomes unnecessary.

---

# ❌ Error Handling

ParserX handles:

### Syntax Errors

```c
int = a 5;
```

### Type Mismatch

```c
int x = 5.5;
```

### Duplicate Declaration

```c
int a = 5;
int a = 10;
```

### Undeclared Variable

```c
x = 5;
```

### Invalid Expressions

```c
string s = "Hello";
int x = s + 5;
```

---

# 📂 Project Structure

```text
ParserX/
│
├── app.py
│
├── compiler/
│   ├── main.c
│   ├── lexer.c
│   ├── lexer.h
│   ├── parser.c
│   ├── parser.h
│   ├── parse_table.c
│   ├── parse_table.h
│   ├── ast.c
│   ├── ast.h
│   ├── symbol_table.c
│   ├── symbol_table.h
│   ├── optimizer.c
│   ├── optimizer.h
│   ├── json_output.c
│   ├── json_output.h
│   └── grammar_tool.py
│
├── templates/
│   └── index.html
│
├── static/
│
└── README.md
```

---

# 🚀 Installation

## Clone Repository

```bash
git clone https://github.com/your-username/ParserX.git

cd ParserX
```

---

## Install Dependencies

```bash
pip install flask
```

---

## Build Compiler

### Windows

```bash
gcc compiler/*.c -o tinylang_compiler.exe
```

### Linux

```bash
gcc compiler/*.c -o tinylang_compiler
```

---

## Run Project

```bash
python app.py
```

---

## Open Browser

```text
http://localhost:3000
```

---

# 🧪 Sample Compiler Programs

## Example 1

```c
int a = 5;
int b = 10;

int c = a + b;

print(c);
```

---

## Example 2

```c
int x = 2 + 3;

print(x);
```

---

## Example 3

```c
int a = 5;
int b = a + 2;

print(b);
```

---

## Example 4

```c
int marks = 85;

if (marks > 40) {
    print(marks);
}
```

---

# 📖 Sample Grammars

## Expression Grammar

```text
E -> T E'
E' -> + T E' | ε
T -> id
```

---

## Arithmetic Grammar

```text
E -> T E'
E' -> + T E' | ε
T -> F T'
T' -> * F T' | ε
F -> (E) | id
```

---

## Left Recursive Grammar

```text
E -> E + T | T
```

---

# 🎓 Educational Concepts Covered

- Compiler Design
- Lexical Analysis
- Syntax Analysis
- Context-Free Grammar
- LL(1) Parsing
- FIRST Sets
- FOLLOW Sets
- Parse Tables
- Abstract Syntax Trees
- Symbol Tables
- Semantic Analysis
- Code Optimization
- Predictive Parsing

---

# 📈 Future Enhancements

- Three Address Code Generation
- Intermediate Code Generation
- Assembly Generation
- Bytecode Generation
- Function Support
- Scope Management
- Advanced Semantic Analysis
- Data Flow Analysis
- Control Flow Graphs
- Loop Optimizations
- IDE Features
- Syntax Highlighting
- Auto Completion

---

# 🎯 Learning Outcomes

After using ParserX, students can understand:

- How compilers work internally
- How lexical analyzers generate tokens
- How parsers validate syntax
- How ASTs represent source code
- How symbol tables store program information
- How semantic analysis detects errors
- How compiler optimizations improve performance
- How LL(1) parsing works

---

# 👨‍💻 Author

**Dhruv**  
B.Tech Computer Science & Engineering

---

# ⭐ ParserX

### "Learn Compiler Design by Building, Visualizing, and Analyzing Every Phase of Compilation."