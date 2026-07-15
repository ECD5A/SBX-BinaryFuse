/*
 * SBX Binary Fuse16
 * Static Binary Fuse16 implementation for compact 64-bit membership lookup.
 *
 * Copyright (c) 2026 ECD5A
 * Licensed under the MIT License. See LICENSE in this directory.
 *
 * GitHub: https://github.com/ECD5A/SBX-BinaryFuse
 */
#include "binary_fuse.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const uint32_t SBX_BINARY_FUSE_MAX_ATTEMPTS = 100;

static int sbx_binary_fuse_compare_u64(const void *left, const void *right) {
  uint64_t a = *(const uint64_t*)left;
  uint64_t b = *(const uint64_t*)right;
  return a < b ? -1 : (a > b ? 1 : 0);
}

static uint32_t sbx_binary_fuse_sort_unique(uint64_t *keys, uint32_t count) {
  if(count < 2) {
    return count;
  }
  qsort(keys, count, sizeof(uint64_t), sbx_binary_fuse_compare_u64);
  uint32_t unique_count = 1;
  for(uint32_t i = 1; i < count; i++) {
    if(keys[i] != keys[unique_count - 1]) {
      keys[unique_count++] = keys[i];
    }
  }
  return unique_count;
}

static uint64_t sbx_binary_fuse_next_seed(uint64_t *state) {
  uint64_t value = (*state += UINT64_C(0x9e3779b97f4a7c15));
  value = (value ^ (value >> 30U)) * UINT64_C(0xbf58476d1ce4e5b9);
  value = (value ^ (value >> 27U)) * UINT64_C(0x94d049bb133111eb);
  return value ^ (value >> 31U);
}

static uint32_t sbx_binary_fuse_segment_length(uint32_t count) {
  if(count < 2) {
    return 4;
  }
  unsigned shift = (unsigned)floor(log((double)count) / log(3.33) + 2.25);
  uint32_t length = UINT32_C(1) << shift;
  return length > 262144U ? 262144U : length;
}

static double sbx_binary_fuse_size_factor(uint32_t count) {
  double factor = 0.875 + 0.25 * log(1000000.0) / log((double)count);
  return factor < 1.125 ? 1.125 : factor;
}

static void sbx_binary_fuse_set_layout(SbxBinaryFuse16 *filter, uint32_t count) {
  memset(filter, 0, sizeof(*filter));
  filter->size = count;
  filter->segment_length = sbx_binary_fuse_segment_length(count);
  filter->segment_length_mask = filter->segment_length - 1U;
  if(count < 2) {
    filter->segment_count = 1;
  }
  else {
    uint32_t capacity = (uint32_t)llround((double)count * sbx_binary_fuse_size_factor(count));
    uint32_t segments = (capacity + filter->segment_length - 1U) / filter->segment_length;
    filter->segment_count = segments > 2U ? segments - 2U : 1U;
  }
  filter->segment_count_length = filter->segment_count * filter->segment_length;
  filter->array_length = (filter->segment_count + 2U) * filter->segment_length;
}

uint64_t sbx_binary_fuse16_estimate_bytes(uint32_t count) {
  SbxBinaryFuse16 filter;
  sbx_binary_fuse_set_layout(&filter, count);
  return (uint64_t)filter.array_length * sizeof(uint16_t);
}

void sbx_binary_fuse16_free(SbxBinaryFuse16 *filter) {
  if(filter == NULL) {
    return;
  }
  free(filter->fingerprints);
  memset(filter, 0, sizeof(*filter));
}

static uint32_t sbx_binary_fuse_selected_position(uint32_t vertex,
                                                  const SbxBinaryFuseHashes *positions) {
  if(vertex == positions->h0) {
    return 0;
  }
  return vertex == positions->h1 ? 1U : 2U;
}

