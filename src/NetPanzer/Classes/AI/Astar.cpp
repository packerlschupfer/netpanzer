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

#include "Astar.hpp"

#include <string.h>

#include <functional>

#include "PathingState.hpp"
#include "Util/Log.hpp"
#include "Util/Timer.hpp"

#define HEURISTIC_WEIGHT 10

Astar::Astar() {
  node_list = 0;

  // Everything below used to be left uninitialised, and one of these fields
  // decides whether the search writes into astar_set_array -- a BitArray that
  // is only ever allocated by setDebugMode(). If sample_set_array_flag
  // happened to come up non-zero, initializePath() set start_sampling_flag,
  // and process_succ() then called setBit() through a null pointer: a
  // segfault during pathfinding, dependent on whatever the memory happened to
  // hold. It reproduces as an immediate crash in a release build while an
  // AddressSanitizer build, which poisons that memory differently, runs
  // clean.
  node_index = 0;
  node_list_size = 0;
  free_list_ptr = 0;
  dynamic_node_management_flag = false;

  best_node = 0;

  sample_set_array_flag = false;
  start_sampling_flag = false;
  debug_mode_flag = false;

  steps = 0;
  step_limit = 0;
  total_steps = 0;
  total_pathing_time = 0.0f;
  heuristic_weight = HEURISTIC_WEIGHT;
  succ_swap_flag = false;
  path_type_flag = 0;
  ini_flag = false;

  path_request_ptr = 0;
  path_merge_type = 0;

  // The two node structs are plain aggregates and were left as-is until a
  // search populated them.
  memset(&current_node, 0, sizeof(current_node));
  memset(&goal_node, 0, sizeof(goal_node));
}

Astar::~Astar() {
  // initializeNodeList() allocates node_list and there was no destructor to
  // release it. The shipped game only ever builds the two static pathers, so
  // this leaked once at shutdown rather than growing -- but it also meant any
  // Astar with automatic or dynamic storage leaked 192 KB per instance.
  delete[] node_list;
  node_list = 0;
}

void Astar::initializeAstar(unsigned long node_list_size,
                            unsigned long step_limit) {
  open_set.initialize(MapInterface::getWidth(), MapInterface::getHeight());
  closed_set.initialize(MapInterface::getWidth(), MapInterface::getHeight());
  open = std::priority_queue<AstarNode *, std::vector<AstarNode *>,
                             AstarNodePtrCompare>();

  initializeNodeList(node_list_size);

  Astar::step_limit = step_limit;

  ini_flag = true;
}

AstarNode *Astar::getNewNode() {
  AstarNode *node_ptr;

  if (dynamic_node_management_flag == true) {
    if (free_list_ptr == 0) {
      LOG(("No new node! (freelist empty)"));
      return 0;
    } else {
      node_ptr = free_list_ptr;
      free_list_ptr = free_list_ptr->parent;
      return node_ptr;
    }
  } else {
    if (node_index >= node_list_size) {
      LOG(("no new node! (nodelist full)"));
      return 0;
    }

    node_ptr = &node_list[node_index];
    node_index++;

    return node_ptr;
  }
}

void Astar::releaseNode(AstarNode *node) {
  if (dynamic_node_management_flag == true) {
    node->parent = free_list_ptr;
    free_list_ptr = node;
  }
}

void Astar::resetNodeList() {
  node_index = 0;

  if (dynamic_node_management_flag == true) {
    int node_list_index;
    int node_list_start;

    node_list_start = node_list_size - 2;

    node_list[node_list_size - 1].parent = 0;

    for (node_list_index = node_list_start; node_list_index >= 0;
         node_list_index--) {
      node_list[node_list_index].parent = &(node_list[node_list_index + 1]);
    }

    free_list_ptr = &(node_list[0]);
  }
}

void Astar::initializeNodeList(unsigned long initial_size) {
  node_index = 0;
  node_list_size = initial_size;

  delete[] node_list;
  node_list = new AstarNode[node_list_size];
}

