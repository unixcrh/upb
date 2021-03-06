//
// upb - a minimalist implementation of protocol buffers.
//
// Copyright (c) 2011-2012 Google Inc.  See LICENSE for details.
// Author: Josh Haberman <jhaberman@gmail.com>
//
// Note that we have received an exception from c-style-artiters regarding
// dynamic_cast<> in this file:
// https://groups.google.com/a/google.com/d/msg/c-style/7Zp_XCX0e7s/I6dpzno4l-MJ
//
// IMPORTANT NOTE!  This file is compiled TWICE, once with UPB_GOOGLE3 defined
// and once without!  This allows us to provide functionality against proto2
// and protobuf opensource both in a single binary without the two conflicting.
// However we must be careful not to violate the ODR.

#include "upb/google/proto2.h"

#include "upb/google/proto1.h"
#include "upb/bytestream.h"
#include "upb/def.h"
#include "upb/handlers.h"

namespace upb {
namespace proto2_bridge_google3 { class FieldAccessor; }
namespace proto2_bridge_opensource { class FieldAccessor; }
}  // namespace upb

// BEGIN DOUBLE COMPILATION TRICKERY. //////////////////////////////////////////

#ifdef UPB_GOOGLE3

// TODO(haberman): friend upb so that this isn't required.
#define protected public
#include "net/proto2/public/repeated_field.h"
#undef protected

#define private public
#include "net/proto2/public/generated_message_reflection.h"
#undef private

#include "net/proto2/proto/descriptor.pb.h"
#include "net/proto2/public/descriptor.h"
#include "net/proto2/public/lazy_field.h"
#include "net/proto2/public/message.h"
#include "net/proto2/public/string_piece_field_support.h"
#include "upb/google/cord.h"

namespace goog = ::proto2;
namespace me = ::upb::proto2_bridge_google3;

#else

// TODO(haberman): friend upb so that this isn't required.
#define protected public
#include "google/protobuf/repeated_field.h"
#undef protected

#define private public
#include "google/protobuf/generated_message_reflection.h"
#undef private

#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/message.h"

namespace goog = ::google::protobuf;
namespace me = ::upb::proto2_bridge_opensource;

#endif  // ifdef UPB_GOOGLE3

// END DOUBLE COMPILATION TRICKERY. ////////////////////////////////////////////

// Have to define this manually since older versions of proto2 didn't define
// an enum value for STRING.
#define UPB_CTYPE_STRING 0

template<class T> static T* GetPointer(void *message, size_t offset) {
  return reinterpret_cast<T*>(static_cast<char*>(message) + offset);
}

