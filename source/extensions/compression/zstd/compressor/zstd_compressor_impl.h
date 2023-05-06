#pragma once

#include "envoy/compression/compressor/compressor.h"

#include "source/common/common/logger.h"
#include "source/extensions/compression/zstd/common/base.h"
#include "source/extensions/compression/zstd/common/dictionary_manager.h"

namespace Envoy {
namespace Extensions {
namespace Compression {
namespace Zstd {
namespace Compressor {

using ZstdCDictManager =
    Common::DictionaryManager<ZSTD_CDict, ZSTD_freeCDict, ZSTD_getDictID_fromCDict>;
using ZstdCDictManagerPtr = std::unique_ptr<ZstdCDictManager>;

/**
 * Implementation of compressor's interface.
 */
class ZstdCompressorImpl : public Common::Base,
                           public Envoy::Compression::Compressor::Compressor,
                           public Logger::Loggable<Logger::Id::compression>,
                           NonCopyable {
public:
  ZstdCompressorImpl(uint32_t compression_level, bool enable_checksum, uint32_t strategy,
                     const ZstdCDictManagerPtr& cdict_manager, uint32_t chunk_size,
                     bool enable_qat_zstd, uint32_t qat_zstd_fallback_threshold);
  ~ZstdCompressorImpl() override;

  // Compression::Compressor::Compressor
  void compress(Buffer::Instance& buffer, Envoy::Compression::Compressor::State state) override;

private:
  void process(Buffer::Instance& output_buffer, ZSTD_EndDirective mode);
  std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> cctx_;
  void* sequenceProducerState_;
  const ZstdCDictManagerPtr& cdict_manager_;
  const uint32_t compression_level_;
  bool enable_qat_zstd_;
  const uint32_t qat_zstd_fallback_threshold_;
};

} // namespace Compressor
} // namespace Zstd
} // namespace Compression
} // namespace Extensions
} // namespace Envoy
