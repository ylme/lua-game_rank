local mrandom = math.random

local binary_rank = require "binary_rank"

math.randomseed(os.time())

local N = 10 * 10000
local M = 100 * 10000

local function simple_cmp(v1, v2)
    if v1 > v2 then
        return 1
    elseif v1 < v2 then
        return -1
    else
        return 0
    end
end

local function check_sort(obj, keys, func)
    for i=1, #keys-1 do
        local k1, k2 = keys[i], keys[i + 1]
        local v1 = obj:item_by_key(k1)
        local v2 = obj:item_by_key(k2)
        assert(func(v1, v2) >= 0)
    end
end

local function test()
    local obj = binary_rank.new("stress test", simple_cmp, N)
    print(collectgarbage("count"))
    local start = os.clock()
    for i = 1, N do
        obj:update_item(i, mrandom(1, M))
    end
    print("sort finish use sec:" .. (os.clock() - start))
    local keys = obj:dump_from_left()
    check_sort(obj, keys, simple_cmp)

    print(collectgarbage("count"))
    obj = nil
    collectgarbage()
    collectgarbage()
    print(collectgarbage("count")) -- maby memory leak

    local list = {}
    for i = 1, N do
        list[#list + 1] = {i = i, val = mrandom(M)}
    end

    local func = function(v1, v2)
        if v1.val > v2.val then
            return true
        elseif v1.val < v2.val then
            return false
        else
            return v1.i < v2.i
        end
    end
    start = os.clock()
    table.sort(list, func)
    print("table sort finish use sec:" .. (os.clock() - start))
end

test()

