#include <iostream>
#include <filesystem>
using namespace std;

int main() {
    string p = "../../../datasets/LA.txt";
    cout << "Path: " << p << endl;
    cout << "filesystem::exists(): " << std::filesystem::exists(p) << endl;
    return 0;
}
