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

# ldso_path = "./sysroot/usr/lib/ld.so"
# ldso_base = get_txt_ba(ldso_path) + 0x3000
# print(f"loading {ldso_path} at 0x{ldso_base}")
# gdb.execute(
#     f"add-symbol-file {ldso_path} {hex(ldso_base)}",
#     to_string=True
# )