int sbx_binary_fuse16_build(SbxBinaryFuse16 *filter, uint64_t *keys, uint32_t count) {
  sbx_binary_fuse_set_layout(filter, count);
  filter->fingerprints = (uint16_t*)calloc(filter->array_length, sizeof(uint16_t));
  if(filter->fingerprints == NULL) {
    return 0;
  }

  if(count == 0) {
    filter->seed = UINT64_C(0x9e3779b97f4a7c15);
    return 1;
  }

  uint8_t *degrees = (uint8_t*)calloc(filter->array_length, sizeof(uint8_t));
  uint64_t *vertex_xors = (uint64_t*)calloc(filter->array_length, sizeof(uint64_t));
  uint32_t *queue = (uint32_t*)malloc((size_t)filter->array_length * sizeof(uint32_t));
  uint64_t *stack_hashes = (uint64_t*)malloc((size_t)count * sizeof(uint64_t));
  uint8_t *stack_selected = (uint8_t*)malloc((size_t)count * sizeof(uint8_t));
  if(degrees == NULL || vertex_xors == NULL || queue == NULL ||
     stack_hashes == NULL || stack_selected == NULL) {
    free(degrees);
    free(vertex_xors);
    free(queue);
    free(stack_hashes);
    free(stack_selected);
    sbx_binary_fuse16_free(filter);
    return 0;
  }

  uint64_t seed_state = UINT64_C(0x726b2b9d438b9d4d);
  uint32_t stack_size = 0;
  uint32_t active_count = count;
  for(uint32_t attempt = 0; attempt < SBX_BINARY_FUSE_MAX_ATTEMPTS; attempt++) {
    memset(degrees, 0, filter->array_length * sizeof(uint8_t));
    memset(vertex_xors, 0, filter->array_length * sizeof(uint64_t));
    filter->seed = sbx_binary_fuse_next_seed(&seed_state);
    int degree_overflow = 0;

    for(uint32_t i = 0; i < active_count; i++) {
      uint64_t hash = sbx_binary_fuse_mix(keys[i] + filter->seed);
      SbxBinaryFuseHashes positions = sbx_binary_fuse16_hashes(hash, filter);
      const uint32_t vertices[3] = {positions.h0, positions.h1, positions.h2};
      for(uint32_t j = 0; j < 3; j++) {
        if(degrees[vertices[j]] == UINT8_MAX) {
          degree_overflow = 1;
          break;
        }
        degrees[vertices[j]]++;
        vertex_xors[vertices[j]] ^= hash;
      }
      if(degree_overflow) {
        break;
      }
    }
    if(degree_overflow) {
      if(attempt == 7) {
        active_count = sbx_binary_fuse_sort_unique(keys, active_count);
      }
      continue;
    }

    uint32_t queue_size = 0;
    for(uint32_t i = 0; i < filter->array_length; i++) {
      if(degrees[i] == 1) {
        queue[queue_size++] = i;
      }
    }

    stack_size = 0;
    while(queue_size > 0) {
      uint32_t vertex = queue[--queue_size];
      if(degrees[vertex] != 1) {
        continue;
      }
      uint64_t hash = vertex_xors[vertex];
      SbxBinaryFuseHashes positions = sbx_binary_fuse16_hashes(hash, filter);
      uint32_t selected = sbx_binary_fuse_selected_position(vertex, &positions);
      stack_hashes[stack_size] = hash;
      stack_selected[stack_size] = (uint8_t)selected;
      stack_size++;

      const uint32_t vertices[3] = {positions.h0, positions.h1, positions.h2};
      for(uint32_t j = 0; j < 3; j++) {
        uint32_t other = vertices[j];
        if(other == vertex || degrees[other] == 0) {
          continue;
        }
        degrees[other]--;
        vertex_xors[other] ^= hash;
        if(degrees[other] == 1) {
          queue[queue_size++] = other;
        }
      }
      degrees[vertex] = 0;
      vertex_xors[vertex] = 0;
    }

    if(stack_size == active_count) {
      break;
    }
    if(attempt == 7) {
      active_count = sbx_binary_fuse_sort_unique(keys, active_count);
    }
  }

  if(stack_size != active_count) {
    free(degrees);
    free(vertex_xors);
    free(queue);
    free(stack_hashes);
    free(stack_selected);
    sbx_binary_fuse16_free(filter);
    return 0;
  }

  filter->size = active_count;

  while(stack_size > 0) {
    stack_size--;
    uint64_t hash = stack_hashes[stack_size];
    SbxBinaryFuseHashes positions = sbx_binary_fuse16_hashes(hash, filter);
    const uint32_t vertices[3] = {positions.h0, positions.h1, positions.h2};
    uint32_t selected = stack_selected[stack_size];
    uint16_t value = sbx_binary_fuse16_fingerprint(hash);
    value ^= filter->fingerprints[vertices[(selected + 1U) % 3U]];
    value ^= filter->fingerprints[vertices[(selected + 2U) % 3U]];
    filter->fingerprints[vertices[selected]] = value;
  }

  free(degrees);
  free(vertex_xors);
  free(queue);
  free(stack_hashes);
  free(stack_selected);
  return 1;
}
