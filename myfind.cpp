// myfind.cpp — parallele Dateisuche mit fork()+pipe()+poll()
// Optionen: -R (rekursiv), -i (case-insensitive)

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <system_error>
#include <filesystem>
#include <iostream>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cerrno>

using namespace std;
namespace fs = std::filesystem;

static void usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s [-R] [-i] searchpath filename1 [filename2] ...\n"
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

// sicheres Schreiben (behandelt partielle write()s und EINTR)
static void write_all(int fd, const char* data, size_t len) {
    const char* p = data;
    size_t n = len;
    while (n > 0) {
        ssize_t k = write(fd, p, n);
        if (k < 0) {
            if (errno == EINTR) continue; // erneut versuchen
            break;                        // bei echtem Fehler abbrechen
        }
        p += k;
        n -= static_cast<size_t>(k);      // da ssize_t k "signed" ist, muss gecasted werden da sonst signed/unsigned mismatch
    }
}

static void child_search(
    const fs::path& root,         // Wurzelverzeichnis
    const string& target_name,    // ggf. bereits lowercase für -i
    const string& original_name,  // Originalname für Ausgabe
    bool recursive,
    bool insensitive,
    int outfd                     // Schreib-Ende der Pipe
) {
    const pid_t me = getpid();

    // Eine Fundstelle als Zeile emittieren: "<pid>: <original_name>: <absoluter_pfad>\n"
    auto emit = [&](const fs::path& p) {
        string line = to_string(me) + ": " + original_name + ": ";
        try {
            line += fs::absolute(p).string();    // Typo: .string() statt ,string() >_<
        } catch (...) {
            line += p.string();
        }
        line.push_back('\n');
        write_all(outfd, line.data(), line.size());
    };

    error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) return;

    fs::directory_options opts = fs::directory_options::skip_permission_denied;

    try {
        if (recursive) {
            for (fs::recursive_directory_iterator it(root, opts, ec), end; it != end; it.increment(ec)) {
                if (ec) { ec.clear(); continue; }
                const fs::directory_entry& de = *it;
                if (!de.is_directory(ec)) {
                    const string fname = de.path().filename().string();
                    if (names_equal(fname, target_name, insensitive)) {
                        emit(de.path());
                    }
                }
            }
        } else {
            for (fs::directory_iterator it(root, opts, ec), end; it != end; it.increment(ec)) {
                if (ec) { ec.clear(); continue; }
                const fs::directory_entry& de = *it;
                if (!de.is_directory(ec)) {
                    const string fname = de.path().filename().string();
                    if (names_equal(fname, target_name, insensitive)) {
                        emit(de.path());
                    }
                }
            }
        }
    } catch (...) {
        // bewusst still: Permission-Probleme etc. sind durch opts/EC abgefedert
    }
}

