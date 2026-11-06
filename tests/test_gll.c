/* Quoted path so a syntax check without -I still finds the public header. */
#include "../include/gll.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static int failures = 0;
static int releases = 0;
static int tests_run = 0;
static int tests_failed = 0;
static const char *current_test = NULL;

static void expect(int condition, const char *file, int line, const char *expr)
{
    if (!condition) {
        printf("FAIL %s %s:%d: %s\n",
               current_test != NULL ? current_test : "?",
               file, line, expr);
        fflush(stdout);
        failures++;
    }
}

#define EXPECT(cond) expect((cond), __FILE__, __LINE__, #cond)

static void *xmalloc(size_t size)
{
    void *memory = malloc(size);

    if (memory == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(EXIT_FAILURE);
    }
    return memory;
}

static int *box(int value)
{
    int *memory = xmalloc(sizeof(*memory));

    *memory = value;
    return memory;
}

static void release_payload(void *payload)
{
    releases++;
    free(payload);
}

static int semver_major_minor(const char *version)
{
    int parts = 0;

    if (version == NULL) {
        return 0;
    }

    while (parts < 2) {
        if (*version < '0' || *version > '9') {
            return 0;
        }
        while (*version >= '0' && *version <= '9') {
            version++;
        }
        parts++;
        if (parts == 2) {
            return 1;
        }
        if (*version != '.') {
            return 0;
        }
        version++;
    }
    return 0;
}

static int cmp_int(const void *existing, const void *incoming)
{
    int left = *(const int *)existing;
    int right = *(const int *)incoming;

    return (left > right) - (left < right);
}

static void expect_chain(gll *list, const int *values, size_t count)
{
    gll_node *node = gll_head(list);
    gll_node *prev = NULL;
    size_t seen = 0;

    EXPECT(gll_count(list) == count);
    if (count == 0) {
        EXPECT(node == NULL);
        EXPECT(gll_tail(list) == NULL);
        return;
    }

    EXPECT(gll_prev(node) == NULL);
    while (node != NULL) {
        EXPECT(seen < count);
        if (seen < count) {
            EXPECT(*(int *)gll_payload(node) == values[seen]);
        }
        EXPECT(gll_prev(node) == prev);
        prev = node;
        node = gll_next(node);
        seen++;
    }
    EXPECT(seen == count);
    EXPECT(prev == gll_tail(list));
    if (prev != NULL) {
        EXPECT(gll_next(prev) == NULL);
    }

    node = gll_tail(list);
    seen = count;
    while (seen > 0) {
        seen--;
        EXPECT(node != NULL);
        if (node != NULL) {
            EXPECT(*(int *)gll_payload(node) == values[seen]);
            node = gll_prev(node);
        }
    }
    EXPECT(node == NULL);
}

static void test_version_string_is_semver(void)
{
    EXPECT(semver_major_minor(GLL_VERSION_STRING));
}

static void test_empty_list(void)
{
    gll *list = NULL;

    EXPECT(gll_create(&list, NULL) == GLL_OK);
    EXPECT(list != NULL);
    EXPECT(gll_count(list) == 0);
    EXPECT(gll_head(list) == NULL);
    EXPECT(gll_tail(list) == NULL);
    EXPECT(gll_next(NULL) == NULL);
    EXPECT(gll_prev(NULL) == NULL);
    EXPECT(gll_payload(NULL) == NULL);
    EXPECT(gll_destroy(&list) == GLL_OK);
    EXPECT(list == NULL);
    EXPECT(gll_destroy(&list) == GLL_OK);
}

