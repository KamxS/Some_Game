#include <stdio.h>
#include <math.h>
#include "include/kxecs.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 450

typedef struct {
  Vector2 position;
  Vector2 size;
  float speed;
  Vector2 velocity;
} C_Transform;

enum ColliderType { COLLIDER_VERTICES, COLLIDER_CIRCLE };

typedef struct {
    enum ColliderType collider_t;
    union {
        // Vertices' positions are relative to object's position
        // TODO: Not fixed amount of vertices
        struct {
            Vector2 vertices[32];
            size_t n_of_vertices;
        } vertices_info;
        struct {
            Vector2 offset;
            float radius;
        } circle_info;
    };
} ColliderInfo;

typedef struct {
    ColliderInfo collider_info;
    enum ColliderType collider_t;
    long layer;
    long layer_mask;
    bool is_colliding;
    Vector2 simplex[3];
} C_Collider;

enum ShapeType {RECT, CIRCLE};
typedef struct {
    Color color;
    Texture texture;
    bool has_texture;
    enum ShapeType shape_t;
} C_Renderer;

typedef struct {
    Camera2D camera;
    char *following_tag;
} C_Camera;

typedef struct {
    Vector2 start;
    Vector2 end;
    Vector2 pen_vec;
} C_Debug;

C_Transform new_transform(Vector2 position, Vector2 size, float speed) {
    return (C_Transform){position, size, speed, (Vector2){0, 0}};
}

C_Collider new_collider_circle(float cx, float cy, float radius, long layer, long layer_mask) {
    C_Collider nc = {0};
    nc.collider_info.collider_t = COLLIDER_CIRCLE;
    nc.collider_info.circle_info.offset = (Vector2){cx,cy};
    nc.collider_info.circle_info.radius = radius;
    nc.layer = layer;
    nc.layer_mask = layer_mask;
    return nc;
}
C_Collider new_collider_rect(float x, float y, float width, float height, long layer, long layer_mask) {
    C_Collider nc = {0};
    nc.collider_info.collider_t = COLLIDER_VERTICES;
    nc.collider_info.vertices_info.n_of_vertices = 4;
    nc.collider_info.vertices_info.vertices[0] = (Vector2){0,0};
    nc.collider_info.vertices_info.vertices[1] = (Vector2){width,0};
    nc.collider_info.vertices_info.vertices[2] = (Vector2){width,height};
    nc.collider_info.vertices_info.vertices[3] = (Vector2){0,height};

    nc.layer = layer;
    nc.layer_mask = layer_mask;
    return nc;
}
/* TODO LIST */
/*
    - Tags for systems:
    ecs_register_system(move)
    ecs_register_system(pause_menu, PAUSE_SYS)
    if(key==Escape) ECS_SET_SYSTEM_TAG(PAUSE_SYS)
    - Tilemap
    - Cleanup
    - Pause
    - Generations
    - Colliders Display Debug
    - GJK Display Debug
    - OnEntityDelete component hook
    - https://www.researchgate.net/publication/228574502_How_to_implement_a_pressure_soft_body_model
    - More Flexible Entity Management
    - Better Kill & Spawn System
    - Gameplay
*/

