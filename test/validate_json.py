import json
from pathlib import Path
import re
import sys
import subprocess
import difflib # Added import

try:
    import jsonschema
except ImportError:
    print("Warning: jsonschema library not found. Skipping JSON validation.", file=sys.stderr)
    print("Install it with: pip install jsonschema", file=sys.stderr)
    sys.exit(0)

# --- Configuration ---
SCRIPT_DIR = Path(__file__).resolve().parent
ROOT_DIR = SCRIPT_DIR.parent
SCHEMA_PATH = SCRIPT_DIR / "cg3-cohort.schema.json"
CG_CONV_PATH = (ROOT_DIR / "src" / "cg-conv").resolve()
CG_CONV_EXTRA_ARGS = ["--parse-dep", "--deleted"]
assert CG_CONV_PATH.is_file(), f"cg-conv not found at {CG_CONV_PATH}. Make sure it is built."
INPUT_GLOB_PATTERN = "**/input.txt"

# --- End Configuration ---

# Add a timeout value (in seconds)
PROCESS_TIMEOUT = 5 # Adjust as needed

def jsonl_has_validation_errors(output_lines, validator, filename):
    """Validates each line of JSONL output."""
    has_errors = False
    for i, line in enumerate(output_lines):
        line = line.strip()
        if not line:
            continue
        try:
            instance = json.loads(line)
            validator.validate(instance)
        except json.JSONDecodeError as e:
            print(f"ERROR: Failed to decode JSON on line {i+1} of output for {filename}: {e}", file=sys.stderr)
            print(f"       Line content: {line}", file=sys.stderr)
            has_errors = True
        except jsonschema.ValidationError as e:
            print(f"ERROR: Schema validation failed on line {i+1} of output for {filename}:", file=sys.stderr)
            print(f"       Error: {e.message}", file=sys.stderr)
            print(f"       Path: {list(e.path)}", file=sys.stderr)
            print(f"       Instance: {e.instance}", file=sys.stderr)
            has_errors = True
        except Exception as e:
            print(f"ERROR: Unexpected error validating line {i+1} for {filename}: {e}", file=sys.stderr)
            has_errors = True
    return has_errors


print(f"Loading schema: {SCHEMA_PATH}")
with open(SCHEMA_PATH, 'r') as f:
    schema = json.load(f)
validator = jsonschema.Draft7Validator(schema)


print(f"Searching for input files matching '{INPUT_GLOB_PATTERN}' in {SCRIPT_DIR}")
input_files = list(SCRIPT_DIR.rglob(INPUT_GLOB_PATTERN))

if not input_files:
    print("Warning: No input.txt files found.", file=sys.stderr)
    sys.exit(0)

print(f"Found {len(input_files)} input files.")
overall_errors = False

output = {}
for input_file in input_files:
    if re.search(r'Apertium', str(input_file)):
        continue
    try:
        # Read the input file content
        with open(input_file, 'r', encoding='utf-8') as f:
            input_content = f.read()

        cC_process = subprocess.run(
            [str(CG_CONV_PATH), "-c", "-C"] + CG_CONV_EXTRA_ARGS,
            input=input_content, # Pass file content as stdin
            capture_output=True,
            text=True,
        )
        output["cC"] = {"in": input_content,
                        "process": cC_process}

    for stream_format in "afjn":
        to_process = subprocess.run(
            [str(CG_CONV_PATH), "-c", f"-{stream_format.upper()}"] + CG_CONV_EXTRA_ARGS,
            input=cC_process.stdout,
            capture_output=True,
            text=True,
        )
        output[f'c{stream_format.upper()}'] = {"process": to_process}
        fro_process = subprocess.run(
            [str(CG_CONV_PATH), f"-{stream_format}", "-C"] + CG_CONV_EXTRA_ARGS,
            input=to_process.stdout,
            capture_output=True,
            text=True,
        )
        output[f'{stream_format}C'] = {"process": fro_process}
        if cC_process.stdout != jC_process.stdout:
            print(f"ERROR: cg-conv C -> C output differs from C -> J -> C output for {input_file}", file=sys.stderr)
            # Generate and print a unified diff
            diff = difflib.unified_diff(
                cC_process.stdout.splitlines(keepends=True),
                jC_process.stdout.splitlines(keepends=True),
                fromfile='cC_output',
                tofile='jC_output',
                lineterm='\n'
            )
            print("       Differences:", file=sys.stderr)
            for line in diff:
                sys.stderr.write(f"       {line}") # Write directly to preserve formatting
            print(f"       Intermediate JSONL:{cJ_process.stdout.strip()}\n\n{"="*79}", file=sys.stderr)
            overall_errors = True
            continue

        output_lines = cJ_process.stdout.strip().split('\n')
        if jsonl_has_validation_errors(output_lines, validator, str(input_file)):
            overall_errors = True

    except FileNotFoundError:
        print(f"ERROR: Command not found: {CG_CONV_PATH}. Make sure cg-conv is built.", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"ERROR: An unexpected error occurred while processing {input_file}: {e}", file=sys.stderr)
        overall_errors = True


if overall_errors:
    print("\nValidation finished with errors.")
    sys.exit(1)
else:
    print("\nValidation finished successfully.")
    sys.exit(0)
