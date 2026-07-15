/*
 * SBX Binary Fuse16
 * Static Binary Fuse16 implementation for compact 64-bit membership lookup.
 *
 * Copyright (c) 2026 ECD5A
 * Licensed under the MIT License. See LICENSE in this directory.
 *
 * GitHub: https://github.com/ECD5A/SBX-BinaryFuse
 */
#ifndef SBX_BINARY_FUSE_H
#define SBX_BINARY_FUSE_H

#include <stddef.h>
#include <stdint.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

struct SbxBinaryFuse16 {
  uint64_t seed;
  uint32_t size;
  uint32_t segment_length;
  uint32_t segment_length_mask;
  uint32_t segment_count;
  uint32_t segment_count_length;
  uint32_t array_length;
  uint16_t *fingerprints;
};

struct SbxBinaryFuseHashes {
  uint32_t h0;
  uint32_t h1;
  uint32_t h2;
};

int sbx_binary_fuse16_build(SbxBinaryFuse16 *filter, uint64_t *keys, uint32_t count);
void sbx_binary_fuse16_free(SbxBinaryFuse16 *filter);
uint64_t sbx_binary_fuse16_estimate_bytes(uint32_t count);

static inline uint64_t sbx_binary_fuse_mix(uint64_t value) {
  value ^= value >> 33U;
  value *= UINT64_C(0xff51afd7ed558ccd);
  value ^= value >> 33U;
  value *= UINT64_C(0xc4ceb9fe1a85ec53);
  value ^= value >> 33U;
  return value;
}

static inline uint64_t sbx_binary_fuse_mulhi(uint64_t a, uint64_t b) {
#if defined(__SIZEOF_INT128__)
  return (uint64_t)(((__uint128_t)a * b) >> 64U);
#elif defined(_M_X64) || defined(_M_ARM64)
  return __umulh(a, b);
#else
  const uint64_t a0 = (uint32_t)a;
  const uint64_t a1 = a >> 32U;
  const uint64_t b0 = (uint32_t)b;
  const uint64_t b1 = b >> 32U;
  const uint64_t p11 = a1 * b1;
  const uint64_t p01 = a0 * b1;
  const uint64_t p10 = a1 * b0;
  const uint64_t p00 = a0 * b0;
  const uint64_t middle = p10 + (p00 >> 32U) + (uint32_t)p01;
  return p11 + (middle >> 32U) + (p01 >> 32U);
#endif
}

static inline uint16_t sbx_binary_fuse16_fingerprint(uint64_t hash) {
  return (uint16_t)(hash ^ (hash >> 32U));
}

static inline SbxBinaryFuseHashes sbx_binary_fuse16_hashes(uint64_t hash,
                                                            const SbxBinaryFuse16 *filter) {
  SbxBinaryFuseHashes result;
  result.h0 = (uint32_t)sbx_binary_fuse_mulhi(hash, filter->segment_count_length);
  result.h1 = (result.h0 + filter->segment_length) ^
              ((uint32_t)(hash >> 18U) & filter->segment_length_mask);
  result.h2 = (result.h0 + filter->segment_length * 2U) ^
              ((uint32_t)hash & filter->segment_length_mask);
  return result;
}

static inline int sbx_binary_fuse16_contains(const SbxBinaryFuse16 *filter, uint64_t key) {
  uint64_t hash = sbx_binary_fuse_mix(key + filter->seed);
  SbxBinaryFuseHashes positions = sbx_binary_fuse16_hashes(hash, filter);
  uint16_t value = sbx_binary_fuse16_fingerprint(hash);
  value ^= filter->fingerprints[positions.h0];
  value ^= filter->fingerprints[positions.h1];
  value ^= filter->fingerprints[positions.h2];
  return value == 0;
}

#endif
