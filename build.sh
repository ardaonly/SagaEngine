#!/bin/bash
set -euo pipefail  # FAIL-FAST

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ============================================================================
# CONFIGURATION
# ============================================================================
TOOLCHAIN_DIR="$SCRIPT_DIR/.toolchain"
VERSION_FILE="$TOOLCHAIN_DIR/version.json"
LOCKFILE_PATH="$SCRIPT_DIR/conan.lock"
LOCKFILE_HASH_PATH="$SCRIPT_DIR/conan.lock.sha256"
PYTHON_PATH_FILE="$TOOLCHAIN_DIR/python_path.txt"

DEFAULT_PROFILE="linux-gcc-13"
DEFAULT_PRESET="linux-gcc-13"
CI_MODE="${SAGA_CI_MODE:-false}"

# ============================================================================
# HELPER FUNCTIONS
# ============================================================================
detect_python() {
    if [ -f "$PYTHON_PATH_FILE" ]; then
        local saved_path
        saved_path=$(cat "$PYTHON_PATH_FILE")
        if [ -f "$saved_path" ] && "$saved_path" --version &>/dev/null; then
            echo "$saved_path"
            return 0
        fi
    fi
    
    for cmd in python3 python py; do
        if command -v "$cmd" &>/dev/null; then
            if "$cmd" -c "import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)" 2>/dev/null; then
                echo "$cmd"
                return 0
            fi
        fi
    done
    echo ""
    return 1
}

test_lockfile_required() {
    local force="$1"
    
    if [ "$force" = "true" ]; then
        echo "  --Force: Lockfile validation skipped"
        return 0
    fi
    
    if [ ! -f "$LOCKFILE_PATH" ]; then
        echo ""
        echo "========================================="
        echo "  CRITICAL: conan.lock NOT FOUND"
        echo "========================================="
        echo ""
        echo "Lockfile is REQUIRED for deterministic builds."
        echo "Options:"
        echo "  1. Commit conan.lock to repository"
        echo "  2. Run: ./build.sh setup --Force (first time only)"
        echo "  3. Contact team for valid lockfile"
        echo ""
        exit 1
    fi
    return 0
}

test_lockfile_integrity() {
    local force="$1"
    
    if [ "$force" = "true" ]; then
        echo "  --Force: Lockfile integrity check skipped"
        return 0
    fi
    
    echo "  Validating lockfile integrity..."
    
    local python_cmd
    python_cmd=$(detect_python)
    if [ -z "$python_cmd" ]; then
        echo "  ⚠ Python not found, skipping hash validation"
        return 0
    fi
    
    local validation_script="$SCRIPT_DIR/Tools/scripts/validate_lockfile.py"
    if [ -f "$validation_script" ]; then
        if ! "$python_cmd" "$validation_script" \
            --lockfile "$LOCKFILE_PATH" \
            --version-json "$VERSION_FILE" \
            --strict; then
            echo ""
            echo "========================================="
            echo "  LOCKFILE INTEGRITY FAILED"
            echo "========================================="
            echo ""
            echo "Run './build.sh setup --Force' to regenerate"
            exit 1
        fi
        echo "  ✓ Lockfile integrity verified"
    fi
    return 0
}

get_build_policy() {
    local command="$1"
    local force="$2"
    
    if [ "$CI_MODE" = "true" ]; then
        echo "never"
        return
    fi
    
    if [ "$command" = "setup" ] || [ "$force" = "true" ]; then
        echo "missing"
        return
    fi
    
    echo "missing"
}

check_prerequisites() {
    echo ""
    echo "=== Checking Prerequisites ==="
    
    if ! command -v cmake &>/dev/null; then
        echo "  ✗ CMake: NOT FOUND"
        exit 1
    fi
    echo "  ✓ CMake: $(cmake --version | head -1)"
    
    if ! command -v ninja &>/dev/null; then
        echo "  ✗ Ninja: NOT FOUND"
        exit 1
    fi
    echo "  ✓ Ninja: $(ninja --version)"
    
    local python_cmd
    python_cmd=$(detect_python)
    if [ -z "$python_cmd" ]; then
        echo "  ✗ Python: NOT FOUND"
        echo "  Run: ./bootstrap.sh"
        exit 1
    fi
    echo "  ✓ Python: $($python_cmd --version)"
    
    echo ""
}

