#include <pips/vm.hpp> 

int main(int argc, char *argv[]) {
  pips::VM vm;

  if (argc == 1) {
    // REPL
    vm.repl('\n');
    return 0;
  }
  std::string lines;
  std::vector<std::string> files;
  bool verbose = false;
  int i = 1;
  while (i < argc) {
     if (*argv[i] == '-' && *(argv[i] + 1) != '\0' && *(argv[i] + 2) == '\0') {
        char opt = *(argv[i] + 1);
        switch (opt) {
          case 'i':
            // Run a script
            i++;
            if (i >= argc) {
                printf("Usage: pips -i [script]\n");
                return -1;
            }
            files.push_back(argv[i]);
            break;
          case 'v':
            verbose = true;
            break;
          case 'c':
              // consume arguments until another -? is hit
              do {
                  i++;
                  if (i >= argc)
                      break;
                  if (*argv[i] == '-') {
                    i--;
                    break;
                  }
                  std::string line = argv[i];
                  if (!line.empty() && line.back() == ';') {
                      line.pop_back();
                  }
                  lines += line + ";\n";
              } while (i < argc);
              break;
           case 'h':
              printf("Usage: repl [options] [script]\n");
              printf("Options:\n");
              printf("                          enter the REPL\n");
              printf("  -i  [script]            run script then enter REPL\n");
              printf("  -c  'line1' 'line2' ... run code snippet\n");
              printf("  -h                      display this help message\n");
              return 0;
        }
     }
     i++;
  }
  if (!lines.empty()) {
      if (verbose) {
          printf("Input::\n%s\n", lines.c_str());
      }
      auto result = vm.interpret(lines.c_str());
      if (result == pips::InterpretResult::COMPILE_ERROR) return 65;
      if (result == pips::InterpretResult::RUNTIME_ERROR) return 70;
  }
  for (const auto &file : files) {
      if (verbose) {
          printf("Running script: %s\n", file.c_str());
      }
      vm.runFile(file);
  }
  return 0;
}