#include <stdlib.h>

void* reallocate(void* p, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(p);
        return NULL;
    }
    void* result = realloc(p, newSize);
    if (result == NULL) exit(1);
    return result;
}
