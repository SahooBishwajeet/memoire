# Memoire

A simple, single-file command-line key-value store utility written in C. Perfect for quick access to frequently used text snippets, passwords, commands, or any other key-value pairs through application launchers like rofi or dmenu.

## Features

- **Simple text-based storage**: Store key-value pairs in a human-readable format
- **Multiple operations**: List, get, set, update, and delete entries
- **Fuzzy search**: Find entries with partial or out-of-order character matching
- **Atomic file operations**: Safe concurrent access with temporary file writes
- **Confirmation prompts**: Optional confirmations for destructive operations
- **Memory safe**: Proper memory management with no leaks
- **Error handling**: Comprehensive error checking for file operations

## Installation

```bash
# Compile the program
gcc -o memoire memoire.c

# Or use the provided build script
./build.sh
```

## Usage

### Basic Commands

```bash
# List all entries
./memoire

# Get a specific key (supports fuzzy search)
./memoire get mykey

# Set a key-value pair (creates or overwrites)
./memoire set mykey "my value here"

# Update existing key only (won't create new)
./memoire update mykey "new value"

# Delete a key
./memoire delete mykey
```

### Options

```bash
# Use custom data file
./memoire -f /path/to/custom.txt get mykey

# Skip confirmation prompts
./memoire -y delete mykey

# Show help
./memoire -h
```

## Data Format

The data is stored in a simple text file (`data.txt` by default) with one entry per line:

```
email:user@example.com
password:my_secret_123
random:some random text here
command:git log --oneline --graph
```

## Fuzzy Search

The `get` command supports fuzzy searching, allowing you to find entries with partial matches:

For a key like "random":

- `ran` will match
- `dom` will match
- `rnd` will match
- `rdm` will match

Characters must appear in order but don't need to be consecutive.

## Examples

```bash
# Initial setup
./memoire set email "john@example.com"
./memoire set password "super_secret_123"
./memoire set git-command "git log --oneline --decorate"

# List all entries
./memoire
# Output:
# email: john@example.com
# password: super_secret_123
# git-command: git log --oneline --decorate

# Fuzzy search examples
./memoire get mail        # Finds "email"
./memoire get pass        # Finds "password"
./memoire get git         # Finds "git-command"

# Update existing entry
./memoire update email "john.doe@company.com"

# Delete entry
./memoire delete password
```

## Integration with Launchers

Memoire is designed to integrate well with application launchers:

### Basic Integration

```bash
# Get all keys for selection
./memoire | rofi -dmenu -p "Select:" | cut -d ":" -f 2-

# Or with dmenu
./memoire | dmenu -p "Select:" | cut -d ":" -f 2-

# Or pipe it to wl-copy
./memoire | rofi -dmenu -p "Select:" | cut -d ":" -f 2- | wl-copy
```

## License

This project is released under the [MIT License](LICENSE).
