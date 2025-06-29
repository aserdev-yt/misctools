#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <utime.h>

extern char **environ;

// Recursively remove files and directories
int remove_recursive(const char *path) {
    struct stat st;
    if (!path) {
        fprintf(stderr, "remove_recursive: NULL path\n");
        return -1;
    }
    if (lstat(path, &st) != 0) {
        fprintf(stderr, "lstat failed for '%s': %s\n", path, strerror(errno));
        return -1;
    }
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (!d) {
            fprintf(stderr, "opendir failed for '%s': %s\n", path, strerror(errno));
            return -1;
        }
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            char buf[4096];
            int n = snprintf(buf, sizeof(buf), "%s/%s", path, entry->d_name);
            if (n < 0 || n >= (int)sizeof(buf)) {
                fprintf(stderr, "Path too long: %s/%s\n", path, entry->d_name);
                closedir(d);
                return -1;
            }
            if (remove_recursive(buf) != 0) {
                closedir(d);
                return -1;
            }
        }
        if (closedir(d) != 0) {
            fprintf(stderr, "closedir failed for '%s': %s\n", path, strerror(errno));
            return -1;
        }
        if (rmdir(path) != 0) {
            fprintf(stderr, "rmdir failed for '%s': %s\n", path, strerror(errno));
            return -1;
        }
        printf("Removed directory '%s'\n", path);
    } else {
        if (remove(path) != 0) {
            fprintf(stderr, "remove failed for '%s': %s\n", path, strerror(errno));
            return -1;
        }
        printf("Removed '%s'\n", path);
    }
    return 0;
}

// List all files in the current directory with size and owner
void list_files(void) {
    DIR *d = opendir(".");
    if (!d) {
        fprintf(stderr, "opendir failed: %s\n", strerror(errno));
        return;
    }
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        struct stat st;
        if (stat(dir->d_name, &st) == 0) {
            struct passwd *pw = getpwuid(st.st_uid);
            printf("%-20s %10ld %s\n", dir->d_name, (long)st.st_size, pw ? pw->pw_name : "unknown");
        } else {
            fprintf(stderr, "%-20s [stat error: %s]\n", dir->d_name, strerror(errno));
        }
    }
    if (closedir(d) != 0) {
        fprintf(stderr, "closedir failed: %s\n", strerror(errno));
    }
}

// Create a directory
void make_directory(const char *dirname) {
    if (!dirname) {
        fprintf(stderr, "make_directory: NULL dirname\n");
        return;
    }
    if (mkdir(dirname, 0755) == 0) {
        printf("Directory '%s' created.\n", dirname);
    } else {
        fprintf(stderr, "mkdir failed for '%s': %s\n", dirname, strerror(errno));
    }
}

// Spawn a new shell in the specified directory
void spawn_shell(const char *dirname) {
    if (!dirname) {
        fprintf(stderr, "spawn_shell: NULL dirname\n");
        return;
    }
    pid_t pid = fork();
    if (pid == 0) {
        if (chdir(dirname) != 0) {
            fprintf(stderr, "chdir failed for '%s': %s\n", dirname, strerror(errno));
            exit(1);
        }
        char *shell = getenv("SHELL");
        if (!shell) shell = "/bin/sh";
        execlp(shell, shell, (char *)NULL);
        fprintf(stderr, "execlp failed for '%s': %s\n", shell, strerror(errno));
        exit(1);
    } else if (pid > 0) {
        int status = 0;
        if (waitpid(pid, &status, 0) == -1) {
            fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
        }
    } else {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
    }
}

// Print the current working directory
void print_cwd(void) {
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        fprintf(stderr, "getcwd failed: %s\n", strerror(errno));
    }
}

// Print the current user
void print_user(void) {
    char *user = getenv("USER");
    if (user) {
        printf("%s\n", user);
    } else {
        fprintf(stderr, "No user found\n");
    }
}

// Find the full path of an executable in PATH
void which_cmd(const char *prog) {
    if (!prog || !*prog) {
        fprintf(stderr, "which: No program specified\n");
        return;
    }
    char *path = getenv("PATH");
    if (!path) {
        fprintf(stderr, "which: PATH not set\n");
        return;
    }
    char buf[4096];
    const char *p = path;
    while (*p) {
        const char *start = p;
        const char *end = strchr(p, ':');
        size_t len = end ? (size_t)(end - start) : strlen(start);
        if (len == 0) {
            snprintf(buf, sizeof(buf), "./%s", prog);
        } else {
            if (len + 1 + strlen(prog) + 1 > sizeof(buf)) {
                p = end ? end + 1 : p + strlen(p);
                continue;
            }
            snprintf(buf, sizeof(buf), "%.*s/%s", (int)len, start, prog);
        }
        if (access(buf, X_OK) == 0) {
            printf("%s\n", buf);
            return;
        }
        p = end ? end + 1 : p + strlen(p);
    }
    printf("%s not found in PATH\n", prog);
}

