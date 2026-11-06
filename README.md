<p align="center">
  <a href="https://github.com/lupaxa-developers-toolbox">
    <img src="https://raw.githubusercontent.com/the-lupaxa-project/brand-assets/master/logos/organisations/developers-toolbox/readme-logo.png" alt="Developers Toolbox" />
  </a>
</p>

<h1 align="center">Libgll</h1>

C library for generic doubly linked lists.

Each list is a header with a head, a tail, and a count. Payloads are `void *`.
The node type is opaque. Insert at either end, before or after a node, or in
sorted order. Unlink a node while walking, destroy one node, or destroy the
list. Count the nodes, and walk with next and previous links. The public
header is `include/gll.h`. It uses C linkage, so a C++ translation unit can
include it.

## Requirements

- GNU make.
- gcc, unless `CC` is set. The C skill uses gcc for syntax checks.
- cppcheck, for `make c-check`.

## Build

`make lib` writes the static archive `build/libgll.a`. The archive is built
as C11 with `-Wall -Wextra -Wpedantic -Werror`. The compiler is gcc unless
`CC` is set. `make clean` removes `build/`.

```bash
make lib
cc -std=c11 -Iinclude -o demo demo.c build/libgll.a
```

`make lib CC=clang` selects the compiler. `make lib CFLAGS=-g` adds flags to
that C11 build. `make` with no target prints the makefile help.

`make update` refreshes the makefile skills. `make lib` and `make test` do
not need that checkout. `make c-check` and `make version` do.

## Quick Start

Create a list with `free` as the destructor, append one heap `int`, walk from
the head, then destroy the list. `gll_destroy` calls `free` on the payload.

```c
#include "gll.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    gll *list = NULL;
    gll_node *node;
    int *value;

    if (gll_create(&list, free) != GLL_OK) {
        return 1;
    }

    value = malloc(sizeof(*value));
    if (value == NULL) {
        gll_destroy(&list);
        return 1;
    }
    *value = 1;

    if (gll_insert_tail(list, value, NULL) != GLL_OK) {
        free(value);
        gll_destroy(&list);
        return 1;
    }

    for (node = gll_head(list); node != NULL; node = gll_next(node)) {
        printf("%d\n", *(int *)gll_payload(node));
    }

    gll_destroy(&list);
    return 0;
}
```

## Ownership

### Payloads

The list stores the pointer it is given. It does not copy the bytes. Pass a
destructor to `gll_create`, or pass NULL.

When the destructor is set, `gll_destroy` and `gll_destroy_node` call it with
the payload, including a NULL payload. When the destructor is NULL, those
calls free the node only, and the caller frees the payload.

`gll_take` is the exception. It hands that one payload back and frees the
node without calling the destructor. The caller then owns the pointer. The
other nodes keep the destructor.

The destructor is copied onto the node at insert time.

### Detached Nodes

`gll_unlink` removes a node from the list and leaves the payload in place.
The caller then owns that node and finishes it with `gll_destroy_node`.
`gll_destroy` frees only the nodes still linked.

After a successful unlink, `gll_destroy_node` and `gll_take` accept a NULL
list, because the destructor already sits on the node. Destroy or take a
linked node with the list that owns it. The node pointer is invalid after
either call succeeds.

Read the following node before `gll_take` when the walk must continue:

```c
gll_node *next = gll_next(node);
void *kept = NULL;

if (gll_take(list, node, &kept) == GLL_OK) {
    node = next;
}
```

> **Note:** Callers serialise use of a given list.

## Return Codes

Every function that can fail returns an `int`.

| Code        | Value | Meaning                                                                                 |
| ----------- | ----- | --------------------------------------------------------------------------------------- |
| `GLL_OK`    | `0`   | The call succeeded.                                                                     |
| `GLL_NOMEM` | `-1`  | Allocation failed. Inserts leave the list unchanged. `gll_create` sets `*list` to NULL. |
| `GLL_INVAL` | `-2`  | The list, comparator, or out-pointer is missing, or the node is not in that list.       |

`gll_destroy(NULL)` returns `GLL_INVAL`. `gll_destroy` on a pointer that is
already NULL returns `GLL_OK` and leaves it NULL.

On failure, an insert, unlink, or take out-pointer is left unchanged.

## Functions

Inserts take an optional `gll_node **out`. On success it receives the new
node. Pass NULL when the caller does not need it.

```c
typedef struct gll gll;
typedef struct gll_node gll_node;
typedef void (*gll_destructor)(void *payload);
typedef int (*gll_compare)(const void *existing, const void *incoming);

int gll_create(gll **list, gll_destructor destroy_payload);
int gll_destroy(gll **list);
int gll_insert_head(gll *list, void *payload, gll_node **out);
int gll_insert_tail(gll *list, void *payload, gll_node **out);
int gll_insert_before(gll *list, gll_node *node, void *payload, gll_node **out);
int gll_insert_after(gll *list, gll_node *node, void *payload, gll_node **out);
int gll_insert_sorted(gll *list, void *payload, gll_compare compare,
                      gll_node **out);
int gll_unlink(gll *list, gll_node *node, gll_node **next);
int gll_destroy_node(gll *list, gll_node *node);
int gll_take(gll *list, gll_node *node, void **payload);
size_t gll_count(const gll *list);
gll_node *gll_head(const gll *list);
gll_node *gll_tail(const gll *list);
gll_node *gll_next(const gll_node *node);
gll_node *gll_prev(const gll_node *node);
void *gll_payload(const gll_node *node);
```

