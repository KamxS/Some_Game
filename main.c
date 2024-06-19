#include "raylib.h"
#include "raymath.h"
#include "vector.h"
#include "hashmap.h"
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

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

typedef struct Entity {
    uint32_t id;
    char* tag;
    size_t c_transform;
    size_t c_renderer;
    size_t c_collider;
} Entity;

#define MAX_COMPONENTS 32
typedef struct ECS {
    Entity* entities;
    struct hashmap *components;
    int __n_of_components;
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

size_t new_entity(ECS *ecs, char *tag) {
    Entity n_entity = {.id = 0,.tag = tag, .c_transform = -1,.c_renderer = -1,.c_collider = -1};
    vec_push(ecs->entities,n_entity);
    return vec_size(ecs->entities)-1;
}

struct component_kv{
    char *name;
    void *component_vec;
};

int user_compare(const void *a, const void *b, void *udata) {
    const struct component_kv *ca = a;
    const struct component_kv *cb = b;
    return strcmp(ca->name, cb->name);
}

uint64_t user_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const struct component_kv *component = item;
    return hashmap_sip(component->name, strlen(component->name), seed0, seed1);
}

ECS *init_ecs() {
    ECS *ecs = malloc(sizeof(ECS));
    vec_init(ecs->entities, 32);
    ecs->components = hashmap_new(sizeof(struct component_kv), 0, 0, 0,user_hash, user_compare, NULL, NULL);
    ecs->__n_of_components = 0;
    return ecs;
}

void free_ecs(ECS *ecs) {
    vec_free(ecs->entities);
    hashmap_free(ecs->components);
    free(ecs);
}


#define add_component(entity_id, entity_component, components, component) \
    do {\
        entity_component = vec_size(components);\
        vec_push(components, component);\
        components[entity_component].entity = entity_id;\
    }while(0)


#define __register_component(ecs,  component)\
    do { \
        component *vec = NULL;\
        vec_init(vec, 16);\
        hashmap_set(ecs->components, &(struct component_kv){ #component, vec });\
        ecs->__n_of_components++;\
    }while(0)

#define __add_component(ecs, entity_component, component, component_init)\
    do { \
        component *vec= ((struct component_kv*)hashmap_get(ecs->components, &(struct component_kv){ .name=#component }))->component_vec;\
        entity_component = vec_size(vec);\
        bool update = false;\
        if(vec_size(vec) >= vec_capacity(vec)) update=true; \
        vec_push(vec, component_init);\
        if(update) hashmap_set(ecs->components, &(struct component_kv){ #component, vec });\
    }while(0)

#define __get_component(ecs, entity_id, component)\
    &((component*)((struct component_kv*)hashmap_get(ecs->components, &(struct component_kv){.name=#component}))->component_vec)[entity_id]

#define __get_components(ecs, component)\
    (component*)((struct component_kv*)hashmap_get(ecs->components, &(struct component_kv){.name=#component}))->component_vec

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 450;

    srand(time(0));
    InitWindow(screenWidth, screenHeight, "Game");

    ECS *ecs = init_ecs();
    __register_component(ecs, C_Transform);
    __register_component(ecs, C_Renderer);
    __register_component(ecs, C_Collider);

    size_t player_ind = new_entity(ecs, "Player");
    __add_component(ecs, ecs->entities[player_ind].c_transform, C_Transform, new_transform((Vector2){20,20}, (Vector2){60,60}, 300.f));
    __add_component(ecs, ecs->entities[player_ind].c_renderer, C_Renderer, new_renderer(WHITE));
    __add_component(ecs, ecs->entities[player_ind].c_collider, C_Collider, new_collider(0, 0, 60, 60, 0, 0));
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
        C_Transform *transform = __get_component(ecs, player_ind, C_Transform);
        transform->velocity = Vector2Scale(Vector2Normalize(player_dir), transform->speed);

        if(IsKeyPressed(KEY_N)) {
            size_t entity_ind = new_entity(ecs, "Enemy");
            C_Renderer e_renderer = new_renderer((Color){rand()%255, rand()%255, rand()%255, 255});
            __add_component(ecs, ecs->entities[entity_ind].c_renderer, C_Renderer, e_renderer);
            C_Transform e_transform = new_transform((Vector2){rand()%(screenWidth), rand()%screenHeight}, (Vector2){10,10}, 200);
            __add_component(ecs, ecs->entities[entity_ind].c_transform, C_Transform, e_transform);
        }

        // Enemy "AI"
        for(size_t entity_ind=0;entity_ind<vec_size(ecs->entities);entity_ind++) {
            if(ecs->entities[entity_ind].tag!="Enemy") continue;
            C_Transform *entity_transform = __get_component(ecs, entity_ind, C_Transform);
            Vector2 dir = Vector2Normalize(Vector2Subtract(transform->position, entity_transform->position));
            entity_transform->velocity = Vector2Scale(dir, entity_transform->speed);
        }

        // Velocity 
        C_Transform *c_transforms = __get_components(ecs, C_Transform);
        for(C_Transform* transform=vec_begin(c_transforms); transform<vec_end(c_transforms); transform++) {
            transform->position = Vector2Add(transform->position, Vector2Scale(transform->velocity, GetFrameTime()));
        }

        // Collisions
        /*
        C_Collider *c_colliders = __get_components(ecs, C_Collider);
        for(C_Collider* collider=vec_begin(c_colliders); collider<vec_end(c_colliders); collider++) {
            for(C_Collider* collision=collider+1; collision<vec_end(c_colliders); collision++) {
                // Category Check
                // Bounding Box Check
                C_Transform collider_transform = c_transforms[entities[collider->entity].c_transform];
                C_Transform collision_transform = c_transforms[entities[collision->entity].c_transform];
                Rectangle collider_rect  = {collider_transform.position.x, collider_transform.position.y, collider_transform.size.x, collider_transform.size.y};
                Rectangle collision_rect = {collision_transform.position.x, collision_transform.position.y, collision_transform.size.x, collision_transform.size.y};
                bool is_colliding = CheckCollisionRecs(collider_rect, collision_rect);
                collider->is_colliding = is_colliding;
                collision->is_colliding = is_colliding;
            }
        }
        */

        BeginDrawing();
            ClearBackground(BLACK);
            for(size_t entity_ind=0;entity_ind<vec_size(ecs->entities);entity_ind++) {
                //if(entity->tag!="Enemy" && entity->tag!="Player") continue;
                C_Transform* transform = __get_component(ecs, entity_ind, C_Transform);
                C_Renderer* renderer = __get_component(ecs, entity_ind, C_Renderer);
                DrawRectangleV(transform->position, transform->size, renderer->color);
                /*
                Color c = WHITE;
                if(c_colliders[entity->c_collider].is_colliding) {
                    c = RED;
                }
                DrawRectangleLines(transform.position.x, transform.position.y, transform.size.x, transform.size.y, c);
                */
            }
        EndDrawing();
    }

    free_ecs(ecs);

    CloseWindow();
    return 0;
}
