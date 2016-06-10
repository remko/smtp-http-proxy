import os, platform

vars = Variables("config.py")
vars.Add(BoolVariable("optimize", "Compile with optimizations turned on", "no"))
vars.Add(BoolVariable("debug", "Compile with debug information", "yes"))
vars.Add(BoolVariable("check", "Run unit tests", "no"))
# FIXME: Don't hardcode this
vars.Add(PathVariable("boost_includedir", "Boost headers location", "/usr/local/homebrew/opt/boost/include" , PathVariable.PathAccept))
vars.Add(PathVariable("boost_libdir", "Boost library location", "/usr/local/homebrew/opt/boost/lib", PathVariable.PathAccept))

env = Environment(ENV = {'PATH': os.environ['PATH']}, variables = vars)
Help(vars.GenerateHelpText(env))

# Compiler
if os.environ.get("CC", False) :
	env["CC"] = os.environ["CC"]
if os.environ.get("CXX", False) :
	env["CXX"] = os.environ["CXX"]

# Flags
if env["PLATFORM"] == "win32" :
	env.Append(LINKFLAGS = ["/INCREMENTAL:no"])
	env.Append(CCFLAGS = ["/EHsc", "/MD"])
	env.Append(CPPDEFINES = [("_WIN32_WINNT", "0x0501")])
	if env["debug"] :
		env.Append(CCFLAGS = ["/Zi"])
		env.Append(LINKFLAGS = ["/DEBUG"])
	# Workaround for broken SCons + MSVC2012 combo
	env["ENV"]["LIB"] = os.environ["LIB"]
	env["ENV"]["INCLUDE"] = os.environ["INCLUDE"]
else :
	env["CXXFLAGS"] = ["-std=c++14"]
	if env["debug"] :
		env.Append(CCFLAGS = ["-g"])
		env.Append(CXXFLAGS = ["-g"])
	if env["optimize"] :
		env.Append(CCFLAGS = ["-O2"])
		env.Append(CXXFLAGS = ["-O2"])
	if env["PLATFORM"] == "posix" :
		env.Append(LIBS = ["pthread"])
		env.Append(CXXFLAGS = ["-Wextra", "-Wall"])
	elif env["PLATFORM"] == "darwin" :
		env["CC"] = "clang"
		env["CXX"] = "clang++"
		env["LINK"] = "clang++"
		if platform.machine() == "x86_64" :
			env["CCFLAGS"] = ["-arch", "x86_64"]
			env.Append(LINKFLAGS = ["-arch", "x86_64"])
		env.Append(CXXFLAGS = ["-Wall"])

# LibCURL
libcurl_flags = {
	"LIBS" : ["curl"]
}

# Boost
boost_flags = {
	"CXXFLAGS": ["-isystem", env["boost_includedir"]],
	"LIBPATH": [env["boost_libdir"]],
	"LIBS": ["boost_system", "boost_program_options"]
}

# Executable
prog_env = env.Clone()
prog_env.MergeFlags(libcurl_flags)
prog_env.MergeFlags(boost_flags)
prog_env.Append(CPPPATH = ["Vendor/json"])
prog_env.Program("smtp-http-proxy", [
	"main.cpp"
])

# Tests
check_env = env.Clone()
check_env.Replace(CXXFLAGS = [f for f in env["CXXFLAGS"] if not f.startswith("-W")])
check_env.Append(CPPPATH = ["Vendor/catch"])
check_env.Append(CPPPATH = ["Vendor/json"])
unittests = check_env.Program("unittests/unittests", [
	"unittests/unittests.cpp",
])

if env["check"] :
	check_env.Command("**dummy**", unittests, unittests[0].abspath)