// This class contains handlers that can write into a proto2 class whose
// reflection class is GeneratedMessageReflection.  (Despite the name, even
// DynamicMessage uses GeneratedMessageReflection, so this covers all proto2
// messages generated by the compiler.)  To do this it must break the
// encapsulation of GeneratedMessageReflection and therefore depends on
// internal interfaces that are not guaranteed to be stable.  This class will
// need to be updated if any non-backward-compatible changes are made to
// GeneratedMessageReflection.
//
// TODO(haberman): change class name?  In retrospect, "FieldAccessor" isn't the
// best (something more specific like GeneratedMessageReflectionHandlers or
// GMR_Handlers would be better) but we're depending on a "friend" declaration
// in proto2 that already specifies "FieldAccessor."  No versions of proto2 have
// been released that include the "friend FieldAccessor" declaration, so there's
// still time to change this.  On the other hand, perhaps it's simpler to just
// rely on "#define private public" since it may be a long time before new
// versions of proto2 open source are pervasive enough that we can remove this
// anyway.
class me::FieldAccessor {
 public:
  // Returns true if we were able to set an accessor and any other properties
  // of the FieldDef that are necessary to read/write this field to a
  // proto2::Message.
  static bool TrySet(const goog::FieldDescriptor* proto2_f,
                     const goog::Message& m,
                     const upb::FieldDef* upb_f, upb::Handlers* h) {
    const goog::Reflection* base_r = m.GetReflection();
    // See file comment re: dynamic_cast.
    const goog::internal::GeneratedMessageReflection* r =
        dynamic_cast<const goog::internal::GeneratedMessageReflection*>(base_r);
    if (!r) return false;
    // Extensions not supported yet.
    if (proto2_f->is_extension()) return false;

    switch (proto2_f->cpp_type()) {
#define PRIMITIVE_TYPE(cpptype, cident) \
      case goog::FieldDescriptor::cpptype: \
        SetPrimitiveHandlers<cident>(proto2_f, r, upb_f, h); return true;
      PRIMITIVE_TYPE(CPPTYPE_INT32,  int32_t);
      PRIMITIVE_TYPE(CPPTYPE_INT64,  int64_t);
      PRIMITIVE_TYPE(CPPTYPE_UINT32, uint32_t);
      PRIMITIVE_TYPE(CPPTYPE_UINT64, uint64_t);
      PRIMITIVE_TYPE(CPPTYPE_DOUBLE, double);
      PRIMITIVE_TYPE(CPPTYPE_FLOAT,  float);
      PRIMITIVE_TYPE(CPPTYPE_BOOL,   bool);
#undef PRIMITIVE_TYPE
      case goog::FieldDescriptor::CPPTYPE_ENUM:
        SetEnumHandlers(proto2_f, r, upb_f, h);
        return true;
      case goog::FieldDescriptor::CPPTYPE_STRING: {
        // Old versions of the open-source protobuf release erroneously default
        // to Cord even though that has never been supported in the open-source
        // release.
        int32_t ctype = proto2_f->options().has_ctype() ?
            proto2_f->options().ctype() : UPB_CTYPE_STRING;
        switch (ctype) {
#ifdef UPB_GOOGLE3
          case goog::FieldOptions::STRING:
            SetStringHandlers<string>(proto2_f, m, r, upb_f, h);
            return true;
          case goog::FieldOptions::CORD:
            SetCordHandlers(proto2_f, r, upb_f, h);
            return true;
          case goog::FieldOptions::STRING_PIECE:
            SetStringPieceHandlers(proto2_f, r, upb_f, h);
            return true;
#else
          case UPB_CTYPE_STRING:
            SetStringHandlers<std::string>(proto2_f, m, r, upb_f, h);
            return true;
#endif
          default:
            return false;
        }
      }
      case goog::FieldDescriptor::CPPTYPE_MESSAGE:
#ifdef UPB_GOOGLE3
        if (proto2_f->options().lazy()) {
          return false;  // Not yet implemented.
        } else {
          SetSubMessageHandlers(proto2_f, m, r, upb_f, h);
          return true;
        }
#else
        SetSubMessageHandlers(proto2_f, m, r, upb_f, h);
        return true;
#endif
      default:
        return false;
    }
  }

  static const goog::Message* GetFieldPrototype(
      const goog::Message& m,
      const goog::FieldDescriptor* f) {
    // We assume that all submessages (and extensions) will be constructed
    // using the same MessageFactory as this message.  This doesn't cover the
    // case of CodedInputStream::SetExtensionRegistry().
    // See file comment re: dynamic_cast.
    const goog::internal::GeneratedMessageReflection* r =
        dynamic_cast<const goog::internal::GeneratedMessageReflection*>(
            m.GetReflection());
    if (!r) return NULL;
    return r->message_factory_->GetPrototype(f->message_type());
  }

 private:
  static upb_selector_t GetSelector(const upb::FieldDef* f,
                                    upb::Handlers::Type type) {
    upb::Handlers::Selector selector;
    bool ok = upb::Handlers::GetSelector(f, type, &selector);
    UPB_ASSERT_VAR(ok, ok);
    return selector;
  }

  static int64_t GetHasbit(
      const goog::FieldDescriptor* f,
      const goog::internal::GeneratedMessageReflection* r) {
    // proto2 does not store hasbits for repeated fields.
    assert(!f->is_repeated());
    return (r->has_bits_offset_ * 8) + f->index();
  }