void player_movement_sys(ECS *ecs, entity_t entity_id) {
    Vector2 player_dir = {0};
    if (IsKeyDown(KEY_W)) {
        player_dir.y = -1;
    }
    if (IsKeyDown(KEY_S)) {
        player_dir.y = 1;
    }
    if (IsKeyDown(KEY_A)) {
        player_dir.x = -1;
    }
    if (IsKeyDown(KEY_D)) {
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
    Vector2 dir = Vector2Normalize(
    Vector2Subtract(player_transform->position, transform->position));
    transform->velocity = Vector2Scale(dir, transform->speed);
}

void draw_entity_sys(ECS *ecs, entity_t entity_id) {
    C_Renderer *renderer = ecs_get_component(ecs, entity_id, C_Renderer);
    C_Transform *transform = ecs_get_component(ecs, entity_id, C_Transform);

    if (renderer->has_texture) {
        DrawTextureV(renderer->texture, transform->position, renderer->color);
    } else {
        if(renderer->shape_t==RECT) DrawRectangleV(transform->position, transform->size, renderer->color);
        if(renderer->shape_t==CIRCLE) DrawCircleV(transform->position, (transform->size.x)/2, renderer->color);
    }
}

void draw_colliders_debug_sys(ECS *ecs, entity_t entity_id) {
    C_Transform *transform = ecs_get_component(ecs, entity_id, C_Transform);
    C_Collider *collider = ecs_get_component(ecs, entity_id, C_Collider);
    Color c = WHITE;
    if(collider->is_colliding) c=RED;

    switch(collider->collider_info.collider_t) {
        case(COLLIDER_VERTICES):
            for(size_t ind = 0;ind<collider->collider_info.vertices_info.n_of_vertices-1;ind++) {
                Vector2 pos1 = Vector2Add(collider->collider_info.vertices_info.vertices[ind], transform->position);
                Vector2 pos2 = Vector2Add(collider->collider_info.vertices_info.vertices[ind+1], transform->position);
                DrawLineV(pos1,pos2,c);
            }
            Vector2 pos1 = Vector2Add(collider->collider_info.vertices_info.vertices[0], transform->position);
            Vector2 pos2 = Vector2Add(collider->collider_info.vertices_info.vertices[collider->collider_info.vertices_info.n_of_vertices-1], transform->position);
            DrawLineV(pos1,pos2, c);
            break;
        case(COLLIDER_CIRCLE):
            Vector2 pos = Vector2Add(transform->position, collider->collider_info.circle_info.offset);
            float r = collider->collider_info.circle_info.radius;
            DrawRing(pos, r-2, r, 0, 360, 36, c);
            break;
    }
}

Vector2 support_function_vertices(Vector2 position, Vector2 *vertices, size_t n_of_vertices, Vector2 dir) {
    Vector2 v0 = Vector2Add(position, vertices[0]);
    float dotmax = Vector2DotProduct(v0, dir);
    Vector2 vmax = v0;
    for(size_t ind=1;ind<n_of_vertices;ind++) {
        Vector2 v = Vector2Add(vertices[ind], position);
        float dot = Vector2DotProduct(v, dir);
        if(dot>dotmax) {
            vmax=v;
            dotmax = dot;
        }
    }
    return vmax;
}

Vector2 support_function_circle(Vector2 center, float radius, Vector2 dir) {
    return Vector2Add(center, Vector2Scale(dir,radius));
}
Vector2 support_function(Vector2 position, ColliderInfo collider_info, Vector2 dir) {
    switch(collider_info.collider_t) {
        case COLLIDER_VERTICES:
            return support_function_vertices(position, collider_info.vertices_info.vertices, collider_info.vertices_info.n_of_vertices, dir);
            break;
        case COLLIDER_CIRCLE:
            return support_function_circle(Vector2Add(position,collider_info.circle_info.offset), collider_info.circle_info.radius, dir);
            break;
    }
}

Vector2 get_epa_penetration_vec(C_Collider *collider, C_Transform *transform, C_Debug *_debug) {
    Vector2 *simplex=NULL;
    vec_init(simplex, 16);
    vec_push(simplex, collider->simplex[0]);
    vec_push(simplex, collider->simplex[1]);
    vec_push(simplex, collider->simplex[2]);

    // winding of simplex
    float e0 = (simplex[1].x-simplex[0].x) * (simplex[1].y + simplex[0].y);
    float e1 = (simplex[2].x - simplex[1].x) * (simplex[2].y + simplex[1].y);
    float e2 = (simplex[0].x - simplex[2].x) * (simplex[0].y + simplex[2].y);
    bool clockwise_winding = (e0 + e1 + e2 >= 0);

    Vector2 penetration_vec = {0};
    for(int it=0;it<32;it++) {
        // Find closest edge to origin
        size_t closest_ind =-1;
        size_t closest_ind2 =-1;
        float closest_dist;
        Vector2 closest_normal;

        Vector2 d_edge;

        for(size_t i=0;i<vec_size(simplex);i++) {
            size_t j = (i+1)%vec_size(simplex);

            // calculate the edge
            Vector2 edge = Vector2Subtract(simplex[i], simplex[j]);

            // calculate the outward-facing normal of the edge
            Vector2 edge_normal_out;
            if(clockwise_winding) {
                edge_normal_out=(Vector2){edge.y,-edge.x};
            }else {
                edge_normal_out=(Vector2){-edge.y,edge.x};
            }

            edge_normal_out = Vector2Normalize(edge_normal_out);

            // calculate how far away the edge is from the origin
            float dist = Vector2DotProduct(edge_normal_out, simplex[i]);
            
            if(dist < closest_dist || closest_ind==-1) {
                closest_dist = dist;
                closest_normal = edge_normal_out;
                closest_ind=j;

                d_edge = edge;
            }
        }

        //_debug->pen_vec=closest_normal;
        _debug->start=simplex[closest_ind];
        _debug->end=d_edge;

        Vector2 sup = support_function((Vector2){0,0}, collider->collider_info,closest_normal);
        float dist = Vector2DotProduct(sup, closest_normal);
        penetration_vec = Vector2Scale(closest_normal, dist);

        if(fabsf(dist-closest_dist)<=0.000001) {
            return penetration_vec;
        }else {
            vec_push(simplex, simplex[closest_ind]);
            simplex[closest_ind]=sup;
        }
    }
    vec_free(simplex);
    _debug->pen_vec=penetration_vec;
    return penetration_vec;
}

bool check_gjk_collision(ECS *ecs, C_Collider *collider, C_Collider* collision) {
    C_Transform *collider_transform = ecs_get_component(ecs, ecs_get_entity_id(ecs, C_Collider, collider), C_Transform);
    C_Transform *collision_transform = ecs_get_component(ecs, ecs_get_entity_id(ecs, C_Collider, collision), C_Transform);

    size_t simplex_vertices = 0;
    Vector2 simplex[3] = {0};
    Vector2 dir = Vector2Normalize((Vector2){1,1});

    simplex[simplex_vertices] = Vector2Subtract(
        support_function(collider_transform->position, collider->collider_info, dir),
        support_function(collision_transform->position, collision->collider_info, Vector2Scale(dir,-1.f))
    );
    dir = Vector2Normalize(Vector2Scale(simplex[0],-1));

    while(1) {
        simplex[++simplex_vertices] = Vector2Subtract(
            support_function(collider_transform->position, collider->collider_info, dir),
            support_function(collision_transform->position, collision->collider_info, Vector2Scale(dir,-1.f))
        );
        if (Vector2DotProduct(simplex[simplex_vertices], dir) <= 0) {
            return false;
        }

        // p1 - newest vertex
        Vector2 p1_to_p2 = Vector2Subtract(simplex[simplex_vertices-1], simplex[simplex_vertices]);
        Vector2 p1_to_origin = Vector2Scale(simplex[simplex_vertices], -1);
        Vector3 p1_to_p2_perpendicular = Vector3CrossProduct(Vector3CrossProduct((Vector3){p1_to_p2.x,p1_to_p2.y,0}, (Vector3){p1_to_origin.x,p1_to_origin.y,0}),(Vector3){p1_to_p2.x,p1_to_p2.y,0});
        if(simplex_vertices<2) {
            dir = Vector2Normalize((Vector2){p1_to_p2_perpendicular.x,p1_to_p2_perpendicular.y});
            continue;
        }
        Vector2 p1_to_p3 = Vector2Subtract(simplex[0], simplex[simplex_vertices]);
        p1_to_p2_perpendicular = Vector3CrossProduct(Vector3CrossProduct((Vector3){p1_to_p3.x,p1_to_p3.y,0}, (Vector3){p1_to_p2.x,p1_to_p2.y,0}),(Vector3){p1_to_p2.x,p1_to_p2.y,0});
        Vector3 p1_to_p3_perpendicular = Vector3CrossProduct(Vector3CrossProduct((Vector3){p1_to_p2.x,p1_to_p2.y,0}, (Vector3){p1_to_p3.x,p1_to_p3.y,0}),(Vector3){p1_to_p3.x,p1_to_p3.y,0});

        if(Vector3DotProduct((Vector3){p1_to_origin.x, p1_to_origin.y,0}, p1_to_p2_perpendicular)>=0) {
            simplex[0]=simplex[1];
            dir=Vector2Normalize((Vector2){p1_to_p2_perpendicular.x, p1_to_p2_perpendicular.y});
        }else if(Vector3DotProduct((Vector3){p1_to_origin.x, p1_to_origin.y,0}, p1_to_p3_perpendicular)>=0) {
            dir=Vector2Normalize((Vector2){p1_to_p3_perpendicular.x, p1_to_p3_perpendicular.y});
        }else {
            memcpy(collider->simplex,simplex, sizeof(simplex));
            return true;
        }
        simplex[1]=simplex[2];
        simplex_vertices--;
    }
    return false;
}

void check_collisions_sys(ECS *ecs, entity_t entity_id) {
    C_Collider *collider = ecs_get_component(ecs, entity_id, C_Collider);
    C_Transform *transform = ecs_get_component(ecs, entity_id, C_Transform);

    collider->is_colliding =false;
    C_Collider *c_colliders = ecs_iter_components(ecs, C_Collider);
    for (C_Collider *collision = vec_begin(c_colliders); collision < vec_end(c_colliders); collision++) {
        if (collider == collision) {
            continue;
        }
        // Category Check
        // GJK Check
        if(check_gjk_collision(ecs, collider, collision)) {
            collider->is_colliding = true;
            //Vector2 pen_test = get_epa_penetration_vec(collider, transform);

            sds tag = ecs_get_tag(ecs, entity_id);
            if(tag) {
                if(strcmp(tag, "Player")==0) {
                    C_Debug *debug = ecs_get_component(ecs, entity_id, C_Debug);
                    Vector2 pen_test = get_epa_penetration_vec(collider, transform, debug);
                    //debug->pen_vec=pen_test;
                    printf("%d %d\n", pen_test.x, pen_test.y);
                    transform->position = Vector2Subtract(transform->position, pen_test);
                }
            }
            break;
        }
    }
}

void camera_follow_sys(ECS *ecs, entity_t entity_id) {
    C_Camera *c_camera = ecs_get_component(ecs, entity_id, C_Camera);
    size_t ind = sds_vector_find(ecs->tags, sdsnew(c_camera->following_tag), 0);
    C_Transform *to_follow = ecs_get_component(ecs, ecs_find_entity_with_tag(ecs, c_camera->following_tag), C_Transform);
    c_camera->camera.target = to_follow->position;
}

void spawn_enemy_sys(ECS *ecs, entity_t _) {
  if (IsKeyPressed(KEY_N)) {
        entity_t entity_ind = new_entity_with_tag(ecs, "Enemy");
        C_Renderer e_renderer = {(Color){rand() % 255, rand() % 255, rand() % 255, 255}, (Texture){0},false, RECT};
        ecs_add_component(ecs, entity_ind, C_Renderer, e_renderer);
        C_Transform e_transform = new_transform((Vector2){rand() % (GetScreenWidth()), rand() % GetScreenHeight()}, (Vector2){10, 10}, 200);
        ecs_add_component(ecs, entity_ind, C_Transform, e_transform);
        ecs_add_component(ecs, entity_ind, C_Collider,new_collider_rect(0, 0, 10, 10, 0, 0));
    }
}

void erase_entities_sys(ECS *ecs, entity_t _) {
    if (vec_size(ecs->entities_to_kill) > 0) {
        for (entity_t *entity = vec_begin(ecs->entities_to_kill);
            entity < vec_end(ecs->entities_to_kill); entity++) {
            __ecs_erase_entity(ecs, *entity);
        }
        vec_erase(ecs->entities_to_kill, 0, vec_size(ecs->entities_to_kill));
    }
}

int main(void) {
    srand(time(0));
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Game");

    // TODO: Resource Management
    Shader space_curvature_shd = LoadShader("resources/shaders/spacecurvature.fs", 0);
    int secondsLoc = GetShaderLocation(space_curvature_shd, "seconds");
    Texture player_texture = LoadTexture("resources/player_ship.png");

    char id_display_buf[64] = "";
    ECS *ecs = init_ecs();
    ecs_register_component(ecs, C_Transform);
    ecs_register_component(ecs, C_Renderer);
    ecs_register_component(ecs, C_Collider);
    ecs_register_component(ecs, C_Camera);
    ecs_register_component(ecs, C_Debug);

    // Player Definition
    entity_t player_id = new_entity_with_tag(ecs, "Player");
    C_Transform player_transform = new_transform((Vector2){20, 20}, (Vector2){60, 60}, 300.f);
    ecs_add_component(ecs, player_id, C_Transform, player_transform);
    ecs_add_component(ecs, player_id, C_Renderer, {WHITE, player_texture, true, RECT});
    //ecs_add_component(ecs, player_id, C_Collider, new_collider_circle(15.f, 15.f, 20.f, 0, 0));
    ecs_add_component(ecs, player_id, C_Collider, new_collider_rect(0, 0, 30, 30, 0,0));
    ecs_add_component(ecs, player_id, C_Debug, {(Vector2){0}});
    // End Of Player Definition
    
    entity_t static_e = new_entity(ecs);
    C_Transform static_t = new_transform((Vector2){120, 20}, (Vector2){100, 100}, 0.f);
    ecs_add_component(ecs, static_e, C_Transform, static_t);
    ecs_add_component(ecs, static_e, C_Renderer, {RED, (Texture){0}, false, RECT});
    ecs_add_component(ecs, static_e, C_Collider, new_collider_rect(0, 0, 100, 100, 0, 0));

    entity_t camera_id = new_entity_with_tag(ecs, "Main Camera");
    Camera2D camera = {
        (Vector2){(GetScreenWidth() / 2.f) - player_transform.size.x / 2, 
        GetScreenHeight() / 2.f - player_transform.size.y / 2},
        (Vector2){0, 0}, 0.f, 1.f
    };
    ecs_add_component(ecs, camera_id, C_Camera, {camera, "Player"});

    ecs_register_system(ecs, ON_PREUPDATE, erase_entities_sys);
    ecs_register_component_system(ecs, ON_UPDATE, apply_velocity_sys, C_Transform);
    ecs_register_component_system(ecs, ON_UPDATE, check_collisions_sys, C_Transform, C_Collider);
    ecs_register_component_system(ecs, ON_UPDATE, camera_follow_sys, C_Camera);
    ecs_register_tag_system(ecs, ON_UPDATE, player_movement_sys, "Player");
    ecs_register_tag_system(ecs, ON_UPDATE, enemy_ai_sys, "Enemy");
    ecs_register_system(ecs, ON_UPDATE, spawn_enemy_sys);
    ecs_register_component_system(ecs, ON_DRAW, draw_entity_sys, C_Renderer, C_Transform);
    ecs_register_component_system(ecs, ON_DRAW, draw_colliders_debug_sys,C_Transform, C_Collider); 

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
        for (C_Transform *transform = vec_begin(c_transforms);
            transform < vec_end(c_transforms); transform++) {
            if (CheckCollisionPointRec(mousePos, (Rectangle){transform->position.x, transform->position.y, transform->size.x, transform->size.y})) {
                selected_entity = ecs_get_entity_id(ecs, C_Transform, transform);
            }
        }
        if (IsMouseButtonPressed(0) && selected_entity != -1) {
            kill_entity(ecs, selected_entity);
        }

        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode2D(c_camera->camera);
            ecs_call_system(ecs, ON_DRAW);

            // DELETE  DEBUG
            entity_t player_id = ecs_find_entity_with_tag(ecs, "Player");
            C_Debug *player_debug = ecs_get_component(ecs, player_id, C_Debug);
            C_Transform *player_transform= ecs_get_component(ecs, player_id, C_Transform);
            C_Collider *player_collider= ecs_get_component(ecs, player_id, C_Collider);

            DrawCircleV((Vector2){0,0}, 5.f, WHITE);
            DrawLineV(player_collider->simplex[0], player_collider->simplex[1], PURPLE);
            DrawLineV(player_collider->simplex[1], player_collider->simplex[2], PURPLE);
            DrawLineV(player_collider->simplex[2], player_collider->simplex[0], PURPLE);

            DrawLineV(player_debug->start, Vector2Add(player_debug->start,player_debug->end), GREEN);
            Vector2 mid =Vector2Add(player_debug->start,Vector2Scale(player_debug->end,0.5f));
            DrawLineV((Vector2){0,0}, player_debug->pen_vec, DARKGREEN);

        EndMode2D();

        // BeginShaderMode(space_curvature_shd);
        // EndShaderMode();

        // ID Display
        if (selected_entity != -1) {
            C_Transform *t = ecs_get_component(ecs, selected_entity, C_Transform);
            sprintf(id_display_buf, "ID: %d, GEN: 0, X: %6.2f, Y: %6.2f", selected_entity, t->position.x, t->position.y);
        }
        DrawText(id_display_buf, GetScreenWidth() - 300, 20, 16, WHITE);
        EndDrawing();
    }
    free_ecs(ecs);
    CloseWindow();
    return 0;
}
