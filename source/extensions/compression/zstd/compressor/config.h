#pragma once

#include "envoy/compression/compressor/factory.h"
#include "envoy/extensions/compression/zstd/compressor/v3/zstd.pb.h"
#include "envoy/extensions/compression/zstd/compressor/v3/zstd.pb.validate.h"
#include "envoy/event/dispatcher.h"
#include "envoy/thread_local/thread_local.h"
#include "qatseqprod.h"

#include "source/common/http/headers.h"
#include "source/extensions/compression/common/compressor/factory_base.h"
#include "source/extensions/compression/zstd/compressor/zstd_compressor_impl.h"

namespace Envoy {
namespace Extensions {
namespace Compression {
namespace Zstd {
namespace Compressor {

// Default threshold for qat_zstd fallback to software.
const uint32_t DefaultQatZstdFallbackThreshold = 4000;

namespace {

const std::string& zstdStatsPrefix() { CONSTRUCT_ON_FIRST_USE(std::string, "zstd."); }
const std::string& zstdExtensionName() {
  CONSTRUCT_ON_FIRST_USE(std::string, "envoy.compression.zstd.compressor");
}

} // namespace

class ZstdCompressorFactory : public Envoy::Compression::Compressor::CompressorFactory {
public:
  ZstdCompressorFactory(const envoy::extensions::compression::zstd::compressor::v3::Zstd& zstd,
                        Event::Dispatcher& dispatcher, Api::Api& api,
                        ThreadLocal::SlotAllocator& tls);

  // Envoy::Compression::Compressor::CompressorFactory
  Envoy::Compression::Compressor::CompressorPtr createCompressor() override;
  const std::string& statsPrefix() const override { return zstdStatsPrefix(); }
  const std::string& contentEncoding() const override {
    return Http::CustomHeaders::get().ContentEncodingValues.Zstd;
  }

private:
  struct QatzstdThreadLocal : public ThreadLocal::ThreadLocalObject {
    QatzstdThreadLocal();
    ~QatzstdThreadLocal() override;
    void* GetQATSession();
    bool initialized_;
    void* sequenceProducerState_;
  };
  const uint32_t compression_level_;
  const bool enable_checksum_;
  const uint32_t strategy_;
  const uint32_t chunk_size_;
  const bool enable_qat_zstd_;
  const uint32_t qat_zstd_fallback_threshold_;
  ZstdCDictManagerPtr cdict_manager_{nullptr};
  ThreadLocal::TypedSlotPtr<QatzstdThreadLocal> tls_slot_;
};

class ZstdCompressorLibraryFactory
    : public Compression::Common::Compressor::CompressorLibraryFactoryBase<
          envoy::extensions::compression::zstd::compressor::v3::Zstd> {
public:
  ZstdCompressorLibraryFactory() : CompressorLibraryFactoryBase(zstdExtensionName()) {}

private:
  Envoy::Compression::Compressor::CompressorFactoryPtr createCompressorFactoryFromProtoTyped(
      const envoy::extensions::compression::zstd::compressor::v3::Zstd& config,
      Server::Configuration::FactoryContext& context) override;
};

DECLARE_FACTORY(ZstdCompressorLibraryFactory);

} // namespace Compressor
} // namespace Zstd
} // namespace Compression
} // namespace Extensions
} // namespace Envoy
