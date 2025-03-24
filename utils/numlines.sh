find . -iregex '.*\.\(cpp\|s\|h\)$' -exec cat {} \; | wc -l

