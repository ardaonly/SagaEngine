import os

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

IGNORE_DIRS = {
    ".git",
    "build",
    "Build",
    "cache",
    "bin",
    "Thirdparty",
    ".vscode",
    ".thirdparty_build",
    "__pycache__",
    ".toolchain"
}

def print_tree(path, prefix=""):
    items = sorted(os.listdir(path))
    items = [i for i in items if i not in IGNORE_DIRS]

    for index, item in enumerate(items):
        full_path = os.path.join(path, item)
        is_last = index == len(items) - 1

        connector = "└── " if is_last else "├── "
        print(prefix + connector + item)

        if os.path.isdir(full_path):
            extension = "    " if is_last else "│   "
            print_tree(full_path, prefix + extension)

if __name__ == "__main__":
    print("ENGINE STRUCTURE:\n")
    print_tree(ROOT_DIR)
