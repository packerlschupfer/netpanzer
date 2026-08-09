
function pairs(t)
    local mt = getmetatable(t)
    local iter = mt and mt.__next or next
    return iter, t, nil
end

game = { tasks = {} };
game.addTask = function(name, task)
--    if name ~= nil then
        game.tasks[name] = task;
--    else
--        table.insert(game.tasks, task);
--    end
end;

game.hasTask = function(name)
    return game.tasks[name] ~= nil;
end;

function dump_table(result, t, extra)
    local lin = extra or ""

    if type(t) ~= 'table' then
        --LOGGER:log("ERROR dumping table: it is not a table")
        return
    end    
    
    -- Sorted, not raw pairs(). Lua 5.4 randomises its hash seed for every
    -- state, so pairs() hands back a different order on every launch and the
    -- config file was being rewritten shuffled each time. Sorting makes the
    -- output depend only on the contents.
    local keys = {}
    for key in pairs(t) do
        keys[#keys + 1] = key
    end
    table.sort(keys, function(a, b)
        if type(a) == type(b) then
            return a < b
        end
        -- Mixed key types never compare with <; order them by type name so
        -- the result is still deterministic.
        return type(a) < type(b)
    end)

    for _,key in ipairs(keys) do
        local value = t[key]
        local keytext
	    local valuetext
    
        if type(key) == 'number' then
            if extra then
                keytext = "[" .. key .. "]"
            end
        else
            if extra then
                keytext = "." .. key
            else
                keytext = key;
            end
        end
        
	    if type(value) == 'table' then
	        if keytext then
	            dump_table(result, value, (lin or "") .. keytext)
            end
	    elseif type(value) == 'string' then
            valuetext = '"' .. string.gsub(value,'"','\\"') .. '"'
        elseif type(value) ~= 'function' then
            valuetext = tostring(value)
        end
        
        if keytext and valuetext then
            table.insert(result, lin .. keytext .. " = " .. valuetext)
        end
	end
end

gconcat = table.concat;

config.dump = function(table)
    result = {}
    dump_table(result, table)
    return gconcat(result,"\n");
end

count_time = 0;

function scriptLoop()
    for i, func in pairs(game.tasks) do
        if func() then game.tasks[i] = nil end
    end
end

--LOGGER:log("Dumping conf:\n" .. config.dump(config))

