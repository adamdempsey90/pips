#include <pips/vm.hpp> 

int main(int argc, char *argv[]) {
    pips::VM vm;
    if (argc == 2) {
        vm.runFile(argv[1]);
        return 0;
    } else {
      vm.repl('\n');  
    }
    return 0;
}