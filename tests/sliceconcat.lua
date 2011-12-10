require("tests/testlib")

local ram = druid.ram(1,8)
test.type(ram, "device")
test.ok(ram:write_bytes(0, string.char(0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07)))

-- first, slicing

local s1 = druid.slice(ram, 0, 1)
test.type(s1, "device")
test.eq(s1:read_bytes(0,1), string.char(0x00))
test.ok(s1:write_bytes(0,string.char(0x80)))
test.eq(s1:read_bytes(0,1), string.char(0x80))
s1:sync()
test.eq(ram:read_bytes(0, 8), string.char(0x80, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07))

test.doeserr(function () druid.slice(ram) end)
test.doeserr(function () druid.slice(ram, 0) end)
test.doeserr(function () druid.slice(ram, 0, 9) end)
test.doeserr(function () druid.slice(ram, -1, 4) end)
test.doeserr(function () druid.slice(ram, 0, -3) end)
test.doeserr(function () druid.slice(0, 0, 4) end)
test.doeserr(function () druid.slice(ram, 0.5, 1) end)
test.doeserr(function () druid.slice(ram, 0, 0) end)
test.doeserr(function () druid.slice(ram, 1, 0) end)
test.doeserr(function () druid.slice(ram, 2, 1.5) end)

local s2 = druid.slice(ram, 1, 2)
test.type(s2, "device")
test.eq(s2:read_bytes(0,2), string.char(0x01,0x02))
test.ok(s2:write_bytes(1,string.char(0x82)))
test.eq(s2:read_bytes(0,2), string.char(0x01,0x82))
s2:sync()
test.eq(ram:read_bytes(0, 8), string.char(0x80, 0x01, 0x82, 0x03, 0x04, 0x05, 0x06, 0x07))

local s3 = druid.slice(ram, 3, 3)
test.type(s3, "device")
test.eq(s3:read_bytes(1,2), string.char(0x04,0x05))
test.ok(s3:write_bytes(1,string.char(0x84,0x85)))
s3:sync()
test.eq(ram:read_bytes(0, 8), string.char(0x80, 0x01, 0x82, 0x03, 0x84, 0x85, 0x06, 0x07))

local s4 = druid.slice(ram, 6, 2)
test.type(s4, "device")
test.eq(s4:read_bytes(0,2), string.char(0x06,0x07))
s4:clear_caches()
test.ok(ram:write_bytes(6,string.char(0x86)))
test.eq(s4:read_bytes(0,2), string.char(0x86,0x07))

test.eq(ram:read_bytes(0, 8), string.char(0x80, 0x01, 0x82, 0x03, 0x84, 0x85, 0x86, 0x07))

-- then, concatenation

local c1 = druid.concat(s1, s2)
test.type(c1, "device")
test.eq(c1.block_count, 3)
test.eq(c1:read_bytes(0,3), string.char(0x80, 0x01, 0x82))

local c2 = druid.concat(s2, s3, s4)
test.type(c2, "device")
test.eq(c2.block_count, 7)
test.eq(c2:read_bytes(0,7), string.char(0x01, 0x82, 0x03, 0x84, 0x85, 0x86, 0x07))
test.ok(c2:write_bytes(0,string.char(0x81)))
test.eq(c2:read_bytes(0,1), string.char(0x81))
c2:sync()
test.eq(ram:read_bytes(0,8), string.char(0x80, 0x81, 0x82, 0x03, 0x84, 0x85, 0x86, 0x07))

local c3 = druid.concat(s3)
test.type(c3, "device")
test.eq(c3.block_count, s3.block_count)
test.eq(c3:read_bytes(0,c3.block_count), s3:read_bytes(0,s3.block_count))

test.doeserr(function () druid.concat() end)
test.doeserr(function () druid.concat(s1,s1) end)
test.doeserr(function () druid.concat(s1,s2,s3,s1) end)

local rebuilt = druid.concat(c1, s3, s4)
test.type(rebuilt, "device")
test.eq(rebuilt.block_count, 8)
test.eq(rebuilt:read_bytes(0,8), ram:read_bytes(0,8))
test.ok(rebuilt:write_bytes(3,string.char(0x83)))
test.eq(rebuilt:read_bytes(0,8), string.char(0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x07))
rebuilt:sync()
test.eq(ram:read_bytes(0,8), string.char(0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x07))
rebuilt:clear_caches()
test.ok(ram:write_bytes(7,string.char(0x87)))
test.eq(rebuilt:read_bytes(6,2), string.char(0x86, 0x87))