  static uint16_t GetOffset(
      const goog::FieldDescriptor* f,
      const goog::internal::GeneratedMessageReflection* r) {
    return r->offsets_[f->index()];
  }

  class FieldOffset {
   public:
    FieldOffset(
        const goog::FieldDescriptor* f,
        const goog::internal::GeneratedMessageReflection* r)
        : offset_(GetOffset(f, r)),
          is_repeated_(f->is_repeated()) {
      if (!is_repeated_) {
        int64_t hasbit = GetHasbit(f, r);
        hasbyte_ = hasbit / 8;
        mask_ = 1 << (hasbit % 8);
      }
    }

    template<class T> T* GetFieldPointer(void *message) const {
      return GetPointer<T>(message, offset_);
    }

    void SetHasbit(void* m) const {
      assert(!is_repeated_);
      uint8_t* byte = GetPointer<uint8_t>(m, hasbyte_);
      *byte |= mask_;
    }

   private:
    const size_t offset_;
    bool is_repeated_;

    // Only for non-repeated fields.
    int32_t hasbyte_;
    int8_t mask_;
  };

  // StartSequence /////////////////////////////////////////////////////////////

  static void SetStartSequenceHandler(
      const goog::FieldDescriptor* proto2_f,
      const goog::internal::GeneratedMessageReflection* r,
      const upb::FieldDef* f, upb::Handlers* h) {
    assert(f->IsSequence());
    h->SetStartSequenceHandler(
        f, &PushOffset, new FieldOffset(proto2_f, r),
        &upb::DeletePointer<FieldOffset>);
  }

  static void* PushOffset(void *m, void *fval) {
    const FieldOffset* offset = static_cast<FieldOffset*>(fval);
    return offset->GetFieldPointer<void>(m);
  }

  // Primitive Value (numeric, bool) ///////////////////////////////////////////

  template <typename T> static void SetPrimitiveHandlers(
      const goog::FieldDescriptor* proto2_f,
      const goog::internal::GeneratedMessageReflection* r,
      const upb::FieldDef* f,
      upb::Handlers* h) {
    if (f->IsSequence()) {
      SetStartSequenceHandler(proto2_f, r, f, h);
      h->SetValueHandler<T>(f, &AppendPrimitive<T>, NULL, NULL);
    } else {
      upb::SetStoreValueHandler<T>(
          f, GetOffset(proto2_f, r), GetHasbit(proto2_f, r), h);
    }
  }

  template <typename T>
  static bool AppendPrimitive(void *_r, void *fval, T val) {
    UPB_UNUSED(fval);
    goog::RepeatedField<T>* r = static_cast<goog::RepeatedField<T>*>(_r);
    r->Add(val);
    return true;
  }

  // Enum //////////////////////////////////////////////////////////////////////

  class EnumHandlerData : public FieldOffset {
   public:
    EnumHandlerData(
        const goog::FieldDescriptor* proto2_f,
        const goog::internal::GeneratedMessageReflection* r,
        const upb::FieldDef* f)
        : FieldOffset(proto2_f, r),
          field_number_(f->number()),
          unknown_fields_offset_(r->unknown_fields_offset_),
          enum_(upb_downcast_enumdef(f->subdef())) {
    }

    bool IsValidValue(int32_t val) const {
      return enum_->FindValueByNumber(val) != NULL;
    }

    int32_t field_number() const { return field_number_; }

    goog::UnknownFieldSet* mutable_unknown_fields(goog::Message* m) const {
      return GetPointer<goog::UnknownFieldSet>(m, unknown_fields_offset_);
    }

   private:
    int32_t field_number_;
    size_t unknown_fields_offset_;
    const upb::EnumDef* enum_;
  };

