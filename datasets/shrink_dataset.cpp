#include <bits/stdc++.h>
using namespace std;

// Run: 

/*
Head: primeras 2000 líneas
./cut_dataset LA.txt LA_2k.txt

Head con 5000 líneas
./cut_dataset LA.txt LA_5k.txt -n 5000

Random con 2000 líneas, seed fija
./cut_dataset LA.txt LA_2k.txt --mode random --num-lines 2000 --seed 12345

*/

void cut_head(const string& input_path, const string& output_path, int n) {
    ifstream fin(input_path);
    if (!fin.is_open()) {
        cerr << "[ERROR] No se pudo abrir input: " << input_path << "\n";
        return;
    }
    ofstream fout(output_path);
    if (!fout.is_open()) {
        cerr << "[ERROR] No se pudo abrir output: " << output_path << "\n";
        return;
    }

    string line;
    int count = 0;
    while (count < n && std::getline(fin, line)) {
        fout << line << '\n';
        count++;
    }

    cout << "[OK] Escribidas " << count << " líneas (head) en " << output_path << "\n";
}

void cut_random(const string& input_path, const string& output_path, int n, unsigned int seed = 12345) {
    ifstream fin(input_path);
    if (!fin.is_open()) {
        cerr << "[ERROR] No se pudo abrir input: " << input_path << "\n";
        return;
    }

    std::mt19937 rng(seed);

    vector<string> reservoir;
    reservoir.reserve(n);

    string line;

    if (!std::getline(fin, line)) {
        cerr << "[ERROR] El archivo está vacío.\n";
        return;
    }

    long long total = 0; 

    while (std::getline(fin, line)) {
        total++;
        if ((int)reservoir.size() < n) {
            reservoir.push_back(line);
        } else {
            std::uniform_int_distribution<long long> dist(0, total - 1);
            long long j = dist(rng);
            if (j < n) {
                reservoir[j] = line;
            }
        }
    }

    ofstream fout(output_path);
    if (!fout.is_open()) {
        cerr << "[ERROR] No se pudo abrir output: " << output_path << "\n";
        return;
    }

    for (const auto& l : reservoir) {
        fout << l << '\n';
    }

    cout << "[OK] Escribidas " << reservoir.size() << " líneas (random) en " << output_path << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Uso:\n"
             << "  " << argv[0] << " <input> <output> [opciones]\n\n"
             << "Opciones:\n"
             << "  -n, --num-lines N    Numero de lineas a conservar (defecto 2000)\n"
             << "  --mode head|random   Modo de seleccion (defecto head)\n"
             << "  --seed S             Semilla para modo random (defecto 12345)\n";
        return 1;
    }

    string input_path = argv[1];
    string output_path = argv[2];

    int num_lines = 2000;
    string mode = "head";
    unsigned int seed = 12345;

    // Parsear flags simples
    for (int i = 3; i < argc; ++i) {
        string arg = argv[i];

        if (arg == "-n" || arg == "--num-lines") {
            if (i + 1 >= argc) {
                cerr << "[ERROR] Falta valor para " << arg << "\n";
                return 1;
            }
            num_lines = stoi(argv[++i]);
        } else if (arg == "--mode") {
            if (i + 1 >= argc) {
                cerr << "[ERROR] Falta valor para --mode\n";
                return 1;
            }
            mode = argv[++i];
        } else if (arg == "--seed") {
            if (i + 1 >= argc) {
                cerr << "[ERROR] Falta valor para --seed\n";
                return 1;
            }
            seed = static_cast<unsigned int>(stoul(argv[++i]));
        } else {
            cerr << "[WARN] Opcion desconocida ignorada: " << arg << "\n";
        }
    }

    if (mode == "head") {
        cut_head(input_path, output_path, num_lines);
    } else if (mode == "random") {
        cut_random(input_path, output_path, num_lines, seed);
    } else {
        cerr << "[ERROR] Modo desconocido: " << mode << " (usa head o random)\n";
        return 1;
    }

    return 0;
}
