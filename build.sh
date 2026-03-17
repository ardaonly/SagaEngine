#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CONAN_VERSION="2.0.17"
DEFAULT_PROFILE="linux-gcc-13"
DEFAULT_PRESET="linux-gcc-13"

# ─────────────────────────────────────────────────────────────
# PYTHON DETECTION (ROBUST)
# ─────────────────────────────────────────────────────────────
detect_python() {
    local python_cmd=""
    
    # Try in order: python3, python, py
    for cmd in python3 python py; do
        if command -v "$cmd" &> /dev/null; then
            if "$cmd" -c "import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)" 2>/dev/null; then
                python_cmd="$cmd"
                break
            fi
        fi
    done
    
    if [ -z "$python_cmd" ]; then
        echo "ERROR: Python 3.8+ not found"
        echo "Please install Python 3.8 or higher"
        exit 1
    fi
    
    echo "$python_cmd"
}

detect_pip() {
    local python_cmd="$1"
    local pip_cmd=""
    
    # Try: python -m pip, pip, pip3
    for cmd in "$python_cmd -m pip" pip3 pip; do
        if $cmd --version &> /dev/null; then
            pip_cmd="$cmd"
            break
        fi
    done
    
    if [ -z "$pip_cmd" ]; then
        echo "ERROR: pip not found"
        echo "Please install pip for $python_cmd"
        exit 1
    fi
    
    echo "$pip_cmd"
}

PYTHON_CMD=$(detect_python)
PIP_CMD=$(detect_pip "$PYTHON_CMD")

# ─────────────────────────────────────────────────────────────
# HELPER FUNCTIONS
# ─────────────────────────────────────────────────────────────
show_help() {
    cat << EOF
SagaEngine Build System

Usage: ./build.sh [command] [options]

Commands:
    setup       Install dependencies and configure
    build       Build the project
    test        Run tests
    clean       Clean build artifacts
    package     Create release package
    docker      Build in Docker container
    help        Show this help

Options:
    -p, --profile PROFILE    Build profile (default: $DEFAULT_PROFILE)
    -c, --config CONFIG      Build config (default: RelWithDebInfo)
    -v, --verbose            Verbose output
    -h, --help               Show this help

Examples:
    ./build.sh setup
    ./build.sh build -p windows-msvc-14.38
    ./build.sh test -c Release
    ./build.sh docker

Python: $PYTHON_CMD
Pip:    $PIP_CMD
EOF
}

check_prerequisites() {
    echo "Checking prerequisites..."
    
    if ! command -v cmake &> /dev/null; then
        echo "ERROR: CMake required"
        echo "Install: https://cmake.org/download/"
        exit 1
    fi
    
    if ! command -v ninja &> /dev/null; then
        echo "ERROR: Ninja required"
        echo "Install: https://ninja-build.org/"
        exit 1
    fi
    
    # Check Conan, install if missing
    if ! $PYTHON_CMD -c "import conan" &> /dev/null; then
        echo "Installing Conan $CONAN_VERSION..."
        $PIP_CMD install --user --quiet conan==$CONAN_VERSION
    fi
    
    echo "Prerequisites OK"
}

verify_lockfile() {
    if [ ! -f "conan.lock" ]; then
        echo "ERROR: conan.lock not found. Run './build.sh setup' first."
        exit 1
    fi
}

# ─────────────────────────────────────────────────────────────
# BUILD COMMANDS
# ─────────────────────────────────────────────────────────────
cmd_setup() {
    echo "=== SagaEngine Setup ==="
    echo "Profile: $PROFILE"
    echo "Preset:  $PRESET"
    echo ""
    
    if [ ! -f "conan.lock" ]; then
        echo "Generating lockfile..."
        conan lock create conanfile.py \
            --profile=profiles/$PROFILE \
            --lockfile-out=conan.lock \
            --build=missing
    fi
    
    echo "Installing dependencies..."
    conan install . \
        --profile=profiles/$PROFILE \
        --lockfile=conan.lock \
        --build=missing \
        --output-folder=Build/$PRESET
    
    echo "Configuring build..."
    cmake --preset $PRESET
    
    echo ""
    echo "=== Setup Complete ==="
    echo "Run: ./build.sh build"
}

cmd_build() {
    verify_lockfile
    echo "=== Building SagaEngine ==="
    echo "Preset: $PRESET"
    echo "Config: $CONFIG"
    echo ""
    
    export SCCACHE_DIR="$SCRIPT_DIR/cache/sccache"
    mkdir -p "$SCCACHE_DIR"
    
    cmake --build --preset $PRESET --config $CONFIG
    
    echo ""
    echo "=== Build Complete ==="
}

cmd_test() {
    verify_lockfile
    echo "=== Running Tests ==="
    echo ""
    
    BUILD_DIR="Build/$PRESET"
    
    local nproc=4
    if command -v nproc &> /dev/null; then
        nproc=$(nproc)
    elif command -v sysctl &> /dev/null; then
        nproc=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    fi
    
    ctest --test-dir "$BUILD_DIR" \
        --output-on-failure \
        --build-config $CONFIG \
        --parallel $nproc
    
    echo ""
    echo "=== Tests Complete ==="
}

cmd_clean() {
    echo "=== Cleaning ==="
    rm -rf Build/
    rm -rf Bin/
    rm -rf Install/
    rm -rf packages/
    rm -rf cache/sccache/*
    echo "=== Clean Complete ==="
}

cmd_package() {
    verify_lockfile
    echo "=== Creating Package ==="
    
    SAGA_VERSION=$(cat BUILD_VERSION 2>/dev/null || echo "0.1.0")
    PACKAGE_NAME="SagaEngine-${SAGA_VERSION}-${PROFILE}"
    PACKAGE_DIR="packages/$PACKAGE_NAME"
    
    mkdir -p "$PACKAGE_DIR"
    cp -r Bin/$CONFIG/* "$PACKAGE_DIR/" 2>/dev/null || true
    cp Build/$PRESET/sbom.json "$PACKAGE_DIR/" 2>/dev/null || true
    
    cd packages
    tar -czf "$PACKAGE_NAME.tar.gz" "$PACKAGE_NAME"
    cd ..
    
    echo "Package: packages/$PACKAGE_NAME.tar.gz"
    echo "=== Package Complete ==="
}

cmd_docker() {
    echo "=== Building in Docker ==="
    
    if ! command -v docker &> /dev/null; then
        echo "ERROR: Docker required"
        exit 1
    fi
    
    docker build -t sagaengine:build .
    docker run --rm -v "$SCRIPT_DIR:/saga" sagaengine:build ./build.sh build -p $PROFILE
    
    echo "=== Docker Build Complete ==="
}

# ─────────────────────────────────────────────────────────────
# MAIN
# ─────────────────────────────────────────────────────────────
PROFILE="$DEFAULT_PROFILE"
PRESET="$DEFAULT_PRESET"
CONFIG="RelWithDebInfo"
COMMAND=""

while [[ $# -gt 0 ]]; do
    case $1 in
        setup|build|test|clean|package|docker|help)
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
        -v|--verbose)
            set -x
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

echo "SagaEngine Build System"
echo "Python: $PYTHON_CMD"
echo "Pip:    $PIP_CMD"
echo ""

check_prerequisites

case $COMMAND in
    setup) cmd_setup ;;
    build) cmd_build ;;
    test) cmd_test ;;
    clean) cmd_clean ;;
    package) cmd_package ;;
    docker) cmd_docker ;;
    help) show_help ;;
    *)
        echo "Unknown command: $COMMAND"
        show_help
        exit 1
        ;;
esac