| Name                | Purpose                                                                     |
| ------------------- | --------------------------------------------------------------------------- |
| `gll_create`        | Allocate an empty list and store the optional destructor.                   |
| `gll_destroy`       | Free every node still linked, free the list, and set `*list` to NULL.       |
| `gll_insert_head`   | Insert a payload at the head.                                               |
| `gll_insert_tail`   | Insert a payload at the tail.                                               |
| `gll_insert_before` | Insert a payload before a node that is in the list.                         |
| `gll_insert_after`  | Insert a payload after a node that is in the list.                          |
| `gll_insert_sorted` | Insert a payload in comparator order.                                       |
| `gll_unlink`        | Detach a node and, when `next` is not NULL, store the following node there. |
| `gll_destroy_node`  | Free one node. A destructor set at insert time runs on the payload.         |
| `gll_take`          | Return the payload and free the node. The destructor does not run.          |
| `gll_count`         | Return the number of linked nodes. A NULL list returns 0.                   |
| `gll_head`          | Return the first node, or NULL.                                             |
| `gll_tail`          | Return the last node, or NULL.                                              |
| `gll_next`          | Return the following node, or NULL.                                         |
| `gll_prev`          | Return the previous node, or NULL.                                          |
| `gll_payload`       | Return the payload pointer stored in the node.                              |

The first insert sets both head and tail to that node. A later head insert
leaves the tail in place. A later tail insert leaves the head in place.
`gll_count` tracks inserts, unlinks, takes, and destruction of a linked node.

`gll_insert_before` and `gll_insert_after` require a node that is already in
that list. A NULL node, or a node from another list, returns `GLL_INVAL` and
leaves both lists unchanged.

`gll_destroy_node` unlinks a node that is still linked, then frees it.
`gll_take` does the same unlink, stores the payload in `*payload`, and frees
the node without the destructor. `payload` itself must be non-NULL. Passing
the wrong list, or NULL, for a node that is still linked returns `GLL_INVAL`
and does not free the node. A second unlink of the same node returns
`GLL_INVAL`. On failure, `*payload` is left unchanged.

## Sorted Insert

`gll_insert_sorted` takes a `qsort`-style comparator. The call is
`compare(existing, incoming)`.

- A positive result places the incoming payload before that node.
- A zero or negative result leaves the existing node ahead of the incoming payload, so equal payloads stay in insertion order.

A NULL comparator returns `GLL_INVAL`, including on an empty list. The first
successful insert is the only node. Later inserts walk from the head.

```c
static int cmp_int(const void *existing, const void *incoming)
{
    int left = *(const int *)existing;
    int right = *(const int *)incoming;

    return (left > right) - (left < right);
}
```

```c
gll_insert_sorted(list, value, cmp_int, NULL);
```

Inserting `2`, then `1`, then `2` yields `1`, `2`, `2`. The two equal
payloads stay in insertion order.

## Walking

`gll_head` and `gll_tail` return the ends. `gll_next` and `gll_prev` move one
step. Both return NULL at the end of the chain, and both return NULL when
given NULL. `gll_payload(NULL)` returns NULL.

Unlink during the walk and keep the following node. Destroy the detached
node in that same step.

```c
gll_node *node = gll_head(list);

while (node != NULL) {
    gll_node *next = NULL;

    if (drop(node) && gll_unlink(list, node, &next) == GLL_OK) {
        gll_destroy_node(NULL, node);
    } else {
        next = gll_next(node);
    }
    node = next;
}
```

A failed unlink leaves its out-pointer unchanged and leaves the node in the
list, so the `else` branch steps forward with `gll_next`.

## Empty List

A new list has a count of 0, and its head and tail are NULL. Destroying it
returns `GLL_OK` and sets the caller's pointer to NULL. Destroying that NULL
pointer again returns `GLL_OK`.

`gll_count(NULL)`, `gll_head(NULL)`, and `gll_tail(NULL)` return an empty
result: 0 or NULL.

## Version

The version text is the macro `GLL_VERSION_STRING` in `include/gll.h`. It is
a dotted semantic version. `.bumpversion.toml` is the version source, and a
bump updates that macro.

After `make init`, `make version` prints the version from
`.bumpversion.toml`.

## Development

```bash
make update
make lib
make check
make clean
```

`make c-check` syntax-checks every `.c` and `.h` file, then runs cppcheck.
`make check` runs `c-check` and then the behaviour tests. `make test` runs
only the behaviour tests.

<a href="https://github.com/the-lupaxa-project">
    <img src="https://raw.githubusercontent.com/the-lupaxa-project/brand-assets/master/logos/components/footer-for-child-orgs.svg" alt="The Lupaxa Project Footer" width="100%" />
</a>
