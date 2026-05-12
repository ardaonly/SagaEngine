{ pkgs ? import <nixpkgs> {} }:

let
  conan = pkgs.conan.overrideAttrs (old: { doCheck = false; doInstallCheck = false; });

  # Conan pre-build hook applied to every package build.  Fixes two NixOS-
  # specific problems before any configure / cmake step runs:
  #
  #  1. autoconf configure scripts hardcode /bin/pwd which does not exist on
  #     NixOS (/bin/ only has sh).  This corrupts top_srcdir in the generated
  #     GNUmakefile, causing targets like `make generated-headers` to vanish.
  #     Fix: replace /bin/pwd with plain pwd (PATH-resolved).
  #
  #  2. CMake 4.x removed compatibility with cmake_minimum_required VERSION < 3.5
  #     and errors out immediately.  Many old ConanCenter recipes carry ancient
  #     CMakeLists.txt (e.g. spirv-cross 1.3.224.0 uses VERSION 2.8).
  #     Fix: bump any VERSION below 3.5 up to 3.5 in-place.
  nixosConanHook = pkgs.writeText "hook_nixos_fix.py" ''
    import os, re, stat

    def pre_build(conanfile):
        _install_cmake_shims(conanfile)
        seen = set()
        for folder in _candidates(conanfile):
            real = os.path.realpath(folder)
            if real in seen:
                continue
            seen.add(real)
            for root, _dirs, files in os.walk(real):
                if "configure" in files:
                    _fix_configure(conanfile, os.path.join(root, "configure"))
                for f in files:
                    if f == "CMakeLists.txt" or f.endswith(".cmake"):
                        _fix_cmake(conanfile, os.path.join(root, f))

    def _install_cmake_shims(conanfile):
        gen_dir = getattr(conanfile, "generators_folder", None) or ""
        if not gen_dir or not os.path.isdir(gen_dir):
            return
        shim = os.path.join(gen_dir, "CMakeDetermineCompileFeatures.cmake")
        if not os.path.exists(shim):
            try:
                with open(shim, "w") as fh:
                    fh.write("macro(cmake_determine_compile_features lang)\nendmacro()\n")
                conanfile.output.info("[nixos-fix] installed cmake shim: " + shim)
            except OSError as exc:
                conanfile.output.warning("[nixos-fix] cannot install cmake shim: " + str(exc))

    def _candidates(conanfile):
        dirs = []
        for attr in ("source_folder", "build_folder"):
            d = getattr(conanfile, attr, None) or ""
            if d and os.path.isdir(d):
                dirs.append(d)
                sibling = os.path.join(os.path.dirname(d.rstrip(os.sep)), "src")
                if os.path.isdir(sibling):
                    dirs.append(sibling)
        return dirs

    def _rewrite(conanfile, path, patched, original):
        if patched == original:
            return
        try:
            mode = os.stat(path).st_mode
            os.chmod(path, mode | stat.S_IWRITE)
            with open(path, "w", encoding="utf-8") as fh:
                fh.write(patched)
            os.chmod(path, mode)
            conanfile.output.info("[nixos-fix] patched " + path)
        except OSError as exc:
            conanfile.output.warning("[nixos-fix] cannot patch " + path + ": " + str(exc))

    def _fix_configure(conanfile, path):
        try:
            raw = open(path, "rb").read()
            text = raw.decode("utf-8", errors="replace")
            _rewrite(conanfile, path, text.replace("/bin/pwd", "pwd"), text)
        except OSError:
            pass

    def _fix_cmake(conanfile, path):
        try:
            text = open(path, encoding="utf-8", errors="replace").read()

            def _bump(m):
                parts = [int(x) for x in m.group(2).split(".")]
                while len(parts) < 2:
                    parts.append(0)
                if tuple(parts[:2]) < (3, 5):
                    return m.group(1) + "3.5"
                return m.group(0)

            patched = re.sub(
                r"(cmake_minimum_required\s*\(\s*VERSION\s+)(\d+\.\d+(?:\.\d+)?)",
                _bump,
                text,
                flags=re.IGNORECASE,
            )
            # Remove internal CMake module removed in CMake 4.x
            patched = re.sub(
                r"^\s*include\s*\(\s*CMakeDetermineCompileFeatures\s*\)\s*\n?",
                "",
                patched,
                flags=re.IGNORECASE | re.MULTILINE,
            )
            patched = re.sub(
                r"^\s*cmake_determine_compile_features\s*\([^)]*\)\s*\n?",
                "",
                patched,
                flags=re.IGNORECASE | re.MULTILINE,
            )
            _rewrite(conanfile, path, patched, text)
        except OSError:
            pass
  '';
in

