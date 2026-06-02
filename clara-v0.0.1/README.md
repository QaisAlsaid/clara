# CL-[Ara](https://en.wikipedia.org/wiki/Ara_(constellation)) ✨

Clara is a single-header C++23 command-line parser that’s light, powerful, and actually fun to use.
it's UTF-8 friendly, and skips the build-system hassle.

## Why Clara?

* Single-header simplicity: Drop clara.hpp in your project and go. No CMake voodoo needed (but we’ve got you covered if you want it).
* Unicode support: UTF-8 support for flags like -😎 or options like --café=latte.
* Modern C++23 vibes: Clean, expressive, and ready for the future.
* Builder-pattern flow: Define your CLI without the headache.
* Cross-platform: Works everywhere… mostly. (Windows has some quirks—see below.)

## Installation
Clara’s a one-file deal. Grab clara.hpp and you’re set. That’s it!

### The Manual Way:
* Copy clara.hpp into your project.
* `#include "path/to/clara.hpp"`.
### Git Submodule + CMake Way (if you’re feeling fancy):

``` bash
git submodule add https://github.com/QaisAlsaid/clara.git vendor/clara
```

In your CMakeLists.txt:
```cmake
add_subdirectory(vendor/clara)  # Optional: catches C++ version mismatches
target_include_directories(my_target PRIVATE vendor/clara/include)
```

## Quick Start
Here’s how to wield Clara’s parsing powers:

```cpp
#include <clara/clara.hpp>
#include <iostream>

int main(int argc, char** argv) {
    clara::parser cmd;

    // Define a flag and an option
    cmd.add_flag("v");  // e.g., -v for verbose
    cmd.add_option("name").requires_value();  // e.g., --name Alice

    // Parse those args!
    auto result = cmd.parse(argc, argv);

    if (result.has_error()) {
        std::cerr << "Oops! Something’s wonky with your args.\n";
        return 64;  // POSIX "usage error" exit code
    }

    // Check the flag
    if (result.root.get_flag("v")) {
        std::cout << "Verbose mode: ON\n";
    }

    // Grab the option
    if (auto name = result.root.get_option("name")) {
        if (auto value = name->get<std::string>()) {
            std::cout << "Hello, " << *value << "!\n";
        }
    }

    return 0;
}
```
Run it:

```bash
./myapp -v --name "Clara Fan"
# Output:
# Verbose mode: ON
# Hello, Clara Fan!
```

### Some Examples
Flags: Solo or Grouped
```cpp
clara::parser cmd;

// Define the flags
cmd.add_flag("a");
cmd.add_flag("b");
cmd.add_flag("c");

// Parse -abc or -a -b -c
auto result = cmd.parse({"myapp", "-abc"});
if (result.root.get_flag("a")) std::cout << "A’s here!\n";
if (result.root.get_flag("b")) std::cout << "B’s also!\n";
if (result.root.get_flag("c")) std::cout << "C’s lastly!\n";
```

Options with Values
```cpp
clara::parser cmd;
cmd.add_option("count").requires_value();

auto result = cmd.parse({"myapp", "--count", "42"});
if (auto opt = result.root.get_option("count")) {
    if (auto num = opt->get<int>()) {
        std::cout << "Count: " << *num << "\n";  // Count: 42
    }
}
```

Subcommands (Git-Style)

```cpp

clara::parser cmd;
auto& sub = cmd.add_subcommand("say");
sub.add_option("msg").requires_value();

auto result = cmd.parse({"myapp", "say", "--msg", "Hello, universe!"});
if (auto say = result.root.get_command("say")) {
    if (auto msg = say->get_option("msg")) {
        std::cout << msg->get_raw() << "\n";  // Hello, universe!
    }
}
```
## Coming Soon
* Docs: Full breakdown of Clara’s tricks.
* Tests: Proving it works (and doesn’t explode) which it does now!.
* Refactor: Splitting parsing and evaluation, to support callbacks and limits (see below).

## Windows Caveat
Clara assumes UTF-8 everywhere, but Windows can be grumpy about it. Use a UTF-8 manifest or tweak your console settings. **We’ll smooth this out eventually!**

## License
Clara’s [MIT licensed](LICENSE)

## The Future: Parser Refactor
Clara’s eyeing a split:

* Parser: Turns args into a neat structure.
* Evaluator: Checks limits, runs callbacks. Think “max 3 args for --list” or “-v triggers a log.” Ideas? Hit me up!
