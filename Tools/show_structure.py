import os

ENGINE_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

IGNORE_DIRS = {
    ".git",
    "build",
    "bin",
    "Thirdparty",
    ".vscode",
    ".thirdparty_build",
    "__pycache__"
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
    print_tree(ENGINE_ROOT)
