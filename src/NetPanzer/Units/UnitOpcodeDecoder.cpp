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

#include "UnitOpcodeDecoder.hpp"

#include "Util/Log.hpp"

UnitOpcodeDecoder::UnitOpcodeDecoder() {
  memset(&opcode_message, 0, sizeof(opcode_message));
  opcode_size = 0;
  reset();
}

UnitOpcodeDecoder::~UnitOpcodeDecoder() {}

void UnitOpcodeDecoder::reset() { opcode_index = 0; }

void UnitOpcodeDecoder::setMessage(const NetMessage* message, size_t size) {
  if (size > sizeof(opcode_message)) {
    LOGGER.warning("Opcodemessage too big.");
    memset(&opcode_message, 0, sizeof(opcode_message));
    // reset() only rewinds opcode_index, so without this the decoder kept the
    // previous message's length over a buffer that has just been cleared.
    // The size comes off the network, so this path is remotely reachable.
    opcode_size = 0;
    reset();
    return;
  }
  memcpy(&opcode_message, message, size);
  opcode_size = size;
  reset();
}

bool UnitOpcodeDecoder::decode(UnitOpcode** opcode) {
  if (opcode_index + opcode_message.getHeaderSize() >= opcode_size)
    return false;

  *opcode = (UnitOpcode*)(opcode_message.data + opcode_index);
  opcode_index += (*opcode)->getSize();
  return true;
}
