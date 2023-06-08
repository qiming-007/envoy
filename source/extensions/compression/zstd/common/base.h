#pragma once

#include <memory>

#include "envoy/buffer/buffer.h"

#include "zstd.h"
#include "qatseqprod.h"

namespace Envoy {
namespace Extensions {
namespace Compression {
namespace Zstd {
namespace Common {

// Keeps a `Zstd` compression stream's state.
struct Base {
  Base(uint32_t chunk_size);

protected:
  void setInput(const uint8_t* input, size_t size);
  void getOutput(Buffer::Instance& output_buffer);

  uint64_t chunk_size_;
  std::unique_ptr<uint8_t[]> chunk_ptr_;
  std::unique_ptr<uint8_t[]> input_ptr_;
  uint64_t input_len_;
  ZSTD_outBuffer output_;
  ZSTD_inBuffer input_;
  unsigned dictionary_id_{0};
};

} // namespace Common
} // namespace Zstd
} // namespace Compression
} // namespace Extensions
} // namespace Envoy
