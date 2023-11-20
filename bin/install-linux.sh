# Run this script as sudo

# Replace apt with yum if applicable

apt update

# Install cmake
apt install cmake -y 

# Install gcc for c++
# Make sure that openmp is supported with this version of gcc
# If using yum run: yum install gcc-c++
apt install g++

# Install necessary python packages
pip3 install pytest