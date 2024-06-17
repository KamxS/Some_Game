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

typedef struct C_Renderer {
    size_t entity;
    Color color;
} C_Renderer;

typedef struct Entity {
    long id;
    char* tag;
    size_t c_transform;
    size_t c_renderer;
} Entity;

Entity new_entity(char* tag) {
    return (Entity) {
        .id = 0,
        .tag = tag, 
        .c_transform = -1,
        .c_renderer = -1
    };
}

C_Renderer new_renderer(Color color) {
    return (C_Renderer){-1,color};
}

C_Transform new_transform(Vector2 position, Vector2 size, float speed) {
    return (C_Transform){-1, position, size, speed, (Vector2){0,0}};
}

#define add_component(entity_id, entity_component, components, component) \
    do {\
        entity_component = vec_size(components); \
        vec_push(components, component);\
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
    Entity* entities = NULL;
    vec_init(entities, 32);

    Entity player = new_entity("Player");
    size_t player_ind = vec_size(entities);
    add_component(player_ind, player.c_renderer, c_renderers, new_renderer(WHITE));
    add_component(player_ind, player.c_transform, c_transforms, new_transform((Vector2){20, 20}, (Vector2){10,10}, 300.f));
    vec_push(entities, player);
    size_t player_transform = player.c_transform;
    Vector2 player_dir = {0};

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
        c_transforms[player_transform].velocity = Vector2Scale(Vector2Normalize(player_dir), c_transforms[player_transform].speed);

        if(IsKeyPressed(KEY_N)) {
            Entity n_entity = new_entity("Enemy");
            C_Renderer e_renderer = new_renderer((Color){rand()%255, rand()%255, rand()%255, 255});
            C_Transform e_transform = new_transform((Vector2){rand()%(screenWidth), rand()%screenHeight}, (Vector2){10,10}, 200);
            add_component(vec_size(entities), n_entity.c_transform, c_transforms, e_transform);
            add_component(vec_size(entities), n_entity.c_renderer, c_renderers, e_renderer);
            vec_push(entities, n_entity); 
        }

        for(Entity* entity=vec_begin(entities);entity<vec_end(entities);entity++) {
            if(entity->tag!="Enemy") continue;
            C_Transform transform = c_transforms[entity->c_transform];
            Vector2 dir = Vector2Normalize(Vector2Subtract(c_transforms[player_transform].position, transform.position));
            c_transforms[entity->c_transform].velocity = Vector2Scale(dir, c_transforms[entity->c_transform].speed);
        }

        // Velocity 
        for(Entity* entity=vec_begin(entities);entity<vec_end(entities);entity++) {
            if(entity->tag!="Enemy" && entity->tag!="Player") continue;
            c_transforms[entity->c_transform].position = Vector2Add(
                c_transforms[entity->c_transform].position,
                Vector2Scale(c_transforms[entity->c_transform].velocity, GetFrameTime())
            );
        }

        BeginDrawing();
            ClearBackground(BLACK);
            for(Entity* entity=vec_begin(entities);entity<vec_end(entities);entity++) {
                if(entity->tag!="Enemy" && entity->tag!="Player") continue;
                C_Transform transform = c_transforms[entity->c_transform];
                DrawRectangleV(transform.position, transform.size, c_renderers[entity->c_renderer].color);
            }
        EndDrawing();
    }

    vec_free(c_renderers);
    vec_free(c_transforms);
    vec_free(entities);

    CloseWindow();
    return 0;
}
