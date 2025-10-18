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

static string to_lower(string s) {
    for (auto &ch : s)
        ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
    return s;
}

static bool names_equal(const string& a, const string& b, bool insensitive) {
    return insensitive ? (to_lower(a) == to_lower(b)) : (a == b);
}

static void child_search() {

}

int main (int argc, char* argv[]) {
    bool opt_recursive = false;
    bool opt_insensitive = false;
    opterr = 0;

    int c;
    while ((c = getopt(argc, argv, "Ri")) != -1 ) {
        switch (c) {
            case 'R': opt_recursive = true; break;
            case 'i': opt_insensitive = true; break;
            default: usage(argv[0]); return EXIT_FAILURE;
        }
    }
}