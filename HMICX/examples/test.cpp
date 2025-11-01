#include <iostream>
#include "hmicx.h"
using namespace HMICX;
using namespace std;

int main() {
    Parser parser("download.hmic");
    parser.parse();

    auto header = parser.getHeader();
    for (auto& [k, v] : header)
        cout << "[HEADER] " << k << "=" << v << "\n";

    auto cmds = parser.getCommands();
    cout << "[DEBUG] Parsed " << cmds.size() << " commands.\n";
    if (!cmds.empty()) {
        cout << "[DEBUG] First color: " << cmds[0].color
             << " with " << cmds[0].pixels.size() << " pixels\n";
    }
}