void Astar::initializePath(iXY &start, iXY &goal, unsigned short path_type) {
  TimeStamp timer_ini_mark = now();

  if (sample_set_array_flag == true) start_sampling_flag = true;

  resetNodeList();

  total_pathing_time = 0;
  steps = 0;
  total_steps = 0;
  succ_swap_flag = false;
  path_type_flag = path_type;

  goal_node.map_loc = goal;

  goal_node.g = getMovementValue(goal);
  goal_node.abs_loc = mapXYtoAbsloc(goal);

  best_node = 0;
  best_node = getNewNode();

  assert(best_node != 0);

  best_node->map_loc = start;
  best_node->abs_loc = mapXYtoAbsloc(start);
  best_node->g = 0;
  best_node->h = heuristic(best_node->map_loc, goal);
  best_node->f = best_node->g + best_node->h;
  best_node->parent = 0;

  assert(open.size() == 0);
  open.push(best_node);

  open_set.setBit(best_node->map_loc.x, best_node->map_loc.y);

  total_pathing_time += now() - timer_ini_mark;
}

unsigned long Astar::mapXYtoAbsloc(iXY map_loc) {
  unsigned long abs;

  if ((map_loc.x < 0) || (map_loc.x >= (int)main_map.getWidth()) ||
      (map_loc.y < 0) || (map_loc.y >= (int)main_map.getHeight()))
    return 0xFFFFFFFF;

  abs = MapInterface::mapXYtoOffset(map_loc);

  return abs;
}

long Astar::heuristic(iXY &pointA, iXY &pointB) {
  long delta_x;
  long delta_y;

  delta_x = labs(pointA.x - pointB.x);
  delta_y = labs(pointA.y - pointB.y);

#if 1
  // we can move diagonal...
  return HEURISTIC_WEIGHT * std::max(delta_x, delta_y);
#else
  return HEURISTIC_WEIGHT * (delta_x + delta_y);
#endif
}

unsigned char Astar::generateSucc(unsigned short direction, AstarNode *node,
                                  AstarNode *succ) {
  unsigned long movement_val;

  if (succ_swap_flag) {
    direction = 7 - direction;
  }

  switch (direction) {
    case 0:
      succ->map_loc.x = 1;
      succ->map_loc.y = 0;
      succ->g = 0;
      break;
    case 1:
      succ->map_loc.x = 1;
      succ->map_loc.y = -1;
      succ->g = 1;
      break;
    case 2:
      succ->map_loc.x = 0;
      succ->map_loc.y = -1;
      succ->g = 0;
      break;
    case 3:
      succ->map_loc.x = -1;
      succ->map_loc.y = -1;
      succ->g = 1;
      break;
    case 4:
      succ->map_loc.x = -1;
      succ->map_loc.y = 0;
      succ->g = 0;
      break;
    case 5:
      succ->map_loc.x = -1;
      succ->map_loc.y = 1;
      succ->g = 1;
      break;
    case 6:
      succ->map_loc.x = 0;
      succ->map_loc.y = 1;
      succ->g = 0;
      break;
    case 7:
      succ->map_loc.x = 1;
      succ->map_loc.y = 1;
      succ->g = 1;
      break;
  }

  succ->map_loc = node->map_loc + succ->map_loc;
  succ->abs_loc = mapXYtoAbsloc(succ->map_loc);

  movement_val = getMovementValue(succ->map_loc);
  if (movement_val != 0xFF)
    if (((UnitBlackBoard::unitOccupiesLoc(succ->map_loc) == true) &&
         (succ->map_loc != goal_node.map_loc))) {
      movement_val = 200;
    }

  succ->g += node->g + movement_val;

  succ->h = heuristic(succ->map_loc, goal_node.map_loc);
  succ->f = succ->h + succ->g;

  succ->parent = 0;

  return movement_val;
}

bool Astar::generatePath(PathRequest *path_request,
                         unsigned short path_merge_type,
                         bool dynamic_node_managment, int *result_code) {
  if (ini_flag) {
    Astar::path_request_ptr = path_request;
    Astar::path_merge_type = path_merge_type;
    Astar::dynamic_node_management_flag = dynamic_node_managment;
    initializePath(path_request->start, path_request->goal,
                   path_request->path_type);
    ini_flag = false;
  }

  return (process_succ(path_request->path, result_code));
}

