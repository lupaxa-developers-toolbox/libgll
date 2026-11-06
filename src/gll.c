/* Generic doubly linked list.
   The public contract is in include/gll.h. This file holds the list and
   node layout and the function bodies. */

#include "gll.h"

#include <stdlib.h>

/* One linked element. payload is the caller's pointer. list is the owning
   list, or NULL after the node is unlinked. destroy_payload is the
   destructor copied from the list at insert time, and may be NULL. */
struct gll_node {
    void *payload;
    struct gll_node *prev;
    struct gll_node *next;
    struct gll *list;
    gll_destructor destroy_payload;
};

/* List header. head and tail are NULL when the list is empty. count is the
   number of nodes still linked. destroy_payload may be NULL. */
struct gll {
    struct gll_node *head;
    struct gll_node *tail;
    size_t count;
    gll_destructor destroy_payload;
};

/* Return 1 when node is linked in list. A NULL list, a NULL node, a node
   whose back-pointer names another list, or a node that is not on the
   chain returns 0. */
static int contains(const gll *list, const gll_node *node)
{
    const gll_node *cursor;

    if (list == NULL || node == NULL || node->list != list) {
        return 0;
    }
    for (cursor = list->head; cursor != NULL; cursor = cursor->next) {
        if (cursor == node) {
            return 1;
        }
    }
    return 0;
}

/* Allocate a zeroed node and store payload, the owning list, and the list
   destructor. Returns NULL when allocation fails. The node is not linked. */
static gll_node *new_node(gll *list, void *payload)
{
    gll_node *node = calloc(1, sizeof(*node));

    if (node == NULL) {
        return NULL;
    }
    node->payload = payload;
    node->list = list;
    node->destroy_payload = list->destroy_payload;
    return node;
}

/* Record a successful insert. When out is not NULL, *out receives the new
   node. Returns GLL_OK. */
static int publish(gll_node *node, gll_node **out)
{
    if (out != NULL) {
        *out = node;
    }
    return GLL_OK;
}

/* Create an empty list. destroy_payload may be NULL. When it is set, the
   library calls it for each payload freed by gll_destroy or gll_destroy_node.
   On success *list receives the list. On allocation failure *list is NULL. */
int gll_create(gll **list, gll_destructor destroy_payload)
{
    gll *created;

    if (list == NULL) {
        return GLL_INVAL;
    }

    created = calloc(1, sizeof(*created));
    if (created == NULL) {
        *list = NULL;
        return GLL_NOMEM;
    }

    created->destroy_payload = destroy_payload;
    *list = created;
    return GLL_OK;
}

/* Free every node still in the list, then free the list. Unlinked nodes are
   not reached. *list is set to NULL. Destroying an already NULL list is
   success. */
int gll_destroy(gll **list)
{
    if (list == NULL) {
        return GLL_INVAL;
    }
    if (*list == NULL) {
        return GLL_OK;
    }

    while ((*list)->head != NULL) {
        if (gll_destroy_node(*list, (*list)->head) != GLL_OK) {
            return GLL_INVAL;
        }
    }

    free(*list);
    *list = NULL;
    return GLL_OK;
}

/* Insert payload at the head. On an empty list the new node is also the
   tail. On success *out receives the node when out is not NULL. On failure
   the list and *out are unchanged. */
int gll_insert_head(gll *list, void *payload, gll_node **out)
{
    gll_node *node;

    if (list == NULL) {
        return GLL_INVAL;
    }

    node = new_node(list, payload);
    if (node == NULL) {
        return GLL_NOMEM;
    }

    node->next = list->head;
    if (list->head != NULL) {
        list->head->prev = node;
    } else {
        list->tail = node;
    }
    list->head = node;
    list->count++;
    return publish(node, out);
}

/* Insert payload at the tail. On an empty list the new node is also the
   head. On success *out receives the node when out is not NULL. On failure
   the list and *out are unchanged. */
int gll_insert_tail(gll *list, void *payload, gll_node **out)
{
    gll_node *node;

    if (list == NULL) {
        return GLL_INVAL;
    }

    node = new_node(list, payload);
    if (node == NULL) {
        return GLL_NOMEM;
    }

    node->prev = list->tail;
    if (list->tail != NULL) {
        list->tail->next = node;
    } else {
        list->head = node;
    }
    list->tail = node;
    list->count++;
    return publish(node, out);
}

/* Insert payload before a node that is already in the list. Inserting
   before the head updates the head. On success *out receives the node when
   out is not NULL. A missing list or a node that is not in the list returns
   GLL_INVAL and leaves the list and *out unchanged. */
int gll_insert_before(gll *list, gll_node *before, void *payload, gll_node **out)
{
    gll_node *node;

    if (!contains(list, before)) {
        return GLL_INVAL;
    }

    node = new_node(list, payload);
    if (node == NULL) {
        return GLL_NOMEM;
    }

    node->next = before;
    node->prev = before->prev;
    if (before->prev != NULL) {
        before->prev->next = node;
    } else {
        list->head = node;
    }
    before->prev = node;
    list->count++;
    return publish(node, out);
}

