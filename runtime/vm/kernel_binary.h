// Copyright (c) 2017, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#ifndef RUNTIME_VM_KERNEL_BINARY_H_
#define RUNTIME_VM_KERNEL_BINARY_H_

#if !defined(DART_PRECOMPILED_RUNTIME)

#include <map>

#include "vm/kernel.h"
#include "vm/kernel_to_il.h"
#include "vm/object.h"

namespace dart {
namespace kernel {


static const uint32_t kMagicProgramFile = 0x90ABCDEFu;


// Keep in sync with package:dynamo/lib/binary/tag.dart
enum Tag {
  kNothing = 0,
  kSomething = 1,

  kClass = 2,

  kFunctionNode = 3,
  kField = 4,
  kConstructor = 5,
  kProcedure = 6,

  kInvalidInitializer = 7,
  kFieldInitializer = 8,
  kSuperInitializer = 9,
  kRedirectingInitializer = 10,
  kLocalInitializer = 11,

  kDirectPropertyGet = 15,
  kDirectPropertySet = 16,
  kDirectMethodInvocation = 17,
  kConstStaticInvocation = 18,
  kInvalidExpression = 19,
  kVariableGet = 20,
  kVariableSet = 21,
  kPropertyGet = 22,
  kPropertySet = 23,
  kSuperPropertyGet = 24,
  kSuperPropertySet = 25,
  kStaticGet = 26,
  kStaticSet = 27,
  kMethodInvocation = 28,
  kSuperMethodInvocation = 29,
  kStaticInvocation = 30,
  kConstructorInvocation = 31,
  kConstConstructorInvocation = 32,
  kNot = 33,
  kLogicalExpression = 34,
  kConditionalExpression = 35,
  kStringConcatenation = 36,
  kIsExpression = 37,
  kAsExpression = 38,
  kStringLiteral = 39,
  kDoubleLiteral = 40,
  kTrueLiteral = 41,
  kFalseLiteral = 42,
  kNullLiteral = 43,
  kSymbolLiteral = 44,
  kTypeLiteral = 45,
  kThisExpression = 46,
  kRethrow = 47,
  kThrow = 48,
  kListLiteral = 49,
  kMapLiteral = 50,
  kAwaitExpression = 51,
  kFunctionExpression = 52,
  kLet = 53,

  kPositiveIntLiteral = 55,
  kNegativeIntLiteral = 56,
  kBigIntLiteral = 57,
  kConstListLiteral = 58,
  kConstMapLiteral = 59,

  kInvalidStatement = 60,
  kExpressionStatement = 61,
  kBlock = 62,
  kEmptyStatement = 63,
  kAssertStatement = 64,
  kLabeledStatement = 65,
  kBreakStatement = 66,
  kWhileStatement = 67,
  kDoStatement = 68,
  kForStatement = 69,
  kForInStatement = 70,
  kSwitchStatement = 71,
  kContinueSwitchStatement = 72,
  kIfStatement = 73,
  kReturnStatement = 74,
  kTryCatch = 75,
  kTryFinally = 76,
  kYieldStatement = 77,
  kVariableDeclaration = 78,
  kFunctionDeclaration = 79,
  kAsyncForInStatement = 80,

  kTypedefType = 87,
  kVectorType = 88,
  kBottomType = 89,
  kInvalidType = 90,
  kDynamicType = 91,
  kVoidType = 92,
  kInterfaceType = 93,
  kFunctionType = 94,
  kTypeParameterType = 95,
  kSimpleInterfaceType = 96,
  kSimpleFunctionType = 97,

  kVectorCreation = 102,
  kVectorGet = 103,
  kVectorSet = 104,
  kVectorCopy = 105,

  kClosureCreation = 106,

  kSpecializedTagHighBit = 0x80,  // 10000000
  kSpecializedTagMask = 0xF8,     // 11111000
  kSpecializedPayloadMask = 0x7,  // 00000111

  kSpecializedVariableGet = 128,
  kSpecializedVariableSet = 136,
  kSpecialIntLiteral = 144,
};


static const int SpecializedIntLiteralBias = 3;


class Reader {
 public:
  Reader(const uint8_t* buffer, intptr_t size)
      : buffer_(buffer), size_(size), offset_(0) {}

  uint32_t ReadUInt32() {
    ASSERT(offset_ + 4 <= size_);

    uint32_t value = (buffer_[offset_ + 0] << 24) |
                     (buffer_[offset_ + 1] << 16) |
                     (buffer_[offset_ + 2] << 8) | (buffer_[offset_ + 3] << 0);
    offset_ += 4;
    return value;
  }

  uint32_t ReadUInt() {
    ASSERT(offset_ + 1 <= size_);
    uint8_t byte0 = buffer_[offset_];
    if ((byte0 & 0x80) == 0) {
      // 0...
      offset_++;
      return byte0;
    } else if ((byte0 & 0xc0) == 0x80) {
      // 10...
      ASSERT(offset_ + 2 <= size_);
      uint32_t value = ((byte0 & ~0x80) << 8) | (buffer_[offset_ + 1]);
      offset_ += 2;
      return value;
    } else {
      // 11...
      ASSERT(offset_ + 4 <= size_);
      uint32_t value = ((byte0 & ~0xc0) << 24) | (buffer_[offset_ + 1] << 16) |
                       (buffer_[offset_ + 2] << 8) |
                       (buffer_[offset_ + 3] << 0);
      offset_ += 4;
      return value;
    }
  }

