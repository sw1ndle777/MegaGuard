import os

base_dir = "../deps/libsodium"
entries = []

for root, _, files in os.walk(base_dir):
    for file in files:
        if file.endswith(".h") or file.endswith(".c"):
            full_path = os.path.join(root, file)
            rel_path = os.path.relpath(full_path, start=os.path.dirname(base_dir))
            win_path = rel_path.replace("/", "\\").replace("\\\\", "\\")
            filter_path = os.path.dirname(win_path)
            tag = "ClInclude" if file.endswith(".h") else "ClCompile"
            entries.append(f'  <{tag} Include="..\\{win_path}">')
            entries.append(f'    <Filter>{filter_path}</Filter>')
            entries.append(f'  </{tag}>')

print("<ItemGroup>")
print("\n".join(entries))
print("</ItemGroup>")
