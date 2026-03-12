/* luzbox_lua_bindings.c
   Compile with: gcc -shared -fPIC -o luzbox_lua_bindings.so luzbox_lua_bindings.c -llua
*/

#include "luzbox.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <string.h>
#include <stdio.h>

/* Forward declarations of external buffers from luzbox.h */
extern uint8_t framebuffer[SCREEN_HEIGHT][SCREEN_WIDTH];
extern uint16_t *tilemap; /* length: TILES_PER_ROW * TILES_PER_COL */
extern SpriteAttr sprite_table[MAX_SPRITES];
extern int16_t audio_frame_buffer[SAMPLES_PER_FRAME];
extern AudioChannel audio_channels[AUDIO_CHANNELS];
extern uint16_t palette[PALETTE_COLORS];

/* ---------------------------
   Helper macros and types
   --------------------------- */

#define FRAMEBUFFER_MT "Luzbox.Framebuffer"
#define TILEMAP_MT     "Luzbox.Tilemap"
#define SPRITETABLE_MT "Luzbox.SpriteTable"
#define AUDIOBUF_MT    "Luzbox.AudioBuffer"
#define PATTERN_MT     "Luzbox.PatternTable" /* optional */

typedef struct {
    void *ptr;
    size_t elem_size;
    size_t length;
    int readonly;
} BufferUserdata;

/* Utility: push error and return lua_error */
static int push_lua_error(lua_State *L, const char *msg) {
    lua_pushnil(L);
    lua_pushstring(L, msg);
    return 2;
}

/* ---------------------------
   Framebuffer userdata methods
   --------------------------- */

/* framebuffer:set(x, y, color) */
static int fb_set(lua_State *L) {
    BufferUserdata *ud = luaL_checkudata(L, 1, FRAMEBUFFER_MT);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    int color = luaL_checkinteger(L, 4);

    if (x < 0 || x >= (int)SCREEN_WIDTH || y < 0 || y >= (int)SCREEN_HEIGHT) {
        return push_lua_error(L, "framebuffer:set out of bounds");
    }
    uint8_t (*fb)[SCREEN_WIDTH] = ud->ptr;
    fb[y][x] = (uint8_t)color;
    return 0;
}

/* framebuffer:get(x, y) -> color */
static int fb_get(lua_State *L) {
    BufferUserdata *ud = luaL_checkudata(L, 1, FRAMEBUFFER_MT);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);

    if (x < 0 || x >= (int)SCREEN_WIDTH || y < 0 || y >= (int)SCREEN_HEIGHT) {
        return push_lua_error(L, "framebuffer:get out of bounds");
    }
    uint8_t (*fb)[SCREEN_WIDTH] = ud->ptr;
    lua_pushinteger(L, fb[y][x]);
    return 1;
}

/* framebuffer:clear(color) */
static int fb_clear(lua_State *L) {
    BufferUserdata *ud = luaL_checkudata(L, 1, FRAMEBUFFER_MT);
    int color = luaL_checkinteger(L, 2);
    uint8_t (*fb)[SCREEN_WIDTH] = ud->ptr;
    for (size_t y = 0; y < SCREEN_HEIGHT; ++y)
        for (size_t x = 0; x < SCREEN_WIDTH; ++x)
            fb[y][x] = (uint8_t)color;
    return 0;
}

/* framebuffer:raw() -> lightuserdata pointer (read-only) */
static int fb_raw(lua_State *L) {
    BufferUserdata *ud = luaL_checkudata(L, 1, FRAMEBUFFER_MT);
    lua_pushlightuserdata(L, ud->ptr);
    return 1;
}

/* ---------------------------
   Tilemap userdata methods
   --------------------------- */

/* tilemap:set(tx, ty, tileIndex) */
static int tm_set(lua_State *L) {
    BufferUserdata *ud = luaL_checkudata(L, 1, TILEMAP_MT);
    int tx = luaL_checkinteger(L, 2);
    int ty = luaL_checkinteger(L, 3);
    int tileIndex = luaL_checkinteger(L, 4);

    if (tx < 0 || tx >= (int)TILES_PER_ROW || ty < 0 || ty >= (int)TILES_PER_COL) {
        return push_lua_error(L, "tilemap:set out of bounds");
    }
    uint16_t *map = ud->ptr;
    size_t idx = (size_t)ty * TILES_PER_ROW + (size_t)tx;
    map[idx] = (uint16_t)tileIndex;
    return 0;
}

