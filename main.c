#include "include/kxecs.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 450

#define MAX_DOT_PRODUCT(v1,v2, dir) \
    Vector2DotProduct(v1,dir)>Vector2DotProduct(v2,dir) ? v1 : v2

typedef struct C_Transform {
  Vector2 position;
  Vector2 size;
  float speed;
  Vector2 velocity;
} C_Transform;

enum C_Collider_Type { COLLIDER_RECT, COLLIDER_CIRCLE };

typedef struct C_Collider {
  union {
    struct {
      float offset_x;
      float offset_y;
      float width;
      float height;
    } as_rect;
    struct {
      float offset_x;
      float offset_y;
      float radius;
    } as_circle;
  };
  enum C_Collider_Type collider_t;
  long layer;
  long layer_mask;
  bool is_colliding;

  // GJK DEBUG
  Vector2 gjk_points[20];
} C_Collider;

typedef struct C_Renderer {
  Color color;
  Texture texture;
  bool has_texture;
} C_Renderer;

typedef struct C_Camera {
  Camera2D camera;
  char *following_tag;
} C_Camera;

C_Transform new_transform(Vector2 position, Vector2 size, float speed) {
  return (C_Transform){position, size, speed, (Vector2){0, 0}};
}

C_Collider new_collider_circle(float cx, float cy, float radius, long collides_with, long collision_category) {
    printf("[TODO] Implement circle colliders!\n");
}
C_Collider new_collider_rect(float x, float y, float width, float height, long layer, long layer_mask) {
    C_Collider nc = {0};
    nc.as_rect.offset_x = x;
    nc.as_rect.offset_y = y;
    nc.as_rect.width = width;
    nc.as_rect.height = height;
    nc.collider_t = COLLIDER_RECT;
    nc.layer = layer;
    nc.layer_mask = layer_mask;
    return nc;
}
/* TODO LIST */
/*
    - Cleanup
    - Pause
    - Generations
    - Collisions
    -
   https://www.researchgate.net/publication/228574502_How_to_implement_a_pressure_soft_body_model
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
  C_Transform *player_transform = ecs_get_component(
      ecs, ecs_find_entity_with_tag(ecs, "Player"), C_Transform);
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
    DrawRectangleV(transform->position, transform->size, renderer->color);
  }
}

Vector2 furthest_point_in_rect(Rectangle rect, Vector2 direction) {
    return MAX_DOT_PRODUCT(
        MAX_DOT_PRODUCT(
            ((Vector2){rect.x, rect.y}),
            ((Vector2){rect.x+rect.width, rect.y}), 
            direction
        ),
        MAX_DOT_PRODUCT(
            ((Vector2){rect.x, rect.y+rect.height}),
            ((Vector2){rect.x+rect.width, rect.y+rect.height}),
            direction
        ),
        direction
    ); 
}

Vector2 support_function_rect(Rectangle rect, Rectangle rect2,  Vector2 direction) {
    return Vector2Subtract(furthest_point_in_rect(rect, direction),furthest_point_in_rect(rect2, Vector2Scale(direction,-1)));
}

Vector2 support_function_circle(Vector2 center, float radius, Vector2 direction) {}

bool check_gjk_collision(ECS *ecs, C_Collider *collider, C_Collider* collision) {
    size_t simplex_vertices = 0;
    Vector2 dir = (Vector2){1,1};
    C_Transform *collider_transform = ecs_get_component(ecs, ecs_get_entity_id(ecs, C_Collider, collider), C_Transform);
    C_Transform *collision_transform = ecs_get_component(ecs, ecs_get_entity_id(ecs, C_Collider, collision), C_Transform);

    Vector2 simplex[3] = {0};
    simplex[simplex_vertices] = support_function_rect(
        (Rectangle){collider_transform->position.x, collider_transform->position.y, collider_transform->size.x, collider_transform->size.y}, 
        (Rectangle){collision_transform->position.x, collision_transform->position.y, collision_transform->size.x, collision_transform->size.y}, 
        dir
    );
    collider->gjk_points[0] = simplex[0];
    dir = Vector2Scale(simplex[0],-1);

    while(true) {
        simplex[++simplex_vertices] = support_function_rect(
            (Rectangle){collider_transform->position.x, collider_transform->position.y, collider_transform->size.x, collider_transform->size.y}, 
            (Rectangle){collision_transform->position.x, collision_transform->position.y, collision_transform->size.x, collision_transform->size.y}, 
            dir
        );
        collider->gjk_points[simplex_vertices] = simplex[simplex_vertices];
        if (Vector2DotProduct(simplex[simplex_vertices], dir) <= 0) return false;

        // p1 - newest vertex
        Vector2 p1_to_p2 = Vector2Subtract(simplex[simplex_vertices-1], simplex[simplex_vertices]);
        Vector2 p1_to_origin = Vector2Scale(simplex[simplex_vertices], -1);
        Vector3 p1_to_p2_perpendicular = Vector3CrossProduct(Vector3CrossProduct((Vector3){p1_to_p2.x,p1_to_p2.y,0}, (Vector3){p1_to_origin.x,p1_to_origin.y,0}),(Vector3){p1_to_p2.x,p1_to_p2.y,0});
        if(simplex_vertices==1) {
            dir = (Vector2){p1_to_p2_perpendicular.x,p1_to_p2_perpendicular.y};
            continue;
        }
        Vector2 p1_to_p3 = Vector2Subtract(simplex[0], simplex[simplex_vertices]);
        p1_to_p2_perpendicular = Vector3CrossProduct(Vector3CrossProduct((Vector3){p1_to_p3.x,p1_to_p3.y,0}, (Vector3){p1_to_p2.x,p1_to_p2.y,0}),(Vector3){p1_to_p2.x,p1_to_p2.y,0});
        Vector3 p1_to_p3_perpendicular = Vector3CrossProduct(Vector3CrossProduct((Vector3){p1_to_p2.x,p1_to_p2.y,0}, (Vector3){p1_to_p3.x,p1_to_p3.y,0}),(Vector3){p1_to_p3.x,p1_to_p3.y,0});

        if(Vector3DotProduct((Vector3){p1_to_origin.x, p1_to_origin.y,0}, p1_to_p2_perpendicular)>=0) {
            simplex[0]=simplex[1];
            dir=(Vector2){p1_to_p2_perpendicular.x, p1_to_p2_perpendicular.y};
        }else if(Vector3DotProduct((Vector3){p1_to_origin.x, p1_to_origin.y,0}, p1_to_p3_perpendicular)>=0) {
            dir=(Vector2){p1_to_p3_perpendicular.x, p1_to_p3_perpendicular.y};
        }else {
            return true;
        }
        simplex[1]=simplex[2];
        simplex_vertices--;
        collider->gjk_points[0]=simplex[0];
        collider->gjk_points[1]=simplex[1];
    }
    return false;
}

void check_collisions_sys(ECS *ecs, entity_t entity_id) {
    C_Collider *collider = ecs_get_component(ecs, entity_id, C_Collider);

    C_Collider *c_colliders = ecs_iter_components(ecs, C_Collider);
    for (C_Collider *collision = vec_begin(c_colliders); collision < vec_end(c_colliders); collision++) {
        if (collider == collision) {
            continue;
        }
        // Category Check
        // GJK Check
        if(check_gjk_collision(ecs, collider, collision)) printf("COLLISION!\n");
        break;
    }
}

void display_gjk_sys(ECS *ecs, entity_t entity_id) {
    C_Collider *collider = ecs_get_component(ecs, entity_id, C_Collider);
    C_Transform *collider_transform = ecs_get_component(ecs, entity_id, C_Transform);
    DrawCircleV((Vector2){0,0}, 5.f, WHITE);

    DrawLineV(collider->gjk_points[0], collider->gjk_points[1], BROWN);
    DrawLineV(collider->gjk_points[1], collider->gjk_points[2], BROWN);
    DrawLineV(collider->gjk_points[2], collider->gjk_points[0], BROWN);
}

void camera_follow_sys(ECS *ecs, entity_t entity_id) {
    C_Camera *c_camera = ecs_get_component(ecs, entity_id, C_Camera);
    C_Transform *to_follow = ecs_get_component(ecs, ecs_find_entity_with_tag(ecs, c_camera->following_tag), C_Transform);
    c_camera->camera.target = to_follow->position;
}

void spawn_enemy_sys(ECS *ecs, entity_t _) {
  if (IsKeyPressed(KEY_N)) {
        entity_t entity_ind = new_entity_with_tag(ecs, "Enemy");
        C_Renderer e_renderer = {(Color){rand() % 255, rand() % 255, rand() % 255, 255}, (Texture){0},false};
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

  // Player Definition
  entity_t player_id = new_entity_with_tag(ecs, "Player");
  C_Transform player_transform = new_transform((Vector2){20, 20}, (Vector2){60, 60}, 300.f);
  ecs_add_component(ecs, player_id, C_Transform, player_transform);
  //ecs_add_component(ecs, player_id, C_Renderer, {WHITE, player_texture, true});
  //ecs_add_component(ecs, player_id, C_Collider, new_collider_rect(0, 0, 60, 60, 0, 0));
  // End Of Player Definition

  entity_t camera_id = new_entity_with_tag(ecs, "Main Camera");
  Camera2D camera = {
      (Vector2){(GetScreenWidth() / 2) - player_transform.size.x / 2,
                GetScreenHeight() / 2 - player_transform.size.y / 2},
      (Vector2){0, 0}, 0.f, .5f};
  ecs_add_component(ecs, camera_id, C_Camera, {camera, "Player"});

  entity_t rect = new_entity_with_tag(ecs, "Player_Test");
  ecs_add_component(ecs, rect, C_Transform, new_transform((Vector2){-230,100}, (Vector2){50,50}, 300.f));
  ecs_add_component(ecs, rect, C_Renderer, {RED, (Texture){0}, false});
  ecs_add_component(ecs, rect, C_Collider,new_collider_rect(0, 0, 50, 50, 0, 0)); 
  entity_t circle = new_entity(ecs);
  ecs_add_component(ecs, circle, C_Transform, new_transform((Vector2){120,3}, (Vector2){100,100}, 0));
  ecs_add_component(ecs, circle, C_Renderer, {BLUE, (Texture){0}, false});
  ecs_add_component(ecs, circle, C_Collider,new_collider_rect(0, 0, 100, 100, 0, 0)); 

  ecs_register_system(ecs, ON_PREUPDATE, erase_entities_sys);
  ecs_register_component_system(ecs, ON_UPDATE, apply_velocity_sys, C_Transform);
  //ecs_register_component_system(ecs, ON_UPDATE, check_collisions_sys, C_Transform, C_Collider);
  ecs_register_tag_system(ecs, ON_UPDATE, check_collisions_sys, "Player_Test");
  ecs_register_component_system(ecs, ON_UPDATE, camera_follow_sys, C_Camera);
  ecs_register_tag_system(ecs, ON_UPDATE, player_movement_sys, "Player_Test");
  //ecs_register_tag_system(ecs, ON_UPDATE, enemy_ai_sys, "Enemy");
  //ecs_register_system(ecs, ON_UPDATE, spawn_enemy_sys);

  ecs_register_component_system(ecs, ON_DRAW, draw_entity_sys, C_Renderer, C_Transform);
  ecs_register_tag_system(ecs, ON_DRAW, display_gjk_sys, "Player_Test");

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
    EndMode2D();

    // BeginShaderMode(space_curvature_shd);
    // EndShaderMode();

    // ID Display
    if (selected_entity != -1)
      sprintf(id_display_buf, "ID: %d, GEN: 0", selected_entity);
    DrawText(id_display_buf, GetScreenWidth() - 140, 20, 16, WHITE);
    EndDrawing();
  }
  free_ecs(ecs);
  CloseWindow();
  return 0;
}
