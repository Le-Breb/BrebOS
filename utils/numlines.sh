find src/kernel src/programs src/libc src/libk src/gcc -iregex '.*\.\(cpp\|s\|h\)$' -exec cat {} \; | wc -l

