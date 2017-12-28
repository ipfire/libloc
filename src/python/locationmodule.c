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

PyMODINIT_FUNC PyInit_location(void);

static PyMethodDef location_module_methods[] = {
	{ NULL },
};

static struct PyModuleDef location_module = {
	.m_base = PyModuleDef_HEAD_INIT,
	.m_name = "location",
	.m_size = -1,
	.m_doc = "Python module for libloc",
	.m_methods = location_module_methods,
};

PyMODINIT_FUNC PyInit_location(void) {
	PyObject* m = PyModule_Create(&location_module);
	if (!m)
		return NULL;

	return m;
}
