import gdb
import subprocess as sp

struct_addr = "0xD0000000"

class ProcessLoadedBreakpoint(gdb.Breakpoint):
    def stop(self):
        path = gdb.parse_and_eval(
            f"((struct GDB::process_info*){struct_addr})->path"
        ).string()

        base = int(gdb.parse_and_eval(
            f"((struct GDB::process_info*){struct_addr})->base_address"
        ))

        print(f"Loading ELF: {path} at {hex(base)}")

        gdb.execute(
            f"add-symbol-file {path} {hex(base)}",
            to_string=True
        )

        return False  # continue automatically

class UnloadProcessBreakpoint(gdb.Breakpoint):
    def stop(self):
        path = gdb.parse_and_eval(
            f"((struct GDB::process_info*){struct_addr})->path"
        ).string()

        base = int(gdb.parse_and_eval(
            f"((struct GDB::process_info*){struct_addr})->base_address"
        ))

        print(f"Unloading ELF {path} at {hex(base)}")

        gdb.execute(
            f"remove-symbol-file -a {hex(base)}",
            to_string=True
        )

        return False # Continue

def get_symbol_address(name):
    sym = gdb.parse_and_eval(name)
    return int(sym.address)

## Setting a bp to __gdb_process_loaded causes the bp to be triggered twice for no reason
## Setting it to the symbol address does not
load_addr = get_symbol_address("__gdb_load_process")
ProcessLoadedBreakpoint(f"*{load_addr}")

unload_addr = get_symbol_address("__gdb_unload_process")
UnloadProcessBreakpoint(f"*{unload_addr}")

# Get .text base address of an ELF
def get_txt_ba(elf_path):
    import subprocess as sp

    out = sp.run(["readelf", "-SW", elf_path], capture_output=True)
    t = out.stdout.decode()
    tt = t.split('\n')
    tt2 = [ttt for ttt in tt if "text" in ttt][0].split()[4]
    off = int(tt2, 16)

    return off

def load_program(elf_name):
    elf_path = f"./src/programs/build/{elf_name}"
    elf_base = get_txt_ba(elf_path)
    print(f"loading {elf_path} at 0x{elf_base}")
    gdb.execute(
        f"add-symbol-file {elf_path} {hex(elf_base)}",
        to_string=True
    )

gdb.execute(
    f"add-symbol-file ./bootloader/build/bootloader2.elf 0x1000",
    to_string=True
)
load_program("seq")

# ldso_path = "./sysroot/usr/lib/ld.so"
# ldso_base = get_txt_ba(ldso_path) + 0x1e000
# print(f"loading {ldso_path} at 0x{ldso_base}")
# gdb.execute(
#     f"add-symbol-file {ldso_path} {hex(ldso_base)}",
#     to_string=True
# )
#
# libcso_path = "./sysroot/usr/lib/libc.so"
# libcso_base = get_txt_ba(libcso_path) + 0x41800000
# print(f"loading {libcso_path} at 0x{hex(libcso_base)}")
# gdb.execute(
#     f"add-symbol-file {libcso_path} {hex(libcso_base)}",
#     to_string=True
# )

# stdcpp_path = "./toolchain/usr/i686-brebos/lib/libstdc++.so.6.0.34"
# stdcpp_base = get_txt_ba(stdcpp_path) + 0x41000000
# print(f"loading {stdcpp_path} at 0x{hex(stdcpp_base)}")
# gdb.execute(
#     f"add-symbol-file {stdcpp_path} {hex(stdcpp_base)}",
#     to_string=True
# )