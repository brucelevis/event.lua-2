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

#define PHASE_SEARCH 0
#define PHASE_MATCH 1

struct length_list;

KHASH_MAP_INIT_STR(word,struct length_list*);
KHASH_SET_INIT_STR(word_set);

struct string {
	char* data;
	size_t size;
	size_t offset;
};

struct length_info {
	uint8_t length;
	uint32_t utf8;
};

struct length_list {
	struct length_info* slots;
	size_t offset;
	size_t size;
};

struct word_map {
	khash_t(word)* hash;
	khash_t(word_set)* set;
};

static inline void
string_init(struct string* str,size_t size) {
	if (size < 64)
		size = 64;

	str->size = size;
	str->offset = 0;
	str->data = malloc(size);
	memset(str->data,0,str->size);
}

static inline void
string_release(struct string* str) {
	free(str->data);
}

static inline size_t 
string_length(struct string* str) {
	return str->offset;
}

static inline void
string_push(struct string* str,char* data,size_t size) {
	if (str->offset + size + 1 >= str->size) {
		str->size = str->size * 2;
		if (str->size <= str->offset + size + 1)
			str->size = str->offset + size + 1;
		char* odata = str->data;
		str->data = malloc(str->size);
		memset(str->data,0,str->size);
		memcpy(str->data,odata,str->offset);
		free(odata);
	}
	memcpy(str->data + str->offset, data, size);
	str->offset += size;
}

static inline void
string_append(struct string* str,uint32_t code) {
	char* data = (char*)&code;

	char ch = data[0];
	if((ch & 0x80) == 0) {  
		string_push(str,&data[0],1);
		return;
    } else if((ch & 0xF0) == 0xF0) { 
    	string_push(str,&data[0],4);
    	return;
    } else if((ch & 0xE0) == 0xE0) {  
    	string_push(str,&data[0],3);
    	return;
    } else if((ch & 0xC0) == 0xC0) {  
    	string_push(str,&data[0],2);
    	return;
    } else {
    	assert(0);
    }
}

static inline void
string_append_ch(struct string* str,const char ch) {
	string_push(str,(char*)&ch,1);
}

static inline int
length_sort(const void * left, const void * right) {
	const struct length_info* l = left;
	const struct length_info* r = right;
	return l->length < r->length;
}

static inline size_t
split_utf8(const char* word,size_t size,uint32_t* utf8) {
	size_t count = 0;
	int i;
	for(i = 0;i < size;) {
		char ch = word[i];
		if((ch & 0x80) == 0) {  
			if (utf8)
				utf8[count] = ch;
			count++;
			i += 1;
        } else if((ch & 0xF0) == 0xF0) { 
        	if (utf8) {
        		uint32_t val = 0;
	        	memcpy(&val,&word[i],4);
	        	utf8[count] = val; 
        	}
        	count++;
        	i += 4;
        } else if((ch & 0xE0) == 0xE0) {  
        	if (utf8) {
        		uint32_t val = 0;
	        	memcpy(&val,&word[i],3);
	        	utf8[count] = val; 
        	}
            count++;
        	i += 3;
        } else if((ch & 0xC0) == 0xC0) {  
        	if (utf8) {
        		uint32_t val = 0;
	        	memcpy(&val,&word[i],2);
	        	utf8[count] = val; 
        	}
           	count++;
        	i += 2; 
        } else {
        	assert(0);
        }
	}
	return count;
}

void
word_add(struct word_map* map, const char* word,size_t size) {
	size_t count = split_utf8(word,size,NULL);
	uint32_t* utf8 = malloc(sizeof(uint32_t) * count);
	split_utf8(word,size,utf8);

	char last[5] = {0};
	memcpy(last,&utf8[count-1],4);

	khiter_t k = kh_get(word, map->hash, last);
	struct length_list* list = NULL;
	if (k == kh_end(map->hash)) {
		list = malloc(sizeof(*list));
		list->size = 2;
		list->offset = 0;
		list->slots = malloc(sizeof(struct length_info) * list->size);
		memset(list->slots,0,sizeof(struct length_info) * list->size);
		int result;
		k = kh_put(word, map->hash, strdup(last), &result);
		if (result == 1 || result == 2) {
			kh_value(map->hash, k) = list;
		}else {
			assert(0);
		}

	} else {
		list = kh_value(map->hash, k);
	}

	if (list->offset >= list->size) {
		list->size = list->size * 2;
		list->slots = realloc(list->slots,sizeof(struct length_info) * list->size);
	}
	struct length_info* slot = &list->slots[list->offset++];

	slot->length = count;
	slot->utf8 = utf8[0];

	free(utf8);

	qsort(list->slots,list->offset,sizeof(struct length_info),length_sort);
}

