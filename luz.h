#ifndef LUZBOX_H
#define LUZBOX_H

#include <stdint.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
   Platform constants
   ------------------------------------------------------------------------- */

#define SCREEN_WIDTH        240u
#define SCREEN_HEIGHT       224u

#define TILE_WIDTH          8u
#define TILE_HEIGHT         8u

#define TILES_PER_ROW       (SCREEN_WIDTH / TILE_WIDTH)
#define TILES_PER_COL       (SCREEN_HEIGHT / TILE_HEIGHT)

#define PALETTE_COLORS      16u    /* number of colors in palette */
#define MAX_SPRITES         64u    /* hardware sprite limit (example) */

#define AUDIO_SAMPLE_RATE   44100u
#define FRAMES_PER_SECOND   60u
#define SAMPLES_PER_FRAME   (AUDIO_SAMPLE_RATE / FRAMES_PER_SECOND) /* ~735 */

/* -------------------------------------------------------------------------
   Input bitmasks
   Keep these values stable so Lua scripts and Luzbox assets can use them.
   ------------------------------------------------------------------------- */

#define BUTTON_A            (1u << 0)
#define BUTTON_B            (1u << 1)
#define BUTTON_SELECT       (1u << 2)
#define BUTTON_START        (1u << 3)
#define BUTTON_UP           (1u << 4)
#define BUTTON_DOWN         (1u << 5)
#define BUTTON_LEFT         (1u << 6)
#define BUTTON_RIGHT        (1u << 7)
#define BUTTON_L            (1u << 8)
#define BUTTON_R            (1u << 9)

/* -------------------------------------------------------------------------
   Video data structures and memory layout
   - Pattern table: raw tile bitmaps (indexed by tile id)
   - Tilemap: indices into pattern table for background
   - Sprite attribute table: per-sprite x,y,tile,flags
   ------------------------------------------------------------------------- */

/* Pattern (tile) format: each tile is TILE_WIDTH x TILE_HEIGHT pixels.
   Representation is implementation-defined (e.g., 2bpp, 4bpp). Use bytes
   per tile depending on chosen bitdepth. */
typedef struct {
    uint8_t *data;      /* pointer to raw tile bytes */
    size_t   size;      /* bytes per tile */
} PatternTable;

/* Tilemap: linear array of tile indices, row-major */
extern uint16_t *tilemap;        /* length: TILES_PER_ROW * TILES_PER_COL */

/* Sprite attribute entry */
typedef struct {
    int16_t x;           /* sprite X position (signed to allow offscreen) */
    int16_t y;           /* sprite Y position */
    uint16_t tile;       /* tile index in pattern table */
    uint8_t  flags;      /* bitflags: flipX, flipY, palette, priority */
    uint8_t  reserved;
} SpriteAttr;

/* Sprite attribute table */
extern SpriteAttr sprite_table[MAX_SPRITES];

/* Palette: array of PALETTE_COLORS entries (format: 16-bit RGB565 or custom) */
extern uint16_t palette[PALETTE_COLORS];

/* Optional linear framebuffer (if you choose to implement pixel API) */
extern uint8_t framebuffer[SCREEN_HEIGHT][SCREEN_WIDTH];

/* -------------------------------------------------------------------------
   Audio structures
   - Channel model: queue notes or samples per channel
   - Mixer: fills audio frame buffer each 60Hz frame
   ------------------------------------------------------------------------- */

#define AUDIO_CHANNELS      4u

typedef struct {
    uint8_t  active;     /* 0 = free, 1 = playing */
    uint8_t  volume;     /* 0..255 */
    uint16_t note;       /* note or sample id */
    uint16_t duration;   /* frames remaining */
    int32_t  phase;      /* internal phase accumulator */
} AudioChannel;

extern AudioChannel audio_channels[AUDIO_CHANNELS];

/* Per-frame audio output buffer (signed 16-bit PCM) */
extern int16_t audio_frame_buffer[SAMPLES_PER_FRAME];

