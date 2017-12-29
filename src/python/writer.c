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
#include <loc/writer.h>

#include "locationmodule.h"
#include "writer.h"

static PyObject* Writer_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
	WriterObject* self = (WriterObject*)type->tp_alloc(type, 0);

	return (PyObject*)self;
}

static void Writer_dealloc(WriterObject* self) {
	if (self->writer)
		loc_writer_unref(self->writer);

	Py_TYPE(self)->tp_free((PyObject* )self);
}

static int Writer_init(WriterObject* self, PyObject* args, PyObject* kwargs) {
	// Create the writer object
	int r = loc_writer_new(loc_ctx, &self->writer);
	if (r)
		return -1;

	return 0;
}

static PyObject* Writer_get_vendor(WriterObject* self) {
	const char* vendor = loc_writer_get_vendor(self->writer);

	return PyUnicode_FromString(vendor);
}

static int Writer_set_vendor(WriterObject* self, PyObject* args) {
	const char* vendor = NULL;

	if (!PyArg_ParseTuple(args, "s", &vendor))
		return -1;

	int r = loc_writer_set_vendor(self->writer, vendor);
	if (r) {
		PyErr_Format(PyExc_ValueError, "Could not set vendor: %s", vendor);
		return r;
	}

	return 0;
}

static struct PyGetSetDef Writer_getsetters[] = {
	{
		"vendor",
		(getter)Writer_get_vendor,
		(setter)Writer_set_vendor,
		NULL,
		NULL,
	},
	{ NULL },
};

PyTypeObject WriterType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	tp_name:                "location.Writer",
	tp_basicsize:           sizeof(WriterObject),
	tp_flags:               Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
	tp_new:                 Writer_new,
	tp_dealloc:             (destructor)Writer_dealloc,
	tp_init:                (initproc)Writer_init,
	tp_doc:                 "Writer object",
	tp_getset:              Writer_getsetters,
};
