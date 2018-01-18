# This script queries Git to determine the version information for the
# current repository and writes a C++ header file with that information,
# represented as a cradle::repository_info structure.

import subprocess
import sys

def generate(unused_input_file, unusued_source_file_path, header_file_path):
    # Run 'git describe' and capture its output.
    result = \
        subprocess.run(
            ["git", "describe", "--tags", "--dirty", "--long"],
            check=True,
            stdout=subprocess.PIPE)
    output = result.stdout.decode('utf-8')

    # Split the output into its parts.
    parts = output.rstrip().split('-')
    if len(parts) < 3:
        print("unable to parse 'git describe' output:\n" + output.rstrip(), file=sys.stderr)
        sys.exit(1)

    # And work backwards interpreting the parts...

    # Check for the dirty flag.
    is_dirty = False
    index = -1
    if parts[index] == "dirty":
        is_dirty = True
        index -= 1

    # Get the commit name.
    commit_name = parts[index][1:]
    index -= 1

    # Get the commits since the tag.
    commits_since_tag = int(parts[index])

    # The rest should be the tag.
    tag = '-'.join(parts[0:index])

    # Generate the C++ output.
    cpp_output = \
        'static cradle::repository_info const version_info{{ "{}", {}, "{}", {} }};\n'.format(
            commit_name, str(is_dirty).lower(), tag, commits_since_tag)

    # Only write the header if it needs to be updated.
    update_needed = True
    try:
        with open (header_file_path, "r") as output_file:
            if output_file.read() == cpp_output:
                update_needed = False
    except FileNotFoundError:
        # The file doesn't exist yet.
        pass
    if update_needed:
        with open(header_file_path, "w") as output_file:
            output_file.write(cpp_output)
