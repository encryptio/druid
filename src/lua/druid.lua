-- !file2h-name!druid_lua_porcelain!

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
-- bdev utilities

function druid.zero(dev, progress)
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

return druid
