find src/kernel src/programs src/libc src/libk src/gcc -iregex '.*\.\(cpp\|s\|h\|hxx\)$' -print0 | wc -l --files0-from=-
