#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdint.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "khash.h"

struct word_tree;

KHASH_MAP_INIT_STR(word,struct word_tree*);

struct word_tree {
	khash_t(word)* hash;
};

int
split_utf8(const char* word,size_t size,int offset) {
	int i;
	for(i = offset;i < size;i++) {
		char ch = word[i];
		if((ch & 0x80) == 0) {  
            return 1;
        } else if((ch & 0xF8) == 0xF8) {  
            return 5;  
        } else if((ch & 0xF0) == 0xF0) {  
            return 4;  
        } else if((ch & 0xE0) == 0xE0) {  
            return 3;  
        } else if((ch & 0xC0) == 0xC0) {  
            return 2;  
        }  
	}
	assert(0);
}

void
word_add(struct word_tree* root_tree, const char* word,size_t size) {
	struct word_tree* tree = root_tree;
	int i;
	for(i = 0;i < size;) {
		char ch[5] = {0};
		
		int length = split_utf8(word,size,i);
		memcpy(ch,&word[i],length);
		i += length;

		khiter_t k = kh_get(word, tree->hash, ch);
		int missing = (k == kh_end(tree->hash));
		if (missing) {
			struct word_tree* next_tree = NULL;
			int result;
			khiter_t k = kh_put(word, tree->hash, strdup(ch), &result);
			if (result == 1 || result == 2) {
				next_tree = malloc(sizeof(*next_tree));
				next_tree->hash = NULL;
				kh_value(tree->hash, k) = next_tree;
			} else if (result == 0) {
				next_tree = kh_value(tree->hash, k);
			} else {
				assert(0);
			}
			
			if (i != size) {
				if (!next_tree->hash)
					next_tree->hash = kh_init(word);
			}
			tree = next_tree;
		} else {
			tree = kh_value(tree->hash, k);
			if (!tree->hash)
				tree->hash = kh_init(word);
		}
	}
}

void
word_filter(struct word_tree* root_tree,char* word,size_t size) {
	struct word_tree* tree = root_tree;

	int start = 0;
	int i;
	for(i = 0;i < size;) {
		char ch[5] = {0};
		
		int length = split_utf8(word,size,i);
		memcpy(ch,&word[i],length);
		i += length;

		khiter_t k = kh_get(word, tree->hash, ch);
		int missing = (k == kh_end(tree->hash));
		if (!missing) {
			tree = kh_value(tree->hash, k);
			if (!tree->hash) {
				memset(word + start + 1,'*',i - start - 1);
				tree = root_tree;
				start = i;
			}
		} else {
			tree = root_tree;
			start = i - length;
		}
	}
}

static int
lcreate(lua_State* L) {
	struct word_tree* tree = lua_newuserdata(L,sizeof(*tree));
	tree->hash = kh_init(word);
	luaL_newmetatable(L,"meta_filter");
 	lua_setmetatable(L, -2);
	return 1;
}

void
release(char* word,struct word_tree* tree) {
	if (tree->hash) {
		struct word_tree* child = NULL;
		const char* word = NULL;
		kh_foreach(tree->hash, word, child, release((char*)word, child));
		kh_destroy(word,tree->hash);
	}
	free(tree);
	free(word);
}

static int
lrelease(lua_State* L) {
	struct word_tree* tree = lua_touserdata(L,1);

	struct word_tree* child = NULL;
	const char* word = NULL;
	kh_foreach(tree->hash, word, child, release((char*)word, child));
	kh_destroy(word,tree->hash);
	
	return 0;
}

static int
ladd(lua_State* L) {
	struct word_tree* tree = lua_touserdata(L,1);
	size_t size;
	const char* word = lua_tolstring(L,2,&size);
	word_add(tree,word,size);
	return 0;
}

static int
lfilter(lua_State* L) {
	struct word_tree* tree = lua_touserdata(L,1);
	size_t size;
	const char* word = lua_tolstring(L,2,&size);
	char* block = strdup(word);
	word_filter(tree,block,size); 
	lua_pushlstring(L,block,size);
	free(block);
	return 1;
}

void
dump(const char* word,struct word_tree* tree) {
	printf("%s\n",word);
	if (tree->hash) {
		struct word_tree* child = NULL;
		const char* word = NULL;
		kh_foreach(tree->hash, word, child, dump((char*)word, child));
	}
}

static int
ldump(lua_State* L) {
	struct word_tree* tree = lua_touserdata(L,1);
	struct word_tree* child = NULL;
	const char* word = NULL;
	kh_foreach(tree->hash, word, child, dump((char*)word, child));
	return 0;
}

int 
luaopen_filter_core(lua_State *L) {
	luaL_checkversion(L);

	luaL_newmetatable(L, "meta_filter");
	const luaL_Reg meta[] = {
		{ "add", ladd },
		{ "filter", lfilter },
		{ "dump", ldump },
		{ NULL, NULL },
	};
	luaL_newlib(L,meta);
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L,lrelease);
	lua_setfield(L, -2, "__gc");
	lua_pop(L,1);


	const luaL_Reg l[] = {
		{ "create", lcreate },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
