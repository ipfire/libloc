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

#include "locationmodule.h"
#include "as.h"
#include "database.h"

PyMODINIT_FUNC PyInit_location(void);

static void location_free(void) {
	// Release context
	if (loc_ctx)
		loc_unref(loc_ctx);
}

static PyMethodDef location_module_methods[] = {
	{ NULL },
};

static struct PyModuleDef location_module = {
	.m_base = PyModuleDef_HEAD_INIT,
	.m_name = "location",
	.m_size = -1,
	.m_doc = "Python module for libloc",
	.m_methods = location_module_methods,
	.m_free = (freefunc)location_free,
};

PyMODINIT_FUNC PyInit_location(void) {
	// Initialise loc context
	int r = loc_new(&loc_ctx);
	if (r)
		return NULL;

	PyObject* m = PyModule_Create(&location_module);
	if (!m)
		return NULL;

	// AS
	if (PyType_Ready(&ASType) < 0)
		return NULL;

	Py_INCREF(&ASType);
	PyModule_AddObject(m, "AS", (PyObject *)&ASType);

	// Database
	if (PyType_Ready(&DatabaseType) < 0)
		return NULL;

	Py_INCREF(&DatabaseType);
	PyModule_AddObject(m, "Database", (PyObject *)&DatabaseType);

	return m;
}