int main(int argc, char* argv[]) {
    bool opt_recursive = false;
    bool opt_insensitive = false;

    // getopt: eigene Fehlerausgabe übernehmen
    opterr = 0;

    int c;
    while ((c = getopt(argc, argv, "Ri")) != -1) {
        switch (c) {
            case 'R': opt_recursive   = true; break;
            case 'i': opt_insensitive = true; break;
            default: usage(argv[0]); return EXIT_FAILURE;
        }
    }

    // Pflichtargumente prüfen
    if (optind >= argc) {
        fprintf(stderr, "Missing searchpath\n");
        usage(argv[0]);
        return 1;
    }
    fs::path searchpath = argv[optind++];

    if (optind >= argc) {
        fprintf(stderr, "Missing filenames\n");
        usage(argv[0]);
        return 1;
    }

    // Alle restlichen Argumente sind Dateinamen/Patterns
    vector<string> filenames;
    for (int i = optind; i < argc; ++i) filenames.push_back(argv[i]);

    // Verwaltung eines Kindprozesses + Pipe-Lese-Ende
    struct ChildPipe {
        int   rfd  = -1; // read file descriptor
        pid_t pid  = -1; // Kindprozess-ID
        string buffer;   // Zeilenpuffer (Zwischenpuffer bis '\n')
        bool  done = false;
    };

    vector<ChildPipe> cps(filenames.size());

    // Für jeden Dateinamen Pipe anlegen + Kind forken
    for (size_t i = 0; i < filenames.size(); ++i) {
        int fds[2];
        if (pipe(fds) != 0) {
            perror("pipe");
            return 1;
        }

        // Verhindert FD-Leaks über exec() (falls exec* genutzt wird)
        fcntl(fds[0], F_SETFD, FD_CLOEXEC);
        fcntl(fds[1], F_SETFD, FD_CLOEXEC);

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            // Aufräumen des gerade erzeugten Pipe-Paares
            close(fds[0]); close(fds[1]);
            return 1;
        }

        if (pid == 0) {
            // --- Kindprozess ---
            close(fds[0]); // Kind schreibt nur
            // Bei -i den Suchbegriff in lowercase geben, Vergleichslogik ist identisch
            const string tname = opt_insensitive ? to_lower(filenames[i]) : filenames[i];
            child_search(searchpath, tname, filenames[i], opt_recursive, opt_insensitive, fds[1]);
            close(fds[1]);
            _exit(0); // sofort beenden (keine doppelten Destruktoren/Flushes)
        } else {
            // --- Elternprozess ---
            close(fds[1]);        // Eltern liest nur
            cps[i].rfd = fds[0];
            cps[i].pid = pid;
        }
    }

    // poll() über alle offenen Lese-FDs
    size_t remaining = cps.size();
    vector<pollfd> pfds(cps.size());

    while (remaining > 0) {
        // pollfd-Liste der noch aktiven FDs befüllen
        size_t idx = 0;
        for (auto& cp : cps) {
            if (!cp.done) {
                pfds[idx].fd      = cp.rfd;
                pfds[idx].events  = POLLIN | POLLHUP | POLLERR;
                pfds[idx].revents = 0;
                idx++;
            }
        }
        if (idx == 0) break; // nichts mehr offen

        int ret = poll(pfds.data(), idx, 250 /*ms*/);
        if (ret < 0) {
            if (errno == EINTR) continue; // z. B. durch Signal unterbrochen
            perror("poll");
            break;
        }

        size_t k = 0; // läuft synchron zu pfds[0..idx)
        for (auto& cp : cps) {
            if (cp.done) continue;
            pollfd& p = pfds[k++];

            if (p.revents & POLLIN) {
                char buf[4096];
                ssize_t n = read(cp.rfd, buf, sizeof(buf));
                if (n > 0) {
                    cp.buffer.append(buf, buf + n);

                    // vollständige Zeilen ausgeben
                    size_t pos = 0;
                    for (;;) {
                        size_t nl = cp.buffer.find('\n', pos);
                        if (nl == string::npos) break;
                        string line = cp.buffer.substr(pos, nl - pos + 1);
                        fwrite(line.data(), 1, line.size(), stdout);
                        pos = nl + 1;
                    }
                    // konsumierten Teil entfernen
                    cp.buffer.erase(0, pos);
                } else if (n == 0) {
                    // EOF: Restpuffer (falls vorhanden) ausgeben
                    if (!cp.buffer.empty()) {
                        cp.buffer.push_back('\n');
                        fwrite(cp.buffer.data(), 1, cp.buffer.size(), stdout);
                        cp.buffer.clear();
                    }
                    close(cp.rfd);
                    cp.done = true;
                    --remaining;
                } else { // n < 0
                    if (errno == EINTR) {
                        // beim nächsten poll/read weiter
                        continue;
                    }
                    // optional: perror("read");
                    close(cp.rfd);
                    cp.done = true;
                    --remaining;
                }
            } else if (p.revents & (POLLHUP | POLLERR)) {
                // Gegenstelle zu, oder Fehler -> schließen
                close(cp.rfd);
                cp.done = true;
                --remaining;
            }
        }

        // bereits beendete Kinder „abernten“, ohne zu blockieren
        for (;;) {
            int status = 0;
            pid_t w = waitpid(-1, &status, WNOHANG);
            if (w <= 0) break;
        }
    }

    // restliche Kinder blockierend abwarten (falls noch welche übrig sind)
    while (waitpid(-1, nullptr, 0) > 0) {}

    return 0;
}