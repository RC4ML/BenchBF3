// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: dma_connect.proto

#include "dma_connect.pb.h"

#include <algorithm>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
// @@protoc_insertion_point(includes)
#include <google/protobuf/port_def.inc>

PROTOBUF_PRAGMA_INIT_SEG

namespace _pb = ::PROTOBUF_NAMESPACE_ID;
namespace _pbi = _pb::internal;

namespace dma_connect {
PROTOBUF_CONSTEXPR doca_conn_info::doca_conn_info(
    ::_pbi::ConstantInitialized)
  : exports_()
  , buffers_ptr_()
  , _buffers_ptr_cached_byte_size_(0)
  , buffers_size_()
  , _buffers_size_cached_byte_size_(0){}
struct doca_conn_infoDefaultTypeInternal {
  PROTOBUF_CONSTEXPR doca_conn_infoDefaultTypeInternal()
      : _instance(::_pbi::ConstantInitialized{}) {}
  ~doca_conn_infoDefaultTypeInternal() {}
  union {
    doca_conn_info _instance;
  };
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 doca_conn_infoDefaultTypeInternal _doca_conn_info_default_instance_;
}  // namespace dma_connect
namespace dma_connect {

// ===================================================================

class doca_conn_info::_Internal {
 public:
};

doca_conn_info::doca_conn_info(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::MessageLite(arena, is_message_owned),
  exports_(arena),
  buffers_ptr_(arena),
  buffers_size_(arena) {
  SharedCtor();
  // @@protoc_insertion_point(arena_constructor:dma_connect.doca_conn_info)
}
doca_conn_info::doca_conn_info(const doca_conn_info& from)
  : ::PROTOBUF_NAMESPACE_ID::MessageLite(),
      exports_(from.exports_),
      buffers_ptr_(from.buffers_ptr_),
      buffers_size_(from.buffers_size_) {
  _internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
  // @@protoc_insertion_point(copy_constructor:dma_connect.doca_conn_info)
}

inline void doca_conn_info::SharedCtor() {
}

doca_conn_info::~doca_conn_info() {
  // @@protoc_insertion_point(destructor:dma_connect.doca_conn_info)
  if (auto *arena = _internal_metadata_.DeleteReturnArena<std::string>()) {
  (void)arena;
    return;
  }
  SharedDtor();
}

inline void doca_conn_info::SharedDtor() {
  GOOGLE_DCHECK(GetArenaForAllocation() == nullptr);
}

void doca_conn_info::SetCachedSize(int size) const {
  _cached_size_.Set(size);
}

void doca_conn_info::Clear() {
// @@protoc_insertion_point(message_clear_start:dma_connect.doca_conn_info)
  uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  exports_.Clear();
  buffers_ptr_.Clear();
  buffers_size_.Clear();
  _internal_metadata_.Clear<std::string>();
}

const char* doca_conn_info::_InternalParse(const char* ptr, ::_pbi::ParseContext* ctx) {
#define CHK_(x) if (PROTOBUF_PREDICT_FALSE(!(x))) goto failure
  while (!ctx->Done(&ptr)) {
    uint32_t tag;
    ptr = ::_pbi::ReadTag(ptr, &tag);
    switch (tag >> 3) {
      // repeated bytes exports = 1;
      case 1:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 10)) {
          ptr -= 1;
          do {
            ptr += 1;
            auto str = _internal_add_exports();
            ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
            CHK_(ptr);
            if (!ctx->DataAvailable(ptr)) break;
          } while (::PROTOBUF_NAMESPACE_ID::internal::ExpectTag<10>(ptr));
        } else
          goto handle_unusual;
        continue;
      // repeated uint64 buffers_ptr = 2;
      case 2:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 18)) {
          ptr = ::PROTOBUF_NAMESPACE_ID::internal::PackedUInt64Parser(_internal_mutable_buffers_ptr(), ptr, ctx);
          CHK_(ptr);
        } else if (static_cast<uint8_t>(tag) == 16) {
          _internal_add_buffers_ptr(::PROTOBUF_NAMESPACE_ID::internal::ReadVarint64(&ptr));
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      // repeated uint64 buffers_size = 3;
      case 3:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 26)) {
          ptr = ::PROTOBUF_NAMESPACE_ID::internal::PackedUInt64Parser(_internal_mutable_buffers_size(), ptr, ctx);
          CHK_(ptr);
        } else if (static_cast<uint8_t>(tag) == 24) {
          _internal_add_buffers_size(::PROTOBUF_NAMESPACE_ID::internal::ReadVarint64(&ptr));
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      default:
        goto handle_unusual;
    }  // switch
  handle_unusual:
    if ((tag == 0) || ((tag & 7) == 4)) {
      CHK_(ptr);
      ctx->SetLastTag(tag);
      goto message_done;
    }
    ptr = UnknownFieldParse(
        tag,
        _internal_metadata_.mutable_unknown_fields<std::string>(),
        ptr, ctx);
    CHK_(ptr != nullptr);
  }  // while
