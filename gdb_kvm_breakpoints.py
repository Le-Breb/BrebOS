import gdb

# Under KVM, gdb inserts software breakpoints by writing int3 in guest memory. From the reset vector, kernel addresses
# aren't mapped (paging is off, the kernel isn't even loaded), so insertion fails and the first `continue` aborts.
# Workaround: under KVM, keep breakpoint locations disabled until the kernel runs with paging enabled. An internal
# hardware breakpoint on kmain (hardware breakpoints don't write to memory) then re-enables them, and gdb inserts them
# as regular software breakpoints when it resumes.
# Locations are disabled rather than breakpoints themselves, so that the IDE keeps showing breakpoints as enabled.
# Nothing is reported to the IDE: internal breakpoints are hidden from it, and stop() returning False resumes silently.

PAGING_ENABLED_SYMBOL = "kmain"

waiting_for_paging = False
# (breakpoint number, address) of the locations disabled by this script. Location objects themselves can't be kept, as
# gdb replaces them whenever it re-resolves breakpoints (e.g. when symbols are loaded)
disabled_locations = set()
updating_locations = False # Changing a location triggers breakpoint_modified, ignore those


def set_locations_enabled(enable):
    """Disables enabled locations (and records them), or re-enables recorded ones."""
    global updating_locations
    updating_locations = True
    try:
        for breakpoint in gdb.breakpoints():
            if breakpoint.number < 0: # Internal breakpoint, such as PagingEnabledBreakpoint
                continue
            for location in breakpoint.locations:
                key = (breakpoint.number, location.address)
                if not enable and location.enabled:
                    location.enabled = False
                    disabled_locations.add(key)
                elif enable and key in disabled_locations:
                    location.enabled = True
    finally:
        updating_locations = False

    if enable:
        disabled_locations.clear()


def on_breakpoint_event(breakpoint):
    # Breakpoints created or re-resolved before paging is enabled must be disabled too
    if waiting_for_paging and not updating_locations:
        set_locations_enabled(False)


class PagingEnabledBreakpoint(gdb.Breakpoint):
    def stop(self):
        global waiting_for_paging
        waiting_for_paging = False
        set_locations_enabled(True)

        # Breakpoints can't be deleted from their own stop(), defer it. Frees the debug register for the user
        gdb.post_event(self.delete)
        return False


def brebos_kvm_arm():
    global waiting_for_paging
    try:
        if "enabled" not in gdb.execute("monitor info kvm", to_string=True):
            return # TCG: breakpoints work from the reset vector, nothing to do
    except gdb.error:
        return

    waiting_for_paging = True
    set_locations_enabled(False)

    address = int(gdb.parse_and_eval(PAGING_ENABLED_SYMBOL).address)
    PagingEnabledBreakpoint(f"*{address}", type=gdb.BP_HARDWARE_BREAKPOINT, internal=True)


gdb.events.breakpoint_created.connect(on_breakpoint_event)
gdb.events.breakpoint_modified.connect(on_breakpoint_event)
