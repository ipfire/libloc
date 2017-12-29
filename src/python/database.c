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

#include "../database.h"
#include "database.h"

static PyObject* Database_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
	// Create libloc context
	struct loc_ctx* ctx;
	int r = loc_new(&ctx);
	if (r)
		return NULL;

	DatabaseObject* self = (DatabaseObject*)type->tp_alloc(type, 0);
	if (self) {
		self->ctx = ctx;
	}

	return (PyObject*)self;
}

static void Database_dealloc(DatabaseObject* self) {
	if (self->db)
		loc_database_unref(self->db);

	if (self->ctx)
		loc_unref(self->ctx);

	Py_TYPE(self)->tp_free((PyObject* )self);
}

static int Database_init(DatabaseObject* self, PyObject* args, PyObject* kwargs) {
	const char* path = NULL;

	if (!PyArg_ParseTuple(args, "s", &path))
		return -1;

	// Open the file for reading
	FILE* f = fopen(path, "r");
	if (!f)
		return -1;

	// Load the database
	int r = loc_database_new(self->ctx, &self->db, f);
	fclose(f);

	// Return on any errors
	if (r)
		return -1;

	return 0;
}

static struct PyMethodDef Database_methods[] = {
	{ NULL },
};

PyTypeObject DatabaseType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	tp_name:                "location.Database",
	tp_basicsize:           sizeof(DatabaseObject),
	tp_flags:               Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
	tp_new:                 Database_new,
	tp_dealloc:             (destructor)Database_dealloc,
	tp_init:                (initproc)Database_init,
	tp_doc:                 "Database object",
	tp_methods:             Database_methods,
};
