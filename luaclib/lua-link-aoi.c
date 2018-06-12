#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include "aoi.h"

#define AOI_ENTITY 		1
#define AOI_LOW_BOUND 	2
#define AOI_HIGH_BOUND 	4

#define UNLIMITED -1000

#ifdef _WIN32
#define inline __inline
#endif

struct aoi_entity;
struct aoi_trigger;

typedef struct position {
	int x;
	int z;
} position_t;

typedef struct aoi_object {
	int uid;
	struct aoi_entity* entity;
	struct aoi_trigger* trigger;

	struct aoi_object* next_x;
	struct aoi_object* prev_x;

	struct aoi_object* next_z;
	struct aoi_object* prev_z;
} aoi_object_t;

typedef struct linknode {
	aoi_object_t* owner;
	struct linknode* prev;
	struct linknode* next;
	position_t pos;
	uint8_t flag;
} linknode_t;

typedef struct linklist {
	linknode_t* head;
	linknode_t* tail;
} linklist_t;

typedef struct aoi_entity {
	position_t center;
	linknode_t node[2];
	entity_func func;
	void* ud;
} aoi_entity_t;

typedef struct aoi_trigger {
	position_t center;
	linknode_t node[4];
	int range;
	trigger_func func;
	void* ud;
} aoi_trigger_t;

typedef struct aoi_context {
	linklist_t linklist[2];

	struct aoi_object* enter_x;
	struct aoi_object* leave_x;

	struct aoi_object* enter_z;
	struct aoi_object* leave_z;

} aoi_context_t;

void
insert_node(aoi_context_t* aoi_ctx, int xorz, linknode_t* linknode) {
	linklist_t* first;
	if ( xorz == 0 ) {
		first = &aoi_ctx->linklist[0];
	}
	else {
		first = &aoi_ctx->linklist[1];
	}
	if ( first->head == NULL ) {
		assert(first->head == first->tail);
		first->head = first->tail = linknode;
	}
	else {
		if (first->head == first->tail) {
			first->head = linknode;
			linknode->next = first->tail;
			first->tail->prev = linknode;
		}
		else {
			linknode->next = first->head;
			first->head->prev = linknode;
			first->head = linknode;
		}
	}
}

static inline void
link_enter_result(aoi_context_t* aoi_ctx, aoi_object_t* self, aoi_object_t* other, int flag) {
	if ( flag == 0 ) {
		//printf("x:enter:%d %d\n", self->uid, other->uid);
		if (!aoi_ctx->enter_x) {
			aoi_ctx->enter_x = other;
		}
		else {
			aoi_ctx->enter_x->prev_x = other;
			other->next_x = aoi_ctx->enter_x;
			aoi_ctx->enter_x = other;
		}
	}
	else {
		//printf("z:enter:%d %d\n", self->uid, other->uid);
		if ( !aoi_ctx->enter_z ) {
			aoi_ctx->enter_z = other;
		}
		else {
			aoi_ctx->enter_z->prev_z = other;
			other->next_z = aoi_ctx->enter_z;
			aoi_ctx->enter_z = other;
		}
	}
}

static inline void
link_leave_result(aoi_context_t* aoi_ctx, aoi_object_t* self, aoi_object_t* other, int flag) {
	if ( flag == 0 ) {
		//printf("x:leave:%d %d\n", self->uid, other->uid);
		if ( other->next_x || other->prev_x || aoi_ctx->enter_x == other) {
			if ( aoi_ctx->enter_x == other ) {
				aoi_ctx->enter_x = other->next_x;
			}

			if ( other->next_x ) {
				other->next_x->prev_x = other->prev_x;
			}

			if ( other->prev_x ) {
				other->prev_x->next_x = other->next_x;
			}

			other->next_x = other->prev_x = NULL;
			return;
		}

		if ( !aoi_ctx->leave_x) {
			aoi_ctx->leave_x = other;
		}
		else {
			aoi_ctx->leave_x->prev_x = other;
			other->next_x = aoi_ctx->leave_x;
			aoi_ctx->leave_x = other;
		}
	}
	else {
		//printf("z:leave:%d %d\n", self->uid, other->uid);
		if ( other->next_z || other->prev_z || aoi_ctx->enter_z == other ) {
			if ( aoi_ctx->enter_z == other ) {
				aoi_ctx->enter_z = other->next_z;
			}

			if ( other->next_z ) {
				other->next_z->prev_z = other->prev_z;
			}

			if ( other->prev_z ) {
				other->prev_z->next_z = other->next_z;
			}

			other->next_z = other->prev_z = NULL;
			return;
		}

		if ( !aoi_ctx->leave_z ) {
			aoi_ctx->leave_z = other;
		}
		else {
			aoi_ctx->leave_z->prev_x = other;
			other->next_z = aoi_ctx->leave_z;
			aoi_ctx->leave_z = other;
		}
	}
}

