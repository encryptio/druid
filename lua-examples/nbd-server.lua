function make_base()
    return druid.ram(1024, 1024*64)
end

-- use LuaSocket
local socket = require("socket")

-- a few constants used in the protocol
local NBD_PASSWD = "NBDMAGIC"
local NBD_INITMAGIC = string.char(0x00,0x00,0x42,0x02, 0x81,0x86,0x12,0x53)
local NBD_REQUEST_MAGIC  = string.char(0x25,0x60,0x95,0x13)
local NBD_RESPONSE_MAGIC = string.char(0x67,0x44,0x66,0x98)

local NBD_CMD_READ  = string.char(0x00, 0x00, 0x00, 0x00)
local NBD_CMD_WRITE = string.char(0x00, 0x00, 0x00, 0x01)
local NBD_CMD_DISC  = string.char(0x00, 0x00, 0x00, 0x02)

local RESPONSE_OK  = string.char(0x00,0x00,0x00,0x00)
local RESPONSE_NOK = string.char(0x00,0x00,0x00,0x01)

function pack_be64(num)
    local str = ''
    for i = 1,8 do
        str = string.char(num % 0x100) .. str
        num = math.floor(num/0x100)
    end
    return str
end

function pack_be32(num)
    local str = ''
    for i = 1,4 do
        str = string.char(num % 0x100) .. str
        num = math.floor(num/0x100)
    end
    return str
end

function unpack_be64(str)
    local num = 0
    for i = 1,8 do
        num = num*0x100 + str:byte(i)
    end
    return num
end

function unpack_be32(str)
    local num = 0
    for i = 1,4 do
        num = num*0x100 + str:byte(i)
    end
    return num
end

function handle_client(client, device)
    client:send(NBD_PASSWD)
    client:send(NBD_INITMAGIC)
    client:send(pack_be64(device.size))

    local zeroes = ''
    for i = 1,128 do zeroes = zeroes .. string.char(0) end
    client:send(zeroes)

    while true do
        local magic, message = client:receive(4)
        if magic == nil then
            print("read failed: " .. message)
            return
        end
        if magic ~= NBD_REQUEST_MAGIC then
            print("bad request magic - wanted " .. string.format("%q", NBD_REQUEST_MAGIC) .. ", got " .. string.format("%q", magic))
            return
        end

        local cmd = client:receive(4)
        if cmd == NBD_CMD_DISC then
            return
        end

        local handle = client:receive(8)

        local from = unpack_be64(client:receive(8))
        local len  = unpack_be32(client:receive(4))

        if len > 128*1024 then
            print("request length too long - is " .. len)
            return
        end

        if cmd == NBD_CMD_READ then
            print("read " .. from .. "+" .. len)
            local res = nil
            if from + len <= device.size then
                res = device:read_bytes(from, len)
                if res == nil then
                    print("read failed in the underlying device")
                end
            else
                print("oversize read")
            end

            local r = NBD_RESPONSE_MAGIC

            if res ~= nil then
                r = r .. RESPONSE_OK
            else
                r = r .. RESPONSE_NOK
            end
            r = r .. handle
            if res ~= nil then
                r = r .. res
            end

            client:send(r)

        elseif cmd == NBD_CMD_WRITE then
            print("write " .. from .. "+" .. len)
            local data = client:receive(len)
            local res = false
            if from + len > device.size then
                print("oversize write")
            elseif data:len() ~= len then
                print("missized read from socket")
                return
            else
                res = device:write_bytes(from, data)
            end

            local r = NBD_RESPONSE_MAGIC
            if res then
                r = r .. RESPONSE_OK
            else
                r = r .. RESPONSE_NOK
            end
            r = r .. handle

            client:send(r)
        else
            print("bad request type")
            return
        end
    end
end

-- server and accepting loop
local server = assert(socket.bind("*", 0))
local ip, port = server:getsockname()

print("Now listening on " .. ip .. ":" .. port)

local device = make_base()

print("Created device of length " .. device.size)

while true do
    collectgarbage("collect")
    local client = server:accept()
    print("got client")
    handle_client(client, device)
    client:close()
    print("done with client")
    device:flush()
end