/* Insert payload after a node that is already in the list. Inserting after
   the tail updates the tail. On success *out receives the node when out is
   not NULL. A missing list or a node that is not in the list returns
   GLL_INVAL and leaves the list and *out unchanged. */
int gll_insert_after(gll *list, gll_node *after, void *payload, gll_node **out)
{
    gll_node *node;

    if (!contains(list, after)) {
        return GLL_INVAL;
    }

    node = new_node(list, payload);
    if (node == NULL) {
        return GLL_NOMEM;
    }

    node->prev = after;
    node->next = after->next;
    if (after->next != NULL) {
        after->next->prev = node;
    } else {
        list->tail = node;
    }
    after->next = node;
    list->count++;
    return publish(node, out);
}

/* Insert payload in comparator order. compare is called as
   compare(existing, incoming). The incoming payload is placed before the
   first node for which compare returns a positive value. A zero or negative
   result leaves the existing node ahead of the incoming one, so equal
   payloads keep insertion order. A NULL list or a NULL compare returns
   GLL_INVAL. */
int gll_insert_sorted(gll *list, void *payload, gll_compare compare, gll_node **out)
{
    gll_node *cursor;

    if (list == NULL || compare == NULL) {
        return GLL_INVAL;
    }

    cursor = list->head;
    while (cursor != NULL && compare(cursor->payload, payload) <= 0) {
        cursor = cursor->next;
    }
    if (cursor == NULL) {
        return gll_insert_tail(list, payload, out);
    }
    return gll_insert_before(list, cursor, payload, out);
}

/* Detach node without freeing it or its payload. When next is not NULL,
   *next receives the following node so a walk can continue. The caller then
   owns node and must eventually call gll_destroy_node or gll_take. A second
   unlink of the same node fails. On failure *next is left unchanged. */
int gll_unlink(gll *list, gll_node *node, gll_node **next)
{
    gll_node *following;

    if (!contains(list, node)) {
        return GLL_INVAL;
    }

    following = node->next;
    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }
    if (node->next != NULL) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }

    node->prev = NULL;
    node->next = NULL;
    node->list = NULL;
    list->count--;

    if (next != NULL) {
        *next = following;
    }
    return GLL_OK;
}

/* Free one node. A node that is still linked is unlinked first. The
   destructor captured when the node was inserted runs on its payload.
   list must be the owning list while the node is linked. After gll_unlink,
   list may be NULL. node is invalid after success. */
int gll_destroy_node(gll *list, gll_node *node)
{
    gll_destructor destroy_payload;

    if (node == NULL) {
        return GLL_INVAL;
    }
    if (node->list != NULL) {
        if (list != node->list || gll_unlink(list, node, NULL) != GLL_OK) {
            return GLL_INVAL;
        }
    }

    destroy_payload = node->destroy_payload;
    if (destroy_payload != NULL) {
        destroy_payload(node->payload);
    }
    free(node);
    return GLL_OK;
}

/* Hand the payload back to the caller and free the node. The destructor
   does not run. *payload receives the pointer, which may be NULL, and the
   caller owns it. A node that is still linked is unlinked first. list must
   be the owning list while the node is linked. After gll_unlink, list may
   be NULL. node is invalid after success. On failure *payload is unchanged.
   Read gll_next before calling this when the walk must continue. */
int gll_take(gll *list, gll_node *node, void **payload)
{
    void *taken;

    if (node == NULL || payload == NULL) {
        return GLL_INVAL;
    }
    if (node->list != NULL) {
        if (list != node->list || gll_unlink(list, node, NULL) != GLL_OK) {
            return GLL_INVAL;
        }
    }

    taken = node->payload;
    free(node);
    *payload = taken;
    return GLL_OK;
}

/* Return the number of nodes still linked. A NULL list returns 0. */
size_t gll_count(const gll *list)
{
    if (list == NULL) {
        return 0;
    }
    return list->count;
}

/* Return the first node, or NULL when the list is missing or empty. */
gll_node *gll_head(const gll *list)
{
    if (list == NULL) {
        return NULL;
    }
    return list->head;
}

/* Return the last node, or NULL when the list is missing or empty. */
gll_node *gll_tail(const gll *list)
{
    if (list == NULL) {
        return NULL;
    }
    return list->tail;
}

/* Return the following node, or NULL at the tail and for a NULL node. */
gll_node *gll_next(const gll_node *node)
{
    if (node == NULL) {
        return NULL;
    }
    return node->next;
}

/* Return the previous node, or NULL at the head and for a NULL node. */
gll_node *gll_prev(const gll_node *node)
{
    if (node == NULL) {
        return NULL;
    }
    return node->prev;
}

/* Return the payload pointer stored in the node, or NULL for a NULL node.
   A stored NULL payload also returns NULL. */
void *gll_payload(const gll_node *node)
{
    if (node == NULL) {
        return NULL;
    }
    return node->payload;
}
