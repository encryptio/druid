require("tests/testlib")

local r1 = druid.ram(1,4)
test.type(r1, "device")
test.ok(r1:write_bytes(0, "abcd"))

local r2 = druid.ram(1,4)
test.type(r2, "device")
test.ok(r2:write_bytes(0, "efgh"))

local s = druid.stripe(r1, r2)
test.type(s, "device")
test.eq(s.block_count, 8)
test.eq(s.block_size, r1.block_size)
test.eq(s:read_bytes(0,8), "aebfcgdh")

test.ok(s:write_bytes(0,"AB"))
s:sync()
test.eq(r1:read_bytes(0,4), "Abcd")
test.eq(r2:read_bytes(0,4), "Bfgh")

test.ok(s:write_bytes(2,"CDE"))
s:sync()
test.eq(r1:read_bytes(0,4), "ACEd")
test.eq(r2:read_bytes(0,4), "BDgh")

test.ok(s:write_bytes(5,"FGH"))
s:sync()
test.eq(r1:read_bytes(0,4), "ACEG")
test.eq(r2:read_bytes(0,4), "BDFH")

local r3 = druid.ram(1,4)
test.type(r3, "device")
test.ok(r3:write_bytes(0,"ijkl"))

s = druid.stripe(r1, r2, r3)
test.type(s, "device")
test.eq(s.block_count, 12)
test.eq(s.block_size, r1.block_size)

test.eq(s:read_bytes(0,12), "ABiCDjEFkGHl")

test.ok(s:write_bytes(0, "ABCDEFGHIJKL"))
s:sync()
test.eq(r1:read_bytes(0,4), "ADGJ")
test.eq(r2:read_bytes(0,4), "BEHK")
test.eq(r3:read_bytes(0,4), "CFIL")

local r4 = druid.ram(1,6)
test.type(r4, "device")
test.ok(r4:write_bytes(0,"123456"))

druid.log_set_level('err')
s = druid.stripe(r1,r2,r3,r4)
druid.log_set_level('info')
test.type(s, "device")

test.eq(s.block_count, 16)
test.eq(s.block_size, 1)

test.eq(s:read_bytes(0,16), "ABC1DEF2GHI3JKL4")
test.ok(s:write_bytes(0, "ABCDEFGHIJKLMNOP"))
s:sync()
test.eq(r1:read_bytes(0,4), "AEIM")
test.eq(r2:read_bytes(0,4), "BFJN")
test.eq(r3:read_bytes(0,4), "CGKO")
test.eq(r4:read_bytes(0,6), "DHLP56")

druid.log_set_level('err')
s = druid.stripe(r4,r1)
druid.log_set_level('info')
test.type(s, "device")

test.eq(s.block_count, 8)
test.eq(s.block_size, 1)
test.eq(s:read_bytes(0,8), "DAHELIPM")

test.doeserr(function () druid.stripe() end)
test.doeserr(function () druid.stripe(r1,r1) end)
test.doeserr(function () druid.stripe(r1,r2,r3,r1) end)
test.doeserr(function () druid.stripe(r1,r2,r3,r4,r4) end)

