# Memoire

A simple, single-file command-line key-value store utility written in C. Perfect for quick access to frequently used text snippets, passwords, commands, or any other key-value pairs through application launchers like rofi or dmenu.

> You can also use [memoire.sh](memoire.sh) shell script as an alternative to this C implementation. Edit the script to change the data file location, launcher command, and clipboard command as per your preference.

## Features

- **Simple text-based storage**: Store key-value pairs in a human-readable format
- **Multiple operations**: List, get, set, update, and delete entries
- **Fuzzy search**: Find entries with partial or out-of-order character matching
- **Atomic file operations**: Safe concurrent access with temporary file writes
- **XDG Base Directory compliant**: Stores data in `~/.config/memoire/data.txt`
- **Confirmation prompts**: Optional confirmations for destructive operations
- **Memory safe**: Proper memory management with no leaks
- **Error handling**: Comprehensive error checking for file operations
- **System integration**: Easy installation for system-wide access

## Menu of Contents

- [Installation](#installation)
  - [From Source](#from-source)
  - [Manual Installation](#manual-installation)
- [Man page](#man-page)
- [Uninstallation](#uninstallation)
- [Usage](#usage)
  - [Basic Commands](#basic-commands)
  - [Options](#options)
- [Data Format](#data-format)
- [Fuzzy Search](#fuzzy-search)
- [Examples](#examples)
- [Integration with Launchers](#integration-with-launchers)
  - [Basic Integration](#basic-integration)
  - [Complex Integration](#complex-integration)
- [License](#license)

## Installation

### From Source

```bash
# Clone or download the source
git clone git@github.com:SahooBishwajeet/memoire.git
# OR
git clone https://github.com/SahooBishwajeet/memoire.git

cd memoire

# Build
make

# Install system-wide (requires sudo)
sudo make install

# Or install to user directory
make install PREFIX=~/.local
```

### Manual Installation

```bash
# Compile
gcc -Wall -Wextra -std=c99 -O2 -o memoire memoire.c

# Install to system (requires sudo)
sudo cp memoire /usr/local/bin/

# Or install to user directory
mkdir -p ~/.local/bin
cp memoire ~/.local/bin/
# Make sure ~/.local/bin is in your PATH
```

## Man page

The man page will get installed automatically if you use `make install`. You can view it using:

```bash
# View the man page
man ./memoire.1

# Or format it with groff
groff -man -Tascii memoire.1 | less
```

## Uninstallation

```bash
# If installed via make
sudo make uninstall

# Manual removal
sudo rm /usr/local/bin/memoire
# or
rm ~/.local/bin/memoire

# Remove data (optional)
rm -rf ~/.config/memoire
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
./memoire | rofi -dmenu -p "Memoire:" | cut -d ":" -f 2-

# Or with dmenu
./memoire | dmenu -p "Memoire:" | cut -d ":" -f 2-

# Or pipe it to wl-copy
./memoire | rofi -dmenu -p "Memoire:" | cut -d ":" -f 2- | wl-copy
```

### Complex Integration

```bash
memoire | rofi -dmenu -p "Memoire:" | cut -d ":" -f 2- | wl-copy && notify-send "Memoire" "Copied to clipboard"
```

```bash
memoire | dmenu -p "Memoire:" | cut -d ":" -f 2- | wl-copy && notify-send "Memoire" "Copied to clipboard"
```

```bash
#!/bin/bash

selected=$(memoire | rofi -dmenu -p "Memoire:" -format "s")
if [ -n "$selected" ]; then
    value=$(echo "$selected" | cut -d':' -f2-)
    echo -n "$value" | wl-copy
    # echo -n "$value" | xclip
    notify-send "Memoire" "Copied to clipboard: $value"
fi

# Save this file as ~/.local/bin/memoire-rofi or anywhere preferrable
```

```bash
#!/bin/bash

selected=$(memoire | dmenu -p "Memoire:")
if [ -n "$selected" ]; then
    value=$(echo "$selected" | cut -d':' -f2-)
    echo -n "$value" | wl-copy
    # echo -n "$value" | xclip
    notify-send "Memoire" "Copied to clipboard"
fi

# Save this file as ~/.local/bin/memoire-dmenu or anywhere preferrable

```

## License

This project is released under the [MIT License](LICENSE).
