#include "raylib.h"
#include "raymath.h"
#include "vector.h"
#include "hashmap.h"
#include "sds.h"
#include "sdsalloc.h"
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#define MAX_COMPONENTS 32
#define MAX_ENTITIES 5000

typedef struct ComponentVec {
    void *data;
    size_t *entity_to_ind;    
    size_t *ind_to_entity;
    uint32_t signature;
} ComponentVec;
typedef uint32_t entity_t;

typedef struct C_Transform {
    Vector2 position; 
    Vector2 size; 
    float speed;
    Vector2 velocity;
} C_Transform;

typedef struct C_Collider{
    Rectangle collider;
    long collides_with;
    long collision_category;
    bool is_colliding;
} C_Collider;

typedef struct C_Renderer {
    Color color;
} C_Renderer;

typedef struct ECS {
    struct hashmap *components;
    uint32_t *signatures;
    sds *tags;
    int number_of_components;
    int number_of_entities;
} ECS;

C_Renderer new_renderer(Color color) {
    return (C_Renderer){color};
}

C_Transform new_transform(Vector2 position, Vector2 size, float speed) {
    return (C_Transform){position, size, speed, (Vector2){0,0}};
}

C_Collider new_collider(float x, float y, float width, float height, long collides_with, long collision_category) {
    return (C_Collider){(Rectangle){x,y,width,height},collides_with, collision_category, false};
}

size_t new_entity(ECS *ecs) {
    return ecs->number_of_entities++;
}
size_t new_entity_with_tag(ECS *ecs, char *tag) {
    ecs->tags[ecs->number_of_entities] = sdsnew(tag);
    return ecs->number_of_entities++;
}

struct component_kv{
    char *name;
    ComponentVec component_vec;
};

int component_compare(const void *a, const void *b, void *udata) {
    const struct component_kv *ca = a;
    const struct component_kv *cb = b;
    return strcmp(ca->name, cb->name);
}

uint64_t component_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const struct component_kv *component = item;
    return hashmap_sip(component->name, strlen(component->name), seed0, seed1);
}

ECS *init_ecs() {
    ECS *ecs = malloc(sizeof(ECS));
    ecs->components = hashmap_new(sizeof(struct component_kv), 0, 0, 0,component_hash, component_compare, NULL, NULL);
    ecs->number_of_components = 0;
    ecs->number_of_entities = 0;
    vec_init(ecs->signatures, MAX_ENTITIES);
    vec_init(ecs->tags, MAX_ENTITIES);
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
    free(ecs);
}

