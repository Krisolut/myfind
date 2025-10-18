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

static void child_search(
            const fs::path root,
            const string& target_name,
            const string& original_name,
            bool recursive,
            bool insensitive,
            int outfd) {
    const pid_t me = getpid();

}

int main(int argc, char* argv[]) {
    bool opt_recursive = false;
    bool opt_insensitive = false;
    opterr = 0;

    int c;
    while ((c = getopt(argc, argv, "Ri")) != -1) {
        switch (c) {
            case 'R': opt_recursive = true; break;
            case 'i': opt_insensitive = true; break;
            default: usage(argv[0]); return EXIT_FAILURE;
        }
    }

    if (optind >= argc) { 
        fprintf(stderr, "Missing searchpath\n"); 
        usage(argv[0]); 
        return 1; 
    }
    fs::path searchpath = argv[optind++];
    if (optind >= argc) { 
        fprintf(stderr, "Missing filenames\n"); usage(argv[0]); 
        return 1; 
    }

    vector<string> filenames;
    for (int i = optind; i < argc; ++i) filenames.push_back(argv[i]);

    struct ChildPipe { 
        int rfd=-1;                 // read file descriptor
        pid_t pid = -1;             // ProzessID des fork()    
        string buffer;              // Buffer
        bool done=false; 
    };

    vector<ChildPipe> cps(filenames.size());

    // Fork children für jeden filename
    for (size_t i = 0; i < filenames.size(); ++i) {
        int fds[2]; 

        // Pipoe erzeugen mit fds[0] = read | fds[1] = write
        if (pipe(fds) != 0) { 
            perror("pipe"); 
            return 1; 
        }

        pid_t pid = fork();     // Teile Prozess in Parent & Child
        if (pid < 0) {
            perror("fork"); 
            return 1;           // Abbruch wenn Fork failed
        }
        if (pid == 0) {
            close(fds[0]);              // Child braucht nur Schreib-Ende, schließt Lese-Ende
            child_search(searchpath,
                         opt_insensitive ? to_lower(filenames[i]) : filenames[i],
                         filenames[i],
                         opt_recursive, 
                         opt_insensitive, 
                         fds[1]);       // Schreibt Ergebnis ins Schreib-Ende
            close(fds[1]);              // Schließt Schreib-Ende
            _exit(0);                   // Child schließt sich nach fork
        } else {
            close(fds[1]);              // Schreib-Ende schließen
            cps[i].rfd = fds[0];        
            cps[i].pid = pid;           // Lese-FD und PID in cps[i] storen
        }
    }
}
