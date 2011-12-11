print("start")

druid.timer(1, function ()
    print("tick @ 1")
end)

for i=1,9 do
    druid.timer(i/10, function ()
        print("tick @ 0."..i)
    end)
end

print("end")

