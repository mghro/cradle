from conans import ConanFile, CMake
import os


class CradleConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = \
        "boost/1.67.0@conan/stable", \
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
        "yaml-cpp/0.6.2@tmadden/stable", \
        "spdlog/0.16.3@bincrafters/stable"
    generators = "cmake"
    default_options = \
        "boost:without_atomic=True", \
        "boost:without_chrono=True", \
        "boost:without_container=True", \
        "boost:without_context=True", \
        "boost:without_coroutine=True", \
        "boost:without_graph=True", \
        "boost:without_graph_parallel=True", \
        "boost:without_log=True", \
        "boost:without_math=True", \
        "boost:without_mpi=True", \
        "boost:without_serialization=True", \
        "boost:without_signals=True", \
        "boost:without_test=True", \
        "boost:without_timer=True", \
        "boost:without_type_erasure=True", \
        "boost:without_wave=True", \
        "FakeIt:integration=catch", \
        "*:shared=False"

    def imports(self):
        dest = os.getenv("CONAN_IMPORT_PATH", "bin")
        self.copy("*.dll", dst=dest, src="bin")
        self.copy("*.dylib*", dst=dest, src="lib")
        self.copy("cacert.pem", dst=dest, src=".")
