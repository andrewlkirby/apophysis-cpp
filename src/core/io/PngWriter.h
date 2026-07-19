#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace apo {

// Writes an 8-bit-per-channel image to a PNG file via libpng.
//
// `pixels` must be row-major, top-to-bottom, `width*height*channels` bytes
// - the same layout Renderer::render() produces in RenderedImage::pixels
// (see render/Renderer.h), though this doesn't depend on that type: PNG
// writing doesn't need to know how the buffer was produced.
//
// channels must be 3 (RGB) or 4 (RGBA, straight/unpremultiplied alpha -
// matches Flame::transparency's output convention in Renderer.cpp).
//
// Returns false on any I/O or encoding failure (bad path, libpng error,
// unsupported channel count) - the file is not left partially written in
// that case (writePng always writes to a temporary path first, then
// renames into place - see PngWriter.cpp).
bool writePng(const std::string& path, int width, int height, int channels, const std::uint8_t* pixels);

// Reads a PNG file back into raw 8-bit pixel bytes (row-major, top-to-
// bottom), converting whatever the file's own format is (palette,
// grayscale, 16-bit, etc.) into a uniform RGB or RGBA layout - added
// alongside writePng for Phase 5's regression-baseline harness (see
// tests/baseline_regression_test.cpp), which needs to read back the
// checked-in reference images it compares fresh renders against.
//
// Always decodes to RGBA (4 channels) for simplicity - a baseline PNG
// written by writePng with channels=3 still round-trips correctly, just
// with an all-255 alpha channel added back on read. Returns false (leaving
// the outputs unmodified) on any I/O or decoding failure.
bool readPng(const std::string& path, int& outWidth, int& outHeight, std::vector<std::uint8_t>& outPixelsRgba);

} // namespace apo
