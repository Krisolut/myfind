#include <sys/types.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <filesystem>
#include <iostream>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cerrno>

using namespace std;
namespace fs = std::filesystem;

static void usage (const char* prog) {
    fprintf(stderr,
        "Usage: %s [-R] [-i] searchpath filename1 [filename2] ... \n"
        "  -R  recursive search\n"
        "  -i  case-insensitive search\n", prog);
}