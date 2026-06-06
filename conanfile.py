from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import save
import json
from datetime import datetime
from pathlib import Path

def _read_root_version():
    return Path(__file__).with_name("VERSION").read_text(encoding="utf-8").strip()

class SagaEngineConan(ConanFile):
    name = "sagaengine"
    version = _read_root_version()
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("nlohmann_json/3.11.3")
        self.requires("asio/1.30.2")
        self.requires("qt/6.8.3")
        # Linux-only window-system packages. Do not add them to Windows/macOS
        # graphs; those platforms neither build nor need Wayland/XKB.
        if str(self.settings.os) == "Linux":
            self.requires("wayland/1.24.0",   override=True)
            self.requires("xkbcommon/1.5.0",  override=True)
        # ConanCenter dropped all old libpq versions; only 14.22/15.17/16.13 remain.
        self.requires("libpq/16.13",      override=True)
        self.requires("libpqxx/7.9.0")
        self.requires("hiredis/1.2.0")
        self.requires("redis-plus-plus/1.3.12")
        self.requires("gtest/1.17.0")
        self.requires("sdl/2.30.2")
        self.requires("imgui/1.91.5-docking")
        self.requires("rmlui/4.4")
        # RmlUi's Conan recipe pins older transitive versions; keep it aligned
        # with the existing Qt graph instead of forking the graph.
        self.requires("freetype/2.13.2", override=True)
        self.requires("robin-hood-hashing/3.11.5", override=True)
        self.requires("rapidcheck/cci.20231215")
        self.requires("glm/0.9.9.8")

        # SDE is packaged independently (see Tools/SystemDefinitionEngine/conanfile.py).
        # Publish it once with `conan create Tools/SystemDefinitionEngine` and enable
        # this option to link the engine against SDE::Core.
        if self.options.with_sde:
            self.requires("sde/0.1.1")


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
        "with_sde": [True, False],
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
        "with_sde": False,
        "gtest/*:build_gmock": True,
        "qt/*:shared": False,
        "qt/*:qtbase": True,
        # SagaEditor links Qt Core/Gui/Widgets only. ConanCenter's Qt recipe
        # enables all "essential" modules by default, including qttools and
        # lupdate/clang support. Keep the graph to qtbase so the editor does
        # not build unused Qt modules or require LLVM development headers.
        "qt/*:essential_modules": False,
        "qt/*:qtdeclarative": False,
        "qt/*:qttools": False,
        "qt/*:qttranslations": False,
        "qt/*:qtdoc": False,
        "boost/*:header_only": False,
        "boost/*:magic_autolink": False,
    }

    def layout(self):
        cmake_layout(self)
        versioned_build = f"build/RelWithDebInfo-{self.version}"
        if self.options.with_sde:
            self.folders.build = f"{versioned_build}-sde"
            self.folders.generators = f"{versioned_build}-sde/generators"
        else:
            self.folders.build = versioned_build
            self.folders.generators = f"{versioned_build}/generators"

    def generate(self):
        tc = CMakeToolchain(self)
        # The repository owns CMakePresets.json. Conan's generated
        # CMakeUserPresets.json can include profile-specific environment macros
        # that CMake cannot expand directly, so keep Conan presets inside the
        # generators folder and let Forge drive the root presets.
        tc.user_presets_path = None
        tc.variables["SAGA_ENABLE_VULKAN"] = self.options.with_vulkan
        tc.variables["SAGA_ENABLE_OPENGL"] = self.options.with_opengl
        tc.variables["SAGA_ENABLE_D3D11"] = self.options.with_d3d11
        tc.variables["SAGA_ENABLE_D3D12"] = self.options.with_d3d12
        tc.variables["SAGA_ENABLE_METAL"] = self.options.with_metal
        tc.variables["SAGA_ENABLE_SERVER"] = self.options.with_server
        tc.variables["SAGA_ENABLE_EDITOR"] = self.options.with_editor
        tc.variables["SAGA_ENABLE_PROFILING"] = self.options.with_profiling
        tc.variables["SAGA_WITH_SDE"]         = bool(self.options.with_sde)
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
            "dependencies": [],
            "vendorDependencies": []
        }

        for dep in self.dependencies.host.values():
            sbom["dependencies"].append({
                "name": dep.ref.name,
                "version": str(dep.ref.version),
                "package_id": getattr(dep, 'package_id', None),
                "prev": getattr(dep, 'prev', None),
                "checksum": None
            })

        vendor_root = Path(__file__).with_name("Vendor") / "Diligent"
        vendor_commit = "unknown"
        try:
            result = subprocess.run(
                ["git", "-C", str(vendor_root), "rev-parse", "HEAD"],
                capture_output=True,
                text=True
            )
            vendor_commit = result.stdout.strip() or "unknown"
        except:
            pass

        sbom["vendorDependencies"].append({
            "name": "DiligentCore",
            "source": "git-submodule",
            "path": "Vendor/Diligent",
            "remote": "https://github.com/ardaonly/DiligentCore.git",
            "commit": vendor_commit,
            "scope": "graphics-core"
        })

        save(self, "sbom.json", json.dumps(sbom, indent=2))

    def configure(self):
        if str(self.settings.os) == "Linux":
            self.settings.compiler.libcxx = "libstdc++11"
            if str(self.settings.compiler) == "gcc":
                self.settings.compiler.cppstd = "gnu20"
                return

        self.settings.compiler.cppstd = "20"

    def validate(self):
        if str(self.settings.compiler.cppstd) not in ("20", "gnu20"):
            raise Exception("C++20 required")

        if self.options.with_d3d12 and self.settings.os != "Windows":
            raise Exception("D3D12 only available on Windows")

        if self.options.with_metal and self.settings.os not in ["Macos", "iOS"]:
            raise Exception("Metal only available on Apple platforms")