  static void SetEnumHandlers(
      const goog::FieldDescriptor* proto2_f,
      const goog::internal::GeneratedMessageReflection* r,
      const upb::FieldDef* f,
      upb::Handlers* h) {
    EnumHandlerData* data = new EnumHandlerData(proto2_f, r, f);
    if (f->IsSequence()) {
      h->SetInt32Handler(
          f, &AppendEnum, data, &upb::DeletePointer<EnumHandlerData>);
    } else {
      h->SetInt32Handler(
          f, &SetEnum, data, &upb::DeletePointer<EnumHandlerData>);
    }
  }

  static bool SetEnum(void *_m, void *fval, int32_t val) {
    goog::Message* m = static_cast<goog::Message*>(_m);
    const EnumHandlerData* data = static_cast<const EnumHandlerData*>(fval);
    if (data->IsValidValue(val)) {
      int32_t* message_val = data->GetFieldPointer<int32_t>(m);
      *message_val = val;
      data->SetHasbit(m);
    } else {
      data->mutable_unknown_fields(m)->AddVarint(data->field_number(), val);
    }
    return true;
  }

  static bool AppendEnum(void *_m, void *fval, int32_t val) {
    // Closure is the enclosing message.  We can't use the RepeatedField<> as
    // the closure because we need to go back to the message for unrecognized
    // enum values, which go into the unknown field set.
    goog::Message* m = static_cast<goog::Message*>(_m);
    const EnumHandlerData* data = static_cast<const EnumHandlerData*>(fval);
    if (data->IsValidValue(val)) {
      goog::RepeatedField<int32_t>* r =
          data->GetFieldPointer<goog::RepeatedField<int32_t> >(m);
      r->Add(val);
    } else {
      data->mutable_unknown_fields(m)->AddVarint(data->field_number(), val);
    }
    return true;
  }

  // String ////////////////////////////////////////////////////////////////////

  // For scalar (non-repeated) string fields.
  template<class T>
  class StringHandlerData : public FieldOffset {
   public:
    StringHandlerData(const goog::FieldDescriptor* proto2_f,
                      const goog::internal::GeneratedMessageReflection* r,
                      const goog::Message& prototype)
        : FieldOffset(proto2_f, r) {
      // "prototype" isn't guaranteed to be empty, so we create a copy to get
      // the default string instance.
      goog::Message* empty = prototype.New();
      prototype_ = &r->GetStringReference(*empty, proto2_f, NULL);
      delete empty;
    }

    const T* prototype() const { return prototype_; }

    T** GetStringPointer(void *message) const {
      return GetFieldPointer<T*>(message);
    }

   private:
    const T* prototype_;
  };

  template <typename T> static void SetStringHandlers(
      const goog::FieldDescriptor* proto2_f,
      const goog::Message& m,
      const goog::internal::GeneratedMessageReflection* r,
      const upb::FieldDef* f,
      upb::Handlers* h) {
    h->SetStringHandler(f, &OnStringBuf<T>, NULL, NULL);
    if (f->IsSequence()) {
      SetStartSequenceHandler(proto2_f, r, f, h);
      h->SetStartStringHandler(f, &StartRepeatedString<T>, NULL, NULL);
    } else {
      StringHandlerData<T>* data = new StringHandlerData<T>(proto2_f, r, m);
      h->SetStartStringHandler(
          f, &StartString<T>, data, &upb::DeletePointer<StringHandlerData<T> >);
    }
  }

  // This needs to be templated because google3 string is not std::string.
  template <typename T> static void* StartString(
      void *m, void *fval, size_t size_hint) {
    UPB_UNUSED(size_hint);
    const StringHandlerData<T>* data =
        static_cast<const StringHandlerData<T>*>(fval);
    T** str = data->GetStringPointer(m);
    data->SetHasbit(m);
    // If it points to the default instance, we must create a new instance.
    if (*str == data->prototype()) *str = new T();
    (*str)->clear();
    // reserve() here appears to hurt performance rather than help.
    return *str;
  }

  template <typename T> static size_t OnStringBuf(
      void *_str, void *fval, const char *buf, size_t n) {
    UPB_UNUSED(fval);
    T* str = static_cast<T*>(_str);
    str->append(buf, n);
    return n;
  }


