import os

# ---------------- ROOT FIND ----------------

def find_engine_root():
    current = os.path.abspath(os.path.dirname(__file__))

    while True:
        marker = os.path.join(current, "SagaEngineRoot.marker")
        if os.path.isfile(marker):
            return current

        parent = os.path.dirname(current)

        if parent == current:
            raise RuntimeError("Root not found or 'SagaEngineRoot.marker' is missing")

        current = parent


ROOT_DIR = find_engine_root()
OUTPUT_FILE = "engine_ai_snapshot.txt"

# ---------------- IGNORE CONFIG ----------------

IGNORE_DIRS = {
    ".git",
    "build",
    "Build",
    "bin",
    "cache",
    "Thirdparty",
    ".vscode",
    ".thirdparty_build",
    ".toolchain",
    "__pycache__"
}

IGNORE_FILES = {
    "SagaEngineRoot.marker",
    OUTPUT_FILE,
    "LICENSE"
}

IGNORE_EXTENSIONS = {
    ".log",
    ".tmp",
    ".cache",
    ".bin"
}

IGNORE_PATTERNS = [
    ".gitignore",
    ".DS_Store"
]

# ---------------- FILE TYPES ----------------

EXTENSIONS = {
    "headers": [".h", ".hpp"],
    "sources": [".cpp", ".c", ".cc", ".cxx"],
    "cmake": [".cmake", "CMakeLists.txt"],
    "others": [".txt", ".md", ".json", ".yaml", ".yml"]
}

# ---------------- HELPERS ----------------

def should_ignore(file_name):
    if file_name in IGNORE_FILES:
        return True

    _, ext = os.path.splitext(file_name)
    if ext in IGNORE_EXTENSIONS:
        return True

    for pattern in IGNORE_PATTERNS:
        if pattern in file_name:
            return True

    return False

# ---------------- FILE COLLECTION ----------------

def collect_files(root):
    categorized_files = {k: [] for k in EXTENSIONS}

    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in IGNORE_DIRS]

        for file in filenames:
            if should_ignore(file):
                continue

            full_path = os.path.join(dirpath, file)

            added = False
            for category, exts in EXTENSIONS.items():
                if any(file.endswith(ext) for ext in exts) or file in exts:
                    categorized_files[category].append(full_path)
                    added = True
                    break

            if not added:
                categorized_files["others"].append(full_path)

    return categorized_files

# ---------------- LINE COUNT ----------------

def count_lines(files):
    total = 0
    for f in files:
        try:
            with open(f, "r", encoding="utf-8", errors="ignore") as fh:
                total += sum(1 for _ in fh)
        except:
            continue
    return total

# ---------------- SNAPSHOT ----------------

def write_snapshot(output_path, categorized_files):
    with open(output_path, "w", encoding="utf-8", errors="ignore") as out:
        out.write("===== ENGINE AI SNAPSHOT =====\n\n")

        # FILE LIST
        for category in ["headers", "sources", "cmake", "others"]:
            files = categorized_files.get(category, [])
            out.write(f"===== {category.upper()} ({len(files)} files) =====\n")

            for f in sorted(files):
                rel_path = os.path.relpath(f, ROOT_DIR).replace(os.sep, "/")
                out.write(rel_path + "\n")

            out.write("\n")

        # FILE CONTENTS
        for category in ["headers", "sources", "cmake", "others"]:
            files = categorized_files.get(category, [])
            out.write(f"===== {category.upper()} CONTENTS =====\n")

            for f in sorted(files):
                rel_path = os.path.relpath(f, ROOT_DIR).replace(os.sep, "/")

                out.write("\n" + "=" * 90 + "\n")
                out.write(f"FILE: /{rel_path}\n")
                out.write("=" * 90 + "\n\n")

                try:
                    with open(f, "r", encoding="utf-8", errors="ignore") as fh:
                        out.write(fh.read())
                except Exception as e:
                    out.write(f"[ERROR READING FILE: {e}]\n")

            out.write("\n")

        # STATISTICS
        out.write("\n===== STATISTICS =====\n")

        for category in ["headers", "sources", "cmake", "others"]:
            files = categorized_files.get(category, [])
            lines = count_lines(files)
            out.write(f"{category}: {len(files)} files, {lines} lines\n")

        total_files = sum(len(v) for v in categorized_files.values())
        total_lines = sum(count_lines(v) for v in categorized_files.values())

        out.write(f"Total files: {total_files}\n")
        out.write(f"Total lines: {total_lines}\n")

# ---------------- MAIN ----------------

if __name__ == "__main__":
    files = collect_files(ROOT_DIR)
    write_snapshot(OUTPUT_FILE, files)
    print("Engine AI snapshot created.")