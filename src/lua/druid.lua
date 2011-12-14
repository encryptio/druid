-- !file2h-name!druid_lua_porcelain!

assert(druidraw)
local druidraw = druidraw

druid = {}

local setmetatable = setmetatable
local assert = assert
local error = error
local type = type
local math = math
local string = string
local ipairs = ipairs
local unpack = unpack
local druid = druid

_G.druidraw = nil -- keep users from stumbling upon this
setfenv(1, druid)

--------------------------------------------------------------------------------

local function checktype(val, t, name)
    if name == nil then
        name = ""
    else
        name = "'"..name.."' "
    end
    if t == "int" then
        if type(val) ~= "number" then
            error(name.."argument must be an integer (is "..type(val)..")", 3)
        end
        if val ~= math.floor(val) then
            error(name.."argument must be an integer (is fractional)", 3)
        end
    elseif t == "device" then
        if type(val) ~= "userdata" then
            -- TODO: other checks?
            error(name.."argument must be a device (is "..type(val)..")", 3)
        end
    elseif type(val) ~= t then
        error(name.."argument must be a "..t.." (is "..type(val)..")", 3)
    end
end

--------------------------------------------------------------------------------
-- bdev constructors

druid.ram      = druidraw.ram
druid.concat   = druidraw.concat
druid.encrypt_initialize = druidraw.encrypt_initialize
druid.encrypt  = druidraw.encrypt
druid.slice    = druidraw.slice
druid.stripe   = druidraw.stripe
druid.verify   = druidraw.verify
druid.lazyzero_initialize = druidraw.lazyzero_initialize
druid.lazyzero = druidraw.lazyzero
druid.xor      = druidraw.xor

--------------------------------------------------------------------------------
-- bdev utilities

function zero(dev, progress)
    checktype(dev, "device")
    -- progress optional, defaults to false

    local block_ct = dev:block_count()
    local zblock = string.rep(string.char(0), dev:block_size())

    druid.log('info', 'zero', 'Zeroing device with '..block_ct..' blocks')

    for i=0,block_ct-1 do
        dev:write_block(i, zblock)
        if progress and i % 100 == 0 then
            print("zeroing device "..(i+1).."/"..block_ct)
        end
    end

    druid.log('info', 'zero', 'Finished zeroing device')
end

--------------------------------------------------------------------------------
-- logger

log           = druidraw.log
log_set_level = druidraw.log_set_level

--------------------------------------------------------------------------------
-- loop management

stop_loop = druidraw.stop_loop
timer     = druidraw.timer

--------------------------------------------------------------------------------
-- sockets

druid.tcp_connect = druidraw.loop_tcp_connect
-- tcp_connect(host, port, errcb, conncb, readcb)
-- callbacks are called with the socket as the first argument
-- in the error callback, second argument is the error code (will change)
-- in the read callback, second argument is the data that has been read

return druid
