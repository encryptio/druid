require('tests/testlib')

local raw1 = druid.ram(4, 64)
local raw2 = druid.ram(4, 64)
local raw3 = druid.ram(4, 64)
local raw4 = druid.ram(4, 64)

local r1 = druid.verify(raw1)
local r2 = druid.verify(raw2)
local r3 = druid.verify(raw3)
local r4 = druid.verify(raw4)
local rsize = r1:block_count()

druid.log_set_level('warn')
druid.zero(r1)
druid.zero(r2)
druid.zero(r3)
druid.zero(r4)
druid.log_set_level('info')

local x = druid.xor(r1, r2, r3, r4)
test.type(x, "device")
test.eq(x:block_size(), 4)
test.eq(x:block_count(), rsize*3)

local zeroblock  = string.char(0,0,0,0)
local oneblock   = string.char(0,1,0,1)
local twoblock   = string.char(0,2,0,2)
local threeblock = string.char(0,3,0,3)

for i=0,rsize*3-1 do
    test.eq(x:read_block(i), zeroblock)
end

for i=0,rsize*3-1 do
    test.ok(x:write_block(i, oneblock))
end

for i=0,rsize*3-1 do
    test.eq(x:read_block(i), oneblock)
end

function corrupted(blk)
    -- adds corruption that crcs find
    return string.char(string.byte(blk)+1) .. string.sub(blk,2)
end

x:sync()
x:clear_caches()

for i=1,1000 do
    local kill_from = ({ raw1, raw2, raw3, raw4 })[math.random(1,4)]

    for j=1,math.random(1,10) do
        local which_block = math.random(0,kill_from:block_count()-1)
        test.ok(kill_from:write_block(which_block, corrupted(kill_from:read_block(which_block))))
    end

    for i=0,rsize*3-1 do
        test.eq(x:read_block(i), oneblock)
    end
end

