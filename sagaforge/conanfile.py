from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import save
import json
from datetime import datetime

class SagaEngineConan(ConanFile):
    name = "sagaengine"
    version = "0.1.0"
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("asio/1.30.2")
        self.requires("qt/6.6.2")
        self.requires("diligent-core/api.252009")
        self.requires("libpqxx/7.9.0")
        self.requires("hiredis/1.2.0")
        self.requires("redis-plus-plus/1.3.12")
        self.requires("gtest/1.14.0")


    def build_requirements(self):
        self.tool_requires("cmake/3.28.1")
        self.tool_requires("ninja/1.12.1")

    options = {
        "with_vulkan": [True, False],
        "with_opengl": [True, False],
        "with_d3d11": [True, False],
        "with_d3d12": [True, False],
        "with_metal": [True, False],
        "with_server": [True, False],
        "with_editor": [True, False],
        "with_profiling": [True, False],
    }

    default_options = {
        "with_vulkan": True,
        "with_opengl": True,
        "with_d3d11": True,
        "with_d3d12": False,
        "with_metal": False,
        "with_server": True,
        "with_editor": True,
        "with_profiling": False,
        "gtest/*:build_gmock": True,
        "qt/*:shared": False,
        "qt/*:qtbase": True,
        "qt/*:qttools": True,
        "boost/*:header_only": False,
        "boost/*:magic_autolink": False,
    }

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["SAGA_ENABLE_VULKAN"] = self.options.with_vulkan
        tc.variables["SAGA_ENABLE_OPENGL"] = self.options.with_opengl
        tc.variables["SAGA_ENABLE_D3D11"] = self.options.with_d3d11
        tc.variables["SAGA_ENABLE_D3D12"] = self.options.with_d3d12
        tc.variables["SAGA_ENABLE_METAL"] = self.options.with_metal
        tc.variables["SAGA_ENABLE_SERVER"] = self.options.with_server
        tc.variables["SAGA_ENABLE_EDITOR"] = self.options.with_editor
        tc.variables["SAGA_ENABLE_PROFILING"] = self.options.with_profiling
        tc.variables["CMAKE_C_COMPILER_LAUNCHER"] = "sccache"
        tc.variables["CMAKE_CXX_COMPILER_LAUNCHER"] = "sccache"
        tc.variables["SAGA_BUILD_REPRODUCIBLE"] = "ON"
        tc.variables["SOURCE_DATE_EPOCH"] = "0"
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

        self._generate_sbom()

    def _generate_sbom(self):
        import subprocess

        commit = "unknown"
        try:
            result = subprocess.run(
                ["git", "rev-parse", "HEAD"],
                capture_output=True,
                text=True
            )
            commit = result.stdout.strip()
        except:
            pass

        sbom = {
            "name": self.name,
            "version": self.version,
            "commit": commit,
            "timestamp": datetime.utcnow().isoformat() + "Z",
            "dependencies": []
        }

        for dep in self.dependencies.host.values():
            sbom["dependencies"].append({
                "name": dep.ref.name,
                "version": str(dep.ref.version),
                "package_id": getattr(dep, 'package_id', None),
                "prev": getattr(dep, 'prev', None),
                "checksum": None
            })

        save(self, "sbom.json", json.dumps(sbom, indent=2))

    def configure(self):
        self.settings.compiler.cppstd = "20"
        if self.settings.os == "Linux":
            self.settings.compiler.libcxx = "libstdc++11"

    def validate(self):
        if self.settings.compiler.cppstd != "20":
            raise Exception("C++20 required")

        if self.options.with_d3d12 and self.settings.os != "Windows":
            raise Exception("D3D12 only available on Windows")

        if self.options.with_metal and self.settings.os not in ["Macos", "iOS"]:
            raise Exception("Metal only available on Apple platforms")