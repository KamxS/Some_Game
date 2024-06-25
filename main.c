#include "raylib.h"
#include "raymath.h"
#include "vector.h"
#include "hashmap.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct name_component_pair{
    char *name;
    void *component_vec;
};

struct name_component_id_pair{
    char *name;
    size_t component_id;
};

int component_name_cmp(const void *a, const void *b, void *udata) {
    const struct name_component_pair *ca = a;
    const struct name_component_pair *cb = b;
    return strcmp(ca->name, cb->name);
}

uint64_t component_name_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const struct name_component_pair *component = item;
    return hashmap_sip(component->name, strlen(component->name), seed0, seed1);
}
typedef struct C_Transform {
    size_t entity;
    Vector2 position; 
    Vector2 size; 
    float speed;
    Vector2 velocity;
} C_Transform;

typedef struct C_Collider{
    size_t entity;
    Rectangle collider;
    long collides_with;
    long collision_category;
    bool is_colliding;
} C_Collider;

typedef struct C_Renderer {
    size_t entity;
    Color color;
} C_Renderer;

/*
typedef struct Entity {
    uint32_t id;
    char* tag;
    struct hashmap *components;
} Entity;
*/

#define MAX_COMPONENTS 32
typedef struct ECS {
    //Entity* entities;
    uint32_t *free_ids;
    struct hashmap *components;
    struct hashmap **entities_components;
    int __n_of_components;
    int __n_of_entities;
} ECS;

C_Renderer new_renderer(Color color) {
    return (C_Renderer){-1,color};
}

C_Transform new_transform(Vector2 position, Vector2 size, float speed) {
    return (C_Transform){-1, position, size, speed, (Vector2){0,0}};
}

C_Collider new_collider(float x, float y, float width, float height, long collides_with, long collision_category) {
    return (C_Collider){-1,(Rectangle){x,y,width,height},collides_with, collision_category, false};
}

uint32_t new_entity(ECS *ecs, char *tag) {
    uint32_t id = ecs->__n_of_entities-1;
    if(vec_size(ecs->free_ids)>0) {
    }
    //Entity n_entity = {.id=vec_size(ecs->entities),.tag=tag};
    //n_entity.components = hashmap_new(sizeof(struct name_component_id_pair), 0, 0, 0,component_name_hash, component_name_cmp, NULL, NULL);
    vec_push(ecs->entities_components, hashmap_new(sizeof(struct name_component_id_pair), 0, 0, 0,component_name_hash, component_name_cmp, NULL, NULL));
    //vec_push(ecs->entities,n_entity);
    ecs->__n_of_entities++;
    return id;
    //return vec_size(ecs->entities)-1;
}

void kill_entity(ECS *ecs, uint32_t entity_id) {
    size_t iter = 0;
    void *item;
}

// GCII

ECS *init_ecs() {
    ECS *ecs = malloc(sizeof(ECS));
    //vec_init(ecs->entities, 32);
    ecs->components = hashmap_new(sizeof(struct name_component_pair), 0, 0, 0,component_name_hash, component_name_cmp, NULL, NULL);
    vec_init(ecs->entities_components, 32);
    ecs->__n_of_components = 0;
    ecs->__n_of_entities = 0;
    return ecs;
}

void free_ecs(ECS *ecs) {
    /*
    for(Entity* entity=vec_begin(ecs->entities);entity<vec_end(ecs->entities);entity++) {
        hashmap_free(entity->components);
    }
    vec_free(ecs->entities);
    */
    for(size_t component_ind=0;component_ind<vec_size(ecs->entities_components);component_ind++) {
        hashmap_free(ecs->entities_components[component_ind]);
    }
    vec_free(ecs->entities_components);
    hashmap_free(ecs->components);
    free(ecs);
}

