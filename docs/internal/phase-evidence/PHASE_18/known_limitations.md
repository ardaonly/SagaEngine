# Phase 18 Known Limitations

- This phase is not verified.
- No public product claim should be derived from this file.
- Phase 18 is a current-boundary stabilization batch only. It does not add new
  SDE compiler features.
- The direct bootstrap command
  `python3 Tools/SystemDefinitionEngine/build.py --tests --jobs 1` does not
  complete in this local environment. Outside `nix-shell`, `conan` is not on
  `PATH`; inside `nix-shell`, the script's CMake configure step cannot resolve
  `nlohmann_json` after Conan install.
- The standalone Conan package/test path does pass with
  `conan create Tools/SystemDefinitionEngine -o "&:build_tests=True"
  --build=missing -s build_type=Release -c tools.build:jobs=1`.
- No SagaScript, Visual Blocks, runtime, server, editor, package/distribution,
  StarterArena gameplay, or CSharpScriptHost behavior is changed.