bool Astar::process_succ(PathList *path, int *result_code) {
  AstarNode *node;
  AstarNode temp_node;
  unsigned long temp;
  bool done = false;
  unsigned short succ_loop;
  bool goal_reachable = true;
  unsigned short path_length;

  TimeStamp timer_path_mark = now();

  while (!done) {
    best_node = 0;
    if (!open.empty()) {
      best_node = open.top();
      open.pop();
    }

    if (best_node == 0) {
      goal_reachable = false;
      done = true;
      break;
    }

    if ((best_node->map_loc == goal_node.map_loc)) {
      done = true;
      break;  // early exit
    } else {
      for (succ_loop = 0; succ_loop < 8; succ_loop++) {
        unsigned char movement_value;
        movement_value = generateSucc(succ_loop, best_node, &temp_node);

        if (movement_value != 0xFF) {
          if (!open_set.getBit(temp_node.map_loc.x, temp_node.map_loc.y))
            if (!closed_set.getBit(temp_node.map_loc.x, temp_node.map_loc.y)) {
              // best_node->child[ succ_loop ] = old;

              node = 0;
              node = getNewNode();

              if (node == 0) {
                done = true;
                goal_reachable = true;  // it was false - PATCH (to limit
                                        // unreachable nodes issue) !!!
                break;                  // early exit
              } else {
                *node = temp_node;
                node->parent = best_node;

                open_set.setBit(node->map_loc.x, node->map_loc.y);

                if (start_sampling_flag == true)
                  astar_set_array.setBit(node->map_loc.x, node->map_loc.y);

                open.push(node);
              }
            }
        }  // ** if valid succesor
      }    // ** for succ_loop

    }  // **else

    open_set.clearBit(best_node->map_loc.x, best_node->map_loc.y);
    closed_set.setBit(best_node->map_loc.x, best_node->map_loc.y);

    if (start_sampling_flag == true)
      astar_set_array.setBit(best_node->map_loc.x, best_node->map_loc.y);

    releaseNode(best_node);

    succ_swap_flag = !succ_swap_flag;

    if (steps > step_limit) {
      steps = 0;
      break;
    } else {
      steps++;
      total_steps++;
    }
  }  // ** while

  total_pathing_time += now() - timer_path_mark;

  if (!goal_reachable) {
    LOG(("goal unreachable %dx%d", goal_node.map_loc.x, goal_node.map_loc.y));

    ini_flag = true;
    cleanUp();

    PathingState::astar_gen_time = total_pathing_time;
    PathingState::astar_gen_time_total += total_pathing_time;
    *result_code = _path_result_goal_unreachable;
    // return false; // PATCH
    return true;
  }

  if (done) {
    bool insert_successful = true;
    path_length = 0;

    node = best_node;
    while ((node != 0) && (insert_successful == true)) {
      if (path_merge_type == _path_merge_front) {
        insert_successful = path->pushFirst(node->abs_loc);
      } else {
        insert_successful = path->pushLast(node->abs_loc);
      }

      node = node->parent;
      path_length++;
    }

    if (insert_successful == false) {
      path->reset();
    } else {
      if (path_merge_type == _path_merge_front) {
        path->popFirst(&temp);
      } else {
        path->popLast(&temp);
      }
    }

    cleanUp();
    ini_flag = true;

    PathingState::path_length = path_length;
    PathingState::astar_gen_time = total_pathing_time;
    PathingState::astar_gen_time_total += total_pathing_time;
    *result_code = _path_result_success;
    return true;
  }

  return false;
}

void Astar::cleanUp() {
  TimeStamp timer_cleanup_mark = now();

  open_set.clear();
  closed_set.clear();

  // STL doesn't define a clear for some reason, so we try a workaround with
  // asignment
  // open.clear();
  open = std::priority_queue<AstarNode *, std::vector<AstarNode *>,
                             AstarNodePtrCompare>();

  resetNodeList();
  ini_flag = true;

  sample_set_array_flag = false;
  start_sampling_flag = false;

  total_pathing_time += now() - timer_cleanup_mark;
}

void Astar::setDebugMode(bool on_off) {
  debug_mode_flag = on_off;

  if (debug_mode_flag == true) {
    astar_set_array.initialize(MapInterface::getWidth(),
                               MapInterface::getHeight());
  } else {
    astar_set_array.deallocate();
  }
}

