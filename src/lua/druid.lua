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
-- bdevs

bdev = {}

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
        if type(val) ~= "table" or not val.is_bdev then
            error(name.."argument must be a device (is "..type(val)..")", 3)
        end
    elseif type(val) ~= t then
        error(name.."argument must be a "..t.." (is "..type(val)..")", 3)
    end
end

function bdev.new_io(io, children)
    -- "children" argument is simply a table of the children, if any.
    -- used for informational purposes as well as to guide garbage collection
    if children == nil then children = {} end

    assert(io)
    local self = { io = io, is_bdev = true, children = children }

    setmetatable(self, {__index = bdev})

    self.some_finalizer_holder = druidraw.close_on_gc(io)

    self.block_size  = druidraw.bdev_get_block_size(self.io)
    self.block_count = druidraw.bdev_get_block_count(self.io)
    self.size = self.block_size * self.block_count

    return self
end

function bdev:read_bytes(start, len)
    checktype(start, "int", "start")
    checktype(len,   "int", "len")
    if start+len > self.size then
        error("Can't read past the end of the device", 2)
    elseif start < 0 then
        error("Can't read before the start of the device", 2)
    elseif len == 0 then
        error("Can't read 0 bytes", 2)
    end

    return druidraw.bdev_read_bytes(self.io, start, len)
end

function bdev:write_bytes(start, data)
    checktype(start, "int",    "start")
    checktype(data,  "string", "data")
    if start+data:len() > self.size then
        error("Can't write past the end of the device", 2)
    elseif start < 0 then
        error("Can't write before the start of the device", 2)
    elseif data:len() == 0 then
        error("Can't write 0 bytes", 2)
    end

    return druidraw.bdev_write_bytes(self.io, start, data)
end

function bdev:read_block(which)
    checktype(which, "int", "which")
    if which < 0 then
        error("Can't read before the start of the device", 2)
    end
    if which >= self.block_count then
        error("Can't read after the end of the device", 2)
    end

    return druidraw.bdev_read_block(self.io, which)
end

function bdev:write_block(which, data)
    checktype(which, "int",    "which")
    checktype(data,  "string", "data")
    if which < 0 then
        error("Can't write before the start of the device", 2)
    elseif which >= self.block_count then
        error("Can't write after the end of the device", 2)
    elseif data:len() ~= self.block_size then
        error("Can't write a block that isn't the length of the block size (got "..data:len()..", wanted "..self.block_size..")", 2)
    end

    return druidraw.bdev_write_block(self.io, which, data)
end

function bdev:clear_caches()
    druidraw.bdev_clear_caches(self.io)
end

function bdev:flush()
    druidraw.bdev_flush(self.io)
end

function bdev:sync()
    druidraw.bdev_sync(self.io)
end

--------------------------------------------------------------------------------
-- bdev constructors

local function maybe_wrap_bdev(val, children)
    if val == nil then return val end
    return bdev.new_io(val, children)
end

function ram(block_size, block_count)
    checktype(block_size,  "int", "block_size")
    checktype(block_count, "int", "block_count")
    if block_size < 1 then
        error("Unreasonable block size", 2)
    elseif block_count < 1 then
        error("Unreasonable block count", 2)
    end

    -- TODO: power of two verification

    return maybe_wrap_bdev(druidraw.bio_create_malloc(block_size, block_count))
end

function file()
    error("file not implemented", 2)
    -- TODO: something with automatically picking between
    -- mmap and posixfd
end

function concat(...)
    local data_found = {}
    local raw_args = {}
    for _,v in ipairs(arg) do
        checktype(v, "device")
        raw_args[#raw_args+1] = v.io
        if data_found[v.io] then
            error("Can't concat a device with itself", 2)
        end
        data_found[v.io] = true
    end

    return maybe_wrap_bdev(druidraw.concat_open(unpack(raw_args)), arg)
end

function encrypt_initialize(dev, key)
    checktype(dev, "device", "dev")
    checktype(key, "string", "key")

    return druidraw.encrypt_create(dev.io, key)
end

function encrypt(dev, key)
    checktype(dev, "device", "dev")
    checktype(key, "string", "key")

    return maybe_wrap_bdev(druidraw.encrypt_open(dev.io, key), {dev})
end

function slice(dev, start, len)
    checktype(dev,   "device", "dev")
    checktype(start, "int",    "start")
    checktype(len,   "int",    "len")
    if start < 0 then
        error("Can't open a slice to before the start of the device", 2)
    elseif start+len > dev.block_count then
        error("Can't open a slice to after the end of the device", 2)
    elseif len <= 0 then
        error("Can't open a slice with a non-positive size", 2)
    end

    return maybe_wrap_bdev(druidraw.slice_open(dev.io, start, len), {dev})
end

function stripe(...)
    local data_found = {}
    local raw_args = {}
    for _,v in ipairs(arg) do
        checktype(v, "device")
        raw_args[#raw_args+1] = v.io
        if data_found[v.io] then
            error("Can't stripe a device with itself", 2)
        end
        data_found[v.io] = true
    end

    return maybe_wrap_bdev(druidraw.stripe_open(unpack(raw_args)), arg)
end

function verify(dev)
    checktype(dev, "device", "dev")
    return maybe_wrap_bdev(druidraw.verify_create(dev.io), {dev})
end

function lazyzero_create(dev)
    checktype(dev, "device", "dev")
    return druidraw.lazyzero_create(dev.io)
end

function lazyzero(dev)
    checktype(dev, "device", "dev")
    return maybe_wrap_bdev(druidraw.lazyzero_open(dev.io), {dev})
end

function zero(dev, progress)
    checktype(dev, "device")
    -- progress optional, defaults to false

    local block_ct = dev.block_count
    local zblock = ''
    for i=1,dev.block_size do
        zblock = zblock .. string.char(0)
    end

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