  template <typename T>
  static void* StartRepeatedString(void *_r, void *fval, size_t size_hint) {
    UPB_UNUSED(size_hint);
    UPB_UNUSED(fval);
    goog::RepeatedPtrField<T>* r = static_cast<goog::RepeatedPtrField<T>*>(_r);
    T* str = r->Add();
    str->clear();
    // reserve() here appears to hurt performance rather than help.
    return str;
  }

  // SubMessage ////////////////////////////////////////////////////////////////

  class SubMessageHandlerData : public FieldOffset {
   public:
    SubMessageHandlerData(
        const goog::FieldDescriptor* f,
        const goog::internal::GeneratedMessageReflection* r,
        const goog::Message* prototype)
        : FieldOffset(f, r),
          prototype_(prototype) {
    }

    const goog::Message* prototype() const { return prototype_; }

   private:
    const goog::Message* const prototype_;
  };

  static void SetSubMessageHandlers(
      const goog::FieldDescriptor* proto2_f,
      const goog::Message& m,
      const goog::internal::GeneratedMessageReflection* r,
      const upb::FieldDef* f,
      upb::Handlers* h) {
    SubMessageHandlerData* data =
        new SubMessageHandlerData(proto2_f, r, GetFieldPrototype(m, proto2_f));
    upb::Handlers::Free* free = &upb::DeletePointer<SubMessageHandlerData>;
    if (f->IsSequence()) {
      SetStartSequenceHandler(proto2_f, r, f, h);
      h->SetStartSubMessageHandler(f, &StartRepeatedSubMessage, data, free);
    } else {
      h->SetStartSubMessageHandler(f, &StartSubMessage, data, free);
    }
  }

  static void* StartSubMessage(void *m, void *fval) {
    const SubMessageHandlerData* data =
        static_cast<const SubMessageHandlerData*>(fval);
    data->SetHasbit(m);
    goog::Message **subm = data->GetFieldPointer<goog::Message*>(m);
    if (*subm == NULL || *subm == data->prototype()) {
      *subm = data->prototype()->New();
    }
    return *subm;
  }

  class RepeatedMessageTypeHandler {
   public:
    typedef void Type;
    // AddAllocated() calls this, but only if other objects are sitting
    // around waiting for reuse, which we will not do.
    static void Delete(Type* t) {
      (void)t;
      assert(false);
    }
  };

  // Closure is a RepeatedPtrField<SubMessageType>*, but we access it through
  // its base class RepeatedPtrFieldBase*.
  static void* StartRepeatedSubMessage(void* _r, void *fval) {
    const SubMessageHandlerData* data =
        static_cast<const SubMessageHandlerData*>(fval);
    goog::internal::RepeatedPtrFieldBase *r =
        static_cast<goog::internal::RepeatedPtrFieldBase*>(_r);
    void *submsg = r->AddFromCleared<RepeatedMessageTypeHandler>();
    if (!submsg) {
      submsg = data->prototype()->New();
      r->AddAllocated<RepeatedMessageTypeHandler>(submsg);
    }
    return submsg;
  }

  // TODO(haberman): handle Extensions, Unknown Fields.

#ifdef UPB_GOOGLE3
  // Handlers for types/features only included in internal proto2 release:
  // Cord, StringPiece, LazyField, and MessageSet.
  // TODO(haberman): LazyField, MessageSet.

  // Cord //////////////////////////////////////////////////////////////////////

  static void SetCordHandlers(
      const proto2::FieldDescriptor* proto2_f,
      const proto2::internal::GeneratedMessageReflection* r,
      const upb::FieldDef* f, upb::Handlers* h) {
    h->SetStringHandler(f, &OnCordBuf, NULL, NULL);
    if (f->IsSequence()) {
      SetStartSequenceHandler(proto2_f, r, f, h);
      h->SetStartStringHandler(f, &StartRepeatedCord, NULL, NULL);
    } else {
      h->SetStartStringHandler(
          f, &StartCord, new FieldOffset(proto2_f, r),
          &upb::DeletePointer<FieldOffset*>);
    }
  }

