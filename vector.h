#ifndef VECTOR_H
#define VECTOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct _vec_metadata {
    // destructor_ptr?
    size_t size;
    size_t capacity;
} _vec_metadata;

#define vec_get_base(vec) ((_vec_metadata*)((void*)vec-sizeof(_vec_metadata)))
#define vec_get_data_ptr(vec_meta) ((void*)vec_meta+sizeof(_vec_metadata))

#define vec_init(vec, initial_count)                                        \
    if(!vec) {                                                              \
        (vec) = calloc(1, sizeof(_vec_metadata)+sizeof(*vec)*initial_count);\
        ((_vec_metadata*)(vec))->size = 0;                                  \
        ((_vec_metadata*)(vec))->capacity = initial_count;                  \
        (vec) = vec_get_data_ptr(vec);                                      \
    }

#define vec_grow(vec, new_count)                                                            \
    do {                                                                                    \
        if(vec && vec_capacity(vec)<new_count) {                                            \
            void *metadata = vec_get_base(vec);                                             \
            _vec_metadata* new_vec = realloc(metadata, sizeof(_vec_metadata)+sizeof(*vec)*new_count);\
            new_vec->capacity = new_count;                                                  \
            (vec) = vec_get_data_ptr(new_vec);                                              \
        }                                                                                   \
    }while(0)

    
#define vec_free(vec) \
    do { free(vec_get_base(vec)); } while(0)

#define vec_push(vec, value)                                                                            \
do{                                                                                                     \
    _vec_metadata* vec_base = vec_get_base(vec);                                                        \
    if(vec_base->size >= vec_base->capacity) {                                                          \
        _vec_metadata* new_vec = realloc(vec_base, sizeof(_vec_metadata)+sizeof(*vec)*vec_base->size*2);\
        vec_base = new_vec;                                                                             \
        vec_base->capacity*=2;                                                                          \
        vec = vec_get_data_ptr(new_vec);                                                                \
    }                                                                                                   \
    vec[vec_base->size] = value;                                                                        \
    vec_base->size++;                                                                                   \
}while(0)

#define vec_pop(vec) \
    do{if(vec_get_base(vec)->size>0) vec_get_base(vec)->size--;} while(0)

#define vec_size(vec) (vec_get_base(vec)->size)
#define vec_capacity(vec) (vec_get_base(vec)->capacity)

#define vec_begin(vec) (vec)
#define vec_end(vec) (&vec[vec_size(vec)])

#define vec_erase(vec, ind, length)                                                             \
    do {                                                                                        \
        if(vec) {                                                                               \
            if(ind<vec_size(vec)) {                                                             \
                memmove(&vec[ind], &vec[ind+length], sizeof(*vec)*(vec_size(vec)-length-ind));  \
                (vec_get_base(vec))->size-=length;                                              \
            }                                                                                   \
        }                                                                                       \
    }while(0)

#define vec_filter(vec, func)                               \
    do {                                                    \
        size_t ind_to_erase = 0;\
        size_t len = 0;\
        for(size_t ind=0;ind < vec_size(vec); ind++) {      \
            if(func(&vec[ind])) { \
                if(len>0) { \
                    vec_erase(vec, ind_to_erase, len);\
                    len = 0;\
                }\
            }else {\
                if(len==0) {\
                    ind_to_erase = ind;\
                }\
                len+=1; \
            }\
        }                                                   \
    }while(0)
#endif