/* tilemap:get(tx, ty) -> tileIndex */
static int tm_get(lua_State *L) {
    BufferUserdata *ud = luaL_checkudata(L, 1, TILEMAP_MT);
    int tx = luaL_checkinteger(L, 2);
    int ty = luaL_checkinteger(L, 3);

    if (tx < 0 || tx >= (int)TILES_PER_ROW || ty < 0 || ty >= (int)TILES_PER_COL) {
        return push_lua_error(L, "tilemap:get out of bounds");
    }
    uint16_t *map = ud->ptr;
    size_t idx = (size_t)ty * TILES_PER_ROW + (size_t)tx;
    lua_pushinteger(L, map[idx]);
    return 1;
}

/* tilemap:raw() -> lightuserdata */
static int tm_raw(lua_State *L) {
    BufferUserdata *ud = luaL_checkudata(L, 1, TILEMAP_MT);
    lua_pushlightuserdata(L, ud->ptr);
    return 1;
}

/* ---------------------------
   Sprite table userdata methods
   --------------------------- */

/* spr:count() -> number of sprites */
static int spr_count(lua_State *L) {
    BufferUserdata *ud = luaL_checkudata(L, 1, SPRITETABLE_MT);
    lua_pushinteger(L, (int)MAX_SPRITES);
    return 1;
}

/* spr:set(index, x, y, tile, flags) */
static int spr_set(lua_State *L) {
    BufferUserdata *ud = luaL_checkudata(L, 1, SPRITETABLE_MT);
    int idx = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);
    int y = luaL_checkinteger(L, 4);
    int tile = luaL_checkinteger(L, 5);
    int flags = luaL_checkinteger(L, 6);

    if (idx < 0 || idx >= (int)MAX_SPRITES) {
        return push_lua_error(L, "sprite index out of bounds");
    }
    SpriteAttr *table = ud->ptr;
    table[idx].x = (int16_t)x;
    table[idx].y = (int16_t)y;
    table[idx].tile = (uint16_t)tile;
    table[idx].flags = (uint8_t)flags;
    return 0;
}

/* spr:get(index) -> x,y,tile,flags */
static int spr_get(lua_State *L) {
    BufferUserdata *ud = luaL_checkudata(L, 1, SPRITETABLE_MT);
    int idx = luaL_checkinteger(L, 2);
    if (idx < 0 || idx >= (int)MAX_SPRITES) {
        return push_lua_error(L, "sprite index out of bounds");
    }
    SpriteAttr *table = ud->ptr;
    lua_pushinteger(L, table[idx].x);
    lua_pushinteger(L, table[idx].y);
    lua_pushinteger(L, table[idx].tile);
    lua_pushinteger(L, table[idx].flags);
    return 4;
}

/* spr:raw() -> lightuserdata */
static int spr_raw(lua_State *L) {
    BufferUserdata *ud = luaL_checkudata(L, 1, SPRITETABLE_MT);
    lua_pushlightuserdata(L, ud->ptr);
    return 1;
}

/* ---------------------------
   Audio buffer userdata methods
   --------------------------- */

/* audiobuf:set(i, sample) */
static int ab_set(lua_State *L) {
    BufferUserdata *ud = luaL_checkudata(L, 1, AUDIOBUF_MT);
    int i = luaL_checkinteger(L, 2);
    int sample = luaL_checkinteger(L, 3);
    if (i < 0 || i >= (int)SAMPLES_PER_FRAME) {
        return push_lua_error(L, "audio buffer index out of bounds");
    }
    int16_t *buf = ud->ptr;
    buf[i] = (int16_t)sample;
    return 0;
}

/* audiobuf:get(i) -> sample */
static int ab_get(lua_State *L) {
    BufferUserdata *ud = luaL_checkudata(L, 1, AUDIOBUF_MT);
    int i = luaL_checkinteger(L, 2);
    if (i < 0 || i >= (int)SAMPLES_PER_FRAME) {
        return push_lua_error(L, "audio buffer index out of bounds");
    }
    int16_t *buf = ud->ptr;
    lua_pushinteger(L, buf[i]);
    return 1;
}

/* audiobuf:raw() -> lightuserdata */
static int ab_raw(lua_State *L) {
    BufferUserdata *ud = luaL_checkudata(L, 1, AUDIOBUF_MT);
    lua_pushlightuserdata(L, ud->ptr);
    return 1;
}

