import os

root_dir = "deps/libsodium"
cl_include_entries = []
cl_compile_entries = []

for dirpath, _, filenames in os.walk(root_dir):
    for filename in filenames:
        full_path = os.path.join(dirpath, filename).replace("/", "\\")
        if filename.endswith(".h"):
            cl_include_entries.append(f'<ClInclude Include="{full_path}" />')
        elif filename.endswith(".c"):
            cl_compile_entries.append(f'<ClCompile Include="{full_path}" />')

# Output the result (you can redirect this to a file if you want)
print("<!-- HEADER FILES -->")
for line in cl_include_entries:
    print(line)

print("\n<!-- SOURCE FILES -->")
for line in cl_compile_entries:
    print(line)
