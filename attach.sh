cd nodeA
mkdir build
cd build
cmake ..
make
sudo ./nodeA ../../hello.bpf -o out.ccbpf
sudo ./nodeA attach hook_udp_input out.ccbpf
