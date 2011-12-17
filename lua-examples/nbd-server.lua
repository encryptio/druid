function make_base()
    return druid.ram(1024, 1024*64)
end

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

local port = 1234

local device = make_base()
print("Created device of " .. device:block_count() .. " blocks of " .. device:block_size() .. " bytes")
local devicesize = device:block_count() * device:block_size()

local states = {}
local buffers = {}
local extra = {}

function reset_state(id)
    states[id] = 'magic'
    buffers[id] = ''
    extra[id] = { handle = nil, from = nil, len = nil }
end

function start_connection(sock)
    sock:write(NBD_PASSWD)
    sock:write(NBD_INITMAGIC)
    sock:write(pack_be64(device:block_size() * device:block_count()))
    sock:write(string.rep(string.char(0), 128))
end

function nibble(sock, len)
    local id = sock:id()
    assert(buffers[id]:len() >= len)

    local ret = string.sub(buffers[id], 1, len)
    buffers[id] = string.sub(buffers[id], 1+len)

    return ret
end

function check_buffer(sock)
    local id = sock:id()
    local l = buffers[id]:len()
    local s = states[id]

    --print("socket "..id..": "..s.." with "..l.." bytes in buffer")

    if s == 'magic' then
        if l >= 4 then
            local magic = nibble(sock, 4)
            if magic ~= NBD_REQUEST_MAGIC then
                print("bad magic, closing socket")
                sock:close()
                return
            end
            states[id] = 'cmd'
            return check_buffer(sock)
        end
    elseif s == 'cmd' then
        if l >= 4 then
            local cmd = nibble(sock, 4)
            if cmd == NBD_CMD_DISC then
                sock:close()
                return
            elseif cmd == NBD_CMD_READ then
                states[id] = 'read'
                return check_buffer(sock)
            elseif cmd == NBD_CMD_WRITE then
                states[id] = 'write'
                return check_buffer(sock)
            else
                print("bad command, closing socket")
                sock:close()
                return
            end
        end
    elseif s == 'read' or s == 'write' then
        if l >= 20 then
            extra[id]['handle'] = nibble(sock, 8)
            extra[id]['from']   = unpack_be64(nibble(sock, 8))
            extra[id]['len']    = unpack_be32(nibble(sock, 4))

            local from = extra[id]['from']
            local len  = extra[id]['len']

            if len > 128*1024 then
                print("request length too long - is " .. len)
                sock:close()
                return
            end

            if s == 'read' then
                print("read " .. from .. "+" .. len)

                local res = nil
                if from + len <= devicesize then
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
                r = r .. extra[id]['handle']
                if res ~= nil then
                    r = r .. res
                end

                sock:write(r)

                states[id] = 'magic'

            elseif s == 'write' then
                states[id] = 'writedata'
            else
                assert(0)
            end

            return check_buffer(sock)
        end
    elseif s == 'writedata' then
        if l >= extra[id]['len'] then
            local len  = extra[id]['len']
            local from = extra[id]['from']
            print("write " .. from .. "+" .. len)

            local data = nibble(sock, len)
            local res = false
            if from + len > devicesize then
                print("oversize write")
            else
                res = device:write_bytes(from, data)
            end

            local r = NBD_RESPONSE_MAGIC
            if res then
                r = r .. RESPONSE_OK
            else
                r = r .. RESPONSE_NOK
            end
            r = r .. extra[id]['handle']

            sock:write(r)

            states[id] = 'magic'

            return check_buffer(sock)
        end
    end
end

-- don't get too far out of sync
function sync_loop()
    device:sync()
    druid.timer(5, sync_loop)
end
druid.timer(5, sync_loop)

druid.tcp_listen(port, {
    read = function (s,r)
        buffers[s:id()] = buffers[s:id()] .. r
        check_buffer(s)
    end,
    connect = function (s)
        reset_state(s:id())
        start_connection(s)
        print("Got connection")
    end,
    eof = function (s)
        reset_state(s:id())
    end,
    error = function (s,e)
        print("error on socket " .. s .. ": " .. e)
        reset_state(s:id())
    end
})

print("Now listening on port " .. port)

