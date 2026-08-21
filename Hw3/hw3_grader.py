import argparse
import subprocess
import os
import shutil
import time
import tree


STUDENT_EXEC_NAME = "mergeext2fs"
GRADER_EXEC_NAME = "grader"
STUDENT_TAR_NAME = "hw3.tar.gz"
TESTCASE_FOLDER = "./testcases"
STUDENT_FOLDER = "./student"
OUTPUT_FOLDER = "./output"
ALLOWED_EXTENSIONS = {'.c', '.cpp', '.h', '.hpp', '.inl'}
ALLOWED_FILES = {'Makefile', STUDENT_TAR_NAME}

TREE_WEIGHT = 10.0
TREE_MERGED_WEIGHT = 10.0
DIR_WEIGHT = 15.0
CONTENT_WEIGHT = 20.0
METADATA_WEIGHT = 15.0
BITMAP_WEIGHT = 10.0
GROUP_WEIGHT = 10.0
SUPER_WEIGHT = 5.0
BACKUP_WEIGHT = 5.0


def switch_dir(new_dir: str) -> str:
    current_directory = os.getcwd()
    os.chdir(new_dir)
    return current_directory


def run_subprocess(args):
    process = subprocess.run(args, capture_output=True)
    success = process.returncode == 0
    if not success:
        print(f"{process.args}")
        print(f"{process.returncode}\n")
        print(f"{process.stdout}\n")
        print(f"{process.stderr}\n")
    return success


class TestCase:
    def __init__(self, name: str, directory: str, base_img: str, branchA: str, branchB: str, expected: str):
        self.name: str = name
        if len(base_img) > 0:
            self.base_img: str = str(os.path.join(directory, base_img))
        else:
            self.base_img: str = ""

        if len(branchA) > 0:
            self.branchA: str = str(os.path.join(directory, branchA))
        else:
            self.branchA: str = ""

        if len(branchB) > 0:
            self.branchB: str = str(os.path.join(directory, branchB))
        else:
            self.branchB: str = ""

        if len(expected) > 0:
            self.expected: str = str(os.path.join(directory, expected))
        else:
            self.expected: str = ""


def init_testcases():
    case_dict = {}
    if TESTCASE_FOLDER.startswith("/"):
        test_dir = TESTCASE_FOLDER
    else:
        test_dir = os.path.abspath(TESTCASE_FOLDER)
    files = os.listdir(test_dir)
    for file in files:
        split_name = file.rsplit("_", 1)
        if len(split_name) == 1:
            split_name = split_name[0].rsplit(".", 1)
        case_name, case_type = split_name
        case_type = case_type.lower()
        case = case_dict.get(case_name, {"base": "", "A": "", "B": "", "exp": ""})
        if case_type == "base.img" or case_type == "img":
            case["base"] = file
        elif case_type == "a.img" or case_type == "brancha.img":
            case["A"] = file
        elif case_type == "b.img" or case_type == "branchb.img":
            case["B"] = file
        elif case_type == "expected.txt" or case_type == "tree.txt" or case_type == "txt":
            case["exp"] = file
        case_dict[case_name] = case
    testcases = {}
    for case_name in case_dict.keys():
        case = case_dict[case_name]
        testcases[case_name] = TestCase(case_name, test_dir, case["base"], case["A"], case["B"], case["exp"])
    return testcases


def extract_tar():
    current_directory = switch_dir(STUDENT_FOLDER)
    result = run_subprocess(["tar", "-xf", STUDENT_TAR_NAME])
    if not result:
        print(f"Uncompress failed")
    switch_dir(current_directory)
    return result


