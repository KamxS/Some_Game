#include "raylib.h"
#include "raymath.h"
#include "vector.h"
#include <stdlib.h>
#include <time.h>

typedef struct C_Transform {
    Vector2 position; 
    Vector2 size; 
    float speed;
    Vector2 velocity;
} C_Transform;

typedef struct C_Renderer {
    Color color;
} C_Renderer;

typedef struct Entity {
    char* tag;
    size_t c_transform;
    size_t c_renderer;
} Entity;

Entity new_entity(char* tag) {
    return (Entity) {
        .tag = tag, 
        .c_transform = -1,
        .c_renderer = -1
    };
}

C_Renderer* new_renderer(C_Renderer* render_vec, Color color) {
    C_Renderer rend = {color};
    vec_push(render_vec, rend);
    return render_vec;
}

C_Transform* new_transform(C_Transform* transform_vec, Vector2 position, Vector2 size, float speed) {
    C_Transform transform = {position, size, speed, (Vector2){0,0}};
    vec_push(transform_vec, transform);
    return transform_vec;
}

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
    size_t player_renderer = vec_size(c_renderers);
    c_renderers = new_renderer(c_renderers, WHITE);
    size_t player_transform = vec_size(c_transforms);
    c_transforms = new_transform(c_transforms, (Vector2){20, 20}, (Vector2){10,10}, 300.f);
    player.c_renderer = player_renderer;
    player.c_transform = player_transform;
    vec_push(entities, player);
    size_t player_id = vec_size(entities);
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
            n_entity.c_renderer = vec_size(c_renderers);
            n_entity.c_transform = vec_size(c_transforms);
            c_renderers = new_renderer(c_renderers, (Color){rand()%255, rand()%255, rand()%255, 255});
            c_transforms = new_transform(c_transforms, (Vector2){rand()%(screenWidth), rand()%screenHeight}, (Vector2){10,10}, 200);
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