/* ---------------------------
   High-level global functions exposed to Lua
   --------------------------- */

/* video_set_pixel(x,y,color) - convenience wrapper */
static int l_video_set_pixel(lua_State *L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int color = luaL_checkinteger(L, 3);
    if (x < 0 || x >= (int)SCREEN_WIDTH || y < 0 || y >= (int)SCREEN_HEIGHT) {
        return push_lua_error(L, "video_set_pixel out of bounds");
    }
    framebuffer[y][x] = (uint8_t)color;
    return 0;
}

/* video_clear(color) */
static int l_video_clear(lua_State *L) {
    int color = luaL_checkinteger(L, 1);
    for (size_t y = 0; y < SCREEN_HEIGHT; ++y)
        for (size_t x = 0; x < SCREEN_WIDTH; ++x)
            framebuffer[y][x] = (uint8_t)color;
    return 0;
}

/* audio_play_note(channel, note, duration, volume) */
static int l_audio_play_note(lua_State *L) {
    int channel = luaL_checkinteger(L, 1);
    int note = luaL_checkinteger(L, 2);
    int duration = luaL_checkinteger(L, 3);
    int volume = luaL_checkinteger(L, 4);

    if (channel < 0 || channel >= (int)AUDIO_CHANNELS) {
        return push_lua_error(L, "audio_play_note invalid channel");
    }
    Luzbox_PlayNote((uint8_t)channel, (uint16_t)note, (uint16_t)duration, (uint8_t)volume);
    return 0;
}

/* audio_play_sample(channel, sample_id, volume) */
static int l_audio_play_sample(lua_State *L) {
    int channel = luaL_checkinteger(L, 1);
    int sample_id = luaL_checkinteger(L, 2);
    int volume = luaL_checkinteger(L, 3);
    if (channel < 0 || channel >= (int)AUDIO_CHANNELS) {
        return push_lua_error(L, "audio_play_sample invalid channel");
    }
    /* For demo: assume sample loader exists; call Luzbox_PlaySample with sample pointer */
    /* Luzbox_PlaySample(channel, sample_ptr, sample_len, volume); */
    /* Here we just queue a note id into channel for compatibility */
    Luzbox_PlayNote((uint8_t)channel, (uint16_t)sample_id, 1, (uint8_t)volume);
    return 0;
}

/* read_buttons(player) -> bitmask */
static int l_read_buttons(lua_State *L) {
    int player = luaL_checkinteger(L, 1);
    if (player < 0 || player > 1) {
        return push_lua_error(L, "read_buttons invalid player");
    }
    uint16_t mask = Luzbox_ReadButtons((uint8_t)player);
    lua_pushinteger(L, mask);
    return 1;
}

/* sync_frame() - call kernel frame sync */
static int l_sync_frame(lua_State *L) {
    Luzbox_Frame();
    return 0;
}

/* debug_print(msg) */
static int l_debug_print(lua_State *L) {
    const char *msg = luaL_checkstring(L, 1);
    Luzbox_DebugPrint(msg);
    return 0;
}

/* ---------------------------
   Metatable registration helpers
   --------------------------- */

static void create_framebuffer_mt(lua_State *L) {
    luaL_newmetatable(L, FRAMEBUFFER_MT);

    lua_pushcfunction(L, fb_set); lua_setfield(L, -2, "set");
    lua_pushcfunction(L, fb_get); lua_setfield(L, -2, "get");
    lua_pushcfunction(L, fb_clear); lua_setfield(L, -2, "clear");
    lua_pushcfunction(L, fb_raw); lua_setfield(L, -2, "raw");

    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pop(L, 1);
}

static void create_tilemap_mt(lua_State *L) {
    luaL_newmetatable(L, TILEMAP_MT);

    lua_pushcfunction(L, tm_set); lua_setfield(L, -2, "set");
    lua_pushcfunction(L, tm_get); lua_setfield(L, -2, "get");
    lua_pushcfunction(L, tm_raw); lua_setfield(L, -2, "raw");

    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pop(L, 1);
}

static void create_spritetable_mt(lua_State *L) {
    luaL_newmetatable(L, SPRITETABLE_MT);

    lua_pushcfunction(L, spr_count); lua_setfield(L, -2, "count");
    lua_pushcfunction(L, spr_set); lua_setfield(L, -2, "set");
    lua_pushcfunction(L, spr_get); lua_setfield(L, -2, "get");
    lua_pushcfunction(L, spr_raw); lua_setfield(L, -2, "raw");

    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pop(L, 1);
}

