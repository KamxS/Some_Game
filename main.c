#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "include/kxecs.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 450

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
    Texture texture;
    bool has_texture;
} C_Renderer;

typedef struct C_Camera {
    Camera2D camera;
    char* following_tag;
} C_Camera;

C_Transform new_transform(Vector2 position, Vector2 size, float speed) {
    return (C_Transform){position, size, speed, (Vector2){0,0}};
}

C_Collider new_collider(float x, float y, float width, float height, long collides_with, long collision_category) {
    return (C_Collider){(Rectangle){x,y,width,height},collides_with, collision_category, false};
}
/* TODO LIST */
/*
    - Cleanup
    - Pause
    - Generations
    - Collisions
    - More Flexible Entity Management
    - Better Kill & Spawn System
    - Gameplay
        - Heavily Music Based Game
        - Music Pulsating Wormholes
        - Ships in a musical instrument shapes
        - Synthwave Aesthetic
*/

void player_movement_sys(ECS *ecs, entity_t entity_id) {
    Vector2 player_dir = {0};
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
    C_Transform *player_transform = ecs_get_component(ecs, entity_id, C_Transform);
    player_transform->velocity = Vector2Scale(Vector2Normalize(player_dir), player_transform->speed);
}

void apply_velocity_sys(ECS *ecs, entity_t entity_id) {
    C_Transform *transform = ecs_get_component(ecs, entity_id, C_Transform);
    transform->position = Vector2Add(transform->position, Vector2Scale(transform->velocity, GetFrameTime()));
}

void enemy_ai_sys(ECS *ecs, entity_t entity_id) {
    C_Transform *transform = ecs_get_component(ecs, entity_id, C_Transform);
    C_Transform *player_transform = ecs_get_component(ecs, ecs_find_entity_with_tag(ecs, "Player"), C_Transform);
    Vector2 dir = Vector2Normalize(Vector2Subtract(player_transform->position, transform->position));
    transform->velocity = Vector2Scale(dir, transform->speed);
}

void draw_entity_sys(ECS *ecs, entity_t entity_id) {
    C_Renderer *renderer = ecs_get_component(ecs, entity_id, C_Renderer);
    C_Transform *transform = ecs_get_component(ecs, entity_id, C_Transform);

    if(renderer->has_texture) {
        DrawTextureV(renderer->texture, transform->position, renderer->color);
    }else {
        DrawRectangleV(transform->position, transform->size, renderer->color);
    }
}

void check_collisions_sys(ECS *ecs, entity_t entity_id) {
    C_Collider *collider = ecs_get_component(ecs, entity_id, C_Collider);
    C_Transform *collider_transform = ecs_get_component(ecs, entity_id, C_Transform);
    bool is_colliding = false;

    C_Collider *c_colliders = ecs_iter_components(ecs, C_Collider);
    for(C_Collider* collision=vec_begin(c_colliders); collision<vec_end(c_colliders); collision++) {
        if(collider==collision) continue;
        // Category Check
        // Bounding Box Check
        C_Transform *collision_transform = ecs_get_component(ecs, ecs_get_entity_id(ecs, C_Collider, collision), C_Transform);
        Rectangle collider_rect  = {collider_transform->position.x, collider_transform->position.y, collider_transform->size.x, collider_transform->size.y};
        Rectangle collision_rect = {collision_transform->position.x, collision_transform->position.y, collision_transform->size.x, collision_transform->size.y};
        if(CheckCollisionRecs(collider_rect, collision_rect)) {
            is_colliding = true;
            break;
        }
    }
    collider->is_colliding = is_colliding;
}

void camera_follow_sys(ECS *ecs, entity_t entity_id) {
    C_Camera *c_camera = ecs_get_component(ecs, entity_id, C_Camera);
    C_Transform *to_follow = ecs_get_component(ecs, ecs_find_entity_with_tag(ecs, c_camera->following_tag), C_Transform);
    c_camera->camera.target = to_follow->position;
}

void spawn_enemy_sys(ECS *ecs, entity_t _) {
    if(IsKeyPressed(KEY_N)) {
        entity_t entity_ind = new_entity_with_tag(ecs, "Enemy");
        C_Renderer e_renderer = {(Color){rand()%255, rand()%255, rand()%255, 255}, (Texture){0}, false};
        ecs_add_component(ecs, entity_ind, C_Renderer, e_renderer);
        C_Transform e_transform = new_transform((Vector2){rand()%(GetScreenWidth()), rand()%GetScreenHeight()}, (Vector2){10,10}, 200);
        ecs_add_component(ecs, entity_ind, C_Transform, e_transform);
        ecs_add_component(ecs, entity_ind, C_Collider, new_collider(0, 0, 10, 10, 0, 0));
    }
}

