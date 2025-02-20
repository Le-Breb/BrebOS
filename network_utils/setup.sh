sudo ip link add name br0 type bridge
sudo ip link set br0 up
sudo ip addr add 192.168.100.1/24 dev br0
PRIMARY_IFACE=$(ip route get 8.8.8.8 | grep -o 'dev .* src' | awk '{print $2}')
sudo ip link set "$PRIMARY_IFACE" master br0
sudo ip link set br0 promisc on
