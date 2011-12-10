test = {}

function test.type(val, ty)
    if ty == "int" then
        if type(val) ~= "number" then
            error("expected an integer (is "..type(val)..")", 2)
        end
        if val ~= math.floor(val) then
            error("expected an integer (is fractional)", 2)
        end
    elseif ty == "device" then
        if type(val) ~= "table" or not val.is_bdev then
            error("expected a device (is "..type(val)..")", 2)
        end
    elseif type(val) ~= ty then
        error("expected a "..ty.." (is "..type(val)..")", 2)
    end
end

function test.compare(val, expect)
    -- true if equal, false if different

    local t = type(val)
    local e = type(expect)
    if t ~= e then return false end

    if t == "table" then
        for k,v in pairs(val) do
            if not test.compare(expect[k], v) then
                return false
            end
        end
        for k,v in pairs(expect) do
            if not test.compare(val[k], v) then
                return false
            end
        end

        return true
    else
        return val == expect
    end
end

function test.eq(val, expect)
    if not test.compare(val, expect) then
        error("equality failed", 2)
    end
end

function test.neq(val, expect)
    if test.compare(val, expect) then
        error("equality succeeded, expected to fail", 2)
    end
end

function test.doeserr(f, ...)
    if args == nil then args = {} end
    local worked, message = pcall(f, unpack(arg))
    if worked then
        error("was supposed to crash, but returned okay", 2)
    end
end

return test

