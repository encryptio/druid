require('tests/testlib')

-- TODO: test logging itself

druid.log_set_level('junk')
druid.log_set_level('info')
druid.log_set_level('warn')
druid.log_set_level('err')
druid.log_set_level('all')
druid.log_set_level('none')

test.doeserr(function () druid.log_set_level('unknown') end)
test.doeserr(function () druid.log_set_level() end)
test.doeserr(function () druid.log_set_level(1) end)
test.doeserr(function () druid.log_set_level(nil) end)

