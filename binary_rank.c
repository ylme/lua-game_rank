#include <lua.h>
#include <lauxlib.h>

#include <stdlib.h>
#include <string.h>

#define RANK_OBJECT "__BINARY_RANK_OBJECT"
#define RANK_UDATA  "__BINARY_RANK_UDATA"

#define INDEX_NAME  1 // [1]: rank name
#define INDEX_CMP   2 // [2]: compare function
#define INDEX_MAP   3 // [3]: key -> value
#define INDEX_UD    4 // [4]: rank user data

#define RANK_INIT_CAPACITY 512

#define MALLOC  malloc
#define FREE    free

typedef struct {
    int item_capacity;
    int item_count;
    lua_Integer *key_binary_array;
} RankData_t;

static RankData_t*
push_rank_ud(lua_State *L, int obj_pos) {
    lua_rawgeti(L, obj_pos, INDEX_UD);
    return luaL_checkudata(L, -1, RANK_UDATA);
}

static void
push_rank_map(lua_State *L, int obj_pos) {
    lua_rawgeti(L, obj_pos, INDEX_MAP);
}

static void
push_rank_cmp(lua_State *L, int obj_pos) {
    lua_rawgeti(L, obj_pos, INDEX_CMP);
}

static const char*
check_name(lua_State *L, int obj_pos) {
    lua_rawgeti(L, obj_pos, INDEX_NAME);
    const char *name = luaL_checkstring(L, -1);
    lua_pop(L, 1);
    return name;
}

static RankData_t*
check_udata(lua_State *L, int obj_pos) {
    if (!lua_istable(L, obj_pos)) {
        luaL_argerror(L, obj_pos, "argument is not a table");
        return NULL;
    }

    RankData_t *rank = push_rank_ud(L, obj_pos);
    if (rank == NULL) {
        luaL_argerror(L, obj_pos, "argument is not a valid rank object");
        return NULL;
    }
    lua_pop(L, 1);
    return rank;
}

static int
traceback(lua_State *L) {
    const char *msg = lua_tostring(L, -1);
    if (msg)
        luaL_traceback(L, L, msg, 1);
    else
        lua_pushliteral(L, "no error message");
    return 1;
}

// stack(bottom->): obj->traceback
// return: > 0: left, < 0: right
static int
compare_value(lua_State *L, int key, int mid_key) {
    push_rank_map(L, 1);
    push_rank_cmp(L, 1);
    // stack(bottom->): obj->traceback->map->cmp

    lua_rawgeti(L, -2, key);
    lua_rawgeti(L, -3, mid_key);

    if (lua_pcall(L, 2, 1, 2) != LUA_OK)
        luaL_error(L, "binary rank: fail to compare:%d-%d,traceback:%s", key, mid_key, lua_tostring(L, -1));
    if (!lua_isinteger(L, -1))
        luaL_error(L, "binary rank: compare function should return an integer:%d-%d", key, mid_key);
    int ret = lua_tointeger(L, -1);
    return ret;
}

// assume item not exists in rank array
// stack(bottom->): obj
static int
find_insert_pos(lua_State *L, RankData_t *rank, int key) {
    lua_pushcfunction(L, traceback);
    // stack(bottom->): obj->traceback

    const lua_Integer *ptr = rank->key_binary_array;
    int left = 0, right = rank->item_count - 1;

    while (left <= right) {
        int mid = (left + right) / 2;
        int mid_key = ptr[mid];

        int ret = compare_value(L, key, mid_key);
        lua_settop(L, 2);

        if (ret > 0) {
            right = mid - 1;
        } else if (ret < 0) {
            left = mid + 1;
        } else {
            left = mid;
            break;
        }
    }

    return left;
}

// assume item exists in rank
// stack(bottom->): obj
static int
find_exist_pos(lua_State *L, RankData_t *rank, int key) {
    push_rank_map(L, 1);

    lua_rawgeti(L, -1, key);
    if (lua_isnoneornil(L, -1))
        return -1;
    lua_pop(L, 2);

    lua_pushcfunction(L, traceback);
    // stack(bottom->): obj->traceback

    const lua_Integer *ptr = rank->key_binary_array;
    int left = 0, right = rank->item_count - 1;
    int find = -1, exact = 0;

    while (left <= right) {
        int mid = (left + right) / 2;
        int mid_key = ptr[mid];

        int ret = compare_value(L, key, mid_key);
        lua_settop(L, 2);

        if (ret > 0) {
            right = mid - 1;
        } else if (ret < 0) {
            left = mid + 1;
        } else {
            find = mid;
            if (key == mid_key)
                exact = 1;
            break;
        }
    }

    if (find >= 0 && !exact) {
        // deal the case: the values are the same, so those keys are adjencent.
        for (int find_left = find, find_right = find + 1;
             find_left >= 0 || find_right < rank->item_count;) {
            if (find_left >= 0) {
                if (key == ptr[find_left] || compare_value(L, key, ptr[find_left]) == 0) {
                    find = find_left;
                    exact = key == ptr[find_left];
                    find_left = exact ? find_left : find_left - 1;
                } else {
                    find_left = -1;
                }
                lua_settop(L, 2);
            }

            if (!exact && find_right < rank->item_count) {
                if (key == ptr[find_right] || compare_value(L, key, ptr[find_right]) == 0) {
                    find = find_right;
                    exact = key == ptr[find_right];
                    find_right = exact ? find_right : find_right + 1;
                } else {
                    find_right = rank->item_count;
                }
                lua_settop(L, 2);
            }

            if (exact)
                break;
        }
    }

    if (!exact)
        luaL_error(L, "binary rank: fail to find item:%d", key);

    return find;
}

