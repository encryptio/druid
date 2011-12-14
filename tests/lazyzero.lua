require("tests/testlib")

local zeroblock = ''
local oneblock  = ''

for i=1,32 do
    zeroblock = zeroblock .. string.char(0)
    oneblock  =  oneblock .. string.char(1)
end

local ram = druid.ram(32,64)
test.type(ram, "device")
for i=0,63 do
    test.ok(ram:write_block(i,oneblock))
end

druid.lazyzero_initialize(ram)
local z = druid.lazyzero(ram)
test.type(z, "device")
test.ok(z:block_count() < ram:block_count())
test.eq(z:block_size(), ram:block_size())

for i=0,z:block_count()-1 do
    test.eq(z:read_block(i), zeroblock)
end

for i=0,z:block_count()-1 do
    test.ok(z:write_block(i, oneblock))

    for j=0,z:block_count()-1 do
        if j <= i then
            test.eq(z:read_block(j), oneblock)
        else
            test.eq(z:read_block(j), zeroblock)
        end
    end
end

druid.log_set_level('none')
test.doeserr(function ()
    local r = druid.ram(32, 64)
    r:write_block(0, zeroblock)
    local z = druid.lazyzero(r) -- dies, bad magic number
    test.type(z, "device")
end)

for blocksize=1,31 do
    test.doeserr(function ()
        local r = druid.ram(blocksize, 64)
        test.ok(druid.lazyzero_create(r)) -- dies, block size too small
    end)
end
druid.log_set_level('info')

