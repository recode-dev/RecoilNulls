#ifndef SCAN_SCAN_H
#define SCAN_SCAN_H
#include "./offsets.h"
#include <cstdint>
#define OBJ_BLOCK_SIZE 0xe0
#ifndef NSEC_PER_SEC
#    define NSEC_PER_SEC 1000000000ULL
#endif
#ifndef NSEC_PER_USEC
#    define NSEC_PER_USEC 1000ULL
#endif
#include <mach/mach.h>
#include <frida-gum.h>
extern uintptr_t   base;
extern const char* image_name;
extern bool        setup_done;
bool               read_bytes(uintptr_t address, void* out, size_t length);
void               read_cache_reset();
bool               read_int(uintptr_t address, int32_t* out);
bool               read_ptr(uintptr_t address, void** out);
bool               read_ptr(uintptr_t address, uintptr_t* out);
uintptr_t          callable(uintptr_t rva);
int                game_tile_is_passable(uintptr_t tileMap, int cellX, int cellY);
int                game_tile_to_logic(int tile);
int game_segment_hits_circle(int x1, int y1, int x2, int y2, int cx, int cy, int radius, int* out);
int32_t game_vec2_length(int32_t x, int32_t y);
void    game_vec2_normalize(int32_t* v, int32_t scale);
int32_t game_distance(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
bool    find_game_image(uintptr_t* out_base);
void    poll_for_game(int tick);

extern int gum_live;

inline constexpr int  COUNT_MAX = 512;
extern uintptr_t      snapshot[COUNT_MAX];
extern uint8_t        block[COUNT_MAX][OBJ_BLOCK_SIZE];
extern uint8_t        valid[COUNT_MAX];
extern const uint8_t* own_block;
extern int            obj_count;
extern uint32_t       own_hp;
extern int32_t        own_x;
extern int32_t        own_y;
extern uint64_t       frame_index;
void                  scan_world();
void                  workload();
void                  setup();
extern uintptr_t      scene_object;
int                   is_owned_team_object(uintptr_t data);
extern int            coord_ok;
extern int            dead;
extern uintptr_t      elem;
uintptr_t             client();
#endif
