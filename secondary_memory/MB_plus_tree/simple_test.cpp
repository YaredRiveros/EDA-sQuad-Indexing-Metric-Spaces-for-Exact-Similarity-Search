// simple_test.cpp - Test mínimo para depurar
#include "mbpt.hpp"
#include "objectdb.hpp"
#include "datasets/paths.hpp"
#include <iostream>
#include <fstream>

using namespace std;

int main() {
    // Crear dataset pequeño: 10 strings
    ofstream out("tiny_words.txt");
    out << "hello\nworld\ntest\ncode\ndebug\nindex\ntree\nsearch\nquery\ndata\n";
    out.close();
    
    StringDB db("tiny_words.txt");
    cout << "Dataset tiny: N=" << db.size() << endl;
    
    // Construir MB+-tree
    cout << "Construyendo índice..." << endl;
    MBPT_Disk mbpt(&db, 0.1);
    mbpt.build("test_index");
    cout << "Índice construido OK" << endl;
    
    // Test simple
    mbpt.clear_counters();
    vector<int> results;
    mbpt.rangeSearch(0, 3.0, results);
    
    cout << "rangeSearch(0, 3.0): " << results.size() << " resultados" << endl;
    cout << "compDist: " << mbpt.get_compDist() << endl;
    
    return 0;
}
