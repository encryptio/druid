require("tests/testlib")

local nocb = function () end

test.doeserr(function () druid.tcp_connect(nil, 1234, nocb, nocb, nocb) end)
test.doeserr(function () druid.tcp_connect("localhost", nil, nocb, nocb, nocb) end)
test.doeserr(function () druid.tcp_connect("localhost", -1, nocb, nocb, nocb) end)
test.doeserr(function () druid.tcp_connect("", 1234, nocb, nocb, nocb) end)
test.doeserr(function () druid.tcp_connect("localhost", 100000, nocb, nocb, nocb) end)
test.doeserr(function () druid.tcp_connect("localhost", 1234) end)
test.doeserr(function () druid.tcp_connect("localhost", 1234, nocb) end)
test.doeserr(function () druid.tcp_connect("localhost", 1234, nocb, nocb) end)
test.doeserr(function () druid.tcp_connect(1234, "localhost", nocb, nocb, nocb) end)

