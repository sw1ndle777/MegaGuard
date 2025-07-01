import os

base_dir = "../deps/libsodium"
project_root = ".."

cl_include_entries = []
cl_compile_entries = []

# Walk through all files recursively
for root, _, files in os.walk(base_dir):
    for file in files:
        full_path = os.path.join(root, file)
        rel_path = os.path.relpath(full_path, start=project_root)
        include_path = rel_path.replace("/", "\\").replace("\\\\", "\\")
        filter_path = os.path.dirname(include_path)

        if file.endswith(".h"):
            cl_include_entries.append(
                f'  <ClInclude Include="..\\{include_path}">\n    <Filter>{filter_path}</Filter>\n  </ClInclude>'
            )
        elif file.endswith(".c"):
            cl_compile_entries.append(
                f'  <ClCompile Include="..\\{include_path}">\n    <Filter>{filter_path}</Filter>\n  </ClCompile>'
            )

# Output ClInclude section
print("<ItemGroup>")
print("\n".join(cl_include_entries))
print("</ItemGroup>\n")

# Output ClCompile section
print("<ItemGroup>")
print("\n".join(cl_compile_entries))
print("</ItemGroup>")
