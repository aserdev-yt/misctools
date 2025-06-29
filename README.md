# misctools

**misctools** is a simple command-line utility written in C that provides a collection of useful file, directory, and system management tools. It can be used with command-line arguments or as an interactive terminal menu (TUI) when run without arguments. (doesn't work for some reason)

## Features

- List files in the current directory (with size and owner)
- Create and remove directories/files (recursively)
- Print the current working directory or user
- Spawn a shell in a given directory
- Find executables in `$PATH` (like `which`)
- Clear the terminal screen
- Copy, move, and print files
- Show disk usage (like `du`)
- Print environment variables
- Touch files (create or update timestamp)
- Find files by name recursively
- Change file permissions (chmod)
- Print process list (like `ps`)
- Interactive menu (TUI) if run without arguments

## Usage

### Interactive Mode

Run without arguments to use the TUI menu:

```sh
./main
```

### Command-line Options

```
Usage: ./main [-l] [-c <dir>] [-p] [-h]
Options:
  -l         List files in the current directory (with size and owner)
  -c <dir>   Spawn a shell in directory <dir>
  -p         Print the current working directory
  -h         Show this help message
  -w         Print the current user
  -m <dir>   Create a directory named <dir>
  -r <file/dir>   Remove file or directory recursively
  -x <prog>  Show full path of executable <prog> (like 'which')
  -z         Clear the terminal screen (like 'clear')
  -a <src> <dest>  Copy file from <src> to <dest>
  -v <src> <dest>  Move (rename) file from <src> to <dest>
  -t <file>        Print contents of <file> (cat)
  -d <path>        Show disk usage of <path> (du)
  -e               Print all environment variables (env)
  -u <file>        Touch file (create or update timestamp)
  -f <dir> <name>  Find file named <name> under <dir>
  -o <file> <mode> Change file permissions (chmod, octal)
  -s               Print process list (ps)
```

## Build

To build:

```sh
gcc -o main main.c
```

## License

gpl v3

---

**Author:** Your Name  
**Project Home:** [your repository or website link]
