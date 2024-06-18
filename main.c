#include "raylib.h"
#include "raymath.h"
#include "vector.h"
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
    long id;
    char* tag;
    size_t c_transform;
    size_t c_renderer;
    size_t c_collider;
} Entity;

typedef struct ECS {
    Entity* entities;
    C_Transform* c_transforms;
    C_Collider* c_colliders;
    C_Renderer* c_renderers;
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

Entity new_entity(char* tag) {
    return (Entity) {
        .id = 0,
        .tag = tag, 
        .c_transform = -1,
        .c_renderer = -1,
        .c_collider = -1
    };
}

#define add_component(entity_id, entity_component, components, component) \
    do {\
        entity_component = vec_size(components); \
        vec_push(components, component);\
        components[entity_component].entity = entity_id;\
    }while(0)
#define __register_component(component) \
    component* component##s= NULL;\
    vec_init(component##s, 32);\

#define __add_component(entity_id, component_type, component_init) \
    vec_size(component##s);
    do {\
        vec_push(components##s,component_init);\
    }while(0) 

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 450;

    srand(time(0));
    InitWindow(screenWidth, screenHeight, "Game");

    C_Renderer* c_renderers = NULL; 
    vec_init(c_renderers, 32);
    C_Transform* c_transforms = NULL; 
    vec_init(c_transforms, 32);
    C_Collider* c_colliders = NULL;
    vec_init(c_colliders, 32);
    Entity* entities = NULL;
    vec_init(entities, 32);

    __register_component(int);

    // Player Setup
    Entity player = new_entity("Player");
    size_t player_ind = vec_size(entities);
    add_component(player_ind, player.c_renderer, c_renderers, new_renderer(WHITE));
    C_Transform player_transform = new_transform((Vector2){20, 20}, (Vector2){60,60}, 300.f);
    add_component(player_ind, player.c_transform, c_transforms, player_transform);
    add_component(player_ind, player.c_collider, c_colliders, new_collider(0,0,20,20,0,0));
    vec_push(entities, player);
    size_t player_transform_ind = player.c_transform;
    Vector2 player_dir = {0};

    size_t entity_id = vec_size(entities);
    vec_push(entities,new_entity("Enemy"));
    add_component(entity_id, entities[entity_id].c_renderer, c_renderers, new_renderer(WHITE));
    C_Transform entity_transform = new_transform((Vector2){280, 80}, (Vector2){30,50}, 300.f);
    add_component(entity_id, entities[entity_id].c_transform, c_transforms, entity_transform);
    add_component(entity_id, entities[entity_id].c_collider, c_colliders, new_collider(0,0,30,50, 0, 0));

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
        c_transforms[player_transform_ind].velocity = Vector2Scale(Vector2Normalize(player_dir), c_transforms[player_transform_ind].speed);

        /*
        if(IsKeyPressed(KEY_N)) {
            Entity n_entity = new_entity("Enemy");
            C_Renderer e_renderer = new_renderer((Color){rand()%255, rand()%255, rand()%255, 255});
            C_Transform e_transform = new_transform((Vector2){rand()%(screenWidth), rand()%screenHeight}, (Vector2){10,10}, 200);
            add_component(vec_size(entities), n_entity.c_transform, c_transforms, e_transform);
            add_component(vec_size(entities), n_entity.c_renderer, c_renderers, e_renderer);
            vec_push(entities, n_entity); 
        }
        */

        // Enemy "AI"
        /*
        for(Entity* entity=vec_begin(entities);entity<vec_end(entities);entity++) {
            if(entity->tag!="Enemy") continue;
            C_Transform transform = c_transforms[entity->c_transform];
            Vector2 dir = Vector2Normalize(Vector2Subtract(c_transforms[player_transform].position, transform.position));
            c_transforms[entity->c_transform].velocity = Vector2Scale(dir, c_transforms[entity->c_transform].speed);
        }
        */

        // Velocity 
        for(C_Transform* transform=vec_begin(c_transforms); transform<vec_end(c_transforms); transform++) {
            transform->position = Vector2Add(transform->position, Vector2Scale(transform->velocity, GetFrameTime()));
        }

        // Collisions
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

        BeginDrawing();
            ClearBackground(BLACK);
            for(Entity* entity=vec_begin(entities);entity<vec_end(entities);entity++) {
                if(entity->tag!="Enemy" && entity->tag!="Player") continue;
                C_Transform transform = c_transforms[entity->c_transform];
                //DrawRectangleV(transform.position, transform.size, c_renderers[entity->c_renderer].color);
                Color c = WHITE;
                if(c_colliders[entity->c_collider].is_colliding) {
                    c = RED;
                }
                DrawRectangleLines(transform.position.x, transform.position.y, transform.size.x, transform.size.y, c);
            }
        EndDrawing();
    }

    vec_free(c_renderers);
    vec_free(c_transforms);
    vec_free(c_colliders);
    vec_free(entities);

    CloseWindow();
    return 0;
}
