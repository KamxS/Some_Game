#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "include/kxecs.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_COMPONENTS 32
#define MAX_ENTITIES 5000

typedef uint32_t entity_t;
struct ECS;

typedef struct ComponentVec {
    void *data;
    size_t size_of_component;
    size_t *entity_to_ind;    
    size_t *ind_to_entity;
    uint32_t signature;
} ComponentVec;

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

typedef struct C_Camera {
    Camera2D camera;
    char* following_tag;
} C_Camera;

typedef void (*component_func_t)(struct ECS*, entity_t);
typedef struct SystemCallback {
    component_func_t callback;
    uint32_t entity_mask;
    sds *tags;
} SystemCallback;

#define NUM_OF_SYSTEM_TYPES 4
enum system_type {
    ON_START,
    ON_PREUPDATE,
    ON_UPDATE,
    ON_DRAW
};

typedef struct ECS {
    struct hashmap *components;
    uint32_t *signatures;
    entity_t *entities_to_spawn;
    entity_t *entities_to_kill;
    entity_t *free_ids;
    sds *tags;
    struct SystemCallback *systems[NUM_OF_SYSTEM_TYPES];

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

struct component_kv {
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

    uint32_t *signatures = NULL;
    vec_init(signatures, MAX_ENTITIES);
    ecs->signatures = signatures;

    sds *tags = NULL;
    vec_init(tags, MAX_ENTITIES);
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

#define ecs_register_component(ecs, component)\
    do { \
        ComponentVec cvec = {0};\
        component *vec = NULL;\
        vec_init(vec, 16);\
        cvec.data = vec;\
        cvec.size_of_component = sizeof(component);\
        vec_init(cvec.entity_to_ind, MAX_ENTITIES);\
        vec_init(cvec.ind_to_entity, MAX_ENTITIES);\
        cvec.signature = 1<<ecs->number_of_components;\
        hashmap_set(ecs->components, &(struct component_kv){ #component, cvec }); \
        ecs->number_of_components++;\
    }while(0)

#define __ecs_get_id(entity_id) (entity_id & 0x0FFF)
//#define __ecs_get_generation(entity_id) ((entity_id & 0xF000)>>24)

void __link_entity_with_component(ECS *ecs, ComponentVec *cvec, entity_t entity_id, size_t component) {
    cvec->entity_to_ind[__ecs_get_id(entity_id)] = component;\
    cvec->ind_to_entity[component]=__ecs_get_id(entity_id);\
    ecs->signatures[__ecs_get_id(entity_id)] |= cvec->signature;
}
// TODO: Not the biggest fan of this
#define ecs_add_component(ecs, entity_id, component, ...) \
    do {\
        ComponentVec *cvec = __ecs_get_component_vec(ecs, component);\
        __link_entity_with_component(ecs, cvec, entity_id, vec_size(cvec->data));\
        component *vec = cvec->data;\
        bool update = false;\
        if(vec_size(vec) >= vec_capacity(vec)) update=true;\
        component n_component = __VA_ARGS__;\
        vec_push(vec, n_component);\
        if(update) {cvec->data=vec;hashmap_set(ecs->components, &(struct component_kv){ #component, *cvec });}\
    }while(0)

#define ecs_get_component(ecs, entity_id, component)\
    &(((component*)__ecs_get_component_vec(ecs, component)->data)[__ecs_get_component_vec(ecs, component)->entity_to_ind[__ecs_get_id(entity_id)]]);

#define __ecs_get_component_vec(ecs, component) \
    ((ComponentVec*)&(((struct component_kv*)hashmap_get(ecs->components, &(struct component_kv){.name=#component}))->component_vec))

#define ecs_get_component_signature(ecs, component) __ecs_get_component_vec(ecs,component)->signature
#define ecs_get_signature(ecs, entity_id) ecs->signatures[__ecs_get_id(entity_id)]
#define ecs_get_tag(ecs, entity_id) ecs->tags[__ecs_get_id(entity_id)]

#define ecs_iter_components(ecs, component)\
    (component*)((ComponentVec)((struct component_kv*)hashmap_get(ecs->components, &(struct component_kv){.name=#component}))->component_vec).data

#define __component_to_signature(ecs, component) __ecs_get_component_vec(ecs, component)->signature
#define __bitor_component_signatures_1(ecs, component) __component_to_signature(ecs, component)
#define __bitor_component_signatures_2(ecs, component, ...) __component_to_signature(ecs, component) | __bitor_component_signatures_1(ecs, __VA_ARGS__)
#define __bitor_component_signatures_3(ecs, component, ...) __component_to_signature(ecs, component) | __bitor_component_signatures_2(ecs, __VA_ARGS__)
#define __bitor_component_signatures_4(ecs, component, ...) __component_to_signature(ecs, component) | __bitor_component_signatures_3(ecs, __VA_ARGS__)
#define __bitor_component_signatures_5(ecs, component, ...) __component_to_signature(ecs, component) | __bitor_component_signatures_4(ecs, __VA_ARGS__)
#define __choose_correct_bitor(_1,_2,_3,_4,_5,name,...) name

#define ecs_foreach_entity(ecs, function, ...)\
    do {\
        uint32_t mask = __choose_correct_bitor(__VA_ARGS__, __bitor_component_signatures_5, __bitor_component_signatures_4, __bitor_component_signatures_3, __bitor_component_signatures_2, __bitor_component_signatures_1)(ecs, __VA_ARGS__);\
        for(size_t n=0;n<MAX_ENTITIES; n++) {\
            if((ecs->signatures[n] & mask) == mask) {\
                function(ecs, n);\
            }\
        }\
    }while(0)

#define ecs_register_system(ecs, type, function) \
    do {\
        SystemCallback callback = {function, 0, NULL};\
        vec_push(ecs->systems[type],callback);\
    }while(0)

#define ecs_register_component_system(ecs, type, function, ...)\
    do {\
        uint32_t mask = __choose_correct_bitor(__VA_ARGS__, __bitor_component_signatures_5, __bitor_component_signatures_4, __bitor_component_signatures_3, __bitor_component_signatures_2, __bitor_component_signatures_1)(ecs, __VA_ARGS__);\
        SystemCallback callback = {function, mask, NULL};\
        vec_push(ecs->systems[type],callback);\
    }while(0)

#define ecs_register_tag_system(ecs, type, function, ...)\
    do {\
        char* tags[] = {__VA_ARGS__};\
        sds *stags = NULL;\
        vec_init(stags, 8);\
        SystemCallback callback = {function, 0};\
        for(size_t ind=0;ind<sizeof(tags)/sizeof(tags[0]);ind++) {\
            sds tag=sdsnew(tags[ind]);\
            vec_push(stags, tag);\
        }\
        callback.tags = stags;\
        vec_push(ecs->systems[type],callback);\
    }while(0)

entity_t __ecs_get_entity_id(ECS *ecs, ComponentVec *cvec, void *component_ptr) {
    return cvec->ind_to_entity[(component_ptr-cvec->data)/cvec->size_of_component];
}

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

#define ecs_get_entity_id(ecs, component_type, component_ptr) __ecs_get_entity_id(ecs, __ecs_get_component_vec(ecs,component_type), component_ptr)
size_t sds_vector_find(sds *vec, sds value, size_t start) {
    for(size_t ind=start;ind<vec_size(vec);ind++) {
        if(strcmp(vec[ind],value)==0) {
            return ind;
        }
    }
    return -1;
}
#define ecs_find_entity_with_tag(ecs, tag) sds_vector_find(ecs->tags, tag, 0)

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
    sdsclear(ecs->tags[__ecs_get_id(entity_id)]);
    ecs->number_of_entities--;
    vec_push(ecs->free_ids, entity_id);
}
#define kill_entity(ecs, entity_id) vec_push(ecs->entities_to_kill, entity_id)

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
    C_Transform *transform = ecs_get_component(ecs, entity_id, C_Transform);
    C_Collider *collider= ecs_get_component(ecs, entity_id, C_Collider);
    Color c = WHITE;
    if(collider->is_colliding) {
        c = RED;
    }
    DrawRectangleLines(transform->position.x, transform->position.y, transform->size.x, transform->size.y, c);
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

void erase_entities_sys(ECS *ecs, entity_t entity_id) {
    if(vec_size(ecs->entities_to_kill)>0) {
        for(entity_t *entity=vec_begin(ecs->entities_to_kill);entity<vec_end(ecs->entities_to_kill);entity++) {
            __ecs_erase_entity(ecs, *entity);
        }
        vec_erase(ecs->entities_to_kill, 0, vec_size(ecs->entities_to_kill));
    }
}

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 450;

    srand(time(0));
    InitWindow(screenWidth, screenHeight, "Game");
    Shader space_curvature_shd = LoadShader("resources/shaders/spacecurvature.fs",0);
    int secondsLoc = GetShaderLocation(space_curvature_shd, "seconds");

    char id_display_buf[64] = "";
    ECS *ecs = init_ecs();
    ecs_register_component(ecs, C_Transform);
    ecs_register_component(ecs, C_Renderer);
    ecs_register_component(ecs, C_Collider);
    ecs_register_component(ecs, C_Camera);

    entity_t player_id = new_entity_with_tag(ecs, "Player");
    C_Transform player_transform = new_transform((Vector2){20,20}, (Vector2){60,60}, 300.f);
    ecs_add_component(ecs, player_id, C_Transform, player_transform);
    ecs_add_component(ecs, player_id, C_Renderer, {WHITE});
    ecs_add_component(ecs, player_id, C_Collider, new_collider(0, 0, 60, 60, 0, 0));

    entity_t camera_id = new_entity_with_tag(ecs, "Main Camera");
    Camera2D camera = {(Vector2){(screenWidth/2)-player_transform.size.x/2,screenHeight/2-player_transform.size.y/2}, (Vector2){0,0}, 0.f, 1.f};
    ecs_add_component(ecs, camera_id, C_Camera, {camera, "Player"}); 
    
    ecs_register_system(ecs, ON_PREUPDATE, erase_entities_sys); 

    ecs_register_component_system(ecs, ON_UPDATE, apply_velocity_sys, C_Transform);  
    ecs_register_component_system(ecs, ON_UPDATE, check_collisions_sys, C_Transform, C_Collider);  
    ecs_register_component_system(ecs, ON_UPDATE, camera_follow_sys, C_Camera);  
    ecs_register_tag_system(ecs, ON_UPDATE, player_movement_sys, "Player");
    ecs_register_tag_system(ecs, ON_UPDATE, enemy_ai_sys, "Enemy");

    float seconds = 0.f;
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        seconds += GetFrameTime();
        SetShaderValue(space_curvature_shd, secondsLoc, &seconds, SHADER_UNIFORM_FLOAT);

        ecs_call_system(ecs, ON_PREUPDATE);
        // Kill Entities
        /*
        if(vec_size(entities_to_kill)>0) {
            for(entity_t *entity=vec_begin(entities_to_kill);entity<vec_end(entities_to_kill);entity++) {
                __ecs_erase_entity(ecs, *entity);
            }
            vec_erase(entities_to_kill, 0, vec_size(entities_to_kill));
        }
        */

        // Spawn Entities
        if(IsKeyPressed(KEY_N)) {
            entity_t entity_ind = new_entity_with_tag(ecs, "Enemy");
            C_Renderer e_renderer = new_renderer((Color){rand()%255, rand()%255, rand()%255, 255});
            ecs_add_component(ecs, entity_ind, C_Renderer, e_renderer);
            C_Transform e_transform = new_transform((Vector2){rand()%(screenWidth), rand()%screenHeight}, (Vector2){10,10}, 200);
            ecs_add_component(ecs, entity_ind, C_Transform, e_transform);
            ecs_add_component(ecs, entity_ind, C_Collider, new_collider(0, 0, 10, 10, 0, 0));
        }

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
            ecs_foreach_entity(ecs, draw_entity_sys, C_Transform, C_Collider); 
            EndMode2D();

            //BeginShaderMode(space_curvature_shd);
            //EndShaderMode();

            // ID Display
            if(selected_entity!=-1) sprintf(id_display_buf, "ID: %d, GEN: 0", selected_entity);
            DrawText(id_display_buf, screenWidth-140, 20, 16, WHITE);
        EndDrawing();
    }
    free_ecs(ecs);
    CloseWindow();
    return 0;
}
