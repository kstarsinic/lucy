#ifndef DSO_H
#define DSO_H

#include "oo.h"

extern class_t *OBJ;
extern size_t Obj_Hello_OFFSET;
extern method_t Obj_Hello_THUNK_PTR;

void bootstrap();

obj_t *Obj_new(void);

void Obj_Hello_THUNK(obj_t *obj);

#endif /* DSO_H */

