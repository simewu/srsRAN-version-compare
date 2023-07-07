rm -rf benchmark_just_latest_three
g++ -g --std=c++17 benchmark_just_latest_three.cpp -o benchmark_just_latest_three -lssl -lcrypto

./benchmark_just_latest_three