static inline void
exchange_x(aoi_context_t* aoi_ctx, linklist_t* first, linknode_t* A, linknode_t* B, int dir) {
	A->next = B->next;
	if ( B->next ) {
		B->next->prev = A;
	}
		
	linknode_t* tmp_prev = A->prev;
	B->prev = tmp_prev;
	if ( tmp_prev ) {
		tmp_prev->next = B;
	}
		
	B->next = A;
	A->prev = B;

	if ( first->head == A ) {
		first->head = B;
	}
	else if ( first->head == B ) {
		first->head = A;
	}

	if ( first->tail == A ) {
		first->tail = B;
	} else if ( first->tail == B ) {
		first->tail = A;
	}
		
	linknode_t* self, *other;

	if ( dir < 0 ) {
		self = A;
		other = B;
	}
	else {
		self = B;
		other = A;
	}

	if ( self->flag & AOI_ENTITY )
	{
		if ( other->flag & AOI_LOW_BOUND )
		{
			if ( dir < 0 ) {
				link_enter_result(aoi_ctx, self->owner, other->owner, 0);
			}
			else {
				link_leave_result(aoi_ctx, self->owner, other->owner, 0);
			}
		}
		else if ( other->flag & AOI_HIGH_BOUND )
		{
			if ( dir < 0 ) {
				link_leave_result(aoi_ctx, self->owner, other->owner, 0);
			}
			else {
				link_enter_result(aoi_ctx, self->owner, other->owner, 0);
			}
		}
	}
	else {
		if ( other->flag & AOI_ENTITY )
		{
			if ( self->flag & AOI_LOW_BOUND)
			{
				if ( dir < 0 ) {
					link_leave_result(aoi_ctx, self->owner, other->owner, 0);
				}
				else {
					link_enter_result(aoi_ctx, self->owner, other->owner, 0);
				}
			} 
			else if ( self->flag & AOI_HIGH_BOUND )
			{
				if ( dir < 0 ) {
					link_enter_result(aoi_ctx, self->owner, other->owner, 0);
				}
				else {
					link_leave_result(aoi_ctx, self->owner, other->owner, 0);
				}
			}
		}
	}
}

static inline void
exchange_z(aoi_context_t* aoi_ctx, linklist_t* first, linknode_t* A, linknode_t* B, int dir) {
	A->next = B->next;
	if ( B->next ) {
		B->next->prev = A;
	}

	linknode_t* tmp_prev = A->prev;
	B->prev = tmp_prev;
	if ( tmp_prev ) {
		tmp_prev->next = B;
	}

	B->next = A;
	A->prev = B;

	if ( first->head == A ) {
		first->head = B;
	}
	else if ( first->head == B ) {
		first->head = A;
	}

	if ( first->tail == A ) {
		first->tail = B;
	}
	else if ( first->tail == B ) {
		first->tail = A;
	}

	linknode_t* self, *other;

	if ( dir < 0 ) {
		self = A;
		other = B;
	}
	else {
		self = B;
		other = A;
	}

	if ( self->flag & AOI_ENTITY )
	{
		if ( other->flag & AOI_LOW_BOUND )
		{
			if ( dir < 0 ) {
				link_enter_result(aoi_ctx, self->owner, other->owner, 1);
			}
			else {
				link_leave_result(aoi_ctx, self->owner, other->owner, 1);
			}
		}
		else if ( other->flag & AOI_HIGH_BOUND )
		{
			if ( dir < 0 ) {
				link_leave_result(aoi_ctx, self->owner, other->owner, 1);
			}
			else {
				link_enter_result(aoi_ctx, self->owner, other->owner, 1);
			}
		}
	}
	else {
		if ( other->flag & AOI_ENTITY )
		{
			if ( self->flag & AOI_LOW_BOUND )
			{
				if ( dir < 0 ) {
					link_leave_result(aoi_ctx, self->owner, other->owner, 1);
				}
				else {
					link_enter_result(aoi_ctx, self->owner, other->owner, 1);
				}
			}
			else if ( self->flag & AOI_HIGH_BOUND )
			{
				if ( dir < 0 ) {
					link_enter_result(aoi_ctx, self->owner, other->owner, 1);
				}
				else {
					link_leave_result(aoi_ctx, self->owner, other->owner, 1);
				}
			}
		}
	}
}

