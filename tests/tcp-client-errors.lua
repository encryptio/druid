require("tests/testlib")

local nocb = function () end

test.doeserr(function () druid.tcp_connect("localhost", 1234) end)
test.doeserr(function () druid.tcp_connect("localhost", 1234, nocb) end) -- cb returns nil
test.doeserr(function () druid.tcp_connect("localhost", 1234, nocb, nocb) end)
test.doeserr(function () druid.tcp_connect(1234, "localhost", nocb) end)

test.doeserr(function () druid.tcp_connect("localhost", 1234, { }) end)
test.doeserr(function () druid.tcp_connect("localhost", 1234, { connect = nocb, error = nocb, read = nocb }) end)
test.doeserr(function () druid.tcp_connect("localhost", 1234, { connect = nocb, error = nocb, eof = nocb }) end)
test.doeserr(function () druid.tcp_connect("localhost", 1234, { connect = nocb, read = nocb, eof = nocb }) end)
test.doeserr(function () druid.tcp_connect("localhost", 1234, { error = nocb, read = nocb, eof = nocb }) end)

