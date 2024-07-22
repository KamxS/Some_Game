#include "vector.h"
#include "hashmap.h"
#include "sds.h"
#include "sdsalloc.h"

#define MAX_ENTITIES 5000
#define MAX_COMPONENTS 32

typedef uint32_t entity_t;
struct ECS;

typedef struct ComponentVec {
    void *data;
    size_t size_of_component;
    size_t *entity_to_ind;    
    size_t *ind_to_entity;
    uint32_t signature;
} ComponentVec;

typedef void (*component_func_t)(struct ECS*, entity_t);
typedef struct SystemCallback {
    component_func_t callback;
    uint32_t entity_mask;
    sds *tags;
} SystemCallback;

#define NUM_OF_SYSTEM_TYPES 5
enum system_type {
    ON_START,
    ON_PREUPDATE,
    ON_UPDATE,
    ON_DRAW,
    ON_END
};

typedef struct ECS {
    struct hashmap *components;
    uint32_t *signatures;
    entity_t *entities_to_spawn;
    entity_t *entities_to_kill;
    entity_t *free_ids;
    sds *tags;
    struct SystemCallback *systems[NUM_OF_SYSTEM_TYPES];

    int number_of_components;
    int number_of_entities;
} ECS;

struct component_kv {
    char *name;
    ComponentVec component_vec;
};

// ECS Main
ECS *init_ecs();
void free_ecs(ECS *ecs);
// Components
void __link_entity_with_component(ECS *ecs, ComponentVec *cvec, entity_t entity_id, size_t component);
// Entities
entity_t new_entity(ECS *ecs);
entity_t new_entity_with_tag(ECS *ecs, char *tag);
entity_t __ecs_get_entity_id(ECS *ecs, ComponentVec *cvec, void *component_ptr);
void __ecs_erase_entity(ECS *ecs, entity_t entity_id);
// Systems
void ecs_call_system(ECS *ecs, enum system_type system_type);
// Utility
int component_compare(const void *a, const void *b, void *udata);
uint64_t component_hash(const void *item, uint64_t seed0, uint64_t seed1);
size_t sds_vector_find(sds *vec, sds value, size_t start);

#define ecs_register_component(ecs, component)\
    do { \
        ComponentVec cvec = {0};\
        component *vec = NULL;\
        vec_init(vec, 16);\
        cvec.data = vec;\
        cvec.size_of_component = sizeof(component);\
        vec_init(cvec.entity_to_ind, MAX_ENTITIES);\
        vec_init(cvec.ind_to_entity, MAX_ENTITIES);\
        cvec.signature = 1<<ecs->number_of_components;\
        hashmap_set(ecs->components, &(struct component_kv){ #component, cvec }); \
        ecs->number_of_components++;\
    }while(0)

#define __ecs_get_id(entity_id) (entity_id & 0x0FFF)
//#define __ecs_get_generation(entity_id) ((entity_id & 0xF000)>>24)

// TODO: Not the biggest fan of this
#define ecs_add_component(ecs, entity_id, component, ...) \
    do {\
        ComponentVec *cvec = __ecs_get_component_vec(ecs, component);\
        __link_entity_with_component(ecs, cvec, entity_id, vec_size(cvec->data));\
        component *vec = cvec->data;\
        bool update = false;\
        if(vec_size(vec) >= vec_capacity(vec)) update=true;\
        component n_component = __VA_ARGS__;\
        vec_push(vec, n_component);\
        if(update) {cvec->data=vec;hashmap_set(ecs->components, &(struct component_kv){ #component, *cvec });}\
    }while(0)

#define ecs_get_component(ecs, entity_id, component)\
    &(((component*)__ecs_get_component_vec(ecs, component)->data)[__ecs_get_component_vec(ecs, component)->entity_to_ind[__ecs_get_id(entity_id)]]);

#define __ecs_get_component_vec(ecs, component) \
    ((ComponentVec*)&(((struct component_kv*)hashmap_get(ecs->components, &(struct component_kv){.name=#component}))->component_vec))

#define ecs_get_component_signature(ecs, component) __ecs_get_component_vec(ecs,component)->signature
#define ecs_get_signature(ecs, entity_id) ecs->signatures[__ecs_get_id(entity_id)]
#define ecs_get_tag(ecs, entity_id) ecs->tags[__ecs_get_id(entity_id)]

#define ecs_iter_components(ecs, component)\
    (component*)((ComponentVec)((struct component_kv*)hashmap_get(ecs->components, &(struct component_kv){.name=#component}))->component_vec).data

#define __component_to_signature(ecs, component) __ecs_get_component_vec(ecs, component)->signature
#define __bitor_component_signatures_1(ecs, component) __component_to_signature(ecs, component)
#define __bitor_component_signatures_2(ecs, component, ...) __component_to_signature(ecs, component) | __bitor_component_signatures_1(ecs, __VA_ARGS__)
#define __bitor_component_signatures_3(ecs, component, ...) __component_to_signature(ecs, component) | __bitor_component_signatures_2(ecs, __VA_ARGS__)
#define __bitor_component_signatures_4(ecs, component, ...) __component_to_signature(ecs, component) | __bitor_component_signatures_3(ecs, __VA_ARGS__)
#define __bitor_component_signatures_5(ecs, component, ...) __component_to_signature(ecs, component) | __bitor_component_signatures_4(ecs, __VA_ARGS__)
#define __choose_correct_bitor(_1,_2,_3,_4,_5,name,...) name

#define ecs_foreach_entity(ecs, function, ...)\
    do {\
        uint32_t mask = __choose_correct_bitor(__VA_ARGS__, __bitor_component_signatures_5, __bitor_component_signatures_4, __bitor_component_signatures_3, __bitor_component_signatures_2, __bitor_component_signatures_1)(ecs, __VA_ARGS__);\
        for(size_t n=0;n<MAX_ENTITIES; n++) {\
            if((ecs->signatures[n] & mask) == mask) {\
                function(ecs, n);\
            }\
        }\
    }while(0)

#define ecs_register_system(ecs, type, function) \
    do {\
        SystemCallback callback = {function, 0, NULL};\
        vec_push(ecs->systems[type],callback);\
    }while(0)

#define ecs_register_component_system(ecs, type, function, ...)\
    do {\
        uint32_t mask = __choose_correct_bitor(__VA_ARGS__, __bitor_component_signatures_5, __bitor_component_signatures_4, __bitor_component_signatures_3, __bitor_component_signatures_2, __bitor_component_signatures_1)(ecs, __VA_ARGS__);\
        SystemCallback callback = {function, mask, NULL};\
        vec_push(ecs->systems[type],callback);\
    }while(0)

#define ecs_register_tag_system(ecs, type, function, ...)\
    do {\
        char* tags[] = {__VA_ARGS__};\
        sds *stags = NULL;\
        vec_init(stags, 8);\
        SystemCallback callback = {function, 0};\
        for(size_t ind=0;ind<sizeof(tags)/sizeof(tags[0]);ind++) {\
            sds tag=sdsnew(tags[ind]);\
            vec_push(stags, tag);\
        }\
        callback.tags = stags;\
        vec_push(ecs->systems[type],callback);\
    }while(0)

#define ecs_get_entity_id(ecs, component_type, component_ptr) __ecs_get_entity_id(ecs, __ecs_get_component_vec(ecs,component_type), component_ptr)
#define ecs_find_entity_with_tag(ecs, tag) sds_vector_find(ecs->tags, tag, 0)
#define kill_entity(ecs, entity_id) vec_push(ecs->entities_to_kill, entity_id)
