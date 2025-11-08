import os
import subprocess
import sys
from pathlib import Path

# 编译器与通用参数
CC = "cl"
CFLAGS_COMMON = ["/std:c++20", "/utf-8", "/EHsc"]
INCLUDE = [
    "/I./SDL3/include",
    "/I.",
    "/I./imgui"
]
LIBPATH = [
    "/LIBPATH:./SDL3/lib/x64",
    "/LIBPATH:."
]
LIBRARIES = ["user32.lib", "gdi32.lib", "SDL3.lib", "dwmapi.lib"]

# 编译链接参数
CFLAGS = ["/MDd", "/Od", "/Zi",] + INCLUDE
# LFLAGS = ["/link", "/SUBSYSTEM:CONSOLE"] + LIBPATH + LIBRARIES
LFLAGS = ["/link", "/SUBSYSTEM:WINDOWS", "/ENTRY:mainCRTStartup"] + LIBPATH + LIBRARIES

# 生成的exe文件
MAIN_OUT = "another_alt_tab.exe"

# 源文件列表
MAIN_SRC = ["main.cpp"]
IMG_SRC = [
    "imgui/imgui.cpp",
    "imgui/imgui_draw.cpp",
    "imgui/imgui_tables.cpp",
    "imgui/imgui_widgets.cpp",
    "imgui/backends/imgui_impl_sdl3.cpp",
    "imgui/backends/imgui_impl_sdlrenderer3.cpp"
]
IMG_OBJ = [
    "imgui/imgui.obj",
    "imgui/imgui_draw.obj",
    "imgui/imgui_tables.obj",
    "imgui/imgui_widgets.obj",
    "imgui/backends/imgui_impl_sdl3.obj",
    "imgui/backends/imgui_impl_sdlrenderer3.obj"
]

# cpp->obj
def compile_obj(src, suffix=".obj"):
    src_path = Path(src)
    obj = (src_path.parent / (src_path.stem + suffix)).as_posix()
    print(f"--- Compiling Debug Object: {src} ---")
    cmd = [CC] + CFLAGS_COMMON + CFLAGS + ["/c", src, f"/Fo:{obj}"]
    subprocess.run(cmd, check=True)
    return obj

#objs->exe
def link_exe(objs, out):
    print(f"--- Linking Debug Executable: {out} ---")
    cmd = [CC] + objs + [f"/Fe:{out}"] + LFLAGS
    subprocess.run(cmd, check=True)


def imgui():
    img_objs = [compile_obj(f) for f in IMG_SRC]
    print("finish compiling ImGui cpps to objs")
    return img_objs

def all():
    img_objs = imgui()
    main_obj = [compile_obj(f) for f in MAIN_SRC]
    link_exe(img_objs + main_obj, MAIN_OUT)
    run()

def main():
    main_obj = [compile_obj(f) for f in MAIN_SRC]
    link_exe(IMG_OBJ + main_obj, MAIN_OUT)
    run()

def run():
    print("--- run the exe ---")
    subprocess.run([f"./{MAIN_OUT}"])

def clean():
    print("--- Cleaning up all built files ---")
    for ext in ("*.obj", "*.exe", "*.pdb", "*.ilk"):
        for f in Path(".").rglob(ext):
            try:
                print(f"delete {f.name}")
                f.unlink()
            except Exception:
                pass


if __name__ == "__main__":
    target = sys.argv[1] if len(sys.argv) > 1 else "run"

    if target == "all":
        all()
    if target == "main":
        main()