char*
word_filter(struct word_map* map, const char* word,size_t size,int replace) {
	size_t count = split_utf8(word,size,NULL);
	uint32_t* utf8 = malloc(sizeof(uint32_t) * count);
	split_utf8(word,size,utf8);

	struct string result;
	string_init(&result,64);

	int tIndex = 0;

	int index;
	for (index = 0; index < count; index++) {

		khiter_t k = kh_get(word, map->hash, (char*)&utf8[index]);
		if (k != kh_end(map->hash)) {

			struct length_list* list = kh_value(map->hash, k);
			int i;
			for (i = 0; i < list->offset; ++i) {
				struct length_info* info = &list->slots[i];
				int start = index - (info->length - 1);
				if (info->length <= (index + 1) && utf8[start] == info->utf8)
				{
					struct string keyword;
					string_init(&keyword,64);
					int p;
					for (p = 0; p < info->length; p++)
						string_append(&keyword,utf8[start+p]);

					if (string_length(&keyword) != 0) {
						k = kh_get(word_set, map->set, keyword.data);
						if (k != kh_end(map->set)) {
							int k;
							for (k = tIndex; k <= index; k++) {
								if (k >= start) {
									string_append_ch(&result,'*');
								} else {
									string_append(&result,utf8[k]);
								}
							}

							tIndex = index + 1;
							break;
						}
					}

					string_release(&keyword);
				}
			}
		}
	}

	if (tIndex == 0) {
		free(utf8);
		return NULL;
	}

	if (tIndex < count) {
		int p;
		for (p = tIndex; p < count; p++) {
			string_append(&result,utf8[p]);
		}
	}
	free(utf8);
	return result.data;
}
static int
lcreate(lua_State* L) {
	struct word_map* map = lua_newuserdata(L,sizeof(*map));
	map->hash = kh_init(word);
	map->set = kh_init(word_set);
	luaL_newmetatable(L,"meta_filterex");
 	lua_setmetatable(L, -2);
	return 1;
}

void
release_map(char* word,struct length_list* list) {
	free(list->slots);
	free(list);
	free(word);
}

void
release_set(char* word) {
	printf("%s\n",word);
	free(word);

}

static int
lrelease(lua_State* L) {
	struct word_map* map = lua_touserdata(L,1);

	const char* word = NULL;
	struct length_list* list = NULL;

	kh_foreach(map->hash, word, list, release_map((char*)word, list));
	kh_destroy(word,map->hash);

	kh_destroy(word_set,map->set);
	return 0;
}

static int
ladd(lua_State* L) {
	struct word_map* map = lua_touserdata(L,1);
	size_t size;
	const char* word = lua_tolstring(L,2,&size);

	int result;
	kh_put(word_set, map->set, strdup(word), &result);
	
	word_add(map,word,size);
	return 0;
}

static int
lfilter(lua_State* L) {
	struct word_map* map = lua_touserdata(L,1);
	size_t size;
	const char* word = lua_tolstring(L,2,&size);
	int replace = luaL_optinteger(L,3,1);

	char* new_word = word_filter(map, word,size,replace);
	if (new_word) {
		lua_pushstring(L,new_word);
		free(new_word);
		return 1;
	}
	
	return 0;
}

static int
ldump(lua_State* L) {
	struct word_tree* tree = lua_touserdata(L,1);
	
	return 0;
}

int 
luaopen_filterex_core(lua_State *L) {
	luaL_checkversion(L);

	luaL_newmetatable(L, "meta_filterex");
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
