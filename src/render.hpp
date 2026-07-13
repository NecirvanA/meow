#pragma once

#include <cstdint>
#include <vector>

namespace render {

// Tries, in order, kitty's icat kitten, wezterm imgcat, viu and chafa to
// print `bytes` (raw image file contents) into the terminal at the given
// size. Returns false if none of those tools are available.
bool render_image(const std::vector<uint8_t> &bytes, int cols, int rows, bool interactive, bool upscale);

} // namespace render