void
shuffle_x(aoi_context_t* aoi_ctx, linknode_t* node, int x) {
	linklist_t* first = &aoi_ctx->linklist[0];
	node->pos.x = x;
	if ( first->head == first->tail )
		return;
	
	while ( node->next != NULL && node->pos.x > node->next->pos.x ) {
		exchange_x(aoi_ctx, first, node, node->next, -1);
	}

	while ( node->prev != NULL && node->pos.x < node->prev->pos.x ) {
		exchange_x(aoi_ctx, first, node->prev, node, 1);
	}
}

void
shuffle_z(aoi_context_t* aoi_ctx, linknode_t* node, int z) {
	linklist_t* first = &aoi_ctx->linklist[1];
	node->pos.z = z;
	if ( first->head == first->tail )
		return;
	while ( node->next != NULL && node->pos.z > node->next->pos.z ) {
		exchange_z(aoi_ctx, first, node, node->next, -1);
	}

	while ( node->prev != NULL && node->pos.z < node->prev->pos.z ) {
		exchange_z(aoi_ctx, first, node->prev, node, 1);
	}
}

void
shuffle_entity(aoi_context_t* aoi_ctx, aoi_entity_t* entity, int x, int z) {
	shuffle_x(aoi_ctx, &entity->node[0], x);
	
	entity->center.x = x;

	shuffle_z(aoi_ctx, &entity->node[1], z);
	
	entity->center.z = z;

	aoi_object_t* owner = entity->node[0].owner;
	aoi_object_t* cursor = aoi_ctx->enter_x;
	while (cursor) {
		owner->entity->func(owner->uid, cursor->uid, 1, 'x', owner->entity->ud);
		aoi_object_t* tmp = cursor;
		cursor = cursor->next_x;
		tmp->next_x = tmp->prev_x = NULL;
	}

	cursor = aoi_ctx->enter_z;
	while ( cursor ) {
		owner->entity->func(owner->uid, cursor->uid, 1, 'z', owner->entity->ud);
		aoi_object_t* tmp = cursor;
		cursor = cursor->next_z;
		tmp->next_z = tmp->prev_z = NULL;
	}

	aoi_ctx->enter_x = aoi_ctx->enter_z = NULL;

	cursor = aoi_ctx->leave_x;
	while ( cursor ) {
		owner->entity->func(owner->uid, cursor->uid, 0, 'x', owner->entity->ud);
		aoi_object_t* tmp = cursor;
		cursor = cursor->next_x;
		tmp->next_x = tmp->prev_x = NULL;
	}

	cursor = aoi_ctx->leave_z;
	while ( cursor ) {
		owner->entity->func(owner->uid, cursor->uid, 0, 'z', owner->entity->ud);
		aoi_object_t* tmp = cursor;
		cursor = cursor->next_z;
		tmp->next_z = tmp->prev_z = NULL;
	}
	aoi_ctx->leave_x = aoi_ctx->leave_z = NULL;
}

