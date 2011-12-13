require('tests/testlib')

startsize = collectgarbage("count")

iterations = 20000
function show()
    iterations = iterations - 1
    if iterations > 0 then
        druid.timer(0, show)
    else
        endsize = collectgarbage("count")
        test.ok(endsize < startsize*2+8)
    end
end
druid.timer(0, show)

