require("tests/testlib")

test.eq(1,1)
test.eq("a", "a")
test.neq("a", "b")
test.neq(0, 1)
test.eq(0.0, 0)
test.eq("a" .. "b", "ab")
test.neq("a", 1)
test.eq({}, {})
test.eq({1}, {1})
test.neq({}, {1})
test.neq({1}, {})

test.doeserr(function () error("aaargh") end)
test.doeserr(function ()
    test.eq(1,2)
end)
test.doeserr(function ()
    test.neq(1,1)
end)
test.doeserr(function ()
    test.doeserr(function ()
        return -- don't error
    end)
end)
test.doeserr(function (a)
    test.neq(a,1)
end, 1)

