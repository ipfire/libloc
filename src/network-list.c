/*
	libloc - A library to determine the location of someone on the Internet

	Copyright (C) 2020 IPFire Development Team <info@ipfire.org>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.
*/

#include <errno.h>
#include <stdlib.h>

#include <loc/libloc.h>
#include <loc/network.h>
#include <loc/private.h>

struct loc_network_list {
	struct loc_ctx* ctx;
	int refcount;

	struct loc_network* list[1024];
	size_t size;
	size_t max_size;
};

LOC_EXPORT int loc_network_list_new(struct loc_ctx* ctx,
		struct loc_network_list** list) {
	struct loc_network_list* l = calloc(1, sizeof(*l));
	if (!l)
		return -ENOMEM;

	l->ctx = loc_ref(ctx);
	l->refcount = 1;

	// Do not allow this list to grow larger than this
	l->max_size = 1024;

	DEBUG(l->ctx, "Network list allocated at %p\n", l);
	*list = l;
	return 0;
}

LOC_EXPORT struct loc_network_list* loc_network_list_ref(struct loc_network_list* list) {
	list->refcount++;

	return list;
}

static void loc_network_list_free(struct loc_network_list* list) {
	DEBUG(list->ctx, "Releasing network list at %p\n", list);

	for (unsigned int i = 0; i < list->size; i++)
		loc_network_unref(list->list[i]);

	loc_unref(list->ctx);
	free(list);
}

LOC_EXPORT struct loc_network_list* loc_network_list_unref(struct loc_network_list* list) {
	if (!list)
		return NULL;

	if (--list->refcount > 0)
		return list;

	loc_network_list_free(list);
	return NULL;
}

LOC_EXPORT size_t loc_network_list_size(struct loc_network_list* list) {
	return list->size;
}

LOC_EXPORT int loc_network_list_empty(struct loc_network_list* list) {
	return list->size == 0;
}

LOC_EXPORT void loc_network_list_clear(struct loc_network_list* list) {
	for (unsigned int i = 0; i < list->size; i++)
		loc_network_unref(list->list[i]);

	list->size = 0;
}

LOC_EXPORT void loc_network_list_dump(struct loc_network_list* list) {
	struct loc_network* network;
	char* s;

	for (unsigned int i = 0; i < list->size; i++) {
		network = list->list[i];

		s = loc_network_str(network);

		INFO(list->ctx, "%s\n", s);
		free(s);
	}
}

LOC_EXPORT struct loc_network* loc_network_list_get(struct loc_network_list* list, size_t index) {
	// Check index
	if (index >= list->size)
		return NULL;

	return loc_network_ref(list->list[index]);
}

LOC_EXPORT int loc_network_list_push(struct loc_network_list* list, struct loc_network* network) {
	// Do not add networks that are already on the list
	if (loc_network_list_contains(list, network))
		return 0;

	// Check if we have space left
	if (list->size == list->max_size) {
		ERROR(list->ctx, "%p: Could not push network onto the stack: Stack full\n", list);
		return -ENOMEM;
	}

	DEBUG(list->ctx, "%p: Pushing network %p onto stack\n", list, network);

	list->list[list->size++] = loc_network_ref(network);

	return 0;
}

LOC_EXPORT struct loc_network* loc_network_list_pop(struct loc_network_list* list) {
	// Return nothing when empty
	if (loc_network_list_empty(list)) {
		DEBUG(list->ctx, "%p: Popped empty stack\n", list);
		return NULL;
	}

	struct loc_network* network = list->list[--list->size];

	DEBUG(list->ctx, "%p: Popping network %p from stack\n", list, network);

	return network;
}

LOC_EXPORT struct loc_network* loc_network_list_pop_first(struct loc_network_list* list) {
	// Return nothing when empty
	if (loc_network_list_empty(list)) {
		DEBUG(list->ctx, "%p: Popped empty stack\n", list);
		return NULL;
	}

	struct loc_network* network = list->list[0];

	// Move all elements to the top of the stack
	for (unsigned int i = 0; i < --list->size; i++) {
		list->list[i] = list->list[i+1];
	}

	DEBUG(list->ctx, "%p: Popping network %p from stack\n", list, network);

	return network;
}

LOC_EXPORT int loc_network_list_contains(struct loc_network_list* list, struct loc_network* network) {
	for (unsigned int i = 0; i < list->size; i++) {
		if (loc_network_eq(list->list[i], network))
			return 1;
	}

	return 0;
}

static void loc_network_list_swap(struct loc_network_list* list, unsigned int i1, unsigned int i2) {
	// Do nothing for invalid indices
	if (i1 >= list->size || i2 >= list->size)
		return;

	struct loc_network* network1 = list->list[i1];
	struct loc_network* network2 = list->list[i2];

	list->list[i1] = network2;
	list->list[i2] = network1;
}

LOC_EXPORT void loc_network_list_reverse(struct loc_network_list* list) {
	unsigned int i = 0;
	unsigned int j = list->size - 1;

	while (i < j) {
		loc_network_list_swap(list, i++, j--);
	}
}

LOC_EXPORT void loc_network_list_sort(struct loc_network_list* list) {
	unsigned int n = list->size;
	int swapped;

	do {
		swapped = 0;

		for (unsigned int i = 1; i < n; i++) {
			if (loc_network_gt(list->list[i-1], list->list[i]) > 0) {
				loc_network_list_swap(list, i-1, i);
				swapped = 1;
			}
		}

		n--;
	} while (swapped);
}

LOC_EXPORT int loc_network_list_merge(
		struct loc_network_list* self, struct loc_network_list* other) {
	int r;

	for (unsigned int i = 0; i < other->size; i++) {
		r = loc_network_list_push(self, other->list[i]);
		if (r)
			return r;
	}

	return 0;
}