// Clear the terminal screen
void clear_screen(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

// Copy a file from src to dest
void copy_file(const char *src, const char *dest) {
    FILE *fs = fopen(src, "rb");
    if (!fs) {
        fprintf(stderr, "copy: cannot open '%s': %s\n", src, strerror(errno));
        return;
    }
    FILE *fd = fopen(dest, "wb");
    if (!fd) {
        fprintf(stderr, "copy: cannot open '%s': %s\n", dest, strerror(errno));
        fclose(fs);
        return;
    }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fs)) > 0) {
        if (fwrite(buf, 1, n, fd) != n) {
            fprintf(stderr, "copy: write error: %s\n", strerror(errno));
            break;
        }
    }
    fclose(fs);
    fclose(fd);
}

// Move or rename a file
void move_file(const char *src, const char *dest) {
    if (rename(src, dest) != 0) {
        fprintf(stderr, "move: cannot move '%s' to '%s': %s\n", src, dest, strerror(errno));
    }
}

// Print contents of a file
void cat_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "cat: cannot open '%s': %s\n", filename, strerror(errno));
        return;
    }
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        fputs(buf, stdout);
    }
    fclose(f);
}

// Show disk usage of a file or directory
off_t du(const char *path) {
    struct stat st;
    off_t total = 0;
    if (lstat(path, &st) != 0) return 0;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (!d) return st.st_size;
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            char buf[4096];
            snprintf(buf, sizeof(buf), "%s/%s", path, entry->d_name);
            total += du(buf);
        }
        closedir(d);
    }
    total += st.st_size;
    return total;
}
void du_cmd(const char *path) {
    printf("%s\t%lld\n", path, (long long)du(path));
}

// Print all environment variables
void print_env(void) {
    for (char **e = environ; *e; ++e) {
        puts(*e);
    }
}

// Touch a file (create or update timestamp)
void touch_file(const char *filename) {
    FILE *f = fopen(filename, "a");
    if (!f) {
        fprintf(stderr, "touch: cannot open '%s': %s\n", filename, strerror(errno));
        return;
    }
    fclose(f);
    utime(filename, NULL);
}

// Find files by name in a directory tree
void find_by_name(const char *dir, const char *name) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        char buf[4096];
        snprintf(buf, sizeof(buf), "%s/%s", dir, entry->d_name);
        struct stat st;
        if (lstat(buf, &st) == 0) {
            if (strcmp(entry->d_name, name) == 0)
                printf("%s\n", buf);
            if (S_ISDIR(st.st_mode))
                find_by_name(buf, name);
        }
    }
    closedir(d);
}

// Change file permissions
void chmod_file(const char *filename, const char *mode_str) {
    mode_t mode = strtol(mode_str, NULL, 8);
    if (chmod(filename, mode) != 0) {
        fprintf(stderr, "chmod: cannot change mode of '%s': %s\n", filename, strerror(errno));
    }
}

