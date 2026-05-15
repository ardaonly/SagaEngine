# ─── SDE — System Definition Engine — Conan Recipe ───────────────────────────
#
# SDE is published as a standalone Conan package consumed by SagaEngine and any
# other downstream that needs validated, reference-resolved game-data graphs.
#
# This recipe is the canonical packaging contract:
#
#   conan create Tools/SystemDefinitionEngine
#       → publishes sde/0.1.1 to the local Conan cache
#
#   self.requires("sde/0.1.1")        # in a downstream conanfile
#   find_package(SDE CONFIG REQUIRED) # in the downstream CMakeLists
#   target_link_libraries(<tgt> PRIVATE SDE::Core)
#
# The recipe is intentionally narrow: SDE depends only on the C++ standard
# library and `nlohmann_json`. There is no transitive Saga* coupling.
#
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy
import os


class SDEConan(ConanFile):
    name        = "sde"
    version     = "0.1.1"
    license     = "Apache-2.0"
    description = "Model definition, validation, and compilation pipeline (SDE)."
    topics      = ("validation", "compilation", "data-pipeline", "game-data")
    homepage    = "https://example.invalid/sde"
    url         = "https://example.invalid/sde"

    package_type = "static-library"
    settings     = "os", "compiler", "build_type", "arch"

    options = {
        "fPIC":       [True, False],
        "build_cli":  [True, False],
        "build_tests": [True, False],
    }
    default_options = {
        "fPIC":        True,
        "build_cli":   True,
        "build_tests": False,
    }

    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "include/*",
        "src/*",
        "cli/*",
        "tests/*",
        "LICENSE",
        "README.md",
        "CHANGELOG.md",
        "version.json",
    )

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        self.settings.compiler.cppstd = "20"

    def requirements(self):
        self.requires("nlohmann_json/3.11.3", transitive_headers=True)

    def build_requirements(self):
        if self.options.build_tests:
            self.test_requires("gtest/1.14.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["SDE_BUILD_CLI"]   = bool(self.options.build_cli)
        tc.cache_variables["SDE_BUILD_TESTS"] = bool(self.options.build_tests)
        tc.cache_variables["SDE_INSTALL"]     = True
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if self.options.build_tests:
            cmake.test()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE",
             src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name",   "SDE")
        self.cpp_info.set_property("cmake_target_name", "SDE::Core")

        self.cpp_info.libs    = ["SDE"]
        self.cpp_info.libdirs = ["lib"]
        self.cpp_info.includedirs = ["include"]

        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs = ["m"]
