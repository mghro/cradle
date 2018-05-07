from conans import ConanFile, CMake
import os


class CradleConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = \
        "Boost/1.64.0@conan/stable", \
        "catch/1.5.0@TyRoXx/stable", \
        "json/2.0.10@jjones646/stable", \
        "msgpack/2.1.5@bincrafters/stable", \
        "cotire/1.7.6@smspillaz/cotire", \
        "libcurl/7.52.1@bincrafters/stable", \
        "OpenSSL/1.0.2m@conan/stable", \
        "sqlite3/3.21.0@bincrafters/stable", \
        "FakeIt/master@gasuketsu/stable", \
        "websocketpp/0.7.0@TyRoXx/stable", \
        "zlib/1.2.11@conan/stable", \
        "bzip2/1.0.6@conan/stable", \
        "yaml-cpp/0.6.2@tmadden/stable"
    generators = "cmake"
    default_options = \
        "Boost:without_atomic=True", \
        "Boost:without_chrono=True", \
        "Boost:without_container=True", \
        "Boost:without_context=True", \
        "Boost:without_coroutine=True", \
        "Boost:without_coroutine2=True", \
        "Boost:without_graph=True", \
        "Boost:without_graph_parallel=True", \
        "Boost:without_log=True", \
        "Boost:without_math=True", \
        "Boost:without_mpi=True", \
        "Boost:without_serialization=True", \
        "Boost:without_signals=True", \
        "Boost:without_test=True", \
        "Boost:without_timer=True", \
        "Boost:without_type_erasure=True", \
        "Boost:without_wave=True", \
        "FakeIt:integration=catch", \
        "*:shared=False"

    def imports(self):
        dest = os.getenv("CONAN_IMPORT_PATH", "bin")
        self.copy("*.dll", dst=dest, src="bin")
        self.copy("*.dylib*", dst=dest, src="lib")
        self.copy("cacert.pem", dst=dest, src=".")
