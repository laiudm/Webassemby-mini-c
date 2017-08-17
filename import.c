void js(int a, int b);
int  js2(int a, int b);

int called(int a, int b) {
	js(a, b);
	return js2(b, a);
}