// Print process list
void print_ps(void) {
    DIR *d = opendir("/proc");
    if (!d) {
        fprintf(stderr, "ps: cannot open /proc\n");
        return;
    }
    printf("PID\tCMD\n");
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        char procpath[4096];
        snprintf(procpath, sizeof(procpath), "/proc/%s", entry->d_name);
        struct stat st;
        if (stat(procpath, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        int pid = atoi(entry->d_name);
        if (pid <= 0) continue;
        char cmdline[4096];
        snprintf(cmdline, sizeof(cmdline), "/proc/%d/cmdline", pid);
        FILE *f = fopen(cmdline, "r");
        if (f) {
            if (fgets(cmdline, sizeof(cmdline), f)) {
                printf("%d\t%s\n", pid, cmdline);
            }
            fclose(f);
        }
    }
    closedir(d);
}

// Print help message
void print_help(const char *progname) {
    printf("Usage: %s [-l] [-c <dir>] [-p] [-h]\n", progname);
    printf("Options:\n");
    printf("  -l         List files in the current directory (with size and owner)\n");
    printf("  -c <dir>   Spawn a shell in directory <dir>\n");
    printf("  -p         Print the current working directory\n");
    printf("  -h         Show this help message\n");
    printf("  -w         Print the current user\n");
    printf("  -m <dir>   Create a directory named <dir>\n");
    printf("  -r <file/dir>   Remove file or directory recursively\n");
    printf("  -x <prog>  Show full path of executable <prog> (like 'which')\n");
    printf("  -z         Clear the terminal screen (like 'clear')\n");
    printf("  -a <src> <dest>  Copy file from <src> to <dest>\n");
    printf("  -v <src> <dest>  Move (rename) file from <src> to <dest>\n");
    printf("  -t <file>        Print contents of <file> (cat)\n");
    printf("  -d <path>        Show disk usage of <path> (du)\n");
    printf("  -e               Print all environment variables (env)\n");
    printf("  -u <file>        Touch file (create or update timestamp)\n");
    printf("  -f <dir> <name>  Find file named <name> under <dir>\n");
    printf("  -o <file> <mode> Change file permissions (chmod, octal)\n");
    printf("  -s               Print process list (ps)\n");
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'l' && argv[i][2] == '\0') {
                list_files();
            } else if (argv[i][1] == 'm' && argv[i][2] == '\0') {
                if (i + 1 < argc) {
                    make_directory(argv[++i]);
                } else {
                    fprintf(stderr, "No directory specified for -m\n");
                }
            } else if (argv[i][1] == 'c' && argv[i][2] == '\0') {
                if (i + 1 < argc) {
                    spawn_shell(argv[++i]);
                } else {
                    fprintf(stderr, "No directory specified for -c\n");
                }
            } else if (argv[i][1] == 'p' && argv[i][2] == '\0') {
                print_cwd();
            } else if (argv[i][1] == 'r' && argv[i][2] == '\0') {
                if (i + 1 < argc) {
                    remove_recursive(argv[++i]);
                } else {
                    fprintf(stderr, "No file specified for -r\n");
                }
            } else if (argv[i][1] == 'h' && argv[i][2] == '\0') {
                print_help(argv[0]);
            } else if (argv[i][1] == 'w' && argv[i][2] == '\0') {
                print_user();
            } else if (argv[i][1] == 'x' && argv[i][2] == '\0') {
                if (i + 1 < argc) {
                    which_cmd(argv[++i]);
                } else {
                    fprintf(stderr, "No program specified for -x\n");
                }
            } else if (argv[i][1] == 'z' && argv[i][2] == '\0') {
                clear_screen();
            } else if (argv[i][1] == 'a' && argv[i][2] == '\0') {
                if (i + 2 < argc) {
                    copy_file(argv[++i], argv[++i]);
                } else {
                    fprintf(stderr, "Usage: -a <src> <dest>\n");
                }
            } else if (argv[i][1] == 'v' && argv[i][2] == '\0') {
                if (i + 2 < argc) {
                    move_file(argv[++i], argv[++i]);
                } else {
                    fprintf(stderr, "Usage: -v <src> <dest>\n");
                }
            } else if (argv[i][1] == 't' && argv[i][2] == '\0') {
                if (i + 1 < argc) {
                    cat_file(argv[++i]);
                } else {
                    fprintf(stderr, "Usage: -t <file>\n");
                }
            } else if (argv[i][1] == 'd' && argv[i][2] == '\0') {
                if (i + 1 < argc) {
                    du_cmd(argv[++i]);
                } else {
                    fprintf(stderr, "Usage: -d <path>\n");
                }
            } else if (argv[i][1] == 'e' && argv[i][2] == '\0') {
                print_env();
            } else if (argv[i][1] == 'u' && argv[i][2] == '\0') {
                if (i + 1 < argc) {
                    touch_file(argv[++i]);
                } else {
                    fprintf(stderr, "Usage: -u <file>\n");
                }
            } else if (argv[i][1] == 'f' && argv[i][2] == '\0') {
                if (i + 2 < argc) {
                    find_by_name(argv[++i], argv[++i]);
                } else {
                    fprintf(stderr, "Usage: -f <dir> <name>\n");
                }
            } else if (argv[i][1] == 'o' && argv[i][2] == '\0') {
                if (i + 2 < argc) {
                    chmod_file(argv[++i], argv[++i]);
                } else {
                    fprintf(stderr, "Usage: -o <file> <mode>\n");
                }
            } else if (argv[i][1] == 's' && argv[i][2] == '\0') {
                print_ps();
            } else {
                fprintf(stderr, "Unknown option: %s use -h for help\n", argv[i]);
            }
        }
    }
    return 0;
}