  /**
   * Read and return a TokenPosition from this reader.
   * @param record specifies whether or not the read position is saved as a
   * valid token position in the current script.
   * If not be sure to record it later by calling record_token_position (after
   * setting the correct current_script_id).
   */
  TokenPosition ReadPosition() {
    // Position is saved as unsigned,
    // but actually ranges from -1 and up (thus the -1)
    intptr_t value = ReadUInt() - 1;
    TokenPosition result = TokenPosition(value);
    max_position_ = Utils::Maximum(max_position_, result);
    if (min_position_.IsNoSource()) {
      min_position_ = result;
    } else if (result.IsReal()) {
      min_position_ = Utils::Minimum(min_position_, result);
    }

    return result;
  }

  intptr_t ReadListLength() { return ReadUInt(); }

  uint8_t ReadByte() { return buffer_[offset_++]; }

  uint8_t PeekByte() { return buffer_[offset_]; }

  bool ReadBool() { return (ReadByte() & 1) == 1; }

  word ReadFlags() { return ReadByte(); }

  Tag ReadTag(uint8_t* payload = NULL) {
    uint8_t byte = ReadByte();
    bool has_payload = (byte & kSpecializedTagHighBit) != 0;
    if (has_payload) {
      if (payload != NULL) {
        *payload = byte & kSpecializedPayloadMask;
      }
      return static_cast<Tag>(byte & kSpecializedTagMask);
    } else {
      return static_cast<Tag>(byte);
    }
  }

  Tag PeekTag(uint8_t* payload = NULL) {
    uint8_t byte = PeekByte();
    bool has_payload = (byte & kSpecializedTagHighBit) != 0;
    if (has_payload) {
      if (payload != NULL) {
        *payload = byte & kSpecializedPayloadMask;
      }
      return static_cast<Tag>(byte & kSpecializedTagMask);
    } else {
      return static_cast<Tag>(byte);
    }
  }

  const uint8_t* Consume(int count) {
    ASSERT(offset_ + count <= size_);
    const uint8_t* old = buffer_ + offset_;
    offset_ += count;
    return old;
  }

  void EnsureEnd() {
    if (offset_ != size_) {
      FATAL2(
          "Reading Kernel file: Expected to be at EOF "
          "(offset: %" Pd ", size: %" Pd ")",
          offset_, size_);
    }
  }

  // The largest position read yet (since last reset).
  // This is automatically updated when calling ReadPosition,
  // but can be overwritten (e.g. via the PositionScope class).
  TokenPosition max_position() { return max_position_; }
  // The smallest position read yet (since last reset).
  // This is automatically updated when calling ReadPosition,
  // but can be overwritten (e.g. via the PositionScope class).
  TokenPosition min_position() { return min_position_; }

  template <typename T, typename RT>
  T* ReadOptional() {
    Tag tag = ReadTag();
    if (tag == kNothing) {
      return NULL;
    }
    ASSERT(tag == kSomething);
    return RT::ReadFrom(this);
  }

  template <typename T>
  T* ReadOptional() {
    return ReadOptional<T, T>();
  }

  // A canonical name reference of -1 indicates none (for optional names), not
  // the root name as in the canonical name table.
  NameIndex ReadCanonicalNameReference() { return NameIndex(ReadUInt() - 1); }

  intptr_t offset() { return offset_; }
  void set_offset(intptr_t offset) { offset_ = offset; }
  intptr_t size() { return size_; }

  const uint8_t* buffer() { return buffer_; }

 private:
  const uint8_t* buffer_;
  intptr_t size_;
  intptr_t offset_;
  TokenPosition max_position_;
  TokenPosition min_position_;
  intptr_t current_script_id_;

  friend class PositionScope;
  friend class Program;
};


// A helper class that resets the readers min and max positions both upon
// initialization and upon destruction, i.e. when created the min an max
// positions will be reset to "noSource", when destructing the min and max will
// be reset to have they value they would have had, if they hadn't been reset in
// the first place.
class PositionScope {
 public:
  explicit PositionScope(Reader* reader)
      : reader_(reader),
        min_(reader->min_position_),
        max_(reader->max_position_) {
    reader->min_position_ = reader->max_position_ = TokenPosition::kNoSource;
  }

  ~PositionScope() {
    if (reader_->min_position_.IsNoSource()) {
      reader_->min_position_ = min_;
    } else if (min_.IsReal()) {
      reader_->min_position_ = Utils::Minimum(reader_->min_position_, min_);
    }
    reader_->max_position_ = Utils::Maximum(reader_->max_position_, max_);
  }

 private:
  Reader* reader_;
  TokenPosition min_;
  TokenPosition max_;
};

}  // namespace kernel
}  // namespace dart

#endif  // !defined(DART_PRECOMPILED_RUNTIME)
#endif  // RUNTIME_VM_KERNEL_BINARY_H_
