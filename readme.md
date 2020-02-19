Webassembly-mini-c
==================

This is the result of a challenge I set myself - to modify an existing simple compiler to generate webassembly binary and
which can self-compile from within the browser. The end result can be seen here: http://wa-compiler.netlify.com/

As a starting point I used was Sam Nipps (Fedjmike) excellent mini-c compiler here: http://github.com/Fedjmike/mini-c. I was 
already familiar with this code as I had previously modified it to generate arm code instead of x86 code (see: https://github.com/laiudm/mini-c).

This code is a dramatic change from the original code - it's now twice as long - which is why this code is in it's own repo, and not a branch
from the original. The core parser remains very similar, but extensive work was required to add library functions, to work reliably with binary data,
and to produce the relatively complex webassembly wasm binary format.

The following is derived from Sam's original mini-c documents.

Implementation:
- Generates a binary webassebly file (wasm) by combining compilation, assembly, and linking into a single program.
- It is implemented as a single pass compiler - code generation is mixed with parsing.
- The parser peeks at the next token to decide whether to generate an lvalue.
- It compiles and runs in both Windows and Linux host environments, as well as the browser.
- A minimal set of library functions are implemented to allow it to run in both the host environments and in the browser.

Language:
- Local and global variables, parameters.
- Functions, `if`, `while`, `do``while`, `return`.
- `=`, `?:` (ternary), `||`, `&&`, `==`, `!=`, `<`, `<=`, `>`, `>=`, `+`, `-`, `*`, `/`, `%`, `++`, `--` (post-ops), `!`, `-` (unary), `[]`, `()`
- Integer, character, `true` and `false` literals. String literals, with automatic concatenation.
- The language it implements is typeless. Everything is a 4 byte signed integer.
- Pointer indexing works in increments of 4 bytes, pointer arithmetic is byte-by-byte.

The general philosophy was: only include a feature if it is needed by the compiler code.

Building it and Compiling it to wasm for the website
----------------------------------------------------

    git clone https://github.com/laiudm/Webassemby-mini-c
    cd Webassemby-mini-c
    cc -o cc cc.c	## ignore the 3x warnings.
	./cc cc.c		## generates the program.wasm file as expected by the website

If on windows, replace the compilation step with
	cl cc.c

Running in a Browser
--------------------

The files in the directory can be served by any webserver. For instance if python is installed:

	python -m SimpleHTTPServer 9000


License
-------

Copyright (c) 2015 Sam Nipps

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