void
shuffle_trigger(aoi_context_t* aoi_ctx, aoi_trigger_t* trigger, int x, int z) {
	if (trigger->center.x < x) {
		shuffle_x(aoi_ctx, &trigger->node[2], x + trigger->range);
		shuffle_x(aoi_ctx, &trigger->node[0], x - trigger->range);
	}
	else {
		shuffle_x(aoi_ctx, &trigger->node[0], x - trigger->range);
		shuffle_x(aoi_ctx, &trigger->node[2], x + trigger->range);
	}
	
	trigger->center.x = x;

	if ( trigger->center.z < z ) {
		shuffle_z(aoi_ctx, &trigger->node[3], z + trigger->range);
		shuffle_z(aoi_ctx, &trigger->node[1], z - trigger->range);
	}
	else {
		shuffle_z(aoi_ctx, &trigger->node[1], z - trigger->range);
		shuffle_z(aoi_ctx, &trigger->node[3], z + trigger->range);
	}

	trigger->center.z = z;

	aoi_object_t* owner = trigger->node[0].owner;
	aoi_object_t* cursor = aoi_ctx->enter_x;
	while ( cursor ) {
		owner->trigger->func(owner->uid, cursor->uid, 1, 'x', owner->trigger->ud);
		aoi_object_t* tmp = cursor;
		cursor = cursor->next_x;
		tmp->next_x = tmp->prev_x = NULL;
	}

	cursor = aoi_ctx->enter_z;
	while ( cursor ) {
		owner->trigger->func(owner->uid, cursor->uid, 1, 'z', owner->trigger->ud);
		aoi_object_t* tmp = cursor;
		cursor = cursor->next_z;
		tmp->next_z = tmp->prev_z = NULL;
	}

	aoi_ctx->enter_x = aoi_ctx->enter_z = NULL;

	cursor = aoi_ctx->leave_x;
	while ( cursor ) {
		owner->trigger->func(owner->uid, cursor->uid, 0, 'x', owner->trigger->ud);
		aoi_object_t* tmp = cursor;
		cursor = cursor->next_x;
		tmp->next_x = tmp->prev_x = NULL;
	}

	cursor = aoi_ctx->leave_z;
	while ( cursor ) {
		owner->trigger->func(owner->uid, cursor->uid, 0, 'z', owner->trigger->ud);
		aoi_object_t* tmp = cursor;
		cursor = cursor->next_z;
		tmp->next_z = tmp->prev_z = NULL;
	}
	aoi_ctx->leave_x = aoi_ctx->leave_z = NULL;
}

void
create_entity(aoi_context_t* aoi_ctx, aoi_object_t* aoi_object, int x, int z, entity_func func,void* ud) {
	aoi_object->entity = malloc(sizeof( aoi_entity_t ));
	memset(aoi_object->entity, 0, sizeof( aoi_entity_t ));

	aoi_object->entity->func = func;
	aoi_object->entity->ud = ud;

	aoi_object->entity->center.x = UNLIMITED;
	aoi_object->entity->center.z = UNLIMITED;

	aoi_object->entity->node[0].owner = aoi_object;
	aoi_object->entity->node[1].owner = aoi_object;

	aoi_object->entity->node[0].flag |= AOI_ENTITY;
	aoi_object->entity->node[1].flag |= AOI_ENTITY;

	aoi_object->entity->node[0].pos.x = UNLIMITED;
	aoi_object->entity->node[0].pos.z = UNLIMITED;

	aoi_object->entity->node[1].pos.x = UNLIMITED;
	aoi_object->entity->node[1].pos.z = UNLIMITED;

	insert_node(aoi_ctx, 0, &aoi_object->entity->node[0]);
	insert_node(aoi_ctx, 1, &aoi_object->entity->node[1]);

	shuffle_entity(aoi_ctx, aoi_object->entity, x, z);
}

