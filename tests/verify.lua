require("tests/testlib")

local zeroblock = string.char(0,0,0,0, 0,0,0,0)
local someblock = string.char(0xd4, 0x3f, 0x11, 0xff, 0x00, 0xa9, 0x8b, 0x2c)

local ram = druid.ram(8, 8)
test.type(ram, "device")

for i = 0,7 do
    test.ok(ram:write_block(i, zeroblock))
end

local v = druid.verify(ram)
test.type(v, "device")

test.ok(v.block_count < ram.block_count)

for i = 0,v.block_count-1 do
    test.ok(v:read_block(i))
end

test.ok(v:write_block(0, someblock))
test.eq(v:read_block(0), someblock)

v:sync()
v:clear_caches()

test.ok(ram:write_block(1, zeroblock))
test.eq(v:read_block(0), nil)

test.ok(v:write_block(0, someblock))

v:sync()
v:clear_caches()

test.ok(ram:write_block(0, zeroblock))
test.eq(v:read_block(0), nil)
test.eq(v:read_block(v.block_count-1), zeroblock)

druid.log_show_level('none')
test.eq(druid.verify(druid.ram(2,8)), nil)
druid.log_show_level('info')