# ============================================================================
# BUILD COMMANDS
# ============================================================================
cmd_setup() {
    echo ""
    echo "=== SagaEngine Setup ==="
    echo "Profile: $PROFILE"
    echo "Preset:  $PRESET"
    echo "Force:   $FORCE"
    echo ""
    
    if [ ! -f "$VERSION_FILE" ]; then
        echo "  ✗ version.json not found: $VERSION_FILE"
        exit 1
    fi
    
    if [ -f "$LOCKFILE_PATH" ] && [ "$FORCE" != "true" ]; then
        echo "  ⚠ Lockfile exists. Use --Force to regenerate."
        read -p "  Regenerate anyway? (y/N): " confirm
        if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
            echo "  Setup cancelled."
            return 0
        fi
    fi
    
    local build_policy
    build_policy=$(get_build_policy "setup" "$FORCE")
    
    echo ""
    echo "=== Installing Dependencies ==="
    echo "Build Policy: $build_policy"
    echo ""
    
    local conan_cmd="conan"
    
    if [ "$FORCE" = "true" ] && [ -f "$LOCKFILE_PATH" ]; then
        echo "  Removing existing lockfile..."
        rm -f "$LOCKFILE_PATH" "$LOCKFILE_HASH_PATH"
    fi
    
    if [ ! -f "$LOCKFILE_PATH" ]; then
        echo "  Generating new lockfile..."
        if ! $conan_cmd lock create conanfile.py \
            --profile=profiles/$PROFILE \
            --lockfile-out=$LOCKFILE_PATH \
            --build=$build_policy; then
            echo "  ✗ Failed to create lockfile"
            exit 1
        fi
        echo "  ✓ Lockfile created"
    fi
    
    echo "  Installing dependencies..."
    if ! $conan_cmd install . \
        --profile=profiles/$PROFILE \
        --lockfile=$LOCKFILE_PATH \
        --build=$build_policy \
        --output-folder=Build/$PRESET; then
        echo "  ✗ Failed to install dependencies"
        exit 1
    fi
    
    echo "  Configuring CMake..."
    if ! cmake --preset $PRESET; then
        echo "  ✗ Failed to configure CMake"
        exit 1
    fi
    
    echo ""
    echo "=== Setup Complete ==="
    echo "Lockfile: $LOCKFILE_PATH"
    echo "Next: ./build.sh build"
}

cmd_build() {
    echo ""
    echo "=== Building SagaEngine ==="
    echo "Preset: $PRESET"
    echo "Config: $CONFIG"
    echo "CI Mode: $CI_MODE"
    echo ""
    
    test_lockfile_required "$FORCE"
    test_lockfile_integrity "$FORCE"
    
    local build_policy
    build_policy=$(get_build_policy "build" "$FORCE")
    echo "Build Policy: $build_policy"
    echo ""
    
    export SCCACHE_DIR="$SCRIPT_DIR/cache/sccache"
    mkdir -p "$SCCACHE_DIR"
    
    if ! cmake --build --preset $PRESET --config $CONFIG; then
        echo ""
        echo "========================================="
        echo "  BUILD FAILED"
        echo "========================================="
        exit 1
    fi
    
    echo ""
    echo "=== Build Complete ==="
    echo "Binaries: Bin/$CONFIG/"
}

cmd_test() {
    echo ""
    echo "=== Running Tests ==="
    
    test_lockfile_required "false"
    
    local BUILD_DIR="Build/$PRESET"
    if [ ! -d "$BUILD_DIR" ]; then
        echo "  ✗ Build directory not found: $BUILD_DIR"
        exit 1
    fi
    
    local nproc=4
    if command -v nproc &>/dev/null; then
        nproc=$(nproc)
    fi
    
    if ! ctest --test-dir "$BUILD_DIR" \
        --output-on-failure \
        --build-config $CONFIG \
        --parallel $nproc; then
        echo ""
        echo "========================================="
        echo "  SOME TESTS FAILED"
        echo "========================================="
        exit 1
    fi
    
    echo ""
    echo "=== All Tests Passed ==="
}

cmd_clean() {
    echo ""
    echo "=== Cleaning ==="
    rm -rf Build/ Bin/ Install/ packages/ cache/sccache/*
    echo "=== Clean Complete ==="
}

show_help() {
    cat << EOF
=========================================
  SagaEngine Build System v3.0
=========================================

Commands:
  setup     Install dependencies and configure
  build     Build the project
  test      Run tests
  clean     Clean build artifacts
  help      Show this help

Options:
  -p, --profile PROFILE    Build profile (default: $DEFAULT_PROFILE)
  -c, --config CONFIG      Build config (default: RelWithDebInfo)
  -Force                   Force regeneration (setup only)

Environment:
  SAGA_CI_MODE=true        Enable CI mode (--build=never)

EOF
}

# ============================================================================
# MAIN
# ============================================================================
PROFILE="$DEFAULT_PROFILE"
PRESET="$DEFAULT_PRESET"
CONFIG="RelWithDebInfo"
COMMAND=""
FORCE="false"

while [[ $# -gt 0 ]]; do
    case $1 in
        setup|build|test|clean|help)
            COMMAND="$1"
            shift
            ;;
        -p|--profile)
            PROFILE="$2"
            PRESET="$2"
            shift 2
            ;;
        -c|--config)
            CONFIG="$2"
            shift 2
            ;;
        -Force|--force)
            FORCE="true"
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

COMMAND="${COMMAND:-help}"

echo "SagaEngine Build System v3.0"
echo "Python: $(detect_python)"
echo ""

if [ "$COMMAND" != "help" ] && [ "$COMMAND" != "clean" ]; then
    check_prerequisites
fi

case $COMMAND in
    setup) cmd_setup ;;
    build) cmd_build ;;
    test) cmd_test ;;
    clean) cmd_clean ;;
    help) show_help ;;
    *)
        echo "Unknown command: $COMMAND"
        show_help
        exit 1
        ;;
esac