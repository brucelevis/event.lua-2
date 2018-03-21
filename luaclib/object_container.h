#ifndef CONTAINER_H
#define CONTAINER_H


struct object_container;

struct object_container* container_create(int max);
void container_release(struct object_container* container);
void* container_get(struct object_container* container,int id);
int container_add(struct object_container* container,void* object);
void container_remove(struct object_container* container,int id);

#endif