// The following string is the program to be compiled
var inputString = "int x; int add(int a, int b) { return a + b; };";

var inputLength = inputString.length;
var heapMemory = 0x1000;
var ipPtr = 0;
var opstr = "";

var bytecode = new Uint8Array(50000);
var pc = 0;	//pointer into the bytecode

var imports = {
  "env": {
    malloc(sizeInBytes) {
      var addr = heapMemory;
      heapMemory = (heapMemory + sizeInBytes + 3) & (~3); // round to a 4 byte boundary
      return addr;
    },
    fopen  (filename, options) { return 0;},
    fclose (filePointer)       { },
    fgetc  ()                  { return ipPtr < inputLength ? inputString.charCodeAt(ipPtr++) : 0; },
    ungetc (ch, filePointer)   { ipPtr--;},
    feof   ()                  { return ipPtr >= inputLength;},
    fputc  (ch, stream)        { bytecode[pc++] = ch;},	// binary output
    putchar(ch) {
      if (ch == 10) {
        log(opstr);
        opstr = "";
      } else {
        opstr += String.fromCharCode(ch);
      }				
    },
  }
}

WebAssembly.compile(wasmCode)
.then(module => WebAssembly.instantiate(module, imports))
.then(function(instance) {
  var exports = instance.exports;
  var s = performance.now();
  
  var retValue = exports.main(0, 0);
  
  log(retValue ? "Errors found, compile failed\n" : "Compilation successful");
  log("Compilation took " + (performance.now() - s).toFixed(2) + "msec");
  
  var wasmCode = bytecode.slice(0, pc);
  // saveByteArray([wasmCode], "new.wasm");	// uncomment this line to save the generated wasm file
 });
 
 
 
 