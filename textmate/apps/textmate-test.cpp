#include <ux/ux.hpp>

using namespace ion;

int main(int argc, char *argv[]) {
	RegEx regex(utf16("\\w+"), RegEx::Behaviour::global);
    array<utf16> matches = regex.exec(utf16("Hello, World!"));

	if (matches)
		console.log("match: {0}", {matches[0]});

	var grammar = path("style/js.json").read<var>();

    ///
    return 0;
}
