# Run this script as sudo
apt update

# Install cmake
apt install cmake -y 

# Install clang-format, clang-tidy
apt install clang-format -y
apt install clang-tidy -y

# Install necessary python packages
pip3 install pytest