static void test_insert_at_either_end(void)
{
    gll *list = NULL;
    gll_node *node = NULL;
    int *zero = box(0);
    int *one = box(1);
    int *two = box(2);
    const int forward[] = {0, 1, 2};

    EXPECT(gll_create(&list, free) == GLL_OK);
    EXPECT(gll_insert_tail(list, one, &node) == GLL_OK);
    EXPECT(gll_payload(node) == one);
    EXPECT(gll_head(list) == node);
    EXPECT(gll_tail(list) == node);
    EXPECT(gll_next(node) == NULL);
    EXPECT(gll_prev(node) == NULL);

    EXPECT(gll_insert_head(list, zero, NULL) == GLL_OK);
    EXPECT(gll_insert_tail(list, two, NULL) == GLL_OK);
    expect_chain(list, forward, 3);
    EXPECT(gll_destroy(&list) == GLL_OK);
}

static void test_repeated_head_insert_reverses_order(void)
{
    gll *list = NULL;
    const int forward[] = {3, 2, 1};

    EXPECT(gll_create(&list, free) == GLL_OK);
    EXPECT(gll_insert_head(list, box(1), NULL) == GLL_OK);
    EXPECT(gll_insert_head(list, box(2), NULL) == GLL_OK);
    EXPECT(gll_insert_head(list, box(3), NULL) == GLL_OK);
    expect_chain(list, forward, 3);
    EXPECT(gll_destroy(&list) == GLL_OK);
}

static void test_relative_insert(void)
{
    gll *list = NULL;
    gll_node *head = NULL;
    gll_node *tail = NULL;
    gll_node *inserted = NULL;
    const int forward[] = {-1, 0, 1, 9, 2, 4};

    EXPECT(gll_create(&list, free) == GLL_OK);
    EXPECT(gll_insert_tail(list, box(2), &tail) == GLL_OK);
    EXPECT(gll_insert_head(list, box(0), &head) == GLL_OK);
    EXPECT(gll_insert_after(list, head, box(1), NULL) == GLL_OK);
    EXPECT(gll_insert_before(list, tail, box(9), &inserted) == GLL_OK);
    EXPECT(*(int *)gll_payload(inserted) == 9);
    EXPECT(gll_insert_after(list, gll_tail(list), box(4), NULL) == GLL_OK);
    EXPECT(gll_insert_before(list, gll_head(list), box(-1), NULL) == GLL_OK);
    expect_chain(list, forward, 6);
    EXPECT(gll_destroy(&list) == GLL_OK);
}

static void test_sorted_insert_orders_payloads(void)
{
    gll *list = NULL;
    int sample = 7;
    const int inserted[] = {5, -1, 4, 0, 3};
    const int ordered[] = {-1, 0, 3, 4, 5};
    size_t i;

    EXPECT(gll_create(&list, free) == GLL_OK);
    EXPECT(gll_insert_sorted(list, &sample, NULL, NULL) == GLL_INVAL);
    EXPECT(gll_count(list) == 0);

    for (i = 0; i < 5; i++) {
        EXPECT(gll_insert_sorted(list, box(inserted[i]), cmp_int, NULL) == GLL_OK);
    }
    expect_chain(list, ordered, 5);
    EXPECT(gll_destroy(&list) == GLL_OK);
}

static void test_sorted_insert_keeps_equal_payloads_in_order(void)
{
    gll *list = NULL;
    int *first = box(1);
    int *second = box(1);
    gll_node *head;

    EXPECT(gll_create(&list, free) == GLL_OK);
    EXPECT(gll_insert_sorted(list, first, cmp_int, NULL) == GLL_OK);
    EXPECT(gll_insert_sorted(list, box(2), cmp_int, NULL) == GLL_OK);
    EXPECT(gll_insert_sorted(list, second, cmp_int, NULL) == GLL_OK);
    head = gll_head(list);
    EXPECT(gll_payload(head) == first);
    EXPECT(gll_payload(gll_next(head)) == second);
    EXPECT(*(int *)gll_payload(gll_tail(list)) == 2);
    EXPECT(gll_count(list) == 3);
    EXPECT(gll_destroy(&list) == GLL_OK);
}

