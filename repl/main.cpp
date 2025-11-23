#include <pips/vm.hpp>

int main(int argc, char *argv[]) {
  pips::VM vm;

  if (argc == 1) {
    // REPL
    vm.repl('\n');
    return 0;
  }
  std::vector<std::string> files;
  std::vector<bool> isfile;
  bool verbose = false;
  bool repl = false;
  int i = 1;
  while (i < argc) {
     if (*argv[i] == '-' && *(argv[i] + 1) != '\0' && *(argv[i] + 2) == '\0') {
        char opt = *(argv[i] + 1);
        switch (opt) {
          case 'i': {
            // Run a script
            i++;
            if (i >= argc) {
                printf("Usage: pips -i [script]\n");
                return -1;
            }
            files.push_back(argv[i]);
            isfile.push_back(true);
            break;
          }
          case 'v': {
            verbose = true;
            break;
          }
          case 'r': {
            repl = true;
            break;
          }
          case 'c': {
              // consume arguments until another -? is hit
              std::string lines;
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
              if (lines.empty()) {
                  printf("Usage: pips -c 'line1' 'line2' ...\n");
                  return -1;
              }
              files.push_back(lines.c_str());
              isfile.push_back(false);
              break;
          }
          case 'h': {
            printf("Usage: repl [options] [script]\n");
            printf("Options:\n");
            printf("                          enter the REPL\n");
            printf("  -i  [script]            run script then enter REPL\n");
            printf("  -c  'line1' 'line2' ... run code snippet\n");
            printf("  -v                      verbose output\n");
            printf("  -r                      run in REPL mode after executing files\n");
            printf("  -h                      display this help message\n");
            return 0;
          }
        }
     }
     i++;
  }

  if (verbose) {
    printf("Running:\n");
    for(size_t idx = 0; idx < files.size(); ++idx) {
      const auto &file = files[idx];
      bool fileFlag = isfile[idx];
      if (fileFlag) {
          char *source = pips::Utils::readFile(file);
          printf("\n%s\n", source);
          std::free(source);
      } else {
          printf("\n%s\n", file.c_str());
      }
      printf("################################\n");
    }
  }
  for (size_t idx = 0; idx < files.size(); ++idx) {
    const auto &file = files[idx];
    bool fileFlag = isfile[idx];
    if (fileFlag) {
        vm.runFile(file);
    } else {
        auto result = vm.interpret(file.c_str());
        if (result == pips::InterpretResult::COMPILE_ERROR) return 65;
        if (result == pips::InterpretResult::RUNTIME_ERROR) return 70;
    }
  }
  if (repl) {
    if (verbose) {
      printf("Entering REPL mode\n");
    }
    vm.repl('\n');
  }
  return 0;
}