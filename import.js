var imports = {
  "env": {
    js(a, b) {
		log("js()  called, a = " + a + ", b = " + b);
	},
	js2(a, b) {
		log("js2() called, a = " + a + ", b = " + b);
		return a + b;
	},
  }
}


WebAssembly.compile(wasmCode)
.then(module => WebAssembly.instantiate(module, imports))
.then(function(instance) {
	var exports = instance.exports;

	log("Result of called() = " + exports.called(1, 2));
	
 });