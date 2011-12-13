print("start")

druid.timer(1, function()
    print("one")
end)

druid.timer(2, function()
    print("two")
end)

druid.timer(1.5, function()
    print("stopping")
    druid.stop_loop()
end)

