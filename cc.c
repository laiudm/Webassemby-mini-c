//---------------
// mini-c, by Sam Nipps (c) 2015; modified to create wasm binary code instead by Dave Mugridge 2017.
// MIT license
//---------------

// functions to r/w 8 bits when only 32 bit memory access
int  getChar  (const char* buffer) {return buffer[0] & 255;}
void writeChar(char* buffer, char ch) {	buffer[0] = (buffer[0] & (~255)) | (ch & 0xff) ;	}

int puts(const char* s);	// fwd decl.

//#include <stdbool.h> // stdbool.h equivalent

#define bool int
int false = 0;
int true  = 1;

//#include <ctype.h> // ctype.h equivalent

bool isalpha (char ch) {return ( ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'); }
bool isdigit (char ch) {return ch >= '0' && ch <= '9';}
bool isalnum (char ch) {return isalpha(ch) || isdigit(ch);}
bool ishex   (char ch) {return isdigit(ch) || (ch >='a' && ch <= 'f');}	// just lower case for the moment
int  hexValue(char ch) {return isdigit(ch) ? ch - '0' : ch - 'a' + 10;}

//#include <stdlib.h>	// stdlib functions...

void* malloc(int size);

int atoi(char* buffer) { // as used here, buffer will only contain numbers...
	int result = 0;
	if ( getChar(buffer) == '-')
		return -1*atoi(buffer+1);
	while (getChar(buffer))
		result = result*10 + (getChar(buffer++) - '0');
	return result;
}
	
void* calloc(int num, int size) {
	int sizeInBytes = num*size;
	void* memory = malloc(sizeInBytes);
	int sizeInWords = (sizeInBytes >> 2);
	int* ptr = memory;
	while (sizeInWords >= 0) {
		ptr[sizeInWords] = 0;
		sizeInWords--;
	}
	return memory;
}

void free(void* memory) {}	// very simplistic memory allocator; no free space management...

//#include <string.h>	// character/string functions

int strlen(char* buffer) {
	int count = 0;
	while (getChar(buffer++))
		count++;
	return count;
}

int strcmp(char* s1, char* s2) {	// only need ==, != result
	while ( getChar(s1) == getChar(s2) ) {
		if (getChar(s1) == 0) {
			return 0;
		}
		s1++;
		s2++;
	}
	return -1;
}

char* strdup(char* s1) {
	int length = strlen(s1) + 1;
	char* s2 = malloc(length);
	char* retValue = s2;
	while (length--) {
		writeChar(s2++, getChar(s1++));
	}
	return retValue;
}

#include <stdio.h>	//Still needed as it defines the struct FILE & linking fails otherwise. But this is fine

FILE* fopen  (const char* filename, const char* mode);
int   fclose (FILE* stream);
int   fgetc  (FILE* stream);		// char file input
int   ungetc (int c, FILE* stream);
int   feof   (FILE* stream);
int   fputc  (int c, FILE* stream);	// binary file output...
int   putchar(int c);				// write a character to stdout -- 

int puti(int i) {		// write ascii equiv. of int to stdout
	int div = i / 10;
	int rem = i % 10;
	if (i < 0) {
		putchar('-');
		puti(-i);
	} else {
		if (div)
			puti(div);
		putchar('0' + rem);
	}
	return 1;
}

int puts(const char* s);

int printfsmall(const char* s, char* s1) {
	char ch;
	do {
		ch = getChar(s++);
		if (ch == '%') {
			getChar(s++);			// just assume it's a '%s' & skip over it
			printfsmall(s1, "");	// the second string better not include any formatting)
		} else if (ch) 
			putchar(ch);
	} while (ch);
	return 1;
}

int puts(const char* s) {
	return printfsmall(s, "");
}

int ptr_size = 4;
int word_size = 4;

FILE* output;

//=== Bytecode generation ===

char* bytecode;		// remember the starting location
int   bytecodeLength;

char* headerCode;	// remember the starting location
int   headerLength;

char* streamPointer; // generic stream pointer
char* sp;			 // current bytecode pointer

char* createEmitStream(int maxlen) {
	streamPointer = malloc(maxlen);
	sp = streamPointer;
	return streamPointer;
}

int stream_length() {
	return sp - streamPointer;
}

char* emitByte(char op) {
	char* retval = sp;
	writeChar(sp++, op);
	return retval;
}

char* emit2Byte(char op, char p) {
	char* retval = emitByte(op);
	emitByte(p);
	return retval;
}

char* emiti32load(int offset) {
	char* retval = emit2Byte(0x28, 0x02);
	emitByte(offset);
	return retval;
}

char* emiti32store(int offset) {
	char* retval = emit2Byte(0x36, 0x02);
	emitByte(offset);
	return retval;
}

int blockDepth = 0;

int emitBlock(int block_type) {
	emit2Byte(0x02, block_type);
	blockDepth++;
	return  blockDepth;
}

int emitLoop(int block_type) {
	emit2Byte(0x03, block_type);
	blockDepth++;
	return  blockDepth;
}

void emitEnd() {
	emitByte(0x0b);		// end
	blockDepth--;
}

void emitBranch(int code, int blockNo) {
	emit2Byte(code, blockDepth - blockNo);	// number of blocks to skip
}

char* emitConst(int value) {			// value is 32 bit signed...
	//printf("emitConst: %04x\n", (unsigned int) value);
	// from https://en.wikipedia.org/wiki/LEB128#Signed_LEB128
	char* retval = emitByte(0x41);	//i32.const
	int more = 1;
	//int negative = (value < 0);
	int byte;
	while(more) {
		byte = value & 0x7f;	// lowest 7 bits
		value = value >> 7;
		// following is unnecessary if >> is arithmetic shift
		//if (negative) value = value | (- (1 << (32-7)));	// sign extend
		if (((value==0) && !(byte & 0x40)) || ((value==-1) && (byte & 0x40)))
			more = 0;
		else
			byte = byte | 0x80;
		emitByte(byte);
		//printf("emit: %02x ", (unsigned char) byte);
	}
	//printf("\n");
	return retval;
}
	
char* emitLength() {			// called to make space for the length; value is subsequently overwritten
	char* retval = emitByte(0x55);	// preventative: make obvious that's it's not been overwritten
	emitByte(0x57);
	emitByte(0x58);
	emitByte(0x59);
	emitByte(0x5a);
	return retval;
}

void emitNumberFixup(char* p, int no) {
	// turn the number into a 5 char encoded length
	int i = 0;
	while (i<5) {
		char c = no & 0x7f;
		if (i !=4) c = c | 0x80;	// set the top bit of all but the last byte
		writeChar(p++, c);
		no = no >> 7;
		i++;
	}	
}

void emitLengthFixup(char* p) {
	int byteLength = sp - p - 5;	// calculate the no. of bytes between the pointers, offset by the length of the length-field
	emitNumberFixup(p, byteLength);
}

void putstring(char *c) {
	while ((getChar(c) != 0) && (getChar(c+1) != 0)) {
		int upper = hexValue(getChar(c++));
		int lower = hexValue(getChar(c++));
		emitByte(upper*16 + lower); 		
	}
}

void emit5ByteNumber(int length) {
	char* p = emitLength();
	emitNumberFixup(p, length);
}

//==== Global Memory Generation ====

char* memory;		// remember the starting location
char* mp;
int   memoryLength;

void emit4ByteMemory(int n);

char* createMemory(int maxlen) {
	memory = malloc(maxlen);
	mp = memory;
	emit4ByteMemory(0);				// Scratch location - used during fn prologue
	emit4ByteMemory(1024*64*4-16);	// Call stack pointer - start high in memory
	return memory;
}

int getGlobalAddress() {
	return mp - memory;
}

void emit1ByteMemory(int n) {
	writeChar(mp++, n);
}

void emit4ByteMemory(int n) {
	if (n > 0) {
		//printf("init %i to %i\n", getGlobalAddress(), n);
	}
	emit1ByteMemory(n >>  0);	// little endian
	emit1ByteMemory(n >>  8);
	emit1ByteMemory(n >> 16);
	emit1ByteMemory(n >> 24);
}

void emitStringToMemory(char* s) {	// call multiple times to concat strings. MUST call emitStringEndToMemory() to properly terminate.
	while (getChar(s)) {
		emit1ByteMemory(getChar(s++));
	}
}

void emitStringEndToMemory() {
	emit1ByteMemory(0);	// terminate the string
	
	while (getGlobalAddress() & 3 ) {	// round to a 4 byte address
		emit1ByteMemory(0);
	}	
}

void dumpSymTable();
	
//==== Lexer ====

char* inputname;
FILE* input;

int curln;
char curch;

char* buffer;
int buflength;
int token;
	
//No enums :(
int token_other = 0;
int token_ident = 1;
int token_int = 2;
int token_str = 3;

int intResult;	// lexer does ascii int/hex conversion too.

char next_char () {
    if (curch == 10)	// '\n'
        curln++;
	curch = fgetc(input);
	//if (curch == 64) {	// ampersand - use for debug
	//	dumpSymTable();
	//	curch = fgetc(input);
	//}
	//puts("next_char: "); puti(curch); puts("\n");
    return curch;
}

bool prev_char (char before) {
    ungetc(curch, input);
    curch = before;
    return false;
}

void eat_char () {
	writeChar(buffer + buflength, curch);
    next_char();
	buflength++;
}

void decode_char() {
	if (curch == '\\') {
		next_char();
		curch = curch == 'r' ? 13 : curch == 'n' ? 10 : curch == 't' ? 9 : curch; // anyting else, incl. '\\', \', \" just use 2nd char.
	}
	eat_char();
}

void next () {
    //Skip whitespace
    while (curch == ' ' || curch == '\r' || curch == '\n' || curch == '\t')	//  32 13 10 9 ' ', '\r', '\n', '\t'
        next_char();

    //Treat preprocessor lines as line comments
    if (   curch == '#'
        || (curch == '/' && (next_char() == '/' || prev_char('/')))) {
        while (curch != '\n' && !feof(input))
            next_char();

        //Restart the function (to skip subsequent whitespace, comments and pp)
        next();
        return;
    }

    buflength = 0;
    token = token_other;

    //Identifier or keyword
    if (isalpha(curch)) {
        token = token_ident;
        while ((isalnum(curch) || curch == '_') && !feof(input))
            eat_char();

    //Integer literal
    } else if (isdigit(curch)) {	// do ascii to int conversion here - incl. hex format.
        token = token_int;
		intResult = 0;
		eat_char();
		if (getChar(buffer) == '0' && curch == 'x') {	// it's a hex number.
			eat_char();
			while (ishex(curch) && !feof(input)) {
				intResult = intResult*16 + hexValue(curch);
				eat_char();
			}
		} else {
			intResult = getChar(buffer) - '0';
			while (isdigit(curch) && !feof(input)) {
				intResult = intResult * 10 + curch - '0';
				eat_char();
			}
		}

	// character literal
	} else if (curch == 39) {
		token = token_int;
		next_char();	// skip the quote
		
		decode_char();
		intResult = getChar(buffer);
		
		next_char();	// assume closing quote too. Should check.... if (curch != 39)	
		buflength = 0;	// reset the buffer
		
    //String literal
    } else if (curch == '"') {	
        token = token_str;
        next_char();

        while (curch != '"' && !feof(input)) {
            decode_char();
        }

        next_char();

    //Two char operators
    } else if (   curch == '+' || curch == '-' || curch == '|' || curch == '&'
               || curch == '=' || curch == '!' || curch == '>' || curch == '<') {
        eat_char();

        if ((curch == getChar(buffer) && curch != '!') || curch == '=')
            eat_char();

    } else
        eat_char();

	writeChar(buffer + buflength, 0);	// null terminate the token
	//printf("DEBUG: next, token = %i, buflength = %i, buffer = %s\n", token, buflength, buffer);
	//printfsmall("DEBUG: buffer = %s, intResult = ", buffer); puti(intResult); puts(" token = "); puti(token); puts("\n");
}

void lex_init (char* filename, int maxlen) {
    inputname = filename;
    input = fopen(filename, "r");

    //Get the lexer into a usable state for the parser
    curln = 1;
    buffer = malloc(maxlen);
    next_char();
    next();
}

//==== Parser helper functions ====

int errors;

void errorLine() {
    //printf("%s:%d: error: ", inputname, curln);
	puts(inputname); puts(":"); puti(curln); puts(": error: ");
}

void error (char* format) {
    errorLine();
    //Accepting an untrusted format string? Naughty!
    //printf(format, buffer);
	printfsmall(format, buffer);
    errors++;
}

void require (bool condition, char* format) {
    if (!condition)
        error(format);
}

bool see (char* look) {
    return (token!=token_str) && !strcmp(buffer, look);	// ), and ")" both appear the same in buffer[].
}

bool waiting_for (char* look) {
    return !see(look) && !feof(input);
}

void match (char* look) {
    if (!see(look)) {
        errorLine();
        //printf("expected '%s', found '%s'\n", look, buffer);
		printfsmall("expected '%s'", look); printfsmall(", found '%s'\n", buffer);
        errors++;
    }

    next();
}

bool try_match (char* look) {
    if (see(look)) {
        next();
        return true;
    }
	return false;
}

//==== Symbol table ====

char** globals;
int global_no;
int* globalAddr;	// the global variable's address in global memory. Only valid if it this symbol table entry is a global var.
int* is_fn;			// 0 = it's not a function. -1 = function declaration seen (initially, later negative index into the exports section) . >0 = fn body exists, index into Code section (offset by 1)
int* fn_sig;		// index into the Type section for the correct fn signature //ignore the following offset by 1. Therefore 0 if not a fn.

char** locals;
int local_no;
int param_no;
int* offsets;
int bodyCount;
int functionOffset;	// imported functions offset the number for functions with a body. -1 indicates it's not been updated yet. It's a crosscheck to ensure things are done in the right order

void sym_init (int max) {
    globals = malloc(ptr_size*max);
    global_no = 0;
	globalAddr = calloc(max, ptr_size);  // zero initialise
    is_fn      = calloc(max, ptr_size);  // zero initialise
	fn_sig     = calloc(max, ptr_size);  // zero initialise

    locals = malloc(ptr_size*max);
    local_no = 0;	// none of these 0 initialisations are really needed...
    param_no = 0;
	bodyCount = 1;	// offset the count by one for is_fn[] encoding
    offsets = calloc(max, word_size);
	functionOffset = -1;
}

int sym_lookup (char** table, int table_size, char* look) {
    int i = 0;

    while (i < table_size)
        if (!strcmp(table[i++], look))
            return i-1;

    return -1;
}

void new_global (char* ident) {	
	int varAddr = getGlobalAddress();
	//printf("Global: %s = %i\n", ident, varAddr);									
    globals[global_no] = ident;
	globalAddr[global_no++] = varAddr;

}

void new_fn (char* ident, bool returnsValue, int noParams, bool hasBody) {
	//printf("new function %s, global_no = %i, bodycount = %i, signature = %i, hasBody = %i\n", ident, global_no, bodyCount, returnsValue + 2*noParams, hasBody);

	int existing = sym_lookup(globals, global_no, ident);
	if (existing > 0) {	// matches an existing entry - delete that entry by "zeroing" impt fields
		//puts("matches existing entry & deleting it. index = "); puti(existing); puts("\n");
		free(globals[existing]);
		globals[existing] = "<del>";	// these are the 2x fields that need to be reset to fully delete the entry. NB - don't set to 0; other functions assume a null terminated string!
		is_fn[existing] = 0;
		globalAddr[existing] = global_no;		// set up a link to the newly created, correct entry - needed when doing call fixups.
	}
	//  add it
	is_fn[global_no] = hasBody ? bodyCount++ : -1;
	fn_sig[global_no] = returnsValue + (2*noParams); 		// calculate index into Type Section's function signatures
	new_global(ident);

}

int new_local (char* ident) {
    locals[local_no] = ident;
    offsets[local_no] = local_no;
    return local_no++;
}

void new_param (char* ident) {
    new_local(ident);
	param_no++;
}

void dumpSymTable() {
    int i = 0;
	puts("Globals dump\n");
	int j = global_no;
	puti(global_no);
    while (i < global_no) {
        puts(globals[i++]); puts(" ");
	}
	puts("\n");
}	

//Enter the scope of a new function
void new_scope () {
    local_no = 0;
    param_no = 0;
}

//==== Codegen call fixups ====

// At the time we generate a Call instruction we don't yet know the function's index
// Therefore record the location of all the call instructions in this table, and go back to fix them all when we know

int* fixupSymTableEntry;
char** fixupBytecodeAddr;
int  fixupIndex;

void init_codegen(int max) {
	fixupSymTableEntry = calloc(max, ptr_size);
	fixupBytecodeAddr  = calloc(max, ptr_size);
	fixupIndex = 0; // really unnecessary
}

void emitCall(int symtableIndex) {
	emitByte(0x10);		//call instruction
	fixupSymTableEntry[fixupIndex] = symtableIndex;
	//puts("enter fixup no "); puti(fixupIndex); puts(", index = "); puti(symtableIndex); puts("\n");
	fixupBytecodeAddr[fixupIndex] = emitByte(0x55);	// arbitrary no. acting as a sentinel - check on overwriting. Just 1x varuint byte so limited to 127 fns max.
	fixupIndex++;
}

void doCallFixups() {	// must be called after generateImportSection() as the is_fn values for external functions needs to have been updated.
	if (functionOffset < 0) {
		puts("functionOffset is < 0; interlock has failed\n");
	}
	int i = 0;
	while (i<fixupIndex) {
		int symtableIndex = fixupSymTableEntry[i];
		if ( !strcmp("<del>", globals[symtableIndex]) ) {
			//puts("Re-routing call to fwd-declared fn. From "); puti(symtableIndex); puts(" to "); puti(globalAddr[symtableIndex]); puts("\n");
			symtableIndex = globalAddr[symtableIndex];
		}
		int callOffset = is_fn[symtableIndex];
		if (callOffset < 0) {
			callOffset = - callOffset - 1;	// it's calling an imported function; invert the number and allow for the offset
		} else {
			callOffset = callOffset - 1 + functionOffset;	// calling a local function, offset the index by the number of imported fuctions. Also it also has an offset
		}
		char* bytecodeAddr = fixupBytecodeAddr[i];
		//printf("dofixups %i: symtableIndex = %i, calloffset = %i, bytecodeAddr = %i\n", i, symtableIndex, callOffset, bytecodeAddr-bytecode);
		if ( getChar(bytecodeAddr) != 0x55) {
			//printf("Sentinel is invalid - found %i at %i\n", bytecodeAddr[0], bytecodeAddr-bytecode);
			puts("Sentinel is invalid - found "); puti(bytecodeAddr[0]); puts(" at "); puti(bytecodeAddr-bytecode); puts("\n");
		}
		//puts("doCallFixups no. "); puti(i); puts(", addr = "); puti(bytecodeAddr); puts(", offset = "); puti(callOffset); puts("\n");
		writeChar(bytecodeAddr, callOffset);	// this has to be a byte write, otherwise it will obliterate adjacent bytecode
			
		i++;
	}
}

//==== One-pass parser and code generator ====

bool lvalue;

void needs_lvalue (char* msg) {
    if (!lvalue)
        error(msg);

    lvalue = false;
}

int expr (int level, int stackDepth);

//The code generator for expressions works by placing the results on the stack

//Regarding lvalues and assignment:

//An expression which can return an lvalue looks ahead for an
//assignment operator. If it finds one, then it pushes the
//address of its result. Otherwise, it dereferences it.

//The global lvalue flag tracks whether the last operand was an
//lvalue; assignment operators check and reset it.

int factor (int stackDepth) {
	int global;	// compiler limitation that local vars have to be declared 1st, not inside {}
	int local;
	int str;
	int arg_no;
	int stackDelta = 1;		// the stack grows by 1 for most factors; the exception is a function call with a void return.
	
    lvalue = false;

    if (see("true") || see("false")) {
		emitConst( see("true") ? 1 : 0);
        next();

    } else if (token == token_ident) {
        global = sym_lookup(globals, global_no, buffer);
        local = sym_lookup(locals, local_no, buffer);
		// printf("symbol %s: global= %i, local= %i\n", buffer, global, local);

        require(global >= 0 || local >= 0, "no symbol '%s' declared\n");
        next();

        if (see("=") || see("++") || see("--"))
            lvalue = true;

        if (global >= 0) {
			
			if (!is_fn[global]) {
				// ok, it's an ordinary variable
				emitConst(globalAddr[global]);
				if (!lvalue) {
					emiti32load(0x00);		// i32.load offset = 0;
				}
			} else {
				// it's a function call - parse the parameters
				match("(");
				arg_no = 0;
				if (waiting_for(")")) {
					do {
						expr(0, 0);	//start stack counting again for the para. Could add a check that the returned stack is 1 
						arg_no++;
					} while (try_match(","));
				}
		
				match(")");
				emitCall(global);
				if ( !(fn_sig[global] & 1) ) {	// bottom bit of signature determines the return value
					stackDelta = 0;
				}
			}
			
		}
        else if (local >= 0) {					// all locals are in the frame
			emit2Byte(0x20, 2);					// get_local - 'cached' fp
			emitConst(4*(offsets[local]+1));	// position 0 is the old-fp
			emitByte(0x6a);						// i32.add - now have the address of the local
			if (!lvalue) {
				emiti32load(0);					// TODO Hmm, could optimise the read case by using the offset in load. Only use the calc. for lvalue.
			}

		}
		
    } else if (token == token_int) {
		emitConst(intResult);
        next();
		
    } else if (token == token_str) {
        str = getGlobalAddress();
        while (token == token_str) {	//Consecutive string literals are concatenated.
			emitStringToMemory(buffer);
            next();
        }
		emitStringEndToMemory();
		emitConst(str);			// put the address of the string on the stack

    } else if (try_match("(")) {
        expr(0, 0);				// should really verify that the returned stack depth is 1.
        match(")");

    } else
        error("expected an expression, found '%s'\n");
	
	return stackDepth + stackDelta;
}

int object (int stackDepth) {
	int arg_no;
	
    stackDepth = factor(stackDepth);

    while (true) {
		// function parameter processing used to be done here, but is now moved to factor() because of wasm restriction - can't put fn addr on stack
		if (try_match("[")) {
			// array indexing
			// the base address is on the stack
            stackDepth = expr(0, stackDepth);
            match("]");

            if (see("=") || see("++") || see("--"))
                lvalue = true;
			
			emitConst(4);	// word-size
			emitByte(0x6c);	// i32.mul -- multiply the index by the word-size
			emitByte(0x6a);	//i32.add  -- and add in the base address to get the final address

			if (!lvalue) {	// doesn't properly deal with return a[b] ???? The code looks like it should????
				emiti32load(0x00);		// i32.load offset = 0;
			}
			stackDepth--;	// 2 stack positions are replaced by 1
        } else
            return stackDepth;
    }
	return 0;	// keep compiler happy when true is defined as a variable, not a constant...
}

int unary (int stackDepth) {
    if (try_match("!")) {
        //Recurse to allow chains of unary operations, LIFO order
        unary(stackDepth);
		emitByte(0x45);		// i32.eqz - inverts the boolean

    } else if (try_match("~")) {
        //Recurse to allow chains of unary operations, LIFO order
        unary(stackDepth);
		emitConst(-1);
		emitByte(0x73);		// i32.xor with 0 to invert all bits.
		
	}else if (try_match("-")) {
        unary(stackDepth);
		// there's no i32.neg instruction!
		emit2Byte(0x21, 0);				// set_local
		emitConst(0x00);
		emit2Byte(0x20, 0);				// get_local
		emitByte(0x6b);					// i32.sub

    } else {
        //This function call compiles itself
        stackDepth = object(stackDepth);

        if (see("++") || see("--")) {
			needs_lvalue("assignment operator '%s' requires a modifiable object\n");
			
			emit2Byte(0x22, 0);		// tee_local L0
			emit2Byte(0x20, 0);		// get_local L0
			emiti32load(0x00);		// i32.load offset = 0;
			emit2Byte(0x22, 1);		// tee_local L1
			emitConst(0x01);		// 
			emitByte(see("++")? 0x6a : 0x6b);	// i32.add : i32.sub
			emiti32store(0x00);		// update the pointer
			emit2Byte(0x20, 1);		// get_local L1 -- finally put the original value back on the stack
			
            next();					// net result of all this is the stack remains at the same depth
        }
    }
	return stackDepth;
}

void ternary ();

int expr (int level, int stackDepth) {
	char instr;
	char instrNot;
	int shortcircuit;
	
    if (level == 6) {
        stackDepth = unary(stackDepth);
        return stackDepth;
    }

    stackDepth = expr(level+1, stackDepth);

    while ( level == 5 ? see("*") || see("/") || see("%") 
		   : level == 4 ? see("+") || see("-") || see("&") || see("|") || see("<<") || see(">>")
           : level == 3 ? see("==") || see("!=") || see("<") || see(">") || see(">=") || see("<=") 
           : false) {
		
        instr = see("+") ? 0x6a : see("-") ? 0x6b : see("*") ? 0x6c : see("/") ? 0x6d : see("%") ? 0x6f :
			    see("&") ? 0x71 : see("|") ? 0x72 : see("<<") ? 0x74 : see(">>") ? 0x75 :
                see("==") ? 0x46 : see("!=") ? 0x47 : see("<") ? 0x48 : see(">") ? 0x4a :see(">=") ? 0x4e : 0x4c; // last one is <=

        next();
        stackDepth = expr(level+1, stackDepth);
		emitByte(instr);
		stackDepth--;	// 2x operand are replaced by one
    }

    if (level == 2) while (see("||") || see("&&")) {
		// would like to just conditionally jump forward, but this isn't easy in wasm.
		emit2Byte(0x21, 0);				// set_local - we need to both test and optionally restore this value on the stack
		shortcircuit = emitBlock(0x7f);	// the updated condition result is returned on TOS
		emit2Byte(0x20, 0);				// get_local - this will be TOS at the end of the block if we don't need to calc. the next expr.
		emit2Byte(0x20, 0);				// get_local - Get the value to test it
		if (see("&&") )
			emitByte(0x45);				// optionally invert the result
		emitBranch(0x0d, shortcircuit);
		emitByte(0x1a);					// drop the condition result

        next();
        expr(level+1, 0);				// should check that it returns stackDepth == 1
		
		emitEnd();						// end result is that the stackDepth is at the same at exit as at entry, irrespective of the 2x paths 
    }

    if (level == 1 && try_match("?"))
        ternary();

    if (level == 0 && try_match("=")) {
        //lvalue is already on the stack
		
        needs_lvalue("assignment requires a modifiable object\n");
        stackDepth = expr(level+1, stackDepth);
		
		emiti32store(0x00);				// store, 0 offset
		stackDepth = stackDepth - 2;	// 2x values removed from the stack
    }
	return stackDepth;
}

void line ();

void ternary () {
	// the result of the <conditional evaluation> is on the stack
	// need to save into a local and then create the nested blocks for the <then>, <else> parts
	// this code closely matches if_branch() below
	emit2Byte(0x21, 0);					// set_local
	int ifBlock   = emitBlock(0x7f);	// the block exits with a i32
	int elseBlock = emitBlock(0x40);	// Block encloses 'then' with 0 results
	                emitBlock(0x40);	// Block encloses 'conditional' with 0 results
	emit2Byte(0x20, 0);					// get_local
	emitByte(0x45);						// i32.eqz
	emitBranch(0x0d, elseBlock);		// br_if
	emitEnd();							// end of condition eval.
	
	expr(1, 0);							// emit 'then' code
	
	emitBranch(0x0c, ifBlock);			// br
	emitEnd();							// end of 'then'code
	match(":");
    expr(1, 0);							// emit 'else' code

	emitEnd();					
}

void if_branch () {
	// create code all if stmts as follows:
	// Block     -- wraps entire if stmt, contains the 'else' code (if any)
	//   Block    -- contains the 'then' code
	//     Block  -- contains the condition evaluation
	//     ...code to evaluate the condition, with br_if's as required
	//     ...flow thru. or br_if 0x00 to execute 'then' clause
	//     end
	//   ...'then' clause code
	//   br 0x01  -- exit the outer if stmt block
	//   end
	//   ...'else' clause code
	// end
	int ifBlock   = emitBlock(0x40);	// Block encloses 'if' with 0 results 
	int elseBlock = emitBlock(0x40);	// Block encloses 'then' with 0 results
	                emitBlock(0x40);	// Block encloses 'conditional' with 0 results
	
    match("if");
    match("(");
    expr(0, 0);
	match(")");
	
	emitByte(0x45);					// i32.eqz
	emitBranch(0x0d, elseBlock);	// br_if
	emitEnd();						// end of condition eval.
	
	line();							// emit 'then' code
	
	emitBranch(0x0c, ifBlock);		// br
	emitEnd();						// end of 'then'code
	if (try_match("else"))
        line();						// emit 'else' code

	emitEnd();
}

void while_loop () {
	// Block -- wraps entire while/do stmt
	//   Loop -- contains the conditional/body for while loop or body/conditional for do loop. while loop illustrated below
	//     Block
	//       ...code to evaluate the conditional, with br_if's as required
	//       ...flow thru. for "true" condition or br_if 0x01 to exit the while
	//     End
	//     ... body code
	//     br to loop
	//   End
	// End
	int whileBlock = emitBlock(0x40);
	int loopBlock  = emitLoop (0x40);

    bool do_while = try_match("do");

    if (do_while)
        line();

	emitBlock(0x40);				// block enclosing the conditional
		
    match("while");
    match("(");
    expr(0, 0);
    match(")");

	emitByte(0x45);					// i32.eqz
	emitBranch(0x0d, whileBlock);	// br_if
	emitEnd();						// end of condition eval.
	
    if (do_while)
        match(";");
    else
        line();

	emitBranch(0x0c, loopBlock);	// br back to beginning of loop
	emitEnd();						// end of loopBlock
	emitEnd();						// end of whileBlock
}

void decl (int kind);

//See decl() implementation
int decl_module = 1;
int decl_local = 2;
int decl_param = 3;

int return_to;

void line () {
	bool ret;
	int stackDepth = 0;
	
    if (see("if"))
        if_branch();

    else if (see("while") || see("do"))
        while_loop();

    else if (see("int") || see("char") || see("bool") || see("void"))
        decl(decl_local);

    else if (try_match("{")) {
        while (waiting_for("}"))
            line();

        match("}");

    } else {
        ret = try_match("return");

        if (waiting_for(";"))
            stackDepth = expr(0, 0);	// set starting stack-depth to 0

        if (ret) {
            emitBranch(0x0c, return_to);
		} else {
			// non-zero stackDepth without a return. Need to pop it. Caused by i++ or fn-call with discarded ret-value
			if (stackDepth > 0) {
				emitByte(0x1a);			// drop. Pop the unneeded value off the stack
				// printf("Stack non-zero, pop required %i\n", stackDepth);
			}
		}
			
        match(";");
    }
}

void function (char* ident, bool returnsValue) {
	//Header
	char* length = emitLength();		// the length will be fixed up below	
	emitByte(1);						// no. of local entries - just one as all locals are type i32
	emitByte(3);						// no. of locals of type i32. We only ever use 3x locals
	emitByte(0x7f);						// i32 type
	return_to = emitBlock(returnsValue ? 0x7f : 0x40);		// either 0x7f (i32 return) or 0x40 (void return)
	
    //Prologue: - update the frame
	int fp =4;
	int temp = 0;
	emitConst(temp);			  	// save old value in temp location
	emitConst(fp);					// get the fp
	emiti32load(0);
	emiti32store(0);
	
	emitConst(fp);   				// update the fp
	emitConst(fp);
	emiti32load(0);
	emitByte(0x41);					// i32.const
	char* frameSize = emitLength();	// fix up the actual size later
	emitByte(0x6b);					// i32.sub
	emiti32store(0);
	
	emitConst(fp);    				// save old fp in 1st frame location
	emiti32load(0);
	emitConst(temp);
	emiti32load(0);
	emiti32store(0);
	
	int i = 0;
	while (i < param_no) {
		emitConst(fp);    			// save parameters in the frame
		emiti32load(0);				// get the fp
		emit2Byte(0x20, i);			// get_local
		emiti32store(4 + (4*i));	// and save it, using the offset feature at last
		i++;
	}
	
	emitConst(fp);    				// 'cache' the bp value to speed up para, local var access
	emiti32load(0);
	emit2Byte(0x21, 2);				// set_local

    line();

	emitEnd();				// where any return statements jump to
	
	//Epilogue:
	emitConst(fp);			// update the fp
	emit2Byte(0x20, 2);		// get the old value of fp using the cached fp
	emiti32load(0);    		// offset 0, where old fp is saved
	emiti32store(0);    	// fp is now updated; easy.

	emitByte(0x0b);			// function terminator
	
	// Now calculate & fix the frame size. The frame comprises: <old fp> *<paras> *<locals>
	// local_no already = <no-paras> + <no-locals>	-- bad naming....
	emitNumberFixup(frameSize, 4*(local_no+1));
	emitLengthFixup(length);

}

void decl (int kind) {
    //A C declaration comes in three forms:
    // - Local decls, which end in a semicolon and can have an initializer.
    // - Parameter decls, which do not and cannot.
    // - Module decls, which end in a semicolon unless there is a function body.

    bool fn = false;
	bool returnsValue = false;
    bool fn_impl = false;
    int local;
	
	if (see("const"))	// the stdio function declarations need const type definitions to avoid cl compiler warnings
		next();
	
	returnsValue = !see("void");
    next();		// skip over the type declaration - int, void, etc.

    while (try_match("*")) 
        returnsValue = true;	// void* does return a value

    //Owned (freed) by the symbol table
    char* ident = strdup(buffer);
    next();

    //Functions
    if (try_match("(")) {
		// it's a function
        if (kind == decl_module)
            new_scope();

        //Params
        if (waiting_for(")")) do {
            decl(decl_param);
        } while (try_match(","));

        match(")");

        new_fn(ident, returnsValue, param_no, see("{"));	// this captures all we need - returnType, no-params, body
        fn = true;

        //Body
        if (see("{")) {
            require(kind == decl_module, "a function implementation is illegal here\n");

            fn_impl = true;
            function(ident, returnsValue);
        }

    //Add it to the symbol table
    } else {
        if (kind == decl_local) {
            local = new_local(ident);
        } else {
			if (kind == decl_module)
				new_global(ident);
			else
				new_param(ident);
            // kind == decl_module ? new_global(ident) : new_param(ident);
		}
    }

    //Initialization

    if (see("="))
        require(!fn && kind != decl_param,
                fn ? "cannot initialize a function\n" : "cannot initialize a parameter\n");

    if (kind == decl_module) {

        if (try_match("=")) {
            if (token == token_int) {
				emit4ByteMemory(intResult);
			}
            else
                error("expected a constant expression, found '%s'\n");

            next();

        //Static data defaults to zero if no initializer
        } else if (!fn) {
			emit4ByteMemory(0);
		}

	// it's a variable declaration within a function...
    } else if (try_match("=")) {
		// replicate code from factor() for loading the local's address. This could be simplified slightly by using the i32.store's offset field
		emit2Byte(0x20, 2);					// get_local - 'cached' fp
		emitConst(4*(offsets[local]+1));	// position 0 is the old-fp
		emitByte(0x6a);						// i32.add - now have the address of the local
		
        expr(0, 0);
		
		emiti32store(0);					// and save the result
    }

    if (!fn_impl && kind != decl_param && !feof(input))
        match(";");
}

//==== wasm binary file format generation ====

void generateFunctionType(int no_paras, int returnType) {
	emitByte(0x60);	// form
	emitByte(no_paras);
	int i = 0;
	while( i++<no_paras ) {
		emitByte(0x7f);
	}
	emitByte(returnType);
	if (returnType) {
		emitByte(0x7f);
	}
}

void generateTypeSection(int maxParams) {
	// Type Section declares all function signatures that will be used in the module.
	// For simplicity we're going to just create a list of signatures up to 5 paramaters with/without a return value
	// entry	meaning
	// 0		void result, no paras
	// 1		int  result, no paras
	// 2		void result, 1 para
	// 3		int  result. 1 para
	// 4		void result, 2 para
	// 5		int  result, 2 para
	// So index = returnsResult + 2 x no-paras
	// format is: <id> <payload-len><count of entries> <func-type>*
	// where <func-type> is: <form><para-count><para-type>*<return-count><return-type>
	// For us, form = 0x60, para-type, return-type are always int32 = 0x7f (if present)
	emitByte(0x01);					// Section Type
	char* length = emitLength();
	emitByte(2 + (2*maxParams));		// calculate the number of entries
	int i = 0;
	while( i<=maxParams ) {
		generateFunctionType(i, 0);
		generateFunctionType(i, 1);
		i++;
	}
	emitLengthFixup(length);
}

void generateFunctionSection() {
	emitByte(0x03);	// Function Section
	char* length = emitLength();
	emitByte(bodyCount-1);	// remember bodyCount is deliberately off by one
	
	int i = 0;
	int xCheck = 0;
    while (i < global_no) {			// functions are put in the global table
        if (is_fn[i]>0) {
			emitByte(fn_sig[i]);
			xCheck++;
		}
		i++;
	}
	if (bodyCount-1 != xCheck)
		//printf("function cross-checks don't match: %i vs %i\n", bodyCount-1, xCheck);
		puts("function cross-checks don't match\n");
	emitLengthFixup(length);
}

void emitString(char* s) {
	emitByte(strlen(s));
	while (getChar(s) != 0) {
		emitByte(getChar(s++));
	}
}	

void generateImportEntry(char* name, int signature) {
	emitString("env");
	emitString(name);
	emitByte(0x00);		// external kind = function
	emitByte(signature);
}

void generateImportSection() {	// Work even when there are 0 entries
	// this call also updates the is_fn[] indicies for the symbol table entries for imported functions.
	emitByte(0x02);	// Imports Section
	char* length = emitLength();
	char* noEntries = emitLength();	

	int entries = 0;
	
	int i = 0;
	while (i < global_no) {
		if (is_fn[i] < 0) {			// it's a fn, with no body defined
			is_fn[i] = -(entries+1);	// update the function index; it's -ve, and offset by one...
			generateImportEntry(globals[i], fn_sig[i]);
			entries++;
		}
		i++;
	}
	functionOffset = entries;

	emitNumberFixup(noEntries, entries); 
	emitLengthFixup(length);
}

void generateExportEntry(char* name, int kind, int index) {
	emitString(name);
	emitByte(kind);	
	emitByte(index);
}

void generateExportsSection() {
	emitByte(0x07);						// Exports Section
	char* length = emitLength();
	char* noEntries = emitLength();	
	generateExportEntry("memory", 0x02, 0x00);
	int entries = 1;
	
	int i = 0;
	while (i < global_no) {
		if (is_fn[i] > 0) {			// it's a fn, with a body defined
			generateExportEntry(globals[i], 0x00, is_fn[i]-1+functionOffset); // smell - this calc is done twice
			entries++;
		}
		i++;
	}

	emitNumberFixup(noEntries, entries); 
	emitLengthFixup(length);
}

void generateDataSection() {
	emitByte(11);					// Data Section
	char* length = emitLength();
	emitByte(1);					// just 1 data segment
	emitByte(0);					// the linear memory index, = 0
	emitConst(0);					// offset of 0
	emitByte(0x0b);					// terminate with End
	int payloadLength = getGlobalAddress();
	emit5ByteNumber(payloadLength);	// size of data in bytes
	char* p = memory;
	
	while (payloadLength) {
		emitByte(getChar(p++));
		payloadLength--;
	}

	emitLengthFixup(length);
}

// binary file output 

void writeStream(char* ptr, int length, char* header) {	
	int count = 0;
	while (count++<length) {				// output the bytes that's been generated...
		fputc(getChar(ptr++), output);	// write it out to the file
	}
}
	
void program () {
    errors = 0;
	bytecode = createEmitStream(100000);	// start with this - should be big enough
	
	char* count_of_bodies = emitByte(0);	// come back & fix this value after having generated code for all the functions. Assume <127 max.

    while (!feof(input))
        decl(decl_module);
	
	int byteCodeLength = stream_length();
	
	writeChar(count_of_bodies, bodyCount-1);	// Overwrite the earlier no. remember bodyCount is off by 1

	headerCode = createEmitStream(50000);	// create another stream for the header info.
	
	// Generate the header
	putstring("0061736d");		// magic
	putstring("01000000");		// version
	
	// Generate Section: type
	generateTypeSection(4);		// up to just 2x parameters to begin with - this is still 5x entries
	
	//Generate Section: Imports
	generateImportSection();
	
	doCallFixups();		// update the bytecode with correct call offset. Must be called after generateImportSection()
	
	// Generate Section: Function
	generateFunctionSection();
	
	// Generate Section: Table
	putstring("048480808000");	// type, length
	putstring("01700000");		// count, anyfunc-type, resizable limits
	
	// Generate Section: Memory
	putstring("058380808000");	// type, length
	putstring("010010");		// count=1, memory_type = resizable limits. Flags=0, so only initial length present = 1 = 64kbytes; 10 = 16 * 64K
	
	// Generate Section: Global
	putstring("068180808000");	// type, length
	putstring("00");			// count = 0
	
	// Generate Section: Export
	generateExportsSection();
	
	// Generate Section: Code
	emitByte(0x0a);					// Section type = code
	emit5ByteNumber(byteCodeLength);	// the length of the entire code section
	
	// Write out the wasm file:
	
	writeStream(headerCode, stream_length(), "Header");
	
	writeStream(bytecode, byteCodeLength, "Bytecode");
	
	headerCode = createEmitStream(50000);	// create a new stream just for the final Data section
	
	generateDataSection();
	writeStream(headerCode, stream_length(), "Data");
}

int main (int argc, char** argv) {
	
	//--argc; ++argv;
	//if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }
	//if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { debug = 1; --argc; ++argv; }
	//if (argc < 1) { printf("usage: c4 [-s] [-d] file ...\n"); return -1; }
	
	char* ipname;
    if (argc == 2) {
		ipname = argv[1];
        //puts("Usage: cc <Input file>\n");
        //return 1;
    } else
		ipname = "cc.c";

    //output = fopen(argv[1], "w");
	output = fopen("program.wasm", "wb");	// keep it simple to begin with. Must open with 'b' for binary, otherwise it adds \r to each \n
    lex_init(ipname, 256);
    sym_init(800);
	init_codegen(3000);	// one entry for every call - should be enough
	createMemory(20000); // should be enough

    program();

    return errors != 0;
}