pkgs.mkShell {
  name = "sagaengine-dev";

  packages = [
    conan
    pkgs.cmake
    pkgs.ninja
    pkgs.gcc14
    pkgs.sccache
    pkgs.python3
    pkgs.pkg-config
    pkgs.libGL
    pkgs.xorg.libX11
    pkgs.xorg.libXext
    pkgs.xorg.libXrandr
    pkgs.xorg.libXi
    pkgs.xorg.libXcursor
    pkgs.xorg.libXinerama
    pkgs.wayland
    pkgs.wayland-protocols
    pkgs.libxkbcommon
    pkgs.egl-wayland
    pkgs.xkeyboard_config
    pkgs.vulkan-loader
    pkgs.vulkan-headers
    pkgs.libglvnd
    pkgs.mesa
    # xorg/system Conan package requirements
    pkgs.xorg.libfontenc
    pkgs.xorg.libICE
    pkgs.xorg.libSM
    pkgs.xorg.libXrender
    pkgs.xorg.libXfixes
    pkgs.xorg.libxkbfile
    pkgs.xorg.libXScrnSaver
    pkgs.xorg.libXt
    pkgs.xorg.libXmu
    pkgs.xorg.libXpm
    pkgs.xorg.libXcomposite
    pkgs.xorg.libXdamage
    pkgs.xorg.libXtst
    pkgs.xorg.libXxf86vm
    pkgs.xorg.libXaw
    pkgs.xorg.libXv
    pkgs.xorg.libXvMC
    pkgs.xorg.libXres
    pkgs.xorg.libXpresent
    pkgs.xorg.libXau
    pkgs.xorg.libXdmcp
    pkgs.libuuid.dev
    # XCB — required by xorg/system Conan package (Qt6, SDL2)
    pkgs.xorg.libxcb
    pkgs.xorg.xcbutil
    pkgs.xorg.xcbutilimage
    pkgs.xorg.xcbutilkeysyms
    pkgs.xorg.xcbutilrenderutil
    pkgs.xorg.xcbutilwm
    pkgs.xorg.xcbutilcursor
  ];

  shellHook = ''
    export CC=gcc
    export CXX=g++

    # Make nix-provided system libraries visible to pkg-config (needed by
    # Conan's egl/system, opengl/system, and xorg/system packages).
    export PKG_CONFIG_PATH="${pkgs.libGL}/lib/pkgconfig:${pkgs.xorg.libX11}/lib/pkgconfig:${pkgs.xorg.libXext}/lib/pkgconfig:${pkgs.xorg.libXrandr}/lib/pkgconfig:${pkgs.xorg.libXi}/lib/pkgconfig:${pkgs.xorg.libXcursor}/lib/pkgconfig:${pkgs.xorg.libXinerama}/lib/pkgconfig:${pkgs.wayland}/lib/pkgconfig:${pkgs.wayland-protocols}/share/pkgconfig:${pkgs.libxkbcommon}/lib/pkgconfig:${pkgs.egl-wayland}/lib/pkgconfig:${pkgs.vulkan-loader}/lib/pkgconfig:${pkgs.libglvnd}/lib/pkgconfig:${pkgs.mesa}/lib/pkgconfig:${pkgs.xorg.libfontenc}/lib/pkgconfig:${pkgs.xorg.libICE}/lib/pkgconfig:${pkgs.xorg.libSM}/lib/pkgconfig:${pkgs.xorg.libXrender}/lib/pkgconfig:${pkgs.xorg.libXfixes}/lib/pkgconfig:${pkgs.xorg.libxkbfile}/lib/pkgconfig:${pkgs.xorg.libXScrnSaver}/lib/pkgconfig:${pkgs.xorg.libXt}/lib/pkgconfig:${pkgs.xorg.libXmu}/lib/pkgconfig:${pkgs.xorg.libXpm}/lib/pkgconfig:${pkgs.xorg.libXcomposite}/lib/pkgconfig:${pkgs.xorg.libXdamage}/lib/pkgconfig:${pkgs.xorg.libXtst}/lib/pkgconfig:${pkgs.xorg.libXxf86vm}/lib/pkgconfig:${pkgs.xorg.libXaw}/lib/pkgconfig:${pkgs.xorg.libXv}/lib/pkgconfig:${pkgs.xorg.libXvMC}/lib/pkgconfig:${pkgs.xorg.libXres}/lib/pkgconfig:${pkgs.xorg.libXpresent}/lib/pkgconfig:${pkgs.xorg.libXau}/lib/pkgconfig:${pkgs.xorg.libXdmcp}/lib/pkgconfig:${pkgs.libuuid.dev}/lib/pkgconfig:${pkgs.xorg.libxcb}/lib/pkgconfig:${pkgs.xorg.xcbutil}/lib/pkgconfig:${pkgs.xorg.xcbutilimage}/lib/pkgconfig:${pkgs.xorg.xcbutilkeysyms}/lib/pkgconfig:${pkgs.xorg.xcbutilrenderutil}/lib/pkgconfig:${pkgs.xorg.xcbutilwm}/lib/pkgconfig:${pkgs.xorg.xcbutilcursor}/lib/pkgconfig:''${PKG_CONFIG_PATH:-}"

    # Install the NixOS Conan hook.  Written to ~/.conan2/extensions/hooks/
    # which Conan 2.x picks up automatically for every build.
    # Remove first: the Nix store source is read-only, so cp would create a
    # read-only destination; subsequent shell entries would fail to overwrite it.
    _HOOKS_DIR="$HOME/.conan2/extensions/hooks"
    mkdir -p "$_HOOKS_DIR"
    rm -f "$_HOOKS_DIR/hook_nixos_fix.py"
    cp "${nixosConanHook}" "$_HOOKS_DIR/hook_nixos_fix.py"
    unset _HOOKS_DIR

    FORGE_BIN="$PWD/Tools/Forge/tool/bin"

    # Build forge if the binary is missing (first entry or after a clean).
    #
    # cmake's Makefile generator stores include paths as -I"..." (double-quoted).
    # When make expands the CXX_INCLUDES variable into a recipe and the nix gcc
    # wrapper processes the result, the quoting breaks for paths containing spaces
    # (this project lives in "SagaEngine Versions 2/..."). cmake sets
    # CMAKE_CURRENT_SOURCE_DIR to HERE's resolved absolute path, so the -I flag
    # always carries the full path with spaces.
    # Fix: copy the forge source tree to /tmp (no spaces) so cmake generates
    # -I/tmp/forge-src/include instead. Build there, copy binary back.
    if [ ! -x "$FORGE_BIN/forge" ]; then
      echo "[sagaengine-dev]  forge binary not found — building (via /tmp to avoid path-with-spaces quoting bug)..."
      TMP_FORGE="/tmp/sagaengine-forge-src"
      rm -rf "$TMP_FORGE"
      # Exclude build/ and bin/ so no stale CMakeCache.txt is carried over.
      cp -r "$PWD/Tools/Forge/tool" "$TMP_FORGE"
      rm -rf "$TMP_FORGE/build" "$TMP_FORGE/bin"
      ( cd "$TMP_FORGE" && python3 build.py )
      if [ -f "$TMP_FORGE/bin/forge" ]; then
        mkdir -p "$FORGE_BIN"
        cp "$TMP_FORGE/bin/forge" "$FORGE_BIN/forge"
        chmod +x "$FORGE_BIN/forge"
        echo "[sagaengine-dev]  forge staged to $FORGE_BIN/forge"
      else
        echo "[sagaengine-dev]  ERROR: forge build failed. See output above."
      fi
      rm -rf "$TMP_FORGE"
    fi

    if [ -x "$FORGE_BIN/forge" ]; then
      export PATH="$FORGE_BIN:$PATH"
    else
      echo "[sagaengine-dev]  ERROR: forge build failed. Check output above."
    fi

    echo "[sagaengine-dev] ── tool versions ──────────────────────────────────"
    echo "[sagaengine-dev]  cmake   : $(cmake   --version 2>/dev/null | head -1 || echo 'NOT FOUND')"
    echo "[sagaengine-dev]  conan   : $(conan   --version 2>/dev/null          || echo 'NOT FOUND')"
    echo "[sagaengine-dev]  ninja   : $(ninja   --version 2>/dev/null          || echo 'NOT FOUND')"
    echo "[sagaengine-dev]  gcc     : $(gcc     --version 2>/dev/null | head -1 || echo 'NOT FOUND')"
    echo "[sagaengine-dev]  g++     : $(g++     --version 2>/dev/null | head -1 || echo 'NOT FOUND')"
    echo "[sagaengine-dev]  sccache : $(sccache --version 2>/dev/null          || echo 'NOT FOUND')"
    echo "[sagaengine-dev]  forge   : $(forge   --version 2>/dev/null          || echo 'NOT FOUND')"
    echo "[sagaengine-dev] ───────────────────────────────────────────────────"

    # Conan 2.x resolves profile names from ~/.conan2/profiles/.
    # Create the default build profile if missing.
    if [ ! -f "$HOME/.conan2/profiles/default" ]; then
      echo "[sagaengine-dev]  conan default profile missing — running 'conan profile detect'"
      conan profile detect
      echo "[sagaengine-dev]  created: $HOME/.conan2/profiles/default"
    else
      echo "[sagaengine-dev]  conan default profile: $HOME/.conan2/profiles/default"
    fi

    # Symlink all project profiles into ~/.conan2/profiles/ so they can be
    # referenced by name (e.g. --profile linux-gcc) without a path prefix.
    mkdir -p "$HOME/.conan2/profiles"
    for _profile in "$PWD/profiles/"*; do
      _name=$(basename "$_profile")
      _target="$HOME/.conan2/profiles/$_name"
      if [ ! -e "$_target" ] || [ -L "$_target" ]; then
        ln -sf "$_profile" "$_target"
        echo "[sagaengine-dev]  conan profile linked: $_name → $_target"
      fi
    done

    echo "[sagaengine-dev] ── ready ───────────────────────────────────────────"
  '';
}
