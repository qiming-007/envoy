#include "source/extensions/compression/zstd/compressor/zstd_compressor_impl.h"

#include "source/common/buffer/buffer_impl.h"

namespace Envoy {
namespace Extensions {
namespace Compression {
namespace Zstd {
namespace Compressor {

SINGLETON_MANAGER_REGISTRATION(qat_starter);

ZstdCompressorImpl::ZstdCompressorImpl(uint32_t compression_level, bool enable_checksum,
                                       uint32_t strategy, const ZstdCDictManagerPtr& cdict_manager,
                                       uint32_t chunk_size, bool enable_qat_zstd,
                                       uint32_t qat_zstd_fallback_threshold,
                                       void* sequenceProducerState)
    : Common::Base(chunk_size), cctx_(ZSTD_createCCtx(), &ZSTD_freeCCtx),
      cdict_manager_(cdict_manager), compression_level_(compression_level),
      enable_qat_zstd_(enable_qat_zstd), qat_zstd_fallback_threshold_(qat_zstd_fallback_threshold),
      sequenceProducerState_(sequenceProducerState) {
  size_t result;
  result = ZSTD_CCtx_setParameter(cctx_.get(), ZSTD_c_checksumFlag, enable_checksum);
  RELEASE_ASSERT(!ZSTD_isError(result), "");

  result = ZSTD_CCtx_setParameter(cctx_.get(), ZSTD_c_strategy, strategy);
  RELEASE_ASSERT(!ZSTD_isError(result), "");

  ENVOY_LOG(debug,
            "zstd new ZstdCompressorImpl, compression_level: {}, strategy: {}, chunk_size: "
            "{}, enable_qat_zstd: {}, qat_zstd_fallback_threshold: {}",
            compression_level, strategy, chunk_size, enable_qat_zstd, qat_zstd_fallback_threshold);
  if (enable_qat_zstd_) {

    /* register qatSequenceProducer */
    ZSTD_registerSequenceProducer(cctx_.get(), sequenceProducerState_, qatSequenceProducer);

    result = ZSTD_CCtx_setParameter(cctx_.get(), ZSTD_c_enableSeqProducerFallback, 1);
    RELEASE_ASSERT(!ZSTD_isError(result), "");
  }

  if (cdict_manager_) {
    ZSTD_CDict* cdict = cdict_manager_->getFirstDictionary();
    result = ZSTD_CCtx_refCDict(cctx_.get(), cdict);
  } else {
    result = ZSTD_CCtx_setParameter(cctx_.get(), ZSTD_c_compressionLevel, compression_level_);
  }
  RELEASE_ASSERT(!ZSTD_isError(result), "");
}

ZstdCompressorImpl::~ZstdCompressorImpl() {
  ENVOY_LOG(debug, "zstd free ZstdCompressorImpl");
}

void ZstdCompressorImpl::compress(Buffer::Instance& buffer,
                                  Envoy::Compression::Compressor::State state) {
  Buffer::OwnedImpl accumulation_buffer;
  ENVOY_LOG(debug, "zstd compress input size {}", buffer.length());
  if (enable_qat_zstd_ && state == Envoy::Compression::Compressor::State::Flush) {
    // Fallback software if input size less than threshold to achieve better performance.
    if (buffer.length() < qat_zstd_fallback_threshold_) {
      ENVOY_LOG(debug, "zstd compress fall back to software");
      ZSTD_registerSequenceProducer(cctx_.get(), nullptr, nullptr);
    }
  }
  for (const Buffer::RawSlice& input_slice : buffer.getRawSlices()) {
    ENVOY_LOG(debug, "zstd compress input slice {}", input_slice.len_);
    if (input_slice.len_ > 0) {
      setInput(input_slice);
      process(accumulation_buffer, ZSTD_e_continue);
      buffer.drain(input_slice.len_);
    }
  }

  ASSERT(buffer.length() == 0);
  buffer.move(accumulation_buffer);

  if (state == Envoy::Compression::Compressor::State::Finish) {
    ENVOY_LOG(debug, "zstd compress state Finish");
    process(buffer, ZSTD_e_end);
  } else {
    ENVOY_LOG(debug, "zstd compress state Flush");
  }
}

void ZstdCompressorImpl::process(Buffer::Instance& output_buffer, ZSTD_EndDirective mode) {
  bool finished;
  do {
    const size_t remaining = ZSTD_compressStream2(cctx_.get(), &output_, &input_, mode);
    getOutput(output_buffer);
    // If we're on the last chunk we're finished when zstd returns 0,
    // which means its consumed all the input AND finished the frame.
    // Otherwise, we're finished when we've consumed all the input.
    finished = (ZSTD_e_end == mode) ? (remaining == 0) : (input_.pos == input_.size);
  } while (!finished);
}

QATStarter::QATStarter() {
  int status = QZSTD_startQatDevice();
  ENVOY_LOG(debug, "zstd QATStarter status: {}", status);
  QZSTD_Status_e status_e = static_cast<QZSTD_Status_e>(status);
  if (status_e == QZSTD_FAIL) {
    throw EnvoyException("Failed to start QAT device.");
  }
}

QATStarter::~QATStarter() {
  /* Stop QAT device, please call this function when
  you won't use QAT anymore or before the process exits */
  ENVOY_LOG(debug, "zstd QZSTD_stopQatDevice");
  QZSTD_stopQatDevice();
}

} // namespace Compressor
} // namespace Zstd
} // namespace Compression
} // namespace Extensions
} // namespace Envoy