static void create_audiobuf_mt(lua_State *L) {
    luaL_newmetatable(L, AUDIOBUF_MT);

    lua_pushcfunction(L, ab_set); lua_setfield(L, -2, "set");
    lua_pushcfunction(L, ab_get); lua_setfield(L, -2, "get");
    lua_pushcfunction(L, ab_raw); lua_setfield(L, -2, "raw");

    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pop(L, 1);
}

/* ---------------------------
   Create userdata wrappers and push to Lua globals
   --------------------------- */

static void push_framebuffer_userdata(lua_State *L) {
    BufferUserdata *ud = (BufferUserdata *)lua_newuserdata(L, sizeof(BufferUserdata));
    ud->ptr = framebuffer;
    ud->elem_size = sizeof(uint8_t);
    ud->length = SCREEN_WIDTH * SCREEN_HEIGHT;
    ud->readonly = 0;
    luaL_getmetatable(L, FRAMEBUFFER_MT);
    lua_setmetatable(L, -2);
    lua_setglobal(L, "framebuffer");
}

static void push_tilemap_userdata(lua_State *L) {
    BufferUserdata *ud = (BufferUserdata *)lua_newuserdata(L, sizeof(BufferUserdata));
    ud->ptr = tilemap;
    ud->elem_size = sizeof(uint16_t);
    ud->length = TILES_PER_ROW * TILES_PER_COL;
    ud->readonly = 0;
    luaL_getmetatable(L, TILEMAP_MT);
    lua_setmetatable(L, -2);
    lua_setglobal(L, "tilemap");
}

static void push_spritetable_userdata(lua_State *L) {
    BufferUserdata *ud = (BufferUserdata *)lua_newuserdata(L, sizeof(BufferUserdata));
    ud->ptr = sprite_table;
    ud->elem_size = sizeof(SpriteAttr);
    ud->length = MAX_SPRITES;
    ud->readonly = 0;
    luaL_getmetatable(L, SPRITETABLE_MT);
    lua_setmetatable(L, -2);
    lua_setglobal(L, "spritetable");
}

static void push_audiobuf_userdata(lua_State *L) {
    BufferUserdata *ud = (BufferUserdata *)lua_newuserdata(L, sizeof(BufferUserdata));
    ud->ptr = audio_frame_buffer;
    ud->elem_size = sizeof(int16_t);
    ud->length = SAMPLES_PER_FRAME;
    ud->readonly = 0;
    luaL_getmetatable(L, AUDIOBUF_MT);
    lua_setmetatable(L, -2);
    lua_setglobal(L, "audiobuf");
}

/* ---------------------------
   Main registration function
   --------------------------- */

void register_luzbox_api(lua_State *L) {
    /* Create metatables */
    create_framebuffer_mt(L);
    create_tilemap_mt(L);
    create_spritetable_mt(L);
    create_audiobuf_mt(L);

    /* Push userdata globals */
    push_framebuffer_userdata(L);
    push_tilemap_userdata(L);
    push_spritetable_userdata(L);
    push_audiobuf_userdata(L);

    /* Register global functions */
    lua_register(L, "video_set_pixel", l_video_set_pixel);
    lua_register(L, "video_clear", l_video_clear);

    lua_register(L, "audio_play_note", l_audio_play_note);
    lua_register(L, "audio_play_sample", l_audio_play_sample);

    lua_register(L, "read_buttons", l_read_buttons);
    lua_register(L, "sync_frame", l_sync_frame);

    lua_register(L, "debug_print", l_debug_print);

    /* Expose constants */
    lua_newtable(L);
    lua_pushinteger(L, SCREEN_WIDTH); lua_setfield(L, -2, "SCREEN_WIDTH");
    lua_pushinteger(L, SCREEN_HEIGHT); lua_setfield(L, -2, "SCREEN_HEIGHT");
    lua_pushinteger(L, TILES_PER_ROW); lua_setfield(L, -2, "TILES_PER_ROW");
    lua_pushinteger(L, TILES_PER_COL); lua_setfield(L, -2, "TILES_PER_COL");
    lua_setglobal(L, "UZ_CONSTS");
}
