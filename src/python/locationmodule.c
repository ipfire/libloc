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
#include <syslog.h>

#include "locationmodule.h"
#include "as.h"
#include "database.h"
#include "network.h"
#include "writer.h"

PyMODINIT_FUNC PyInit_location(void);

static void location_free(void) {
	// Release context
	if (loc_ctx)
		loc_unref(loc_ctx);
}

static PyObject* set_log_level(PyObject* m, PyObject* args) {
	int priority = LOG_INFO;

	if (!PyArg_ParseTuple(args, "i", &priority))
		return NULL;

	loc_set_log_priority(loc_ctx, priority);

	Py_RETURN_NONE;
}

static PyMethodDef location_module_methods[] = {
	{
		"set_log_level",
		(PyCFunction)set_log_level,
		METH_VARARGS,
		NULL,
	},
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

	// Database Enumerator
	if (PyType_Ready(&DatabaseEnumeratorType) < 0)
		return NULL;

	Py_INCREF(&DatabaseEnumeratorType);
	//PyModule_AddObject(m, "DatabaseEnumerator", (PyObject *)&DatabaseEnumeratorType);

	// Network
	if (PyType_Ready(&NetworkType) < 0)
		return NULL;

	Py_INCREF(&NetworkType);
	PyModule_AddObject(m, "Network", (PyObject *)&NetworkType);

	// Writer
	if (PyType_Ready(&WriterType) < 0)
		return NULL;

	Py_INCREF(&WriterType);
	PyModule_AddObject(m, "Writer", (PyObject *)&WriterType);

	// Add constants
	PyObject* d = PyModule_GetDict(m);

	// Version
	PyDict_SetItemString(d, "__version__", PyUnicode_FromString(VERSION));

	return m;
}
