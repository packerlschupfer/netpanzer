/*
Copyright (C) 1998 Pyrosoft Inc. (www.pyrosoftgames.com), Matthew Bogue

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef TEST_LIB

#include "BitArray.hpp"

#include <assert.h>
#include <string.h>

BitArray::BitArray(void) {
  array = 0;
  // Without these, a default-constructed BitArray carries a garbage size,
  // which is what getBit() bounds-checks against and what clear() passes to
  // memset. Only initialize() used to set them.
  size = 0;
  x_size = 0;
  y_size = 0;
}

//************************************************************************

BitArray::~BitArray(void) {
  if (array != 0) {
    delete[] array;
    array = 0;
  }
}

BitArray::BitArray(unsigned long x_size, unsigned long y_size) {
  initialize(x_size, y_size);
}

//************************************************************************
void BitArray::initialize(unsigned long x, unsigned long y) {
  unsigned long rawsize;
  unsigned long remain;

  // Round up to whole bytes. The remainder has to be taken on the bit count,
  // not on the byte count: "rawsize % 8" (with rawsize already bits/8) missed
  // the extra byte whenever bits/8 landed on a multiple of 8, so x*y of
  // 65..71, 129..135 and so on under-allocated and the last bits of the array
  // were written out of bounds.
  const unsigned long bits = x * y;

  rawsize = bits / 8;

  remain = bits % 8;

  if (remain) rawsize = rawsize + 1;

  if (array != 0) {
    delete[] array;
    array = 0;
  }

  array = new unsigned char[rawsize];
  assert(array != 0);

  x_size = x;
  y_size = y;
  size = rawsize;

  clear();
}

//************************************************************************

void BitArray::deallocate(void) {
  if (array != 0) {
    delete[] array;
    array = 0;
  }
}

//************************************************************************
void BitArray::clear(void) { memset(array, 0, size); }

//************************************************************************
void BitArray::set(void) { memset(array, 0xFF, size); }

//************************************************************************
void BitArray::setBit(unsigned long x, unsigned long y) {
  unsigned long index;
  unsigned char shift;
  unsigned char mask = 1;

  index = ((y * x_size) + x);
  shift = (unsigned char)(7 - (index & (unsigned long)7));  // 7 - (index % 8)
  index = index >> 3;                                       // index / 8
  mask = mask << shift;

  array[index] = array[index] | mask;
}

//************************************************************************
void BitArray::clearBit(unsigned long x, unsigned long y) {
  unsigned long index;
  unsigned char shift;
  unsigned char mask = 1;

  index = ((y * x_size) + x);
  shift = (unsigned char)(7 - (index & (unsigned long)7));
  index = index >> 3;
  mask = ~(mask << shift);

  array[index] = array[index] & mask;
}

//************************************************************************
bool BitArray::getBit(unsigned long x, unsigned long y) const {
  if (x >= x_size || y >= y_size) {
    return false;
  }

  unsigned long index;
  unsigned char shift;
  unsigned char mask = 1;
  unsigned char value;

  index = ((y * x_size) + x);
  shift = (unsigned char)(7 - (index & (unsigned long)7));
  index = index >> 3;
  mask = (mask << shift);

  // Valid byte indices are 0 .. size-1, so "index > size" let a read run one
  // byte past the end.
  if (index >= size) {
    return false;
  }

  value = array[index] & mask;

  if (value) return (true);

  return (false);
}

#else

#include "ArrayUtil/BitArray.hpp"
#include "test.hpp"

namespace {

/// Bytes genuinely needed to hold x*y bits.
unsigned long bytesNeeded(unsigned long x, unsigned long y) {
  const unsigned long bits = x * y;
  return (bits + 7) / 8;
}

}  // namespace

/**
 * The allocation has to cover every addressable bit. The original rounding
 * took the remainder of the *byte* count rather than the bit count
 * (remain = rawsize % 8, where rawsize was already bits/8), which
 * under-allocates whenever bits/8 lands on a multiple of 8 and the bits do
 * not divide evenly -- x*y of 65..71, 129..135, and so on.
 */
static void testAllocationSize(void) {
  for (unsigned long bits = 1; bits <= 600; bits++) {
    BitArray a;
    a.initialize(bits, 1);
    assert(a.size >= bytesNeeded(bits, 1));
  }

  // Two-dimensional shapes take the same path; a map is x by y, not x by 1.
  for (unsigned long x = 1; x <= 40; x++) {
    for (unsigned long y = 1; y <= 40; y++) {
      BitArray a;
      a.initialize(x, y);
      assert(a.size >= bytesNeeded(x, y));
    }
  }
}

/**
 * Touching the last bit of the array must stay inside the allocation. Run
 * under AddressSanitizer this is the direct test for the bug above; without
 * it, the size assertions are the safety net.
 */
static void testLastBitInBounds(void) {
  for (unsigned long bits = 1; bits <= 600; bits++) {
    BitArray a;
    a.initialize(bits, 1);

    a.setBit(bits - 1, 0);
    assert(a.getBit(bits - 1, 0));

    a.clearBit(bits - 1, 0);
    assert(!a.getBit(bits - 1, 0));
  }
}

/**
 * Ordinary set/clear behaviour, including the whole-array helpers.
 */
static void testSetAndClear(void) {
  BitArray a;
  a.initialize(16, 4);  // 64 bits, an exact byte multiple

  a.clear();
  for (unsigned long y = 0; y < 4; y++)
    for (unsigned long x = 0; x < 16; x++) assert(!a.getBit(x, y));

  a.set();
  for (unsigned long y = 0; y < 4; y++)
    for (unsigned long x = 0; x < 16; x++) assert(a.getBit(x, y));

  a.clear();
  a.setBit(3, 2);
  assert(a.getBit(3, 2));
  // Setting one bit must not disturb its neighbours.
  assert(!a.getBit(2, 2));
  assert(!a.getBit(4, 2));
  assert(!a.getBit(3, 1));
  assert(!a.getBit(3, 3));
}

/**
 * initialize() is called again on map change, so it has to be safe to reuse
 * an array and to grow or shrink it.
 */
static void testReinitialize(void) {
  BitArray a;
  a.initialize(100, 100);
  a.set();
  assert(a.getBit(99, 99));

  a.initialize(10, 10);
  assert(a.size >= bytesNeeded(10, 10));
  assert(!a.getBit(9, 9));  // initialize() clears

  a.initialize(200, 200);
  assert(a.size >= bytesNeeded(200, 200));
  a.setBit(199, 199);
  assert(a.getBit(199, 199));
}

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  testAllocationSize();
  testLastBitInBounds();
  testSetAndClear();
  testReinitialize();

  return 0;
}

#endif
