# meow

Prints cat pictures in your terminal. Built in C++ using libcurl and
[TheCatAPI](https://thecatapi.com/).

Directly inspired by [waifu-cli](https://github.com/izeperson/waifu-cli), same idea, cats instead.

## Dependencies

- CMake >= 3.15
- A C++17 compiler
- libcurl (`curl` on Arch)
- An image-capable terminal or CLI viewer: [kitty](https://sw.kovidgoyal.net/kitty/), [WezTerm](https://wezterm.org/), [viu](https://github.com/atanunq/viu), or [chafa](https://hpjansson.org/chafa/)

`nlohmann/json` is vendored as a single header in `third_party/`, no extra install needed.

## Build

```sh
git clone <this repo>
cd meow
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The binary is at `build/meow`. Symlink it onto your `PATH`:

```sh
mkdir -p "$HOME/.local/bin"
ln -sf "$PWD/build/meow" "$HOME/.local/bin/meow"
```

## Usage

```
Usage: meow <command>

Commands:
  -c, --category <name>   Fetch an image from a specific category
  -n, --batch <amount>    Use '-n <amount>' after category to batch download (e.g. -c boxes -n 50)
  -l, --list              List all available categories
  -r, --random            Fetch a random image from any category
  -v, --version           Show version information
  -o                      Open a random image URL in the default system viewer
  -t, --test              Test connectivity
  --min-size <KB>         Filter batch downloads by minimum file size
  --min-width <pixels>    Filter batch downloads by minimum width
  --min-height <pixels>   Filter batch downloads by minimum height
  --no-upscale            Don't upscale small images to fit the terminal
  --check-links           Perform a deep check of category endpoints
  -h, --help              Show this help message
```

Running `meow` with no arguments shows a random cat picture from any category.

While an image is shown: `s` saves it to the working directory, `u` prints its
URL, `a` shows id/size/category info, `o` opens it in the browser, `n` fetches
another, and `q`/Enter quits.

Set `CAT_API_KEY` in your environment to use your own TheCatAPI key for higher
rate limits (not required for normal use).