void
create_trigger(aoi_context_t* aoi_ctx, aoi_object_t* aoi_object, int x, int z, int range, trigger_func func,void* ud) {
	aoi_object->trigger = malloc(sizeof( aoi_trigger_t ));
	memset(aoi_object->trigger, 0, sizeof( aoi_trigger_t ));

	aoi_object->trigger->func = func;
	aoi_object->trigger->ud = ud;

	aoi_object->trigger->range = range;

	aoi_object->trigger->center.x = UNLIMITED;
	aoi_object->trigger->center.z = UNLIMITED;

	aoi_object->trigger->node[0].owner = aoi_object;
	aoi_object->trigger->node[1].owner = aoi_object;
	aoi_object->trigger->node[2].owner = aoi_object;
	aoi_object->trigger->node[3].owner = aoi_object;

	aoi_object->trigger->node[0].flag |= AOI_LOW_BOUND;
	aoi_object->trigger->node[1].flag |= AOI_LOW_BOUND;

	aoi_object->trigger->node[0].pos.x = UNLIMITED;
	aoi_object->trigger->node[0].pos.z = UNLIMITED;

	aoi_object->trigger->node[1].pos.x = UNLIMITED;
	aoi_object->trigger->node[1].pos.z = UNLIMITED;

	aoi_object->trigger->node[2].flag |= AOI_HIGH_BOUND;
	aoi_object->trigger->node[3].flag |= AOI_HIGH_BOUND;

	aoi_object->trigger->node[2].pos.x = UNLIMITED;
	aoi_object->trigger->node[2].pos.z = UNLIMITED;

	aoi_object->trigger->node[3].pos.x = UNLIMITED;
	aoi_object->trigger->node[3].pos.z = UNLIMITED;

	insert_node(aoi_ctx, 0, &aoi_object->trigger->node[0]);
	insert_node(aoi_ctx, 0, &aoi_object->trigger->node[2]);

	insert_node(aoi_ctx, 1, &aoi_object->trigger->node[1]);
	insert_node(aoi_ctx, 1, &aoi_object->trigger->node[3]);

	shuffle_trigger(aoi_ctx, aoi_object->trigger, x, z);

}

void
delete_entity(aoi_context_t* aoi_ctx, aoi_object_t* aoi_object) {
	shuffle_entity(aoi_ctx, aoi_object->entity, UNLIMITED, UNLIMITED);
}

void
delete_trigger(aoi_context_t* aoi_ctx, aoi_object_t* aoi_object) {
	shuffle_trigger(aoi_ctx, aoi_object->trigger, UNLIMITED, UNLIMITED);
}

void
move_entity(aoi_context_t* aoi_ctx, aoi_object_t* aoi_object, int x, int z) {
	shuffle_entity(aoi_ctx, aoi_object->entity, x, z);
}

void
move_trigger(aoi_context_t* aoi_ctx, aoi_object_t* aoi_object, int x, int z) {
	shuffle_trigger(aoi_ctx, aoi_object->trigger, x, z);
}

struct aoi_context*
create_aoi_ctx() {
	struct aoi_context* aoi_ctx = malloc(sizeof( *aoi_ctx ));
	memset(aoi_ctx, 0, sizeof( *aoi_ctx ));
	return aoi_ctx;
}

struct aoi_object*
create_aoi_object(struct aoi_context* aoi_ctx, int uid) {
	struct aoi_object* object = malloc(sizeof( *object ));
	memset(object, 0, sizeof( *object ));
	object->uid = uid;
	return object;
}

void
foreach_aoi_entity(struct aoi_context* aoi_ctx, foreach_entity_func func, void* ud) {
	linklist_t* list = &aoi_ctx->linklist[0];
	linknode_t* cursor = list->head;
	while (cursor) {
		if (cursor->flag & AOI_ENTITY) {
			aoi_object_t* object = cursor->owner;
			func(object->uid, object->entity->center.x, object->entity->center.z, ud);
		}
		cursor = cursor->next;
	}
}

void
foreach_aoi_trigger(struct aoi_context* aoi_ctx, foreach_trigger_func func, void* ud) {
	linklist_t* list = &aoi_ctx->linklist[0];
	linknode_t* cursor = list->head;
	while ( cursor ) {
		if ( cursor->flag & AOI_LOW_BOUND ) {
			aoi_object_t* object = cursor->owner;
			func(object->uid, object->trigger->center.x, object->trigger->center.z, object->trigger->range, ud);
		}
		cursor = cursor->next;
	}
}