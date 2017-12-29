/*
	libloc - A library to determine the location of someone on the Internet

	Copyright (C) 2017 IPFire Development Team <info@ipfire.org>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.
*/

#include <Python.h>

#include <loc/libloc.h>
#include <loc/as.h>
#include <loc/stringpool.h>

#include "locationmodule.h"
#include "as.h"

PyObject* new_as(PyTypeObject* type, struct loc_as* as) {
	ASObject* self = (ASObject*)type->tp_alloc(type, 0);
	if (self) {
		self->as = loc_as_ref(as);
	}

	return (PyObject*)self;
}

static PyObject* AS_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
	// Create stringpool
	struct loc_stringpool* pool;
	int r = loc_stringpool_new(loc_ctx, &pool);
	if (r)
		return NULL;

	ASObject* self = (ASObject*)type->tp_alloc(type, 0);
	if (self) {
		self->ctx = loc_ref(loc_ctx);
		self->pool = pool;
	}

	return (PyObject*)self;
}

static void AS_dealloc(ASObject* self) {
	if (self->as)
		loc_as_unref(self->as);

	if (self->pool)
		loc_stringpool_unref(self->pool);

	if (self->ctx)
		loc_unref(self->ctx);

	Py_TYPE(self)->tp_free((PyObject* )self);
}

static int AS_init(ASObject* self, PyObject* args, PyObject* kwargs) {
	uint32_t number = 0;

	if (!PyArg_ParseTuple(args, "i", &number))
		return -1;

	// Create the AS object
	int r = loc_as_new(self->ctx, self->pool, &self->as, number);
	if (r)
		return -1;

	return 0;
}

static PyObject* AS_get_number(ASObject* self) {
	uint32_t number = loc_as_get_number(self->as);

	return PyLong_FromLong(number);
}

static PyObject* AS_get_name(ASObject* self) {
	const char* name = loc_as_get_name(self->as);

	return PyUnicode_FromString(name);
}

static struct PyGetSetDef AS_getsetters[] = {
	{
		"name",
		(getter)AS_get_name,
		NULL,
		NULL,
		NULL,
	},
	{
		"number",
		(getter)AS_get_number,
		NULL,
		NULL,
		NULL,
	},
	{ NULL },
};

PyTypeObject ASType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	tp_name:                "location.AS",
	tp_basicsize:           sizeof(ASObject),
	tp_flags:               Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
	tp_new:                 AS_new,
	tp_dealloc:             (destructor)AS_dealloc,
	tp_init:                (initproc)AS_init,
	tp_doc:                 "AS object",
	tp_getset:              AS_getsetters,
};
