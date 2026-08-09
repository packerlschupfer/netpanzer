/*
Copyright (C) 2026 NetPanzer (https://github.com/netpanzer/)

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

/**
 * Does a message handler read past the bytes that actually arrived?
 *
 * A NetMessage is two bytes -- a class and an id -- and every real message is
 * a subclass with more fields behind that. So the id chooses how much the
 * handler is going to read, while the peer chooses how much it actually sent.
 * Nothing forces those to agree, and a peer is free to send two bytes that
 * name a 130-byte message.
 *
 * The client router discards packet->size for five of its nine message
 * classes, so those handlers cannot check even if they want to. This asks
 * whether that is exploitable rather than merely untidy.
 *
 * The check does not rely on a sanitizer. Each message is placed so that its
 * last byte sits against a PROT_NONE page: reading one byte too far faults,
 * every time, in any build. The fault is caught and reported as a failed
 * assertion so the suite says which handler overread rather than dying with a
 * bare signal.
 */

#include "test.hpp"

#ifndef _WIN32

#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "Classes/Network/ClientConnectDaemon.hpp"
#include "Classes/Network/GameControlNetMessage.hpp"
#include "Classes/Network/NetMessage.hpp"
#include "Classes/Network/NetworkState.hpp"
#include "Interfaces/GameControlRulesDaemon.hpp"
#include "Interfaces/ConsoleInterface.hpp"
#include "Interfaces/GameManager.hpp"
#include "Objectives/ObjectiveInterface.hpp"
#include "PowerUps/PowerUpInterface.hpp"

namespace {

sigjmp_buf fault_return;
volatile sig_atomic_t faulted = 0;

void onFault(int) {
  faulted = 1;
  siglongjmp(fault_return, 1);
}

/**
 * A buffer of 'size' bytes whose final byte is the last readable byte before
 * an unmapped page. Reading past the end faults rather than quietly returning
 * whatever happened to be in memory.
 */
class GuardedMessage {
 public:
  explicit GuardedMessage(size_t size) : region(0), region_size(0), msg(0) {
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    region_size = page * 2;
    region = (unsigned char *)mmap(0, region_size, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(region != MAP_FAILED);
    // Second page unreadable; the message ends flush against it.
    assert(mprotect(region + page, page, PROT_NONE) == 0);
    msg = region + page - size;
    memset(msg, 0, size);
  }

  ~GuardedMessage() {
    if (region != 0) munmap(region, region_size);
  }

  NetMessage *get() { return (NetMessage *)msg; }

 private:
  unsigned char *region;
  size_t region_size;
  unsigned char *msg;
};

/// Runs handler(msg, size) and returns true if it read past the end of msg.
template <typename Fn>
bool overreads(Fn handler, NetMessage *msg, size_t size) {
  struct sigaction sa, old_segv, old_bus;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = onFault;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_NODEFER;
  sigaction(SIGSEGV, &sa, &old_segv);
  sigaction(SIGBUS, &sa, &old_bus);

  faulted = 0;
  if (sigsetjmp(fault_return, 1) == 0) {
    handler(msg, size);
  }

  sigaction(SIGSEGV, &old_segv, 0);
  sigaction(SIGBUS, &old_bus, 0);
  return faulted != 0;
}

}  // namespace

/**
 * Walks every id of a class through its handler with nothing but the two-byte
 * header present. No id may read further than what arrived, whatever the id
 * claims the message is -- that is the whole contract being asserted here.
 */
template <typename Fn>
static void assertClassNeverOverreads(Fn handler, Uint8 message_class) {
  for (int id = 0; id < 32; id++) {
    GuardedMessage packet(sizeof(NetMessage));
    NetMessage *msg = packet.get();
    msg->message_class = message_class;
    msg->message_id = (Uint8)id;

    const bool read_past_end = overreads(handler, msg, sizeof(NetMessage));

    assert(!read_past_end);
  }
}

/**
 * A two-byte packet claiming to be a cycle-map message. The handler copies
 * map_name out of it, which lives at offset 2 and runs for 128 bytes -- none
 * of which arrived.
 *
 * This is the same message that produced the unterminated-map_name stack
 * overflow: that fix bounded how much the handler writes, but it still reads
 * until it finds a NUL, and with the length discarded by the router there is
 * nothing to tell it to stop.
 */
void test_cycleMapDoesNotOverread(void) {
  NetworkState::setNetworkStatus(_network_state_client);

  GuardedMessage packet(sizeof(NetMessage));
  NetMessage *msg = packet.get();
  msg->message_class = _net_message_class_game_control;
  msg->message_id = _net_message_id_game_control_cycle_map;

  const bool read_past_end = overreads(GameControlRulesDaemon::processNetMessage,
                                       msg, sizeof(NetMessage));

  assert(!read_past_end);
}

/**
 * The same question for the connect class, which is the one a client is
 * exposed to before it trusts the server at all -- every message here arrives
 * from a server the player has merely picked off a list.
 */
void test_connectHandlersDoNotOverread(void) {
  assertClassNeverOverreads(ClientConnectDaemon::processNetMessage,
                            _net_message_class_connect);
}


/// system: set_view, view_control, connect_alert and ping_request all cast to
/// structs with payload behind the header.
void test_systemHandlersDoNotOverread(void) {
  assertClassNeverOverreads(GameManager::processSystemMessage,
                            _net_message_class_system);
}

/// objective: four casts, the widest being the objective sync message.
void test_objectiveHandlersDoNotOverread(void) {
  assertClassNeverOverreads(ObjectiveInterface::clientHandleNetMessage,
                            _net_message_class_objective);
}

/// powerup: create and hit both carry a payload.
void test_powerUpHandlersDoNotOverread(void) {
  assertClassNeverOverreads(PowerUpInterface::processNetMessages,
                            _net_message_class_powerup);
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  // Some handlers report to the on-screen console, which divides by
  // max_char_per_line and takes a modulus by console_size. Both are zero until
  // it is initialised, so an uninitialised console turns any message into
  // SIGFPE -- nothing to do with the packet, but it stops the walk dead.
  ConsoleInterface::initialize(16);

  test_cycleMapDoesNotOverread();
  test_connectHandlersDoNotOverread();
  test_systemHandlersDoNotOverread();
  test_objectiveHandlersDoNotOverread();
  test_powerUpHandlersDoNotOverread();

  return 0;
}

#else  // _WIN32

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  // Some handlers report to the on-screen console, which divides by
  // max_char_per_line and takes a modulus by console_size. Both are zero until
  // it is initialised, so an uninitialised console turns any message into
  // SIGFPE -- nothing to do with the packet, but it stops the walk dead.
  ConsoleInterface::initialize(16);
  // The guard-page trick is POSIX; skipped rather than reimplemented on
  // VirtualAlloc, since the code under test is not platform specific.
  return 0;
}

#endif
