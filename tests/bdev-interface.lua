require("tests/testlib")

local base = druid.ram(4, 8)
test.type(base, "device")
test.eq(base.block_size, 4)
test.eq(base.block_count, 8)

local word1 = string.char(0x00, 0x01, 0x02, 0x03)
local word2 = string.char(0x04, 0x05, 0x06, 0x07)
local word3 = string.char(0x08, 0x09, 0x0A, 0x0B)
local word4 = string.char(0x0C, 0x0D, 0x0E, 0x0F)

test.ok(base:write_block(0, word1))
test.eq(base:read_block(0), word1)

test.ok(base:write_block(1, word2))
test.eq(base:read_block(1), word2)

test.ok(base:write_block(2, word3))
test.eq(base:read_block(2), word3)

test.ok(base:write_block(3, word4))
test.eq(base:read_block(3), word4)

test.eq(base:read_bytes(0,1), string.char(0x00))
test.eq(base:read_bytes(1,1), string.char(0x01))
test.eq(base:read_bytes(2,4), string.char(0x02, 0x03, 0x04, 0x05))

test.ok(base:write_bytes(1,string.char(0x00)))
test.eq(base:read_bytes(0,2), string.char(0x00, 0x00))

test.doeserr(function() base:read_block(-1)   end)
test.doeserr(function() base:read_block( 8)   end) 
test.doeserr(function() base:read_block( 0.5) end)
test.doeserr(function() base:read_block(nil) end)

test.doeserr(function() base:write_block(-1,   word1) end) 
test.doeserr(function() base:write_block( 8,   word1) end) 
test.doeserr(function() base:write_block( 0)          end) 
test.doeserr(function() base:write_block( 0,   string.char(0x00)) end)
test.doeserr(function() base:write_block( 0.5, word1) end)

test.doeserr(function() base:read_bytes(-1,1) end)
test.doeserr(function() base:read_bytes(0,0)  end)
test.doeserr(function() base:read_bytes(8*4,1)  end)
test.doeserr(function() base:read_bytes(3)    end)
test.doeserr(function() base:read_bytes(8*4-3,4)  end)

test.doeserr(function() base:write_bytes(0, "") end)
test.doeserr(function() base:write_bytes(0.5, "a") end)
test.doeserr(function() base:write_bytes(2) end)
test.doeserr(function() base:write_bytes(1, 1) end)
test.doeserr(function() base:write_bytes(4*8-3, "asdf") end)

druid.log_set_level('warn')
local zword = string.char(0,0,0,0)
druid.zero(base)
for i=0,7 do
    test.eq(base:read_block(i), zword)
end