static void test_unlink_while_iterating(void)
{
    gll *list = NULL;
    gll_node *node;
    gll_node *removed[3];
    size_t nremoved = 0;
    size_t i;
    const int start[] = {0, 1, 2, 3, 4};
    const int left[] = {1, 3};

    releases = 0;
    EXPECT(gll_create(&list, release_payload) == GLL_OK);
    for (i = 0; i < 5; i++) {
        EXPECT(gll_insert_tail(list, box(start[i]), NULL) == GLL_OK);
    }

    node = gll_head(list);
    while (node != NULL) {
        gll_node *next = NULL;
        int value = *(int *)gll_payload(node);

        if (value % 2 == 0) {
            EXPECT(gll_unlink(list, node, &next) == GLL_OK);
            EXPECT(*(int *)gll_payload(node) == value);
            EXPECT(gll_next(node) == NULL);
            EXPECT(gll_prev(node) == NULL);
            removed[nremoved++] = node;
        } else {
            next = gll_next(node);
        }
        node = next;
    }

    EXPECT(releases == 0);
    EXPECT(nremoved == 3);
    expect_chain(list, left, 2);
    EXPECT(gll_unlink(list, removed[0], NULL) == GLL_INVAL);

    for (i = 0; i < nremoved; i++) {
        EXPECT(gll_destroy_node(NULL, removed[i]) == GLL_OK);
    }
    EXPECT(releases == 3);
    EXPECT(gll_destroy(&list) == GLL_OK);
    EXPECT(releases == 5);
}

static void test_destroy_linked_node(void)
{
    gll *list = NULL;
    gll_node *middle = NULL;
    const int left[] = {1, 3};

    releases = 0;
    EXPECT(gll_create(&list, release_payload) == GLL_OK);
    EXPECT(gll_insert_tail(list, box(1), NULL) == GLL_OK);
    EXPECT(gll_insert_tail(list, box(2), &middle) == GLL_OK);
    EXPECT(gll_insert_tail(list, box(3), NULL) == GLL_OK);
    EXPECT(gll_destroy_node(list, middle) == GLL_OK);
    EXPECT(releases == 1);
    expect_chain(list, left, 2);

    EXPECT(gll_destroy_node(list, gll_head(list)) == GLL_OK);
    EXPECT(gll_count(list) == 1);
    EXPECT(*(int *)gll_payload(gll_head(list)) == 3);
    EXPECT(gll_head(list) == gll_tail(list));
    EXPECT(gll_prev(gll_head(list)) == NULL);

    EXPECT(gll_destroy_node(list, gll_tail(list)) == GLL_OK);
    EXPECT(gll_count(list) == 0);
    EXPECT(gll_head(list) == NULL);
    EXPECT(gll_tail(list) == NULL);
    EXPECT(releases == 3);
    EXPECT(gll_destroy(&list) == GLL_OK);
    EXPECT(releases == 3);
}

static void test_destroy_without_destructor_keeps_payload(void)
{
    gll *list = NULL;
    int *value = box(42);

    EXPECT(gll_create(&list, NULL) == GLL_OK);
    EXPECT(gll_insert_head(list, value, NULL) == GLL_OK);
    EXPECT(gll_destroy(&list) == GLL_OK);
    EXPECT(*value == 42);
    free(value);
}

static void test_null_payload_runs_destructor(void)
{
    gll *list = NULL;

    releases = 0;
    EXPECT(gll_create(&list, release_payload) == GLL_OK);
    EXPECT(gll_insert_head(list, NULL, NULL) == GLL_OK);
    EXPECT(gll_payload(gll_head(list)) == NULL);
    EXPECT(gll_count(list) == 1);
    EXPECT(gll_destroy(&list) == GLL_OK);
    EXPECT(releases == 1);
}

static void test_unlink_only_node_empties_list(void)
{
    gll *list = NULL;
    gll_node *node = NULL;
    gll_node *next = (gll_node *)(uintptr_t)1;
    int *value = box(8);

    releases = 0;
    EXPECT(gll_create(&list, release_payload) == GLL_OK);
    EXPECT(gll_insert_head(list, value, &node) == GLL_OK);
    EXPECT(gll_unlink(list, node, &next) == GLL_OK);
    EXPECT(next == NULL);
    EXPECT(gll_count(list) == 0);
    EXPECT(gll_head(list) == NULL);
    EXPECT(gll_tail(list) == NULL);
    EXPECT(gll_destroy(&list) == GLL_OK);
    EXPECT(releases == 0);
    EXPECT(*value == 8);
    EXPECT(gll_destroy_node(NULL, node) == GLL_OK);
    EXPECT(releases == 1);
}

