#!/usr/bin/env bash
tmp_prof_file=$(mktemp)
rm -rf "$tmp_prof_file"

echo "Profiling..."
echo "Retrieving profile data from disk image..."
# QEMU runs on stick.img (os.iso + usb_disk.img), so read from its FAT partition rather than from usb_disk.img
fat_start=$(sfdisk -d stick.img | awk '/^stick.img2/{gsub(",","",$4); print $4}')
mcopy -i "stick.img@@$((fat_start*512))" ::/prof.txt "$tmp_prof_file"

tmp_file=$(mktemp)
tot=0

echo "Processing profile data..."
while IFS= read -r l; do
  count=$(echo $l | awk '{print $2}')
  ((tot=tot+10#$count))
done < "$tmp_prof_file"

nm=$(nm -nC build/kernel.elf)
while IFS= read -r l; do
  addr=$(echo "$l" | awk '{print $1}')
  count=$(echo "$l" | awk '{print $2}')
  name=$(echo "$nm" | grep "$addr" | head -n 1 | sed 's/^[^ ]* [^ ]* //' | grep -o "^[^(]*")
  stripped_count=$((10#$count))
  percent=$((stripped_count*100/tot))
  [[ $((10#$count)) -gt 3 ]] && printf '%02d%%: ' $percent >> "$tmp_file" && echo "$count: $name (0x$addr)" >> "$tmp_file"
done < "$tmp_prof_file"

echo "Percentage of total cycles: | Cycles | Function"
sort -n "$tmp_file"