void Astar::sampleSetArrays() {
  if (debug_mode_flag == true) {
    astar_set_array.clear();
    sample_set_array_flag = true;
    start_sampling_flag = false;
  }
}

BitArray *Astar::getSampledSetArrays() { return &astar_set_array; }

#else

#include <cstdio>
#include <vector>

#include "Astar.hpp"
#include "Classes/AI/PathList.hpp"
#include "Classes/Network/NetworkState.hpp"
#include "Interfaces/GameConfig.hpp"
#include "Interfaces/MapInterface.hpp"
#include "Scripts/ScriptManager.hpp"
#include "test.hpp"

namespace {

/// A path request is answered over several calls, the way PathScheduler
/// spreads the work across ticks. The bound is a test harness detail: it
/// turns "the search never terminates" into a failure instead of a hang.
bool runToCompletion(Astar& astar, PathRequest& request, int* result_code) {
  const int MAX_ITERATIONS = 200000;
  for (int i = 0; i < MAX_ITERATIONS; i++) {
    if (astar.generatePath(&request, _path_merge_front, false, result_code)) {
      return true;
    }
  }
  return false;
}

bool passable(const iXY& loc) {
  return MapInterface::getMovementValue(loc) != 0xFF;
}

/// Collect passable tiles spread across the map, so the tests path over real
/// terrain rather than one hand-picked corner.
std::vector<iXY> findPassableTiles(size_t wanted) {
  std::vector<iXY> found;
  const long w = (long)MapInterface::getWidth();
  const long h = (long)MapInterface::getHeight();

  // Walk a coarse grid; a diagonal would only ever sample one line of terrain.
  const long step = 7;
  for (long y = 1; y < h - 1 && found.size() < wanted; y += step) {
    for (long x = 1; x < w - 1 && found.size() < wanted; x += step) {
      const iXY loc(x, y);
      if (passable(loc)) found.push_back(loc);
    }
  }
  return found;
}

}  // namespace

/**
 * The map has to be loaded before any of this means anything; a silently
 * empty map would make every other assertion below vacuous.
 */
static void testMapLoaded(void) {
  assert(MapInterface::isMapLoaded());
  assert(MapInterface::getWidth() > 0);
  assert(MapInterface::getHeight() > 0);
  printf("  map %zux%zu\n", MapInterface::getWidth(),
         MapInterface::getHeight());
  fflush(stdout);
}

/**
 * A path between two passable tiles must be found, and the result must be a
 * real path rather than an empty list reported as success.
 */
static void testFindsPath(void) {
  const std::vector<iXY> tiles = findPassableTiles(64);
  assert(tiles.size() >= 2);

  Astar astar;
  astar.initializeAstar(4000, 50);

  const iXY start = tiles.front();
  const iXY goal = tiles.back();

  PathList path;
  PathRequest request;
  UnitID id = 1;
  iXY s = start, g = goal;
  request.set(id, s, g, 0, &path, _path_request_full);

  int result_code = -1;
  assert(runToCompletion(astar, request, &result_code));

  if (result_code == _path_result_success) {
    // A successful search over distinct tiles must leave steps behind.
    unsigned long tile = 0;
    assert(path.popFirst(&tile));
  } else {
    // Not every pair on a real map is connected; the only thing that is not
    // allowed is an undefined answer.
    assert(result_code == _path_result_goal_unreachable);
  }
}

/**
 * Start equal to goal is a real case -- a unit ordered to where it already
 * stands -- and must terminate rather than search the whole map.
 */
static void testPathToSelf(void) {
  const std::vector<iXY> tiles = findPassableTiles(1);
  assert(!tiles.empty());

  Astar astar;
  astar.initializeAstar(4000, 50);

  PathList path;
  PathRequest request;
  UnitID id = 2;
  iXY s = tiles[0], g = tiles[0];
  request.set(id, s, g, 0, &path, _path_request_full);

  int result_code = -1;
  assert(runToCompletion(astar, request, &result_code));
}