#define ecs_register_component(ecs, component)\
    do { \
        ComponentVec cvec = {0};\
        component *vec = NULL;\
        vec_init(vec, 16);\
        cvec.data = vec;\
        vec_init(cvec.entity_to_ind, MAX_ENTITIES);\
        vec_init(cvec.ind_to_entity, MAX_ENTITIES);\
        cvec.signature = 1<<ecs->number_of_components;\
        hashmap_set(ecs->components, &(struct component_kv){ #component, cvec }); \
        ecs->number_of_components++;\
    }while(0)

// TODO: Not the biggest fan of this
#define ecs_add_component(ecs, entity_id, component, component_init) \
    do {\
        ComponentVec *cvec = __ecs_get_component_vec(ecs, component);\
        size_t ind = vec_size(cvec->data);\
        cvec->entity_to_ind[entity_id] = ind;\
        cvec->ind_to_entity[ind]=entity_id;\
        component *vec = cvec->data;\
        bool update = false;\
        if(vec_size(vec) >= vec_capacity(vec)) update=true;\
        component n_component = component_init;\
        vec_push(vec, n_component);\
        if(update) {cvec->data=vec;hashmap_set(ecs->components, &(struct component_kv){ #component, *cvec });}\
        ecs->signatures[entity_id] |= cvec->signature;\
    }while(0)

#define ecs_get_component(ecs, entity_id, component)\
    &(((component*)__ecs_get_component_vec(ecs, component)->data)[__ecs_get_component_vec(ecs, component)->entity_to_ind[entity_id]]);

#define __ecs_get_component_vec(ecs, component) \
    ((ComponentVec*)&(((struct component_kv*)hashmap_get(ecs->components, &(struct component_kv){.name=#component}))->component_vec))

#define ecs_get_component_signature(ecs, component) __ecs_get_component_vec(ecs,component)->signature
#define ecs_get_signature(ecs, entity_id) ecs->signatures[entity_id]
#define ecs_get_tag(ecs, entity_id) ecs->tags[entity_id]

#define ecs_iter_components(ecs, component)\
    (component*)((ComponentVec)((struct component_kv*)hashmap_get(ecs->components, &(struct component_kv){.name=#component}))->component_vec).data

/* TODO LIST */
/*
    - Enemy Kill
    - Id Recycling
    - Collisions
    - Gameplay
*/

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 450;

    srand(time(0));
    InitWindow(screenWidth, screenHeight, "Game");

    ECS *ecs = init_ecs();
    ecs_register_component(ecs, C_Transform);
    ecs_register_component(ecs, C_Renderer);
    ecs_register_component(ecs, C_Collider);

    size_t player_ind = new_entity_with_tag(ecs, "Player");
    ecs_add_component(ecs, player_ind, C_Transform, new_transform((Vector2){20,20}, (Vector2){60,60}, 300.f));
    ecs_add_component(ecs, player_ind, C_Renderer, {WHITE});
    ecs_add_component(ecs, player_ind, C_Collider, new_collider(0, 0, 60, 60, 0, 0));
    Vector2 player_dir = {0};

    /*
    size_t entity_id = vec_size(entities);
    vec_push(entities,new_entity("Enemy"));
    add_component(entity_id, entities[entity_id].c_renderer, c_renderers, new_renderer(WHITE));
    C_Transform entity_transform = new_transform((Vector2){280, 80}, (Vector2){30,50}, 300.f);
    add_component(entity_id, entities[entity_id].c_transform, c_transforms, entity_transform);
    add_component(entity_id, entities[entity_id].c_collider, c_colliders, new_collider(0,0,30,50, 0, 0));
    */

    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        player_dir = (Vector2){0};
        if(IsKeyDown(KEY_W)) {
            player_dir.y = -1;
        }
        if(IsKeyDown(KEY_S)) {
            player_dir.y = 1;
        }
        if(IsKeyDown(KEY_A)) {
            player_dir.x = -1;
        }
        if(IsKeyDown(KEY_D)) {
            player_dir.x = 1;
        }
        C_Transform *transform = ecs_get_component(ecs, player_ind, C_Transform);
        transform->velocity = Vector2Scale(Vector2Normalize(player_dir), transform->speed);

        if(IsKeyPressed(KEY_N)) {
            size_t entity_ind = new_entity_with_tag(ecs, "Enemy");
            C_Renderer e_renderer = new_renderer((Color){rand()%255, rand()%255, rand()%255, 255});
            ecs_add_component(ecs, entity_ind, C_Renderer, e_renderer);
            C_Transform e_transform = new_transform((Vector2){rand()%(screenWidth), rand()%screenHeight}, (Vector2){10,10}, 200);
            ecs_add_component(ecs, entity_ind, C_Transform, e_transform);
        }

        // Enemy "AI"
        for(uint32_t entity_ind=0;entity_ind<ecs->number_of_entities;entity_ind++) {
            if(strcmp(ecs_get_tag(ecs, entity_ind), "Enemy") != 0) continue;
            C_Transform *entity_transform = ecs_get_component(ecs, entity_ind, C_Transform);
            Vector2 dir = Vector2Normalize(Vector2Subtract(transform->position, entity_transform->position));
            entity_transform->velocity = Vector2Scale(dir, entity_transform->speed);
        }
        // Velocity 
        C_Transform *c_transforms = ecs_iter_components(ecs, C_Transform);
        for(C_Transform* transform=vec_begin(c_transforms); transform<vec_end(c_transforms); transform++) {
            transform->position = Vector2Add(transform->position, Vector2Scale(transform->velocity, GetFrameTime()));
        }
        /*

        // Collisions
        C_Collider *c_colliders = __get_components(ecs, C_Collider);
        for(C_Collider* collider=vec_begin(c_colliders); collider<vec_end(c_colliders); collider++) {
            for(C_Collider* collision=collider+1; collision<vec_end(c_colliders); collision++) {
                // Category Check
                // Bounding Box Check
                C_Transform *collider_transform = __get_component(ecs, collider->entity, C_Transform);
                C_Transform *collision_transform = __get_component(ecs, collision->entity, C_Transform);
                Rectangle collider_rect  = {collider_transform->position.x, collider_transform->position.y, collider_transform->size.x, collider_transform->size.y};
                Rectangle collision_rect = {collision_transform->position.x, collision_transform->position.y, collision_transform->size.x, collision_transform->size.y};
                bool is_colliding = CheckCollisionRecs(collider_rect, collision_rect);
                collider->is_colliding = is_colliding;
                collision->is_colliding = is_colliding;
            }
        }
        */

        BeginDrawing();
            ClearBackground(BLACK);
            C_Renderer *renderers = ecs_iter_components(ecs, C_Renderer);
            for(size_t rind=0;rind< vec_size(renderers); rind++) {
                size_t entity_id = __ecs_get_component_vec(ecs, C_Renderer)->ind_to_entity[rind];

                C_Transform *transform = ecs_get_component(ecs, entity_id, C_Transform);
                DrawRectangleV(transform->position, transform->size, renderers[entity_id].color);
            }
            /*
            for(size_t entity_ind=0;entity_ind<vec_size(ecs->entities);entity_ind++) {
                C_Transform* transform = __get_component(ecs, entity_ind, C_Transform);
                C_Renderer* renderer = __get_component(ecs, entity_ind, C_Renderer);
                DrawRectangleV(transform->position, transform->size, renderer->color);
                Color c = WHITE;
                if(c_colliders[entity->c_collider].is_colliding) {
                    c = RED;
                }
                DrawRectangleLines(transform.position.x, transform.position.y, transform.size.x, transform.size.y, c);
            }
            */
        EndDrawing();
    }
    free_ecs(ecs);

    CloseWindow();
    return 0;
}
