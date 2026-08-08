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

#include "Classes/Network/NetMessageEncoder.hpp"

#include <string.h>

#include "Classes/Network/NetworkServer.hpp"
#include "Classes/Network/NetworkState.hpp"
#include "NetworkClient.hpp"

NetMessageEncoder::NetMessageEncoder() { resetEncoder(); }

NetMessageEncoder::~NetMessageEncoder() {}

void NetMessageEncoder::resetEncoder() {
  encode_message.message_class = _net_message_class_multi;
  encode_message.message_id = 0;
  memset(encode_message.data, 0, sizeof(encode_message.data));
  offset = 0;
}

bool NetMessageEncoder::encodeMessage(NetMessage* message, size_t size) {
  // Each stored message costs its own size plus the two-byte length prefix
  // written just below, so the prefix has to be part of the capacity check.
  // Testing only "offset + size" let a message of exactly
  // _MULTI_PACKET_LIMIT - 1 bytes through, which then wrote one byte past the
  // end of encode_message.data. The size check comes first so the addition
  // cannot wrap.
  if (size > _MULTI_PACKET_LIMIT ||
      offset + size + sizeof(Uint16) > _MULTI_PACKET_LIMIT) {
    return false;
  }

  Uint16* buf = (Uint16*)(encode_message.data + offset);
  alignas(2) Uint16 msg_len = htol16(size);

  // write message size
  memcpy(buf, &msg_len, sizeof(Uint16));
  // write message
  memcpy(buf + 1, message, size);

  offset += size + sizeof(Uint16);
  return true;
}

#else

#include <string.h>

#include "Classes/Network/NetMessageDecoder.hpp"
#include "Classes/Network/NetMessageEncoder.hpp"
#include "test.hpp"

namespace {

/// A message with a known payload, so a round trip can be checked byte for
/// byte rather than just "it did not crash".
struct TestMessage {
  NetMessage header;
  Uint8 payload[16];
};

TestMessage makeMessage(Uint8 id, Uint8 fill) {
  TestMessage m;
  m.header.message_class = _net_message_class_player;
  m.header.message_id = id;
  memset(m.payload, fill, sizeof(m.payload));
  return m;
}

}  // namespace

/**
 * The codec stores its length prefixes little-endian, so these conversions
 * sit under every packet the game sends. They are compiled two different
 * ways depending on WORDS_BIGENDIAN, and only one of those ways is ever
 * exercised on a given machine -- so check the properties that must hold on
 * both rather than specific byte orders.
 */
static void testEndian(void) {
  // Round trips must be exact across the whole 16-bit range.
  for (Uint32 v = 0; v <= 0xFFFF; v++) {
    const Uint16 val = (Uint16)v;
    assert(ltoh16(htol16(val)) == val);
    assert(btoh16(htob16(val)) == val);
  }

  const Uint32 v32[] = {0u,          1u,         0xFFu,       0x100u,
                        0xFFFFu,     0x10000u,   0x12345678u, 0xDEADBEEFu,
                        0x80000000u, 0xFFFFFFFFu};
  for (size_t i = 0; i < sizeof(v32) / sizeof(v32[0]); i++) {
    assert(ltoh32(htol32(v32[i])) == v32[i]);
    assert(btoh32(htob32(v32[i])) == v32[i]);
  }

  // Exactly one of the two families is a byte swap on any given build, and
  // the swap has to be a real reversal rather than a no-op.
  assert(__swap16(0x1234) == 0x3412);
  assert(__swap32(0x12345678u) == 0x78563412u);
  assert(__swap16(__swap16(0xABCD)) == 0xABCD);
  assert(__swap32(__swap32(0xABCDEF01u)) == 0xABCDEF01u);

  // A byte-order conversion must never lose information: distinct inputs stay
  // distinct. This is what stops a length prefix aliasing onto another.
  assert(htol16(0x0100) != htol16(0x0001));
  assert(htob16(0x0100) != htob16(0x0001));
}

/**
 * Several messages packed into one multi-message must come back out in the
 * same order, with the same lengths and the same bytes.
 */
static void testRoundTrip(void) {
  NetMessageEncoder encoder;
  assert(encoder.isEmpty());

  const int COUNT = 5;
  TestMessage sent[COUNT];
  for (int i = 0; i < COUNT; i++) {
    sent[i] = makeMessage((Uint8)(10 + i), (Uint8)(0xA0 + i));
    assert(encoder.encodeMessage((NetMessage*)&sent[i], sizeof(TestMessage)));
  }
  assert(!encoder.isEmpty());

  NetMessageDecoder decoder;
  decoder.setDecodeMessage(encoder.getEncodedMessage(),
                           encoder.getEncodedLen());

  for (int i = 0; i < COUNT; i++) {
    NetMessage* got = 0;
    const Uint16 len = decoder.decodeMessage(&got);
    assert(len == sizeof(TestMessage));
    assert(got != 0);
    assert(got->message_class == _net_message_class_player);
    assert(got->message_id == (Uint8)(10 + i));
    assert(memcmp(((TestMessage*)got)->payload, sent[i].payload,
                  sizeof(sent[i].payload)) == 0);
  }

  // The stream is exhausted; asking again must report so rather than walking
  // off the end.
  NetMessage* extra = 0;
  assert(decoder.decodeMessage(&extra) == 0);
}

/**
 * An empty encoder still produces a valid, decodable multi-message that
 * simply yields nothing.
 */