  static void* StartCord(void *m, void *fval, size_t size_hint) {
    UPB_UNUSED(size_hint);
    const FieldOffset* offset = static_cast<const FieldOffset*>(fval);
    offset->SetHasbit(m);
    Cord* field = offset->GetFieldPointer<Cord>(m);
    field->Clear();
    return field;
  }

  static size_t OnCordBuf(void *_c, void *fval, const char *buf, size_t n) {
    UPB_UNUSED(fval);
    Cord* c = static_cast<Cord*>(_c);
    c->Append(StringPiece(buf, n));
    return n;
  }

  static void* StartRepeatedCord(void *_r, void *fval, size_t size_hint) {
    UPB_UNUSED(size_hint);
    UPB_UNUSED(fval);
    proto2::RepeatedField<Cord>* r =
        static_cast<proto2::RepeatedField<Cord>*>(_r);
    return r->Add();
  }

  // StringPiece ///////////////////////////////////////////////////////////////

  static void SetStringPieceHandlers(
      const proto2::FieldDescriptor* proto2_f,
      const proto2::internal::GeneratedMessageReflection* r,
      const upb::FieldDef* f, upb::Handlers* h) {
    h->SetStringHandler(f, &OnStringPieceBuf, NULL, NULL);
    if (f->IsSequence()) {
      SetStartSequenceHandler(proto2_f, r, f, h);
      h->SetStartStringHandler(f, &StartRepeatedStringPiece, NULL, NULL);
    } else {
      h->SetStartStringHandler(
          f, &StartStringPiece, new FieldOffset(proto2_f, r),
          &upb::DeletePointer<FieldOffset*>);
    }
  }

  static size_t OnStringPieceBuf(void *_f, void *fval,
                                 const char *buf, size_t len) {
    UPB_UNUSED(fval);
    // TODO(haberman): alias if possible and enabled on the input stream.
    // TODO(haberman): add a method to StringPieceField that lets us avoid
    // this copy/malloc/free.
    proto2::internal::StringPieceField* field =
        static_cast<proto2::internal::StringPieceField*>(_f);
    size_t new_len = field->size() + len;
    char *data = new char[new_len];
    memcpy(data, field->data(), field->size());
    memcpy(data + field->size(), buf, len);
    field->CopyFrom(StringPiece(data, new_len));
    delete[] data;
    return len;
  }

  static void* StartStringPiece(void *m, void *fval, size_t size_hint) {
    UPB_UNUSED(size_hint);
    const FieldOffset* offset = static_cast<const FieldOffset*>(fval);
    offset->SetHasbit(m);
    proto2::internal::StringPieceField* field =
        offset->GetFieldPointer<proto2::internal::StringPieceField>(m);
    field->Clear();
    return field;
  }

  static void* StartRepeatedStringPiece(void* _r, void *fval,
                                        size_t size_hint) {
    UPB_UNUSED(size_hint);
    UPB_UNUSED(fval);
    typedef proto2::RepeatedPtrField<proto2::internal::StringPieceField>
        RepeatedStringPiece;
    RepeatedStringPiece* r = static_cast<RepeatedStringPiece*>(_r);
    proto2::internal::StringPieceField* field = r->Add();
    field->Clear();
    return field;
  }

#endif  // UPB_GOOGLE3
};

namespace upb {
namespace google {

bool TrySetWriteHandlers(const goog::FieldDescriptor* proto2_f,
                         const goog::Message& prototype,
                         const upb::FieldDef* upb_f, upb::Handlers* h) {
  return me::FieldAccessor::TrySet(proto2_f, prototype, upb_f, h);
}

const goog::Message* GetFieldPrototype(
    const goog::Message& m,
    const goog::FieldDescriptor* f) {
  return me::FieldAccessor::GetFieldPrototype(m, f);
}

}  // namespace google
}  // namespace upb
