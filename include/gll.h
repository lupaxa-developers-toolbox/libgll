#ifndef GLL_H
#define GLL_H

#include <stddef.h>

#define GLL_VERSION_STRING "0.0.0"

/* GLL_OK is success. GLL_NOMEM means allocation failed and the list was
   left unchanged, except gll_create which sets *list to NULL. GLL_INVAL
   means a missing list, a missing comparator, a missing out-pointer, or a
   node that is not in that list. */
#define GLL_OK 0
#define GLL_NOMEM (-1)
#define GLL_INVAL (-2)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gll gll;
typedef struct gll_node gll_node;

typedef void (*gll_destructor)(void *payload);

/* qsort-style comparator. Called as compare(existing, incoming).
   The incoming payload is placed before the first node for which this
   returns a positive value. Zero and negative results leave the existing
   node ahead of the incoming one, so equal payloads keep insertion order. */
typedef int (*gll_compare)(const void *existing, const void *incoming);

/* Create an empty list. destroy_payload may be NULL. When it is set, the
   library calls it for each payload freed by gll_destroy or gll_destroy_node.
   On success *list receives the list. On allocation failure *list is NULL. */
int gll_create(gll **list, gll_destructor destroy_payload);

/* Free every node still in the list, then free the list. Unlinked nodes are
   not reached. *list is set to NULL. Destroying an already NULL list is
   success. */
int gll_destroy(gll **list);

int gll_insert_head(gll *list, void *payload, gll_node **out);
int gll_insert_tail(gll *list, void *payload, gll_node **out);
int gll_insert_before(gll *list, gll_node *node, void *payload, gll_node **out);
int gll_insert_after(gll *list, gll_node *node, void *payload, gll_node **out);
int gll_insert_sorted(gll *list, void *payload, gll_compare compare,
                      gll_node **out);

/* Detach node without freeing it or its payload. When next is not NULL,
   *next receives the following node so a walk can continue. The caller then
   owns node and must eventually call gll_destroy_node. A second unlink of
   the same node fails. On failure *next is left unchanged. */
int gll_unlink(gll *list, gll_node *node, gll_node **next);

/* Free one node. A node that is still linked is unlinked first. The
   destructor captured when the node was inserted runs on its payload.
   list must be the owning list while the node is linked. After gll_unlink,
   list may be NULL. node is invalid after success. */
int gll_destroy_node(gll *list, gll_node *node);

/* Hand the payload back to the caller and free the node. The destructor
   does not run. *payload receives the pointer, which may be NULL, and the
   caller owns it. A node that is still linked is unlinked first. list must
   be the owning list while the node is linked. After gll_unlink, list may
   be NULL. node is invalid after success. On failure *payload is unchanged.
   Read gll_next before calling this when the walk must continue. */
int gll_take(gll *list, gll_node *node, void **payload);

size_t gll_count(const gll *list);
gll_node *gll_head(const gll *list);
gll_node *gll_tail(const gll *list);
gll_node *gll_next(const gll_node *node);
gll_node *gll_prev(const gll_node *node);
void *gll_payload(const gll_node *node);

#ifdef __cplusplus
}
#endif

#endif /* GLL_H */
