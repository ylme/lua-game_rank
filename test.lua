local assert = assert

local binary_rank = require "binary_rank"

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

local function base_test()
    local obj = binary_rank.new("base test", simple_cmp)
    -- empty test
    assert(obj:rank_from_left(1) == nil)
    assert(obj:rank_from_right(1) == nil)
    assert(obj:item_from_left(1) == nil)
    assert(obj:item_from_right(1) == nil)
    assert(obj:item_by_key(1) == nil)
    assert(obj:item_count() == 0)
    
    -- base test
    assert(obj:update_item(200, "hello 200") == 1)
    assert(obj:update_item(100, "hello 100") == 2)
    assert(obj:update_item(300, "hello 300") == 1)
    assert(obj:update_item(400, "hello 400") == 1)
    assert(obj:item_count() == 4)

    assert(obj:rank_from_left(400) == 1)
    assert(obj:rank_from_left(100) == 4)
    assert(obj:rank_from_right(400) == 4)
    assert(obj:rank_from_right(100) == 1)

    local value, id = obj:item_from_left(1)
    assert(value == "hello 400" and id == 400)
    value, id = obj:item_from_left(4)
    assert(value == "hello 100" and id == 100)

    value, id = obj:item_from_right(1)
    assert(value == "hello 100" and id == 100)
    value, id = obj:item_from_right(4)
    assert(value == "hello 400" and id == 400)

    local keys = obj:dump_from_left()
    assert(#keys == 4)
    assert(keys[1] == 400 and keys[#keys] == 100)
    check_sort(obj, keys, simple_cmp)

    assert(obj:rank_from_left(1) == nil)
    assert(obj:rank_from_right(1) == nil)
    assert(obj:item_from_left(0) == nil)
    assert(obj:item_from_right(5) == nil)
    assert(obj:item_by_key(1) == nil)

    -- remove test
    assert(obj[3][300] == "hello 300")
    obj:remove_item(300)
    assert(obj[3][300] == nil)
    assert(obj:item_count() == 3)
    assert(obj:rank_from_left(300) == nil)

    assert(obj:rank_from_left(400) == 1)
    assert(obj:rank_from_left(100) == 3)
    assert(obj:rank_from_right(400) == 3)
    assert(obj:rank_from_right(100) == 1)

    keys = obj:dump_from_left()
    assert(#keys == 3)
    assert(keys[1] == 400 and keys[#keys] == 100)
    check_sort(obj, keys, simple_cmp)
    
    -- equal item test
    obj:update_item(300, "hello 200")
    obj:update_item(500, "hello 200")

    keys = obj:dump_from_left()
    assert(#keys == 5)
    assert(keys[1] == 400 and keys[#keys] == 100)
    check_sort(obj, keys, simple_cmp)

    obj:remove_item(300)
    assert(obj[3][300] == nil)
    assert(obj[3][500] == "hello 200")
    assert(obj:item_by_key(500) == obj:item_by_key(200))

    keys = obj:dump_from_left()
    assert(#keys == 4)
    assert(keys[1] == 400 and keys[#keys] == 100)
    check_sort(obj, keys, simple_cmp)
end

local function unique_cmp(v1, v2)
    if v1.value > v2.value then
        return 1
    elseif v1.value < v2.value then
        return -1
    else
        if v1.counter < v2.counter then
            return 1
        else
            return -1
        end
    end
end

local function unique_test()
    local counter = 0
    local function counter_n()
        return function() counter = counter + 1 return counter end
    end

    local obj = binary_rank.new("unique rank", unique_cmp)
    obj:update_item(100, {value = 0, counter = counter_n()})
    obj:update_item(200, {value = 0, counter = counter_n()})
    obj:update_item(300, {value = 0, counter = counter_n()})
    obj:update_item(400, {value = 0, counter = counter_n()})
    obj:update_item(500, {value = 0, counter = counter_n()})

    local keys = obj:dump_from_left()
    assert(#keys == 5)
    check_sort(obj, keys, unique_cmp)

    assert(obj:rank_from_left(1) == 100)
    assert(obj:rank_from_right(1) == 500)

    obj:remove_item(100)
    keys = obj:dump_from_left()
    assert(#keys == 4)
    check_sort(obj, keys, unique_cmp)

    assert(obj:rank_from_left(1) == 200)
end

base_test()

