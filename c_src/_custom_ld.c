/* The batman package: fast computation of exoplanet transit light curves
 * Copyright (C) 2015 Laura Kreidberg	 
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <Python.h>
#include "numpy/arrayobject.h"
#include<math.h>

#if defined (_OPENMP)
#  include <omp.h>
#endif

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

static PyObject *_custom_ld(PyObject *self, PyObject *args);

double intensity(double r, double c1, double c2, double c3, double c4, double c5, double c6);

double area(double d, double r, double R)
{
	/*
	Returns area of overlapping circles with radii r and R; separated by a distance d
	*/
	double arg1 = (d*d + r*r - R*R)/(2.*d*r);
	double arg2 = (d*d + R*R - r*r)/(2.*d*R); 
	double arg3 = MAX((-d + r + R)*(d + r - R)*(d - r + R)*(d + r + R), 0.);

	if(r <= R - d) return M_PI*r*r;							//planet completely overlaps stellar circle
	else if(r >= R + d) return M_PI*R*R;						//stellar circle completely overlaps planet
	else return r*r*acos(arg1) + R*R*acos(arg2) - 0.5*sqrt(arg3);			//partial overlap
}

static PyObject *_custom_ld(PyObject *self, PyObject *args)
{
	double rprs, d, fac, A_i, r, I, dr, A_f, r_in, r_out, delta, c1, c2, c3, c4, c5, c6;
	int nthreads;
	npy_intp i, dims[1];
	PyArrayObject *ds, *flux;

  	if(!PyArg_ParseTuple(args,"Oddddddddi", &ds, &rprs, &c1, &c2, &c3, &c4, &c5, &c6, &fac, &nthreads)) return NULL; //parses input arguments
	
	dims[0] = PyArray_DIMS(ds)[0]; 
	flux = (PyArrayObject *) PyArray_SimpleNew(1, dims, PyArray_TYPE(ds));	//creates numpy array to store return flux values

	double *f_array = PyArray_DATA(flux);
	double *d_array = PyArray_DATA(ds);

	/*
		NOTE:  the safest way to access numpy arrays is to use the PyArray_GETITEM and PyArray_SETITEM functions.
		Here we use a trick for faster access and more convenient access, where we set a pointer to the 
		beginning of the array with the PyArray_DATA (e.g., f_array) and access elements with e.g., f_array[i].
		Success of this operation depends on the numpy array storing data in blocks equal in size to a C double.
		If you run into trouble along these lines, I recommend changing the array access to something like:
			d = PyFloat_AsDouble(PyArray_GETITEM(ds, PyArray_GetPtr(ds, &i))); 
		where ds is a numpy array object.


		Laura Kreidberg 07/2015
	*/

	#if defined (_OPENMP)
	omp_set_num_threads(nthreads);	//specifies number of threads (if OpenMP is supported)
	#endif

	#if defined (_OPENMP)
	#pragma omp parallel for private(d, r_in, r_out, delta, r, dr, A_i, A_f, I)
	#endif
	for(i = 0; i < dims[0]; i++)
	{
		d = d_array[i];
		r_in = MAX(d - rprs, 0.);						//lower bound for integration
		r_out = MIN(d + rprs, 1.0);						//upper bound for integration

		if(r_in >= 1.) f_array[i] = 1.0;					//flux = 1. if the planet is not transiting
		else
		{
			delta = 0.;							//variable to store the integrated intensity, \int I dA

			r = r_in;							//starting radius for integration
			dr = fac*acos(r); 						//initial step size 
			r += dr;							//first step
			A_i = 0.;							//initial area
	
			while(r < r_out)
			{
				A_f = area(d, r, rprs);					//calculates area of overlapping circles
				I = intensity(r - dr/2.,c1,c2, c3, c4, c5, c6); 	//intensity at the midpoint
				delta += (A_f - A_i)*I;					//increase in transit depth for this integration step
				dr = fac*acos(r);  					//updating step size
				r = r + dr;						//stepping to next element
				A_i = A_f;						//storing area
			}
			dr = r_out - r + dr;  						//calculating change in radius for last step
			r = r_out;							//final radius for integration
			A_f = area(d, r, rprs);						//area for last integration step
			I = intensity(r - dr/2., c1, c2, c3, c4, c5, c6); 		//intensity at the midpoint 
			delta += (A_f - A_i)*I;						//increase in transit depth for this integration step
			f_array[i] = 1.0 - delta;	//flux equals 1 - \int I dA 
		}
	}
	return PyArray_Return((PyArrayObject *)flux);
} 

static char _custom_ld_doc[] = "This extension module returns a limb darkened light curve for a custom stellar intensity profile.";


static PyMethodDef _custom_ld_methods[] = {
  {"_custom_ld", _custom_ld, METH_VARARGS, _custom_ld_doc},{NULL}};

#if PY_MAJOR_VERSION >= 3
	static struct PyModuleDef _custom_ld_module = {
		PyModuleDef_HEAD_INIT,
		"_custom_ld",
		_custom_ld_doc,
		-1, 
		_custom_ld_methods
	};

	PyMODINIT_FUNC
	PyInit__custom_ld(void)
	{
		PyObject* module = PyModule_Create(&_custom_ld_module);
		if(!module)
		{
			return NULL;
		}
		import_array(); 
		return module;
	}
#else

	void init_custom_ld(void)
	{
		Py_InitModule("_custom_ld", _custom_ld_methods);
		import_array(); 
	}
#endif

