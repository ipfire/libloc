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
#include <loc/database.h>

#include "locationmodule.h"
#include "as.h"
#include "database.h"
#include "network.h"

static PyObject* Database_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
	DatabaseObject* self = (DatabaseObject*)type->tp_alloc(type, 0);

	return (PyObject*)self;
}

static void Database_dealloc(DatabaseObject* self) {
	if (self->db)
		loc_database_unref(self->db);

	if (self->path)
		free(self->path);

	Py_TYPE(self)->tp_free((PyObject* )self);
}

static int Database_init(DatabaseObject* self, PyObject* args, PyObject* kwargs) {
	const char* path = NULL;

	if (!PyArg_ParseTuple(args, "s", &path))
		return -1;

	self->path = strdup(path);

	// Open the file for reading
	FILE* f = fopen(self->path, "r");
	if (!f)
		return -1;

	// Load the database
	int r = loc_database_new(loc_ctx, &self->db, f);
	fclose(f);

	// Return on any errors
	if (r)
		return -1;

	return 0;
}

static PyObject* Database_repr(DatabaseObject* self) {
	return PyUnicode_FromFormat("<Database %s>", self->path);
}

static PyObject* Database_get_description(DatabaseObject* self) {
	const char* description = loc_database_get_description(self->db);

	return PyUnicode_FromString(description);
}

static PyObject* Database_get_vendor(DatabaseObject* self) {
	const char* vendor = loc_database_get_vendor(self->db);

	return PyUnicode_FromString(vendor);
}

static PyObject* Database_get_created_at(DatabaseObject* self) {
	time_t created_at = loc_database_created_at(self->db);

	return PyLong_FromLong(created_at);
}

static PyObject* Database_get_as(DatabaseObject* self, PyObject* args) {
	struct loc_as* as = NULL;
	uint32_t number = 0;

	if (!PyArg_ParseTuple(args, "i", &number))
		return NULL;

	// Try to retrieve the AS
	int r = loc_database_get_as(self->db, &as, number);

	// We got an AS
	if (r == 0) {
		PyObject* obj = new_as(&ASType, as);
		loc_as_unref(as);

		return obj;

	// Nothing found
	} else if (r == 1) {
		Py_RETURN_NONE;
	}

	// Unexpected error
	return NULL;
}

static PyObject* Database_lookup(DatabaseObject* self, PyObject* args) {
	struct loc_network* network = NULL;
	const char* address = NULL;

	if (!PyArg_ParseTuple(args, "s", &address))
		return NULL;

	// Try to retrieve a matching network
	int r = loc_database_lookup_from_string(self->db, address, &network);

	// We got a network
	if (r == 0) {
		PyObject* obj = new_network(&NetworkType, network);
		loc_network_unref(network);

		return obj;

	// Nothing found
	} else if (r == 1) {
		Py_RETURN_NONE;
	}

	// Unexpected error
	return NULL;
}

static struct PyMethodDef Database_methods[] = {
	{
		"get_as",
		(PyCFunction)Database_get_as,
		METH_VARARGS,
		NULL,
	},
	{
		"lookup",
		(PyCFunction)Database_lookup,
		METH_VARARGS,
		NULL,
	},
	{ NULL },
};

static struct PyGetSetDef Database_getsetters[] = {
	{
		"created_at",
		(getter)Database_get_created_at,
		NULL,
		NULL,
		NULL,
	},
	{
		"description",
		(getter)Database_get_description,
		NULL,
		NULL,
		NULL,
	},
	{
		"vendor",
		(getter)Database_get_vendor,
		NULL,
		NULL,
		NULL,
	},
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
	tp_getset:              Database_getsetters,
	tp_repr:                (reprfunc)Database_repr,
};
