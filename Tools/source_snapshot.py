import os

ROOT_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..")
)
OUTPUT_FILE = "engine_bundle.txt"

HEADER_EXT = ".h"
SOURCE_EXT = ".cpp"

IGNORE_DIRS = {
    "Thirdparty",
    ".thirdparty_build",
    ".git",
    "build",
    "bin",
    "out",
    ".vs"
}

def collect_files(root):
    headers = []
    sources = []

    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [
            d for d in dirnames
            if d not in IGNORE_DIRS
        ]

        for file in filenames:
            full_path = os.path.join(dirpath, file)

            if file.endswith(HEADER_EXT):
                headers.append(full_path)
            elif file.endswith(SOURCE_EXT):
                sources.append(full_path)

    return headers, sources


def write_bundle(output_path, header_files, source_files):
    with open(output_path, "w", encoding="utf-8", errors="ignore") as out:
        out.write("===== ENGINE SOURCE CONTEXT BUNDLE =====\n")
        out.write("Purpose: Analysis / AI Context\n\n")

        def write_section(title, files):
            out.write(f"\n\n===== {title} =====\n")
            for path in files:
                out.write("\n" + "=" * 90 + "\n")
                relative_path = os.path.relpath(path, ROOT_DIR)
                out.write(f"FILE: /{relative_path.replace(os.sep, '/')}\n")
                out.write("=" * 90 + "\n\n")

                try:
                    with open(path, "r", encoding="utf-8", errors="ignore") as f:
                        out.write(f.read())
                except Exception as e:
                    out.write(f"\n[ERROR READING FILE: {e}]\n")

        write_section("HEADER FILES (.h)", header_files)
        write_section("SOURCE FILES (.cpp)", source_files)


if __name__ == "__main__":
    headers, sources = collect_files(ROOT_DIR)

    headers.sort()
    sources.sort()

    write_bundle(OUTPUT_FILE, headers, sources)

    print("Context bundle created.")
    print(f"Headers: {len(headers)}")
    print(f"Sources: {len(sources)}")