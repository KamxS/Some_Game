#include "../include/kxecs.h"

// ECS Main
ECS *init_ecs() { ECS *ecs = malloc(sizeof(ECS)); ecs->components = hashmap_new(sizeof(struct component_kv), 0, 0, 0,component_hash, component_compare, NULL, NULL); ecs->number_of_components = 0; ecs->number_of_entities = 0; uint32_t *signatures = NULL; vec_init(signatures, MAX_ENTITIES); ecs->signatures = signatures;
    sds *tags = NULL;
    vec_init(tags, MAX_ENTITIES);
    vec_get_base(tags)->size = MAX_ENTITIES;
    ecs->tags = tags;

    for(size_t ind=0;ind<NUM_OF_SYSTEM_TYPES;ind++) {
        SystemCallback *s_call = NULL;
        vec_init(s_call, 16);
        ecs->systems[ind]=s_call;
    }

    entity_t *entities_to_spawn = NULL;
    vec_init(entities_to_spawn, 64);
    ecs->entities_to_spawn = entities_to_spawn;

    entity_t *entities_to_kill = NULL;
    vec_init(entities_to_kill, 64);
    ecs->entities_to_kill = entities_to_kill;

    entity_t *free_ids = NULL;
    vec_init(free_ids, 64);
    ecs->free_ids = free_ids;
    return ecs;
}

void free_ecs(ECS *ecs) {
    size_t iter = 0;
    void *item;
    while (hashmap_iter(ecs->components, &iter, &item)) {
        ComponentVec cvec = ((struct component_kv*)item)->component_vec;
        vec_free(cvec.data);
        vec_free(cvec.ind_to_entity);
        vec_free(cvec.entity_to_ind);
    }
    hashmap_free(ecs->components);
    vec_free(ecs->signatures);
    for(size_t tag_ind=0;tag_ind<ecs->number_of_entities; tag_ind++) {
        sdsfree(ecs->tags[tag_ind]);
    }
    vec_free(ecs->tags);
    vec_free(ecs->entities_to_spawn);
    vec_free(ecs->entities_to_kill);
    vec_free(ecs->free_ids);

    for(size_t ind=0;ind<NUM_OF_SYSTEM_TYPES;ind++) {
        if(ecs->systems[ind]->tags != NULL) {
            for(sds *tag=vec_begin(ecs->systems[ind]->tags); tag<vec_end(ecs->systems[ind]->tags);tag++) {
                sdsfree(*tag);
            }
            vec_free(ecs->systems[ind]->tags);
        }
        vec_free(ecs->systems[ind]);
    }
    free(ecs);
}

// Components
void __link_entity_with_component(ECS *ecs, ComponentVec *cvec, entity_t entity_id, size_t component) {
    cvec->entity_to_ind[__ecs_get_id(entity_id)] = component;
    cvec->ind_to_entity[component]=__ecs_get_id(entity_id);
    ecs->signatures[__ecs_get_id(entity_id)] |= cvec->signature;
}

// Entities
entity_t new_entity(ECS *ecs) {
    entity_t new_id = ecs->number_of_entities++;
    if(vec_size(ecs->free_ids) > 0) {
        //new_id = (1<<24) | ecs->free_ids[vec_size(ecs->free_ids)-1];
        new_id =ecs->free_ids[vec_size(ecs->free_ids)-1];
        vec_pop(ecs->free_ids);
    }
    return new_id;
}

entity_t new_entity_with_tag(ECS *ecs, char *tag) {
    entity_t id = new_entity(ecs);
    ecs->tags[id] = sdsnew(tag);
    return id;
}

entity_t __ecs_get_entity_id(ECS *ecs, ComponentVec *cvec, void *component_ptr) {
    return cvec->ind_to_entity[(component_ptr-cvec->data)/cvec->size_of_component];
}

void __ecs_erase_entity(ECS *ecs, entity_t entity_id) {
    size_t iter = 0;
    void *item;
    while (hashmap_iter(ecs->components, &iter, &item)) {
        const struct component_kv *component_kv= item;
        if(ecs_get_signature(ecs, entity_id) & component_kv->component_vec.signature) {
            ComponentVec cvec = component_kv->component_vec;
            size_t component_to_replace = cvec.entity_to_ind[__ecs_get_id(entity_id)];
            memcpy(cvec.data+cvec.size_of_component*component_to_replace,
                    cvec.data+cvec.size_of_component*(vec_size(cvec.data)-1),
                    cvec.size_of_component
                  );
            entity_t last_entity = cvec.ind_to_entity[vec_size(cvec.data)-1];
            cvec.entity_to_ind[last_entity] = component_to_replace;
            cvec.ind_to_entity[component_to_replace] = last_entity;
            vec_pop(cvec.data);
        }
    }
    ecs->signatures[__ecs_get_id(entity_id)] = 0;
    if(ecs->tags[__ecs_get_id(entity_id)]) sdsclear(ecs->tags[__ecs_get_id(entity_id)]);
    ecs->number_of_entities--;
    vec_push(ecs->free_ids, entity_id);
}
// Systems
void ecs_call_system(ECS *ecs, enum system_type system_type) {
    for(SystemCallback *func=vec_begin(ecs->systems[system_type]);func<vec_end(ecs->systems[system_type]);func++) {
        if(func->tags!=NULL) {
            for(size_t n=0;n<MAX_ENTITIES; n++) {
                if(ecs->tags[n]==0) continue;
                for(sds *tag=vec_begin(func->tags);tag<vec_end(func->tags);tag++) {
                    if(strcmp(ecs->tags[n], *tag)==0) {
                        func->callback(ecs, n);
                        break;
                    }
                }
            }
            continue;
        }else if(func->entity_mask!=0) {
            for(size_t n=0;n<MAX_ENTITIES; n++) {
                if((ecs->signatures[n] & func->entity_mask) == func->entity_mask) {
                    func->callback(ecs, n);
                }
            }
        }else {
            func->callback(ecs, -1);
        }
    }
}

// Utility
int component_compare(const void *a, const void *b, void *udata) {
    const struct component_kv *ca = a;
    const struct component_kv *cb = b;
    return strcmp(ca->name, cb->name);
}

uint64_t component_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const struct component_kv *component = item;
    return hashmap_sip(component->name, strlen(component->name), seed0, seed1);
}

size_t sds_vector_find(sds *vec, sds value, size_t start) {
    for(size_t ind=start;ind<vec_size(vec);ind++) {
        if(vec[ind]==0) continue;
        if(strcmp(vec[ind],value)==0) {
            return ind;
        }
    }
    return -1;
}
