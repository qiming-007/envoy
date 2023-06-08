#include "source/extensions/compression/zstd/common/base.h"

namespace Envoy {
namespace Extensions {
namespace Compression {
namespace Zstd {
namespace Common {

Base::Base(uint32_t chunk_size)
    : chunk_size_(chunk_size), chunk_ptr_{std::make_unique<uint8_t[]>(chunk_size)},
      input_ptr_{std::make_unique<uint8_t[]>(chunk_size)},
      input_len_(0), output_{chunk_ptr_.get(), chunk_size, 0}, input_{nullptr, 0, 0} {}

void Base::setInput(const uint8_t* input, size_t size) {
  input_.src = input;
  input_.pos = 0;
  input_.size = size;
  input_len_ = 0;
}

void Base::getOutput(Buffer::Instance& output_buffer) {
  if (output_.pos > 0) {
    output_buffer.add(static_cast<void*>(chunk_ptr_.get()), output_.pos);
    output_.pos = 0;
    output_.dst = chunk_ptr_.get();
  }
}

} // namespace Common
} // namespace Zstd
} // namespace Compression
} // namespace Extensions
} // namespace Envoy