#define ecs_register_component(ecs,  component)\
    do { \
        component *vec = NULL;\
        vec_init(vec, 16);\
        hashmap_set(ecs->components, &(struct name_component_pair){ #component, vec });\
        ecs->__n_of_components++;\
    }while(0)

#define ecs_add_component(ecs, entity_id, component, component_init)\
    do { \
        component *vec= ((struct name_component_pair*)hashmap_get(ecs->components, &(struct name_component_pair){ .name=#component }))->component_vec;\
        /*hashmap_set(ecs->entities[entity_id].components, &(struct name_component_id_pair){ #component, vec_size(vec) });*/\
        hashmap_set(ecs->entities_components[entity_id], &(struct name_component_id_pair){ #component, vec_size(vec) });\
        bool update = false;\
        if(vec_size(vec) >= vec_capacity(vec)) update=true; \
        component initialized = component_init;\
        initialized.entity = entity_id;\
        vec_push(vec, initialized);\
        if(update) hashmap_set(ecs->components, &(struct name_component_pair){ #component, vec });\
    }while(0)

/*
#define ecs_check_tag(ecs, entity_id, str) \
    !strcmp(ecs->entities[entity_ind].tag, str)
*/

/*
#define __get_component_id(ecs, entity_id, component) \
    ((struct name_component_id_pair*)hashmap_get(ecs->entities[entity_id].components, &(struct name_component_id_pair){.name=#component}))->component_id
*/
#define __get_component_id(ecs, entity_id, component) \
    ((struct name_component_id_pair*)hashmap_get(ecs->entities_components[entity_id], &(struct name_component_id_pair){.name=#component}))->component_id

#define __get_components(ecs, component)\
    (component*)(((struct name_component_pair*)hashmap_get(ecs->components, &(struct name_component_pair){.name=#component}))->component_vec)

#define ecs_get_component(ecs, entity_id, component)\
    (component*)&(((component*)__get_components(ecs, component))[__get_component_id(ecs, entity_id, component)])

/* 
 TODOS:
    - Tag System
    - Entity Deletion + Recycling
    - Cleanup
    - Collison
    - Gameplay?
*/

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 450;

    srand(time(0));
    InitWindow(screenWidth, screenHeight, "Game");

    ECS *ecs = init_ecs();
    ecs_register_component(ecs, C_Transform);
    ecs_register_component(ecs, C_Renderer);
    ecs_register_component(ecs, C_Collider);

    size_t player_ind = new_entity(ecs, "Player");
    ecs_add_component(ecs, player_ind, C_Transform, new_transform((Vector2){20,20}, (Vector2){60,60}, 300.f));
    ecs_add_component(ecs, player_ind, C_Renderer, new_renderer(RED));
    ecs_add_component(ecs, player_ind, C_Collider, new_collider(0, 0, 60, 60, 0, 0));
    Vector2 player_dir = {0};

    size_t box_ind = new_entity(ecs, "Box");
    ecs_add_component(ecs, box_ind, C_Renderer, new_renderer(WHITE));
    C_Transform entity_transform = new_transform((Vector2){280, 80}, (Vector2){30,50}, 300.f);
    ecs_add_component(ecs,box_ind, C_Transform, entity_transform);
    ecs_add_component(ecs, box_ind, C_Collider, new_collider(0,0,30,50, 0, 0));

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
            size_t entity_ind = new_entity(ecs, "Enemy");
            C_Renderer e_renderer = new_renderer((Color){rand()%255, rand()%255, rand()%255, 255});
            ecs_add_component(ecs, entity_ind, C_Renderer, e_renderer);
            C_Transform e_transform = new_transform((Vector2){rand()%(screenWidth), rand()%screenHeight}, (Vector2){10,10}, 200);
            ecs_add_component(ecs, entity_ind, C_Transform, e_transform);
        }

        // Enemy "AI"
        /*
        for(size_t entity_ind=0;entity_ind<vec_size(ecs->entities);entity_ind++) {
            if(!ecs_check_tag(ecs, entity_ind, "Enemy")) continue;
            C_Transform *entity_transform = ecs_get_component(ecs, entity_ind, C_Transform);
            Vector2 dir = Vector2Normalize(Vector2Subtract(transform->position, entity_transform->position));
            entity_transform->velocity = Vector2Scale(dir, entity_transform->speed);
        }
        */

        // Velocity 
        C_Transform *c_transforms = __get_components(ecs, C_Transform);
        for(C_Transform* transform=vec_begin(c_transforms); transform<vec_end(c_transforms); transform++) {
            transform->position = Vector2Add(transform->position, Vector2Scale(transform->velocity, GetFrameTime()));
        }

        // Collisions
        C_Collider *c_colliders = __get_components(ecs, C_Collider);
        for(C_Collider* collider=vec_begin(c_colliders); collider<vec_end(c_colliders); collider++) {
            for(C_Collider* collision=collider+1; collision<vec_end(c_colliders); collision++) {
                // Category Check
                // Bounding Box Check
                C_Transform *collider_transform = ecs_get_component(ecs, collider->entity, C_Transform);
                C_Transform *collision_transform = ecs_get_component(ecs, collision->entity, C_Transform);
                Rectangle collider_rect  = {collider_transform->position.x, collider_transform->position.y, collider_transform->size.x, collider_transform->size.y};
                Rectangle collision_rect = {collision_transform->position.x, collision_transform->position.y, collision_transform->size.x, collision_transform->size.y};
                bool is_colliding = CheckCollisionRecs(collider_rect, collision_rect);
                collider->is_colliding = is_colliding;
                collision->is_colliding = is_colliding;
            }
        }

        BeginDrawing();
            ClearBackground(BLACK);
            //for(size_t entity_ind=0;entity_ind<vec_size(ecs->entities);entity_ind++) {
            for(size_t entity_ind=0;entity_ind<ecs->__n_of_entities;entity_ind++) {
                C_Transform* transform = ecs_get_component(ecs, entity_ind, C_Transform);
                C_Renderer* renderer = ecs_get_component(ecs, entity_ind, C_Renderer);
                /*
                if(ecs_check_tag(ecs, entity_ind, "Player") || ecs_check_tag(ecs, entity_ind, "Box")) {
                    C_Collider* collider = ecs_get_component(ecs, entity_ind, C_Collider);
                    Color c = WHITE;
                    if(collider->is_colliding) {
                        c = RED;
                    }
                    DrawRectangleLines(transform->position.x, transform->position.y, transform->size.x, transform->size.y, c);
                }else {
                    */
                    DrawRectangleV(transform->position, transform->size, renderer->color);
                //}
            }
        EndDrawing();
    }
    free_ecs(ecs);
    CloseWindow();
    return 0;
}