def remove_others(nuke=False):
    current_directory = switch_dir(STUDENT_FOLDER)

    unauthorized_files = []
    subdirectories = []

    # os.walk traverses the directory tree top-down
    for root, dirs, files in os.walk("."):
        # If we are at the top level, record any subdirectories found
        if root == ".":
            subdirectories.extend(dirs)

        for file in files:
            ext = os.path.splitext(file)[1].lower()
            if ext not in ALLOWED_EXTENSIONS and file not in ALLOWED_FILES:
                file_path = os.path.join(root, file)
                unauthorized_files.append(file_path)

    if not nuke:
        if unauthorized_files or subdirectories:
            print("WARNING: The following items will be DELETED during actual grading!")

            if subdirectories:
                print("\nSubfolders:")
                for d in subdirectories:
                    print(f"  - {d}/")

            if unauthorized_files:
                print("\nFiles:")
                for f in unauthorized_files:
                    print(f"  - {os.path.normpath(f)}")

            switch_dir(current_directory)
            return False

        switch_dir(current_directory)
        return True

    # --- NUKE MODE (Destructive) ---
    else:
        no_problem = True
        # 1. Nuke isolated unauthorized files
        for f in unauthorized_files:
            try:
                os.remove(f)
                print(f"Deleted File: {os.path.normpath(f)}")
            except Exception as e:
                print(f"Failed to delete {os.path.normpath(f)}: {e}")
                no_problem = False

        for d in subdirectories:
            dir_path = os.path.join(".", d)
            if os.path.exists(dir_path):
                try:
                    shutil.rmtree(dir_path)
                    print(f"Deleted Subfolder: {d}/")
                except Exception as e:
                    print(f"Failed to delete subfolder {d}/: {e}")
                    no_problem = False

        switch_dir(current_directory)
        return no_problem


