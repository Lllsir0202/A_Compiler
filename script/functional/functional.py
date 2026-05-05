import argparse
import subprocess
import os

def execute(command):
    """run a command and return the result"""
    return subprocess.run(command, capture_output=True, text=True)

def execute_with_stdin_out(command):
    """run a command with standard input and output"""
    return os.system(command) % 255

def check_file(file1, file2):
    """compare two files and return the result"""
    try:
        result = execute(["diff", "--strip-trailing-cr", file1, file2, "-b"])
    except Exception as e:
        print(f"\033[91mUnknown Error on \033[0m{file1}, \033[91mPlease check your output file\033[0m")
        return 1
    else:
        return result.returncode

def add_returncode(file, ret):
    """add return code to file"""
    need_newline = False
    with open(file, "r") as f:
        try:
            content = f.read()
        except Exception as e:
            print(f"\033[91mUnknown Error on \033[0m{file}, \033[91mPlease check your output file\033[0m")
            return False
        else:
            if len(content) > 0 and not content.endswith("\n"):
                need_newline = True

    with open(file, "a+") as f:
        if need_newline:
            f.write("\n")
        f.write(str(ret))
        f.write("\n")
    return True

def execute_ast(input, output, opt):
    """run AST-related commands"""
    result = execute(["timeout", "10", "../../bin/compiler", input, "-ast", "-o", output, opt])
    if result.returncode != 0:
        print(f"\033[93mCompile Error on \033[0m{input}")
        return 0
    # print(f"\033[92mAccept \033[0m{input}")
    return 1
    
def execute_mlir(input, output, opt):
    """run MLIR-related commands"""
    result = execute(["timeout", "10", "../../bin/compiler", input, "-mlir", "-o", output, opt])
    if result.returncode != 0:
        print(f"\033[93mCompile Error on \033[0m{input}")
        return 0
    # print(f"\033[92mAccept \033[0m{input}")
    return 1

def execute_baseir(input, output, opt):
    """run BaseIR-related commands"""
    result = execute(["timeout", "10", "../../bin/compiler", input, "-baseir", "-o", output, opt])
    if result.returncode != 0:
        print(f"\033[93mCompile Error on \033[0m{input}")
        return 0
    # print(f"\033[92mAccept \033[0m{input}")
    return 1

def execute_ir(input, output, opt, stdin, stdout, testout):
    """run IR-related commands"""
    result = execute(["timeout", "10", "../../bin/compiler", input, "-llvm", "-o", output, opt])
    if result.returncode != 0:
        print(f"\033[93mCompile Error on \033[0m{input}")
        return 0

    result = execute(["clang", output, "-c", "-o", "tmp.o", "-w"])
    if result.returncode != 0:
        print(f"\033[93mOutput Error on \033[0m{input}")
        return 0

    result = execute(["clang", "-static", "tmp.o", "-L./lib", "-lsysy_x86"])
    if result.returncode != 0:
        execute(["rm", "-rf", "tmp.o"])
        print(f"\033[93mLink Error on \033[0m{input}")
        return 0

    execute(["rm", "-rf", "tmp.o"])

    result = 0
    if stdin == "none":
        result = execute_with_stdin_out(f"timeout 30 ./a.out > {testout} 2>/dev/null")
    else:
        result = execute_with_stdin_out(f"timeout 30 ./a.out < {stdin} > {testout} 2>/dev/null")

    if result == 124:
        print(f"\033[93mTime Limit Exceed on \033[0m{input}")
        return 0
    elif result == 139:
        print(f"\033[95mRunTime Error on \033[0m{input}")
        return 0

    add_returncode(testout, result)

    if check_file(testout, stdout) == 0:
        print(f"\033[92mAccept \033[0m{input}")
        return 1
    else:
        print(f"\033[91mWrong Answer on \033[0m{input}")
        return 0

def execute_asm(input, output, opt, stdin, stdout, testout):
    """run assembly-related commands"""
    result = execute(["timeout", "60", "../../bin/compiler", input, "-S", "-o", output, opt])
    if result.returncode != 0:
        print(f"\033[93mCompile Error on \033[0m{input}")
        return 0

    result = execute(["riscv64-unknown-linux-gnu-gcc", output, "-c", "-o", "tmp.o", "-w"])
    if result.returncode != 0:
        print(f"\033[93mOutput Error on \033[0m{input}")
        return 0

    result = execute(["riscv64-unknown-linux-gnu-gcc", "-static", "tmp.o", "-L./lib", "-lsysy_riscv", "-mcmodel=medany"])
    if result.returncode != 0:
        execute(["rm", "-rf", "tmp.o"])
        print(f"\033[93mLink Error on \033[0m{input}")
        return 0

    execute(["rm", "-rf", "tmp.o"])

    result = 0
    if stdin == "none":
        result = execute_with_stdin_out(f"timeout 60 qemu-riscv64 ./a.out > {testout} 2>/dev/null")
    else:
        result = execute_with_stdin_out(f"timeout 60 qemu-riscv64 ./a.out < {stdin} > {testout} 2>/dev/null")

    if result == 124:
        print(f"\033[93mTime Limit Exceed on \033[0m{input}")
        return 0
    elif result == 139:
        print(f"\033[95mRunTime Error on \033[0m{input}")
        return 0

    add_returncode(testout, result)

    if check_file(testout, stdout) == 0:
        print(f"\033[92mAccept \033[0m{input}")
        return 1
    else:
        print(f"\033[91mWrong Answer on \033[0m{input}")
        return 0

def main():
    """parse arguments and execute tests"""
    parser = argparse.ArgumentParser()
    parser.add_argument('input_folder')
    parser.add_argument('output_folder')
    parser.add_argument('opt_level', type=int, default=0)
    parser.add_argument('step', default="llvm")
    args = parser.parse_args()

    input_folder = args.input_folder
    output_folder = args.output_folder
    opt_arg = f"-O{args.opt_level}" if args.opt_level else "-O0"
    step = args.step

    if not os.path.exists(output_folder):
        os.makedirs(output_folder)

    total = 0
    ac = 0

    for file in os.listdir(input_folder):
        if file.endswith(".sy"):
            total += 1
            name = file.split(".")[0]
            input_file = os.path.join(input_folder, f"{name}.in")
            output_file = os.path.join(input_folder, f"{name}.out")
            if step == "ast":
                ac += execute_ast(os.path.join(input_folder, file), os.path.join(output_folder, f"{name}.ast"), opt_arg)
            elif step == "mlir":
                ac += execute_mlir(os.path.join(input_folder, file), os.path.join(output_folder, f"{name}.mlir"), opt_arg)
            elif step == "baseir":
                ac += execute_baseir(os.path.join(input_folder, file), os.path.join(output_folder, f"{name}.baseir"), opt_arg)
            elif step == "llvm":
                ac += execute_ir(os.path.join(input_folder, file), os.path.join(output_folder, f"{name}.ll"), opt_arg, input_file if os.path.exists(input_file) else "none", output_file, "tmp.out")
            elif step == "S":
                ac += execute_asm(os.path.join(input_folder, file), os.path.join(output_folder, f"{name}.s"), opt_arg, input_file if os.path.exists(input_file) else "none", output_file, "tmp.out")
    
    print(f"{step.upper()} Test Grade: {ac}/{total}")
    if os.path.exists("tmp.out"):
        os.system("rm tmp.out")
    if os.path.exists("a.out"):
        os.system("rm a.out")

if __name__ == "__main__":
    main()