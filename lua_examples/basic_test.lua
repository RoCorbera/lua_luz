-- Use the exposed userdata and functions
framebuffer:set(10, 20, 3)
local c = framebuffer:get(10, 20)
framebuffer:clear(0)

tilemap:set(5, 3, 12)
local t = tilemap:get(5, 3)

spritetable:set(0, 50, 80, 2, 0)
local x,y,tile,flags = spritetable:get(0)

audiobuf:set(0, 0) -- set first sample
audio_play_note(0, 60, 10, 128)

local buttons = read_buttons(0)
sync_frame()