void erase_entities_sys(ECS *ecs, entity_t _) {
    if(vec_size(ecs->entities_to_kill)>0) {
        for(entity_t *entity=vec_begin(ecs->entities_to_kill);entity<vec_end(ecs->entities_to_kill);entity++) {
            __ecs_erase_entity(ecs, *entity);
        }
        vec_erase(ecs->entities_to_kill, 0, vec_size(ecs->entities_to_kill));
    }
}

int main(void) {
    srand(time(0));
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Game");

    // TODO: Resource Management
    Shader space_curvature_shd = LoadShader("resources/shaders/spacecurvature.fs",0);
    int secondsLoc = GetShaderLocation(space_curvature_shd, "seconds");
    Texture player_texture = LoadTexture("resources/player_ship.png");

    char id_display_buf[64] = "";
    ECS *ecs = init_ecs();
    ecs_register_component(ecs, C_Transform);
    ecs_register_component(ecs, C_Renderer);
    ecs_register_component(ecs, C_Collider);
    ecs_register_component(ecs, C_Camera);

    entity_t player_id = new_entity_with_tag(ecs, "Player");
    C_Transform player_transform = new_transform((Vector2){20,20}, (Vector2){60,60}, 300.f);
    ecs_add_component(ecs, player_id, C_Transform, player_transform);
    ecs_add_component(ecs, player_id, C_Renderer, {WHITE, player_texture, true});
    ecs_add_component(ecs, player_id, C_Collider, new_collider(0, 0, 60, 60, 0, 0));

    entity_t camera_id = new_entity_with_tag(ecs, "Main Camera");
    Camera2D camera = {(Vector2){(GetScreenWidth()/2)-player_transform.size.x/2,GetScreenHeight()/2-player_transform.size.y/2}, (Vector2){0,0}, 0.f, 1.f};
    ecs_add_component(ecs, camera_id, C_Camera, {camera, "Player"}); 
    
    ecs_register_system(ecs, ON_PREUPDATE, erase_entities_sys); 

    ecs_register_component_system(ecs, ON_UPDATE, apply_velocity_sys, C_Transform);  
    ecs_register_component_system(ecs, ON_UPDATE, check_collisions_sys, C_Transform, C_Collider);  
    ecs_register_component_system(ecs, ON_UPDATE, camera_follow_sys, C_Camera);  
    ecs_register_tag_system(ecs, ON_UPDATE, player_movement_sys, "Player");
    ecs_register_tag_system(ecs, ON_UPDATE, enemy_ai_sys, "Enemy");
    ecs_register_system(ecs, ON_UPDATE, spawn_enemy_sys); 

    ecs_register_component_system(ecs, ON_DRAW, draw_entity_sys, C_Renderer, C_Transform);

    float seconds = 0.f;
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        seconds += GetFrameTime();
        SetShaderValue(space_curvature_shd, secondsLoc, &seconds, SHADER_UNIFORM_FLOAT);

        ecs_call_system(ecs, ON_PREUPDATE);
        ecs_call_system(ecs, ON_UPDATE);

        C_Camera *c_camera = ecs_get_component(ecs, ecs_find_entity_with_tag(ecs, "Main Camera"), C_Camera);
        // ID Display + Entity by Click Kill
        C_Transform *c_transforms = ecs_iter_components(ecs, C_Transform);
        Vector2 mousePos = GetScreenToWorld2D(GetMousePosition(), c_camera->camera);
        size_t selected_entity = -1;
        for(C_Transform *transform=vec_begin(c_transforms);transform<vec_end(c_transforms);transform++) {
            if(CheckCollisionPointRec(mousePos, (Rectangle){transform->position.x, transform->position.y, transform->size.x, transform->size.y})) {
                selected_entity = ecs_get_entity_id(ecs, C_Transform, transform);
            }
        }
        if(IsMouseButtonPressed(0) && selected_entity!=-1) {
            kill_entity(ecs, selected_entity);
        }

        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode2D(c_camera->camera);
            ecs_call_system(ecs, ON_DRAW);
            EndMode2D();

            //BeginShaderMode(space_curvature_shd);
            //EndShaderMode();

            // ID Display
            if(selected_entity!=-1) sprintf(id_display_buf, "ID: %d, GEN: 0", selected_entity);
            DrawText(id_display_buf, GetScreenWidth()-140, 20, 16, WHITE);
        EndDrawing();
    }
    free_ecs(ecs);
    CloseWindow();
    return 0;
}
