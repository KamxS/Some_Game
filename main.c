#include "raylib.h"
#include "raymath.h"
#include "vector.h"
#include "hashmap.h"
#include "sds.h"
#include "sdsalloc.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_COMPONENTS 32
#define MAX_ENTITIES 5000

typedef uint32_t entity_t;
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

typedef struct ECS {
    struct hashmap *components;
    uint32_t *signatures;
    entity_t *free_ids;
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
    vec_free(ecs->free_ids);
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

// TODO: Not the biggest fan of this
#define ecs_add_component(ecs, entity_id, component, component_init) \
    do {\
        ComponentVec *cvec = __ecs_get_component_vec(ecs, component);\
        size_t ind = vec_size(cvec->data);\
        cvec->entity_to_ind[__ecs_get_id(entity_id)] = ind;\
        cvec->ind_to_entity[ind]=__ecs_get_id(entity_id);\
        component *vec = cvec->data;\
        bool update = false;\
        if(vec_size(vec) >= vec_capacity(vec)) update=true;\
        component n_component = component_init;\
        vec_push(vec, n_component);\
        if(update) {cvec->data=vec;hashmap_set(ecs->components, &(struct component_kv){ #component, *cvec });}\
        ecs->signatures[__ecs_get_id(entity_id)] |= cvec->signature;\
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

entity_t __ecs_get_entity_id(ECS *ecs, ComponentVec *cvec, void *component_ptr) {
    return cvec->ind_to_entity[(component_ptr-cvec->data)/cvec->size_of_component];
}
#define ecs_get_entity_id(ecs, component_type, component_ptr) __ecs_get_entity_id(ecs, __ecs_get_component_vec(ecs,component_type), component_ptr)

void kill_entity(ECS *ecs, entity_t entity_id) {
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

/* TODO LIST */
/*
    - Better Pause
    - Generations
    - Collisions
    - Better System Management
    - Entity Management
    - Gameplay
*/

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 450;
    bool paused = false;

    srand(time(0));
    InitWindow(screenWidth, screenHeight, "Game");
    Camera2D camera = {(Vector2){0,0}, (Vector2){0,0}, 0.f, 1.f};
    char id_display_buf[64] = "";

    ECS *ecs = init_ecs();
    ecs_register_component(ecs, C_Transform);
    ecs_register_component(ecs, C_Renderer);
    ecs_register_component(ecs, C_Collider);

    entity_t player_id = new_entity_with_tag(ecs, "Player");
    ecs_add_component(ecs, player_id, C_Transform, new_transform((Vector2){20,20}, (Vector2){60,60}, 300.f));
    ecs_add_component(ecs, player_id, C_Renderer, {WHITE});
    ecs_add_component(ecs, player_id, C_Collider, new_collider(0, 0, 60, 60, 0, 0));
    Vector2 player_dir = {0};

    entity_t *entities_to_kill = NULL;
    vec_init(entities_to_kill, 16);
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        // Kill Entities
        if(vec_size(entities_to_kill)>0) {
            for(entity_t *entity=vec_begin(entities_to_kill);entity<vec_end(entities_to_kill);entity++) {
                kill_entity(ecs, *entity);
            }
            vec_erase(entities_to_kill, 0, vec_size(entities_to_kill));
        }

        // TODO: First (and hopefully last) time using goto's 
        if(paused) goto drawing;

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
        C_Transform *player_transform = ecs_get_component(ecs, player_id, C_Transform);
        player_transform->velocity = Vector2Scale(Vector2Normalize(player_dir), player_transform->speed);

        if(IsKeyPressed(KEY_N)) {
            entity_t entity_ind = new_entity_with_tag(ecs, "Enemy");
            C_Renderer e_renderer = new_renderer((Color){rand()%255, rand()%255, rand()%255, 255});
            ecs_add_component(ecs, entity_ind, C_Renderer, e_renderer);
            C_Transform e_transform = new_transform((Vector2){rand()%(screenWidth), rand()%screenHeight}, (Vector2){10,10}, 200);
            ecs_add_component(ecs, entity_ind, C_Transform, e_transform);
            ecs_add_component(ecs, entity_ind, C_Collider, new_collider(0, 0, 10, 10, 0, 0));
        }

        C_Transform *c_transforms = ecs_iter_components(ecs, C_Transform);
        for(C_Transform* transform=vec_begin(c_transforms); transform<vec_end(c_transforms); transform++) {
            entity_t entity_id = ecs_get_entity_id(ecs, C_Transform, transform);
            // Enemy "AI"
            if(strcmp(ecs_get_tag(ecs, entity_id), "Enemy") == 0) {
                Vector2 dir = Vector2Normalize(Vector2Subtract(player_transform->position, transform->position));
                transform->velocity = Vector2Scale(dir, transform->speed);
            }
            // Velocity 
            transform->position = Vector2Add(transform->position, Vector2Scale(transform->velocity, GetFrameTime()));
        }

        // Collisions
        C_Collider *c_colliders = ecs_iter_components(ecs, C_Collider);
        for(C_Collider* collider=vec_begin(c_colliders); collider<vec_end(c_colliders); collider++) {
            bool is_colliding = false;
            for(C_Collider* collision=vec_begin(c_colliders); collision<vec_end(c_colliders); collision++) {
                if(collider==collision) continue;
                // Category Check
                // Bounding Box Check
                C_Transform *collider_transform = ecs_get_component(ecs, ecs_get_entity_id(ecs, C_Collider, collider), C_Transform);
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

// TODO: Remove This!!
drawing:
        if(IsKeyPressed(KEY_P)) {
            paused = !paused;
        }

        // ID Display + Entity by Click Kill
        Vector2 mousePos = GetScreenToWorld2D(GetMousePosition(), camera);
        size_t selected_entity = -1;
        for(size_t transform_ind =0; transform_ind<vec_size(c_transforms); transform_ind++) {
            C_Transform transform = c_transforms[transform_ind];
            if(CheckCollisionPointRec(mousePos, (Rectangle){transform.position.x, transform.position.y, transform.size.x, transform.size.y})) {
                selected_entity = __ecs_get_component_vec(ecs, C_Transform)->ind_to_entity[transform_ind];
            }
        }
        if(IsMouseButtonPressed(0) && selected_entity!=-1) {
            vec_push(entities_to_kill, selected_entity);
        }

        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode2D(camera);
            C_Renderer *c_renderers = ecs_iter_components(ecs, C_Renderer);
            for(C_Renderer *renderer=c_renderers;renderer<vec_end(c_renderers);renderer++){
                entity_t entity_id = ecs_get_entity_id(ecs, C_Renderer, renderer);
                C_Transform *transform = ecs_get_component(ecs, entity_id, C_Transform);

                C_Collider *collider= ecs_get_component(ecs, entity_id, C_Collider);
                Color c = WHITE;
                if(collider->is_colliding) {
                    c = RED;
                }
                DrawRectangleLines(transform->position.x, transform->position.y, transform->size.x, transform->size.y, c);
                //DrawRectangleV(transform->position, transform->size, renderer->color);
            }
            EndMode2D();
            // ID Display
            if(selected_entity!=-1) sprintf(id_display_buf, "ID: %d, GEN: 0", selected_entity);
            DrawText(id_display_buf, screenWidth-140, 20, 16, WHITE);
        EndDrawing();
    }
    free_ecs(ecs);
    vec_free(entities_to_kill);
    CloseWindow();
    return 0;
}
