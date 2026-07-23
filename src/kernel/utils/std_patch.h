#pragma once

// Implementations of stuff that libstdc++ takes from libsupc++
// Both libs are compiled for hosted environments for now. libstdc++ seems to be usable nontheless for mygreatest pleasure,
// while libsupc++ pulls lots of stuff the kernel does not have.
// This file then provide the missing functions, allowing to compile without linking libsupc++
// The proper way to do things would be to compile libstdc++ and libsupc++ for non-hosted environment, matching kernel
// configuration (no-rtti and no-excepts)

[[noreturn]]
extern int irrecoverable_error(const char* format, ...);

namespace std
{
    void __throw_bad_alloc();

    void __throw_bad_array_new_length();
}