def perform_check():
    current_directory = switch_dir(STUDENT_FOLDER)
    try:
        dry_run = subprocess.run(["make", "-n", "all"], capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError:
        print("Fatal: Makefile failed to execute or target \'all\' missing.")
        switch_dir(current_directory)
        return False
    compilers = ["gcc", "g++", "cc", "c++"]
    required_flags = ["-Werror=unreachable-code", "-Werror=return-type"]
    missing_flags = []
    found_compilation = False

    for line in dry_run.stdout.splitlines():
        if any(compiler in line.split() for compiler in compilers):
            found_compilation = True
            for flag in required_flags:
                if flag not in line.split() and flag not in missing_flags:
                    missing_flags.append(flag)

    if not found_compilation:
        print("Fatal: No C/C++ compiler invocation found in Makefile.")
        switch_dir(current_directory)
        return False

    if missing_flags:
        print(f"MAKEFILE WARNING: You are missing required strict flags ({', '.join(missing_flags)}). You MUST fix this before submission or you will lose points!")
        switch_dir(current_directory)
        return False

    result = run_subprocess([
        "cppcheck",
        "--enable=unusedFunction",
        "--error-exitcode=1",
        "--quiet",
        "."
    ])

    if not result:
        print(f"Extraneous or unused function detected. You MUST fix this before submission or you will lose points!")

    switch_dir(current_directory)
    return result


def make_file():
    current_directory = switch_dir(STUDENT_FOLDER)
    result = run_subprocess(["make", "all"])
    if not result:
        print(f"Make failed")
        switch_dir(current_directory)
        return False
    if not os.path.exists(STUDENT_EXEC_NAME):
        print(f"No File {STUDENT_EXEC_NAME}")
        switch_dir(current_directory)
        return False
    switch_dir(current_directory)
    return True


def run_testcase(testcase: TestCase):
    print(testcase.name)

    student_base = os.path.join(os.path.abspath(STUDENT_FOLDER), "base.img")
    student_a = os.path.join(os.path.abspath(STUDENT_FOLDER), "branchA.img")
    student_b = os.path.join(os.path.abspath(STUDENT_FOLDER), "branchB.img")

    shutil.copyfile(testcase.base_img, student_base)
    shutil.copyfile(testcase.branchA, student_a)
    shutil.copyfile(testcase.branchB, student_b)

    current_directory = switch_dir(STUDENT_FOLDER)
    now = time.time()
    tree_out = ""
    try:
        cmd = [f"./{STUDENT_EXEC_NAME}", "base.img", "branchA.img", "branchB.img"]
        process = subprocess.run(cmd, capture_output=True, timeout=230)
        tree_out = process.stdout.decode("utf-8")
    except Exception as e:
        print(f"Testcase {testcase.name} Run Failed: {e}")
        os.remove("base.img")
        os.remove("branchA.img")
        os.remove("branchB.img")
        return 0

    later = time.time()
    switch_dir(current_directory)

    tree_location = os.path.join(os.path.abspath(OUTPUT_FOLDER), f"{testcase.name}_tree.txt")
    image_location = os.path.join(os.path.abspath(OUTPUT_FOLDER), f"{testcase.name}_out.img")
    with open(tree_location, "w") as file:
        file.write(tree_out)

    shutil.copyfile(os.path.join(os.path.abspath(STUDENT_FOLDER), "base.img"), image_location)

    tree_score, tree_score_new = tree.get_tree_score(tree_location, testcase.expected)

    os.remove(student_base)
    os.remove(student_a)
    os.remove(student_b)

    grade_out = ""
    out_location = os.path.join(os.path.abspath(OUTPUT_FOLDER), f"{testcase.name}_details.txt")

    try:
        grader_cmd = [
            f"./{GRADER_EXEC_NAME}",
            testcase.base_img,
            testcase.branchA,
            testcase.branchB,
            image_location,
            testcase.expected,
            out_location
        ]
        process = subprocess.run(grader_cmd, capture_output=True, timeout=230)

        if process.returncode != 0:
            print(f"Testcase {testcase.name} Grader Error")
            print(f"{process.args}")
            print(f"{process.returncode}\n")
            print(f"{process.stdout.decode('utf-8')}\n")
            print(f"{process.stderr.decode('utf-8')}\n")
            print(f"{tree_score * TREE_WEIGHT}|{tree_score_new * TREE_MERGED_WEIGHT}|0.0|0.0|0.0|0.0|0.0|0.0|0.0")
            return tree_score * TREE_WEIGHT + tree_score_new * TREE_MERGED_WEIGHT

        grade_out = process.stdout.decode("utf-8")
    except Exception as e:
        print(f"Testcase {testcase.name} Grader Failed: {e}")
        print(f"{tree_score * TREE_WEIGHT}|{tree_score_new * TREE_MERGED_WEIGHT}|0.0|0.0|0.0|0.0|0.0|0.0|0.0")
        return tree_score * TREE_WEIGHT + tree_score_new * TREE_MERGED_WEIGHT

    split_score = grade_out.strip().split("|")
    dir_score = float(split_score[0])
    content_score = float(split_score[1])
    metadata_score = float(split_score[2])
    bitmap_score = float(split_score[3])
    group_score = float(split_score[4])
    super_score = float(split_score[5])
    backup_score = float(split_score[6])

    print(f"{tree_score * TREE_WEIGHT}|{tree_score_new * TREE_MERGED_WEIGHT}|{dir_score * DIR_WEIGHT}|{content_score * CONTENT_WEIGHT}|{metadata_score * METADATA_WEIGHT}|{bitmap_score * BITMAP_WEIGHT}|{group_score * GROUP_WEIGHT}|{super_score * SUPER_WEIGHT}|{backup_score * BACKUP_WEIGHT}")
    return tree_score * TREE_WEIGHT + tree_score_new * TREE_MERGED_WEIGHT + dir_score * DIR_WEIGHT + content_score * CONTENT_WEIGHT + metadata_score * METADATA_WEIGHT + bitmap_score * BITMAP_WEIGHT + group_score * GROUP_WEIGHT + super_score * SUPER_WEIGHT + backup_score * BACKUP_WEIGHT


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="MergeExt2FS Autograder")

    # --- Directory Overrides ---
    parser.add_argument("--testcase-folder", type=str, default="./testcases", help="Path to testcases directory")
    parser.add_argument("--student-folder", type=str, default="./student", help="Path to student submission directory")
    parser.add_argument("--output-folder", type=str, default="./output", help="Path to output directory")

    # --- Individual Flags (All default to False to detect explicit user intent) ---
    parser.add_argument("--extract", action="store_true", help="Extract hw3.tar.gz !!!Warning!!! This is a destructive operation, backup your files before you use it.")
    parser.add_argument("--remove-other", action="store_true", help="Delete unauthorized files (Nuke). !!!Warning!!! This is a destructive operation, backup your files before you use it.")
    parser.add_argument("--warn-other", action="store_true", help="Warn about unauthorized files (Dry-run)")
    parser.add_argument("--check", action="store_true", help="Run cppcheck and Makefile flag verification")
    parser.add_argument("--enforce", action="store_true", help="Run checks, deduct 20 points if failed")
    parser.add_argument("--make", action="store_true", help="Compile the student's code")

    # --- Sets (Shorthands) ---
    parser.add_argument("--full-test", action="store_true", help="Runs: extract, remove-other, enforce, make !!!Warning!!! This is a destructive operation, backup your files before you use it.")
    parser.add_argument("--make-test", action="store_true", help="Runs: make")
    parser.add_argument("--check-test", action="store_true", help="Runs: warn-other, check, make")

    # --- Testcase Selection ---
    parser.add_argument("--testcases", nargs="*", default=[],
                        help="Specific testcases to run (e.g. test1 test2). Runs all if empty.")

    args = parser.parse_args()

    # 1. Override Global Directories
    TESTCASE_FOLDER = args.testcase_folder
    STUDENT_FOLDER = args.student_folder
    OUTPUT_FOLDER = args.output_folder

    if not os.path.exists(OUTPUT_FOLDER):
        os.makedirs(OUTPUT_FOLDER)

    # 2. Dynamic Defaults (The UX Fix)
    # If the user doesn't provide ANY execution flags, apply the default --check-test profile
    action_flags = [
        args.extract, args.remove_other, args.warn_other,
        args.check, args.enforce, args.make,
        args.full_test, args.make_test, args.check_test
    ]

    if not any(action_flags):
        args.warn_other = True
        args.check = True
        args.make = True

    # 3. Resolve Sets (Shorthands override individual flags)
    if args.full_test:
        args.extract = True
        args.remove_other = True
        args.enforce = True
        args.make = True

    if args.check_test:
        args.warn_other = True
        args.check = True
        args.make = True

    if args.make_test:
        args.make = True

    # 4. Conflict Resolution
    # Destructive actions yield to warnings (Safety Priority)
    if args.warn_other and args.remove_other:
        args.remove_other = False

    # Standard checks yield to strict enforcement (Strictness Priority)
    if args.enforce and args.check:
        args.check = False

    # 5. Execution Pipeline
    penalty = 0.0

    print("=" * 40)
    print("  MERGEEXT2FS PIPELINE STARTING")
    print("=" * 40)

    if args.extract:
        print("\n[*] Running Extraction...")
        if not extract_tar():
            print("[!] FATAL: Extraction failed. Halting pipeline.")
            exit(1)

    if args.warn_other:
        print("\n[*] Running Sanitization (DRY RUN)...")
        remove_others(nuke=False)
    elif args.remove_other:
        print("\n[*] Running Sanitization (NUKE MODE)...")
        remove_others(nuke=True)

    if args.enforce:
        print("\n[*] Running Enforcement Checks...")
        if not perform_check():
            print("[!] ENFORCEMENT FAILED: Applying -20 point penalty.")
            penalty = 20.0
    elif args.check:
        print("\n[*] Running Checks...")
        perform_check()

    if args.make:
        print("\n[*] Compiling Code...")
        if not make_file():
            print("[!] FATAL: Compilation failed. Halting pipeline.")
            exit(1)

    # 6. Testcase Execution
    print("\n[*] Initializing Testcases...")
    all_testcases = init_testcases()
    cases_to_run = []

    if args.testcases:
        for tc_name in args.testcases:
            if tc_name in all_testcases:
                cases_to_run.append(all_testcases[tc_name])
            else:
                print(f"[!] Warning: Requested testcase '{tc_name}' not found.")
    else:
        cases_to_run = list(all_testcases.values())

    if not cases_to_run:
        print("[!] No valid testcases found. Exiting.")
        exit(0)

    print(f"[*] Running {len(cases_to_run)} testcase(s)...\n")

    total_score = 0.0
    case_scores = []

    for tc in cases_to_run:
        score = run_testcase(tc)
        print(f"--> Testcase [{tc.name}] Score: {score:.2f} / 100.00\n")
        total_score += score
        case_scores.append((tc.name, score))

    # 7. Final Grade Calculation
    average_score = total_score / len(cases_to_run)
    final_grade = max(0.0, average_score - penalty)

    print("=" * 40)
    print("  FINAL GRADING REPORT")
    print("=" * 40)
    for name, score in case_scores:
        print(f"  {name}: {score:.2f}")
    print("-" * 40)
    print(f"  Raw Average: {average_score:.2f}")
    if penalty > 0:
        print(f"  Enforcement Penalty: -{penalty:.2f}")
    print(f"  FINAL GRADE: {final_grade:.2f} / 100.00")
    print("=" * 40)
