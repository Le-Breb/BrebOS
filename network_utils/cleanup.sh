PRIMARY_IFACE=$(ip route get 8.8.8.8 | grep -o 'dev .* src' | awk '{print $2}')
sudo ip link set "$PRIMARY_IFACE" nomaster
sudo ip link delete br0