/**
 * A goal outside the map is reachable from the network: a malformed or
 * hostile move order carries arbitrary coordinates. The search must
 * terminate and stay inside its arrays.
 *
 * Note what it does *not* assert. An unreachable goal makes the search
 * expand until the node list is exhausted, and process_succ then does this:
 *
 *     if (node == 0) { done = true; goal_reachable = true; // PATCH
 *
 * so running out of nodes is reported as _path_result_success with a partial
 * path towards the goal, not as _path_result_goal_unreachable. That is
 * deliberate -- the comment says it limits an "unreachable nodes issue", and
 * units get a partial path instead of refusing to move -- so this test pins
 * termination and memory safety rather than the result code. Anyone tempted
 * to tidy that branch up should know the behaviour is load-bearing.
 */
static void testGoalOutsideMap(void) {
  const std::vector<iXY> tiles = findPassableTiles(1);
  assert(!tiles.empty());

  Astar astar;
  astar.initializeAstar(4000, 50);

  PathList path;
  PathRequest request;
  UnitID id = 3;
  iXY s = tiles[0];
  iXY g((long)MapInterface::getWidth() + 500,
        (long)MapInterface::getHeight() + 500);
  request.set(id, s, g, 0, &path, _path_request_full);

  int result_code = -1;
  assert(runToCompletion(astar, request, &result_code));
  assert(result_code == _path_result_success ||
         result_code == _path_result_goal_unreachable);
}

/**
 * The scheduler reuses one Astar for request after request, so node-list
 * reset has to leave the searcher in a clean state. Run under
 * AddressSanitizer this also exercises the open/closed BitArrays, which are
 * sized from the map dimensions.
 */
static void testManyRequestsReuseOneSearcher(void) {
  const std::vector<iXY> tiles = findPassableTiles(40);
  assert(tiles.size() >= 4);

  Astar astar;
  astar.initializeAstar(4000, 50);

  int solved = 0;
  for (size_t i = 0; i + 1 < tiles.size(); i++) {
    PathList path;
    PathRequest request;
    UnitID id = (UnitID)(100 + i);
    iXY s = tiles[i], g = tiles[i + 1];
    request.set(id, s, g, 0, &path, _path_request_full);

    int result_code = -1;
    assert(runToCompletion(astar, request, &result_code));
    assert(result_code == _path_result_success ||
           result_code == _path_result_goal_unreachable);
    if (result_code == _path_result_success) solved++;
  }

  // Neighbouring passable tiles on a playable map should mostly connect; if
  // nothing at all solved, the searcher is broken rather than the terrain.
  printf("  solved %d of %zu consecutive pairs\n", solved, tiles.size() - 1);
  fflush(stdout);
  assert(solved > 0);
}

/**
 * The corner tiles are where an under-sized open/closed set shows up first,
 * because their bit indices land in the final byte of the BitArray.
 */
static void testPathsAtMapEdges(void) {
  const long w = (long)MapInterface::getWidth();
  const long h = (long)MapInterface::getHeight();

  Astar astar;
  astar.initializeAstar(4000, 50);

  const iXY corners[] = {iXY(0, 0), iXY(w - 1, 0), iXY(0, h - 1),
                         iXY(w - 1, h - 1)};

  for (size_t i = 0; i < 4; i++) {
    for (size_t j = 0; j < 4; j++) {
      if (i == j) continue;

      PathList path;
      PathRequest request;
      UnitID id = (UnitID)(200 + i * 4 + j);
      iXY s = corners[i], g = corners[j];
      request.set(id, s, g, 0, &path, _path_request_full);

      int result_code = -1;
      assert(runToCompletion(astar, request, &result_code));
    }
  }
}

int main(int argc, char* argv[]) {
  (void)argc;

  filesystem::initialize(argv[0], "test_Astar");
  Package::assignDataDir();
  filesystem::addToSearchPath(Package::getDataDir().c_str());

  ScriptManager::initialize();

  // The server path through startMapLoad reads GameConfig::game_mapstyle and
  // does not need any tile graphics, which is what makes this runnable with
  // no window.
  gameconfig = new GameConfig("/config/server.cfg");
  NetworkState::status = _network_state_server;

  MapInterface::startMapLoad("maps/Two clans", "/SummerDay", false, 0);

  testMapLoaded();
  testFindsPath();
  testPathToSelf();
  testGoalOutsideMap();
  testManyRequestsReuseOneSearcher();
  testPathsAtMapEdges();

  return 0;
}

#endif