/* -------------------------------------------------------------------------
   Kernel hooks and lifecycle
   - init: called once at startup
   - frame: called once per frame from main loop or kernel
   - irq-safe helpers: functions that can be called from interrupt context
   ------------------------------------------------------------------------- */

/* Called once at program start to initialize hardware state */
void Luzbox_Init(void);

/* Called once per frame from main loop. Game code should implement
   Luzbox_Frame or register an update callback. */
void Luzbox_Frame(void);

/* Optional: register a frame callback implemented in game code */
typedef void (*LuzboxFrameCallback)(void);
void Luzbox_RegisterFrameCallback(LuzboxFrameCallback cb);

/* IRQ-safe helpers: these functions are safe to call from the audio/video
   interrupt or from the main thread with appropriate locking. Documented
   to be atomic or to require caller to disable interrupts. */
void Luzbox_UpdateSpriteTableIRQ(const SpriteAttr *src, size_t count);
void Luzbox_QueueAudioNoteIRQ(uint8_t channel, uint16_t note, uint16_t duration, uint8_t volume);

/* -------------------------------------------------------------------------
   High-level video helpers (C implementations)
   These are convenience functions you can expose to Lua. They operate on
   tilemap/patterns or framebuffer depending on your chosen rendering model.
   ------------------------------------------------------------------------- */

/* Tilemap helpers */
void Luzbox_SetTile(int tileX, int tileY, uint16_t tileIndex);
uint16_t Luzbox_GetTile(int tileX, int tileY);

/* Sprite helpers */
int Luzbox_AllocSprite(void); /* returns sprite index or -1 */
void Luzbox_FreeSprite(int spriteIndex);
void Luzbox_SetSprite(int spriteIndex, int x, int y, uint16_t tile, uint8_t flags);

/* Framebuffer pixel helpers (if using pixel API) */
void Luzbox_SetPixel(int x, int y, uint8_t color);
uint8_t Luzbox_GetPixel(int x, int y);
void Luzbox_ClearScreen(uint8_t color);

/* Palette helpers */
void Luzbox_SetPaletteEntry(uint8_t index, uint16_t color);
uint16_t Luzbox_GetPaletteEntry(uint8_t index);

/* Audio helpers */
void Luzbox_PlayNote(uint8_t channel, uint16_t note, uint16_t duration, uint8_t volume);
void Luzbox_PlaySample(uint8_t channel, const int16_t *pcm, size_t samples, uint8_t volume);

/* Input helpers */
uint16_t Luzbox_ReadButtons(uint8_t player); /* returns bitmask using BUTTON_* */

/* Debug / system */
void Luzbox_DebugPrint(const char *msg);

/* -------------------------------------------------------------------------
   Lua binding registration
   Provide a single entry point to register all functions with a Lua state.
   The implementation should register both high-level helpers and low-level
   buffer access (via lightuserdata or userdata) so Lua can manipulate
   tilemap, pattern table, sprite table, and audio channels directly.
   ------------------------------------------------------------------------- */

struct lua_State;
void register_luzbox_api(struct lua_State *L);

/* -------------------------------------------------------------------------
   Notes and implementation guidance
   - Keep the memory layout of tilemap, pattern table, and sprite_table
     stable and documented so Luzbox toolchain assets can be loaded unchanged.
   - If the renderer runs in an interrupt, updates to sprite_table or
     tilemap must be done via IRQ-safe helpers or with proper locking.
   - Expose raw pointers to Lua as lightuserdata only when you also provide
     safe helper functions; direct writes from Lua must respect alignment
     and expected formats (e.g., tile bytes per tile).
   - Choose a tile bitdepth (2bpp, 4bpp) and document bytes-per-tile in
     PatternTable.size so loaders can parse assets correctly.
   ------------------------------------------------------------------------- */

#endif /* LUZBOX_H */
