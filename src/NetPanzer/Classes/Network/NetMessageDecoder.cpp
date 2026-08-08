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

#include "NetMessageDecoder.hpp"

#include <string.h>

#include "Util/Log.hpp"

NetMessageDecoder::NetMessageDecoder() {
  memset(&decode_message, 0, sizeof(decode_message));
  // The buffer was cleared but these were not, so a decodeMessage() before
  // the first setDecodeMessage() did its bounds arithmetic on garbage.
  size = 0;
  offset = 0;
}

NetMessageDecoder::~NetMessageDecoder() {}

void NetMessageDecoder::setDecodeMessage(const NetMessage* message,
                                         const size_t size) {
  if (size > sizeof(decode_message)) {
    LOGGER.warning("Multimessage with wrong size!");
    memset(&decode_message, 0, sizeof(decode_message));
    // The buffer is now empty, so the length describing it has to be too.
    // Leaving the previous message's size and offset in place meant the next
    // decodeMessage() walked a zeroed buffer using a stale length -- and on
    // the first packet, an uninitialised one. The size here comes off the
    // network, so this path is reachable by anyone who sends an oversized
    // multi-message.
    this->size = 0;
    offset = 0;
    return;
  }

  memcpy(&decode_message, message, size);
  this->size = size;
  offset = 0;
}

Uint16 NetMessageDecoder::decodeMessage(NetMessage** message) {
  // Everything past the multi-message header is payload. This arrives
  // straight off the network, so every bound below has to hold for a packet
  // that is lying about its contents.
  const size_t data_len =
      (size > sizeof(NetMessage)) ? size - sizeof(NetMessage) : 0;

  // The two-byte length prefix must itself be inside the buffer before it can
  // be read. The old test stopped only once offset reached data_len, so a
  // packet ending with a single spare byte was read two bytes wide.
  if (offset + sizeof(Uint16) > data_len) {
    return 0;  // no more messages
  }

  Uint16 mlen_value;
  memcpy(&mlen_value, decode_message.data + offset, sizeof(Uint16));
  Uint16 msg_len = ltoh16(mlen_value);

  // The claimed length has to fit in what remains *after* the prefix.
  if (msg_len > data_len - offset - sizeof(Uint16)) {
    LOGGER.warning("Malformed Multimessage!!");
    return 0;
  }

  *message = (NetMessage*)(decode_message.data + offset + sizeof(Uint16));

  offset += msg_len + sizeof(Uint16);

  return msg_len;
}