static void test_take_returns_payload_without_destructor(void)
{
    gll *list = NULL;
    gll_node *middle = NULL;
    void *taken = NULL;
    int *value;
    const int left[] = {1, 3};

    releases = 0;
    EXPECT(gll_create(&list, release_payload) == GLL_OK);
    EXPECT(gll_insert_tail(list, box(1), NULL) == GLL_OK);
    value = box(2);
    EXPECT(gll_insert_tail(list, value, &middle) == GLL_OK);
    EXPECT(gll_insert_tail(list, box(3), NULL) == GLL_OK);

    EXPECT(gll_take(list, middle, &taken) == GLL_OK);
    EXPECT(taken == value);
    EXPECT(*(int *)taken == 2);
    EXPECT(releases == 0);
    expect_chain(list, left, 2);

    EXPECT(gll_destroy(&list) == GLL_OK);
    EXPECT(releases == 2);
    EXPECT(*(int *)taken == 2);
    free(taken);
}

static void test_take_unlinked_node(void)
{
    gll *list = NULL;
    gll_node *node = NULL;
    void *taken = (void *)(uintptr_t)1;
    int *value = box(8);

    releases = 0;
    EXPECT(gll_create(&list, release_payload) == GLL_OK);
    EXPECT(gll_insert_head(list, value, &node) == GLL_OK);
    EXPECT(gll_unlink(list, node, NULL) == GLL_OK);
    EXPECT(gll_take(NULL, node, &taken) == GLL_OK);
    EXPECT(taken == value);
    EXPECT(releases == 0);
    EXPECT(gll_count(list) == 0);
    EXPECT(gll_destroy(&list) == GLL_OK);
    EXPECT(releases == 0);
    free(taken);
}

static void test_take_null_payload(void)
{
    gll *list = NULL;
    void *taken = (void *)(uintptr_t)1;

    releases = 0;
    EXPECT(gll_create(&list, release_payload) == GLL_OK);
    EXPECT(gll_insert_head(list, NULL, NULL) == GLL_OK);
    EXPECT(gll_take(list, gll_head(list), &taken) == GLL_OK);
    EXPECT(taken == NULL);
    EXPECT(releases == 0);
    EXPECT(gll_count(list) == 0);
    EXPECT(gll_destroy(&list) == GLL_OK);
    EXPECT(releases == 0);
}