message_done:
  return ptr;
failure:
  ptr = nullptr;
  goto message_done;
#undef CHK_
}

uint8_t* doca_conn_info::_InternalSerialize(
    uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:dma_connect.doca_conn_info)
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  // repeated bytes exports = 1;
  for (int i = 0, n = this->_internal_exports_size(); i < n; i++) {
    const auto& s = this->_internal_exports(i);
    target = stream->WriteBytes(1, s, target);
  }

  // repeated uint64 buffers_ptr = 2;
  {
    int byte_size = _buffers_ptr_cached_byte_size_.load(std::memory_order_relaxed);
    if (byte_size > 0) {
      target = stream->WriteUInt64Packed(
          2, _internal_buffers_ptr(), byte_size, target);
    }
  }

  // repeated uint64 buffers_size = 3;
  {
    int byte_size = _buffers_size_cached_byte_size_.load(std::memory_order_relaxed);
    if (byte_size > 0) {
      target = stream->WriteUInt64Packed(
          3, _internal_buffers_size(), byte_size, target);
    }
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(_internal_metadata_.unknown_fields<std::string>(::PROTOBUF_NAMESPACE_ID::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::PROTOBUF_NAMESPACE_ID::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:dma_connect.doca_conn_info)
  return target;
}

size_t doca_conn_info::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:dma_connect.doca_conn_info)
  size_t total_size = 0;

  uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // repeated bytes exports = 1;
  total_size += 1 *
      ::PROTOBUF_NAMESPACE_ID::internal::FromIntSize(exports_.size());
  for (int i = 0, n = exports_.size(); i < n; i++) {
    total_size += ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::BytesSize(
      exports_.Get(i));
  }

  // repeated uint64 buffers_ptr = 2;
  {
    size_t data_size = ::_pbi::WireFormatLite::
      UInt64Size(this->buffers_ptr_);
    if (data_size > 0) {
      total_size += 1 +
        ::_pbi::WireFormatLite::Int32Size(static_cast<int32_t>(data_size));
    }
    int cached_size = ::_pbi::ToCachedSize(data_size);
    _buffers_ptr_cached_byte_size_.store(cached_size,
                                    std::memory_order_relaxed);
    total_size += data_size;
  }

  // repeated uint64 buffers_size = 3;
  {
    size_t data_size = ::_pbi::WireFormatLite::
      UInt64Size(this->buffers_size_);
    if (data_size > 0) {
      total_size += 1 +
        ::_pbi::WireFormatLite::Int32Size(static_cast<int32_t>(data_size));
    }
    int cached_size = ::_pbi::ToCachedSize(data_size);
    _buffers_size_cached_byte_size_.store(cached_size,
                                    std::memory_order_relaxed);
    total_size += data_size;
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::PROTOBUF_NAMESPACE_ID::internal::GetEmptyString).size();
  }
  int cached_size = ::_pbi::ToCachedSize(total_size);
  SetCachedSize(cached_size);
  return total_size;
}

void doca_conn_info::CheckTypeAndMergeFrom(
    const ::PROTOBUF_NAMESPACE_ID::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const doca_conn_info*>(
      &from));
}

void doca_conn_info::MergeFrom(const doca_conn_info& from) {
// @@protoc_insertion_point(class_specific_merge_from_start:dma_connect.doca_conn_info)
  GOOGLE_DCHECK_NE(&from, this);
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  exports_.MergeFrom(from.exports_);
  buffers_ptr_.MergeFrom(from.buffers_ptr_);
  buffers_size_.MergeFrom(from.buffers_size_);
  _internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void doca_conn_info::CopyFrom(const doca_conn_info& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:dma_connect.doca_conn_info)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool doca_conn_info::IsInitialized() const {
  return true;
}

void doca_conn_info::InternalSwap(doca_conn_info* other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  exports_.InternalSwap(&other->exports_);
  buffers_ptr_.InternalSwap(&other->buffers_ptr_);
  buffers_size_.InternalSwap(&other->buffers_size_);
}

std::string doca_conn_info::GetTypeName() const {
  return "dma_connect.doca_conn_info";
}


// @@protoc_insertion_point(namespace_scope)
}  // namespace dma_connect
PROTOBUF_NAMESPACE_OPEN
template<> PROTOBUF_NOINLINE ::dma_connect::doca_conn_info*
Arena::CreateMaybeMessage< ::dma_connect::doca_conn_info >(Arena* arena) {
  return Arena::CreateMessageInternal< ::dma_connect::doca_conn_info >(arena);
}
PROTOBUF_NAMESPACE_CLOSE

// @@protoc_insertion_point(global_scope)
#include <google/protobuf/port_undef.inc>
