// C compilation

var cc = CC.new()

cc.add_opt("-I/usr/local/include")
cc.add_opt("-Isrc")
cc.add_opt("-fPIC")
cc.add_opt("-std=c99")
cc.add_opt("-Wall")
cc.add_opt("-Wextra")
cc.add_opt("-Werror")

var lib_src = File.list("src/lib")
	.where { |path| path.endsWith(".c") }

var cmd_src = File.list("src/cmd")
	.where { |path| path.endsWith(".c") }

var src = lib_src.toList + cmd_src.toList

src
	.each { |path| cc.compile(path) }

// create static & dynamic libraries

var linker = Linker.new(cc)

linker.archive(lib_src.toList, "libiface.a")
linker.link(lib_src.toList, [], "libiface.so", true)

// create command-line frontend

linker.link(cmd_src.toList, ["iface"], "iface")

// copy over headers

File.list("src")
	.where { |path| path.endsWith(".h") }
	.each  { |path| Resources.install(path) }

// running

class Runner {
	static run(args) { File.exec("iface", args) }
}

// installation map

var install = {
	"iface":       "bin/iface",
	"libiface.a":  "lib/libiface.a",
	"libiface.so": "lib/libiface.so",
	"iface.h":     "include/iface.h",
}

// testing

class Tests {
}

var tests = []