static void testEmpty(void) {
  NetMessageEncoder encoder;
  assert(encoder.isEmpty());
  assert(encoder.getEncodedLen() == sizeof(NetMessage));

  NetMessageDecoder decoder;
  decoder.setDecodeMessage(encoder.getEncodedMessage(),
                           encoder.getEncodedLen());
  NetMessage* got = 0;
  assert(decoder.decodeMessage(&got) == 0);
}

/**
 * The encoder must refuse a message that does not fit. Each stored message
 * costs its own size plus a two-byte length prefix, so the accounting has to
 * include that prefix -- otherwise the last accepted message writes past the
 * end of the buffer.
 */
static void testCapacity(void) {
  NetMessageEncoder encoder;

  // Fill up with a size that divides the buffer awkwardly, so the last
  // accepted message lands hard against the limit.
  const size_t CHUNK = 100;
  Uint8 buf[CHUNK];
  memset(buf, 0x5A, sizeof(buf));
  ((NetMessage*)buf)->message_class = _net_message_class_player;
  ((NetMessage*)buf)->message_id = 1;

  size_t accepted = 0;
  while (encoder.encodeMessage((NetMessage*)buf, CHUNK)) accepted++;
  assert(accepted > 0);

  // Everything the encoder claimed to store has to fit inside the packet it
  // reports, prefixes included.
  const size_t stored = accepted * (CHUNK + sizeof(Uint16));
  assert(encoder.getEncodedLen() == stored + sizeof(NetMessage));
  assert(stored <= _MULTI_PACKET_LIMIT);

  // And it must all decode back out again.
  NetMessageDecoder decoder;
  decoder.setDecodeMessage(encoder.getEncodedMessage(),
                           encoder.getEncodedLen());
  size_t decoded = 0;
  NetMessage* got = 0;
  while (decoder.decodeMessage(&got) != 0) decoded++;
  assert(decoded == accepted);
}

/**
 * The exact boundary. A message of _MULTI_PACKET_LIMIT - 1 bytes passes a
 * guard written as "offset + size >= limit", because that guard forgets the
 * two-byte length prefix it is about to write as well. Accepting it writes
 * one byte past the end of the buffer.
 */
static void testCapacityBoundary(void) {
  static Uint8 big[_MULTI_PACKET_LIMIT];
  memset(big, 0x77, sizeof(big));
  ((NetMessage*)big)->message_class = _net_message_class_player;
  ((NetMessage*)big)->message_id = 2;

  // Largest size that genuinely fits, prefix included.
  {
    NetMessageEncoder encoder;
    const size_t fits = _MULTI_PACKET_LIMIT - sizeof(Uint16);
    assert(encoder.encodeMessage((NetMessage*)big, fits));
    assert(encoder.getEncodedLen() ==
           fits + sizeof(Uint16) + sizeof(NetMessage));
  }

  // One byte more than fits: must be refused, not silently written.
  {
    NetMessageEncoder encoder;
    const size_t overflows = _MULTI_PACKET_LIMIT - sizeof(Uint16) + 1;
    assert(!encoder.encodeMessage((NetMessage*)big, overflows));
    assert(encoder.isEmpty());
  }
}

/**
 * A truncated or lying multi-message arrives from the network like any
 * other. The decoder must reject it instead of reading past its buffer.
 */
static void testMalformed(void) {
  NetMessageEncoder encoder;
  TestMessage m = makeMessage(7, 0x33);
  assert(encoder.encodeMessage((NetMessage*)&m, sizeof(TestMessage)));

  // Claim a payload far longer than the packet actually holds.
  MultiMessage bad;
  memcpy(&bad, encoder.getEncodedMessage(), encoder.getEncodedLen());
  Uint16 lie = htol16(0xFFF0);
  memcpy(bad.data, &lie, sizeof(Uint16));

  NetMessageDecoder decoder;
  decoder.setDecodeMessage(&bad, encoder.getEncodedLen());
  NetMessage* got = 0;
  assert(decoder.decodeMessage(&got) == 0);
}

/**
 * A decoder must be safe before it has been handed anything, and must not
 * carry state across a rejected packet. Both are reachable from the network:
 * the size passed to setDecodeMessage comes off the wire, so an oversized
 * multi-message drives the rejection path.
 */
static void testDecoderState(void) {
  // Fresh decoder, nothing set: must report "no messages" rather than read
  // uninitialised size and offset.
  {
    NetMessageDecoder decoder;
    NetMessage* got = 0;
    assert(decoder.decodeMessage(&got) == 0);
  }

  // A rejected oversized packet must leave the decoder empty, not holding the
  // previous packet's length over a buffer that has just been zeroed.
  {
    NetMessageEncoder encoder;
    TestMessage m = makeMessage(9, 0x11);
    assert(encoder.encodeMessage((NetMessage*)&m, sizeof(TestMessage)));

    NetMessageDecoder decoder;
    decoder.setDecodeMessage(encoder.getEncodedMessage(),
                             encoder.getEncodedLen());

    NetMessage* got = 0;
    assert(decoder.decodeMessage(&got) == sizeof(TestMessage));

    // Now hand it something far too big; it has to reset rather than keep the
    // earlier state.
    MultiMessage oversized;
    memset(&oversized, 0, sizeof(oversized));
    decoder.setDecodeMessage(&oversized, sizeof(oversized) + 1024);

    NetMessage* after = 0;
    assert(decoder.decodeMessage(&after) == 0);
  }
}

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  testEndian();
  testDecoderState();
  testRoundTrip();
  testEmpty();
  testCapacity();
  testCapacityBoundary();
  testMalformed();

  return 0;
}

#endif