static int
meta_gc(lua_State *L) {
    int obj_pos = 1;
    RankData_t *rank = check_udata(L, obj_pos);
    if (rank->key_binary_array) {
        FREE(rank->key_binary_array);
        rank->key_binary_array = NULL;
    }
    return 0;
}

static int
meta_tostring(lua_State *L) {
    int obj_pos = 1;
    const char *name = check_name(L, obj_pos);
    RankData_t *rank = check_udata(L, obj_pos);
    lua_pushfstring(L, "rank:%s-%p,capacity:%d,count:%d", name, rank, rank->item_capacity, rank->item_count);
    return 1;
}

static int
lrank_from_left(lua_State *L) {
    int obj_pos = 1;
    RankData_t *rank = check_udata(L, obj_pos);
    lua_Integer key = luaL_checkinteger(L, 2);
    
    int index = find_exist_pos(L, rank, key);
    if (index >= 0) {
        lua_pushinteger(L, index + 1); // turn rank to from 1 start
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int
lrank_from_right(lua_State *L) {
    int obj_pos = 1;
    RankData_t *rank = check_udata(L, obj_pos);
    lua_Integer key = luaL_checkinteger(L, 2);
    
    int index = find_exist_pos(L, rank, key);
    if (index >= 0) {
        int right_index = rank->item_count-1 - index;
        lua_pushinteger(L, right_index + 1); // trun rank to from 1 start
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int
litem_from_left(lua_State *L) {
    int obj_pos = 1;
    RankData_t *rank = check_udata(L, obj_pos);
    int index = (int)luaL_checkinteger(L, 2) - 1; // turn index to from 0 start

    if (index >= 0 && index < rank->item_count) {
        lua_Integer key = rank->key_binary_array[index];
        push_rank_map(L, obj_pos);
        lua_rawgeti(L, -1, key);
        lua_pushinteger(L, key);
        return 2;
    } else {
        lua_pushnil(L);
        return 1;
    }
}

static int
litem_from_right(lua_State *L) {
    int obj_pos = 1;
    RankData_t *rank = check_udata(L, obj_pos);
    int right_index = (int)luaL_checkinteger(L, 2) - 1; // turn index to from 0 start

    int index = rank->item_count-1 - right_index;
    if (index >= 0 && index < rank->item_count) {
        lua_Integer key = rank->key_binary_array[index];
        push_rank_map(L, obj_pos);
        lua_rawgeti(L, -1, key);
        lua_pushinteger(L, key);
        return 2;
    } else {
        lua_pushnil(L);
        return 1;
    }
}

static int
litem_by_key(lua_State *L) {
    int obj_pos = 1;
    check_udata(L, obj_pos);
    lua_Integer key = luaL_checkinteger(L, 2);

    push_rank_map(L, obj_pos);
    lua_rawgeti(L, -1, key);
    return 1;
}

static int
litem_count(lua_State *L) {
    int obj_pos = 1;
    RankData_t *rank = check_udata(L, obj_pos);
    lua_pushinteger(L, rank->item_count);
    return 1;
}

inline static void
delete_key(RankData_t *rank, int index) {
    if (index < 0 || index >= rank->item_capacity)
        return;
    for (int i = index + 1; i < rank->item_count; i++)
        rank->key_binary_array[i - 1] = rank->key_binary_array[i];
    rank->item_count--;
}

inline static void
insert_key(RankData_t *rank, int index, lua_Integer key) {
    if (index < 0 || index >= rank->item_capacity)
        return;
    for (int i = rank->item_count - 1; i >= index; i--)
        rank->key_binary_array[i + 1] = rank->key_binary_array[i];
    rank->key_binary_array[index] = key;
    rank->item_count++;
}

inline static void
set_item(lua_State *L, lua_Integer key, int obj_pos, int value_pos) {
    push_rank_map(L, obj_pos);
    if (value_pos > 0)
        lua_pushvalue(L, value_pos);
    else
        lua_pushnil(L);
    lua_rawseti(L, -2, key);
    lua_pop(L, 1);
}

static void
expand_array(RankData_t *rank) {
    if (rank->item_count < rank->item_capacity)
        return;
    lua_Integer *prev = rank->key_binary_array;
    rank->item_capacity *= 2;
    lua_Integer *array = MALLOC(sizeof(lua_Integer) * rank->item_capacity);
    memcpy(array, prev, rank->item_count * sizeof(lua_Integer));
    rank->key_binary_array = array;
    FREE(prev);
}

static int
lupdate_item(lua_State *L) {
    int obj_pos = 1, value_pos = 3;
    RankData_t *rank = check_udata(L, obj_pos);
    lua_Integer key = luaL_checkinteger(L, 2);
    luaL_argcheck(L, !lua_isnoneornil(L, value_pos), value_pos, "binary rank: update item but value is nil");

    int index = find_exist_pos(L, rank, key);
    if (index >= 0)
        delete_key(rank, index);

    set_item(L, key, obj_pos, value_pos);
    lua_settop(L, 1); // after: stack(bottom->): obj

    expand_array(rank);

    index = find_insert_pos(L, rank, key);
    if (index < 0) {
        luaL_error(L, "binary rank: update item fail no pos for key:%d", key);
        return 0;
    }

    insert_key(rank, index, key);
    lua_pushinteger(L, index + 1); // return rank from left
    return 1;
}

static int
lremove_item(lua_State *L) {
    int obj_pos = 1;
    RankData_t *rank = check_udata(L, obj_pos);
    lua_Integer key = luaL_checkinteger(L, 2);

    int index = find_exist_pos(L, rank, key);
    if (index >= 0) {
        delete_key(rank, index);
        set_item(L, key, obj_pos, -1);

        lua_pushinteger(L, index + 1); // return prev rank from left
    } else {
        lua_pushnil(L);
    }

    return 1;
}

static int
ldump_from_left(lua_State *L) {
    int obj_pos = 1;
    RankData_t *rank = check_udata(L, obj_pos);

    lua_newtable(L);
    for (int i = 0; i < rank->item_count; i++) {
        lua_Integer key = rank->key_binary_array[i];
        lua_pushinteger(L, key);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int
lnew(lua_State *L) {
    int name_pos = 1, func_pos = 2;
    luaL_argcheck(L, lua_isstring(L, name_pos), name_pos, "binary rank: name is not a string tyep");
    luaL_argcheck(L, lua_isfunction(L, func_pos), func_pos, "binary rank: compare function is not a function type");

    int capacity = luaL_optinteger(L, 3, RANK_INIT_CAPACITY);
    luaL_argcheck(L, capacity >= 0, 3, "binary rank: capacity is less than 0");

    // init rank object
    lua_newtable(L);
    // set name
    lua_pushvalue(L, name_pos);
    lua_rawseti(L, -2, INDEX_NAME);
    // set compare function
    lua_pushvalue(L, func_pos);
    lua_rawseti(L, -2, INDEX_CMP);
    // set map
    lua_newtable(L);
    lua_rawseti(L, -2, INDEX_MAP);
    // set udata
    RankData_t *rank = (RankData_t*)lua_newuserdata(L, sizeof(*rank));
    rank->item_capacity = capacity <= 0 ? RANK_INIT_CAPACITY : capacity;
    rank->item_count = 0;
    rank->key_binary_array = MALLOC(sizeof(lua_Integer) * rank->item_capacity);
    luaL_setmetatable(L, RANK_UDATA);
    lua_rawseti(L, -2, INDEX_UD);

    // set metatable for this rank object
    luaL_setmetatable(L, RANK_OBJECT);
    return 1;
}

LUAMOD_API int 
luaopen_binary_rank(lua_State *L) {
	luaL_checkversion(L);

    if (!luaL_newmetatable(L, RANK_UDATA)) {
        luaL_error(L, "binary rank: rank udata metatable is duplicated:%s", RANK_UDATA);
        return 0;
    }
    lua_pop(L, 1);

    // metatable for rank object:
    luaL_Reg rank_meta[] = {
        {"__gc",            meta_gc},
        {"__tostring",      meta_tostring},
        {NULL, NULL,},
    };

	luaL_Reg rank_lib[] = {
		{"rank_from_left",  lrank_from_left},
		{"rank_from_right", lrank_from_right},
		{"item_from_left",  litem_from_left},
		{"item_from_right", litem_from_right},
		{"item_by_key", 	litem_by_key},
		{"item_count",		litem_count},
		{"update_item",		lupdate_item},
        {"remove_item",     lremove_item},
        {"dump_from_left",  ldump_from_left},
		{NULL, NULL},
	};

    if (!luaL_newmetatable(L, RANK_OBJECT)) {
        luaL_error(L, "binary rank: rank object metatable is duplicated:%s", RANK_OBJECT);
        return 0;
    }

    // set __gc __tostring
    luaL_setfuncs(L, rank_meta, 0);
    // set __index
    lua_pushliteral(L, "__index");
    luaL_newlib(L, rank_lib);
    lua_rawset(L, -3);
    lua_pop(L, 1);

	luaL_Reg lib[] = {
		{"new", lnew},
		{NULL, NULL},
	};
	luaL_newlib(L, lib);
	return 1;
}

