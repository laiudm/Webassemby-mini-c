var imports = {
  "env": {
  }
}


WebAssembly.compile(wasmCode)
.then(module => WebAssembly.instantiate(module, imports))
.then(function(instance) {
	var exports = instance.exports;

	log("Result of calling add: " + exports.add(2, 2));
	log("Result of calling add: " + exports.add(2, 2));
	log("Result of calling add: " + exports.add(3, 2));
	log("Result of calling add: " + exports.add(4, 2));
	log("Result of calling add: " + exports.add(5, 2));
 });
 