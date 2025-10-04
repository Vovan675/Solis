import os
import sys

# Constants
EXCLUDED_FILES = {
    "unwanted_file.h",
    "imgui_impl_",
    ".cpp"
}  # Add filenames to exclude here
OUTPUT_FILENAME = "merged_result.txt"

def get_sorted_files(directory, extensions):
    """Get sorted list of files with given extensions in directory and subdirectories"""
    file_list = []
    for root, _, files in os.walk(directory):
        for file in files:
            # Check if the file has the correct extension and is not excluded
            if (any(file.endswith(ext) for ext in extensions) and
                not any(excluded in file for excluded in EXCLUDED_FILES)):
                file_list.append(os.path.join(root, file))
    return sorted(file_list)

def merge_files(directory, output_filename):
    """Merge .h and .cpp files into output file with specified format"""
    # Get files in order: .h first, then .cpp, both sorted
    h_files = get_sorted_files(directory, ['.h'])
    cpp_files = get_sorted_files(directory, ['.cpp'])
    
    with open(output_filename, 'w', encoding='utf-8') as outfile:
        for file_list in [h_files, cpp_files]:
            for filepath in file_list:
                filename = os.path.basename(filepath)
                outfile.write(f"FILENAME: {filename}\n")
                
                with open(filepath, 'r', encoding='utf-8') as infile:
                    content = infile.read()
                    outfile.write(content)
                    
                    # Ensure proper spacing between files
                    if not content.endswith('\n'):
                        outfile.write('\n')
                    outfile.write('\n')  # Add blank line after content

if __name__ == "__main__":
    # Get directory from argument or default to current directory
    directory = sys.argv[1] if len(sys.argv) > 1 else "."
    
    # Check if the directory exists
    if not os.path.isdir(directory):
        print(f"Error: Directory '{directory}' does not exist.")
        sys.exit(1)
    
    # Merge files
    merge_files(directory, OUTPUT_FILENAME)
    print(f"Files merged successfully into '{OUTPUT_FILENAME}' (excluded files: {EXCLUDED_FILES})")