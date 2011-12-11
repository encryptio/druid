require("tests/testlib")

local v = false

druid.timer(1, function ()
    v = true
    test.ok(true)
end)

local ar = {}
local order = 1

for i=1,9 do
    druid.timer(i/10, function ()
        ar[order] = i
        order = order + 1
    end)
end

druid.timer(2, function ()
    test.ok(v)
    test.eq(ar, {1,2,3,4,5,6,7,8,9})
end)

