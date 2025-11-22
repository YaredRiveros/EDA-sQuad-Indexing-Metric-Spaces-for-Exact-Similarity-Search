#include <iostream>
#include "../../../datasets/paths.hpp"
using namespace std;

int main() {
    cout << "Testing path resolution from GNAT/GNAT/" << endl;
    string ds = path_dataset("LA");
    cout << "Dataset LA: '" << ds << "'" << endl;
    if (ds == "") cout << "  -> NOT FOUND!" << endl;
    
    string qs = path_queries("LA");
    cout << "Queries LA: '" << qs << "'" << endl;
    if (qs == "") cout << "  -> NOT FOUND!" << endl;
    
    return 0;
}