static void test_invalid_arguments(void)
{
    gll *list = NULL;
    gll *other = NULL;
    gll_node *node = NULL;
    gll_node *foreign = NULL;
    gll_node *out = (gll_node *)(uintptr_t)1;
    void *taken = (void *)(uintptr_t)1;
    int payload = 4;
    const int held[] = {4};

    EXPECT(gll_create(NULL, NULL) == GLL_INVAL);
    EXPECT(gll_destroy(NULL) == GLL_INVAL);
    EXPECT(gll_count(NULL) == 0);
    EXPECT(gll_head(NULL) == NULL);
    EXPECT(gll_tail(NULL) == NULL);
    EXPECT(gll_insert_head(NULL, &payload, &out) == GLL_INVAL);
    EXPECT(out == (gll_node *)(uintptr_t)1);
    EXPECT(gll_insert_tail(NULL, &payload, NULL) == GLL_INVAL);
    EXPECT(gll_insert_before(NULL, NULL, &payload, NULL) == GLL_INVAL);
    EXPECT(gll_insert_after(NULL, NULL, &payload, NULL) == GLL_INVAL);
    EXPECT(gll_insert_sorted(NULL, &payload, cmp_int, NULL) == GLL_INVAL);
    EXPECT(gll_unlink(NULL, NULL, NULL) == GLL_INVAL);
    EXPECT(gll_destroy_node(NULL, NULL) == GLL_INVAL);
    EXPECT(gll_take(NULL, NULL, &taken) == GLL_INVAL);
    EXPECT(taken == (void *)(uintptr_t)1);

    EXPECT(gll_create(&list, NULL) == GLL_OK);
    EXPECT(gll_create(&other, NULL) == GLL_OK);
    EXPECT(gll_insert_tail(list, &payload, &node) == GLL_OK);
    EXPECT(gll_insert_tail(other, &payload, &foreign) == GLL_OK);

    EXPECT(gll_insert_before(list, NULL, &payload, &out) == GLL_INVAL);
    EXPECT(out == (gll_node *)(uintptr_t)1);
    EXPECT(gll_insert_after(list, NULL, &payload, NULL) == GLL_INVAL);
    EXPECT(gll_insert_before(list, foreign, &payload, NULL) == GLL_INVAL);
    EXPECT(gll_insert_after(list, foreign, &payload, NULL) == GLL_INVAL);
    EXPECT(gll_unlink(list, foreign, NULL) == GLL_INVAL);
    EXPECT(gll_unlink(other, node, NULL) == GLL_INVAL);
    EXPECT(gll_destroy_node(list, foreign) == GLL_INVAL);
    EXPECT(gll_destroy_node(NULL, node) == GLL_INVAL);
    EXPECT(gll_take(list, node, NULL) == GLL_INVAL);
    EXPECT(gll_take(list, foreign, &taken) == GLL_INVAL);
    EXPECT(taken == (void *)(uintptr_t)1);
    EXPECT(gll_payload(node) == &payload);
    EXPECT(gll_payload(foreign) == &payload);
    expect_chain(list, held, 1);
    expect_chain(other, held, 1);

    EXPECT(gll_destroy(&list) == GLL_OK);
    EXPECT(gll_destroy(&other) == GLL_OK);
}

static void run_test(const char *name, void (*fn)(void))
{
    int before = failures;

    current_test = name;
    fn();
    tests_run++;
    if (failures == before) {
        printf("tests/test_gll.c::%s PASSED\n", name);
    } else {
        tests_failed++;
        printf("tests/test_gll.c::%s FAILED\n", name);
    }
    fflush(stdout);
}

int main(void)
{
    int passed;

    run_test("test_version_string_is_semver", test_version_string_is_semver);
    run_test("test_empty_list", test_empty_list);
    run_test("test_insert_at_either_end", test_insert_at_either_end);
    run_test("test_repeated_head_insert_reverses_order",
             test_repeated_head_insert_reverses_order);
    run_test("test_relative_insert", test_relative_insert);
    run_test("test_sorted_insert_orders_payloads",
             test_sorted_insert_orders_payloads);
    run_test("test_sorted_insert_keeps_equal_payloads_in_order",
             test_sorted_insert_keeps_equal_payloads_in_order);
    run_test("test_unlink_while_iterating", test_unlink_while_iterating);
    run_test("test_destroy_linked_node", test_destroy_linked_node);
    run_test("test_destroy_without_destructor_keeps_payload",
             test_destroy_without_destructor_keeps_payload);
    run_test("test_null_payload_runs_destructor",
             test_null_payload_runs_destructor);
    run_test("test_unlink_only_node_empties_list",
             test_unlink_only_node_empties_list);
    run_test("test_take_returns_payload_without_destructor",
             test_take_returns_payload_without_destructor);
    run_test("test_take_unlinked_node", test_take_unlinked_node);
    run_test("test_take_null_payload", test_take_null_payload);
    run_test("test_invalid_arguments", test_invalid_arguments);

    passed = tests_run - tests_failed;
    if (tests_failed != 0) {
        printf("%d failed, %d passed, %d assertion(s) failed\n",
               tests_failed, passed, failures);
        return 1;
    }
    printf("%d passed\n", passed);
    return 0;
}
