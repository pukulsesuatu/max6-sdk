/**
	@file
	simplemsp~: a simple audio object for Max
	original by: jeremy bernstein, jeremy@bootsquad.com	
	@ingroup examples	
*/

#include "ext.h"			// standard Max include, always required (except in Jitter)
#include "ext_obex.h"		// required for "new" style objects
#include "z_dsp.h"			// required for MSP objects


// struct to represent the object's state
typedef struct _simplemsp {
	t_pxobject		ob;			// the object itself (t_pxobject in MSP instead of t_object)
	double			offset; 	// the value of a property of our object
} t_simplemsp;


// method prototypes
void *simplemsp_new(t_symbol *s, long argc, t_atom *argv);
void simplemsp_free(t_simplemsp *x);
void simplemsp_assist(t_simplemsp *x, void *b, long m, long a, char *s);
void simplemsp_float(t_simplemsp *x, double f);
void simplemsp_dsp(t_simplemsp *x, t_signal **sp, short *count);
void simplemsp_dsp64(t_simplemsp *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
t_int *simplemsp_perform(t_int *w);
void simplemsp_perform64(t_simplemsp *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);


// global class pointer variable
static t_class *simplemsp_class = NULL;


//***********************************************************************************************

int main(void)
{	
	// object initialization, note the use of dsp_free for the freemethod, which is required
	// unless you need to free allocated memory, in which case you should call dsp_free from
	// your custom free function.

	t_class *c = class_new("simplemsp~", (method)simplemsp_new, (method)dsp_free, (long)sizeof(t_simplemsp), 0L, A_GIMME, 0);
	
	class_addmethod(c, (method)simplemsp_float,		"float",	A_FLOAT, 0);
	class_addmethod(c, (method)simplemsp_dsp,		"dsp",		A_CANT, 0);		// Old 32-bit MSP dsp chain compilation for Max 5 and earlier
	class_addmethod(c, (method)simplemsp_dsp64,		"dsp64",	A_CANT, 0);		// New 64-bit MSP dsp chain compilation for Max 6
	class_addmethod(c, (method)simplemsp_assist,	"assist",	A_CANT, 0);
	
	class_dspinit(c);
	class_register(CLASS_BOX, c);
	simplemsp_class = c;

	return 0;
}


void *simplemsp_new(t_symbol *s, long argc, t_atom *argv)
{
	t_simplemsp *x = (t_simplemsp *)object_alloc(simplemsp_class);

	if (x) {
		dsp_setup((t_pxobject *)x, 1);	// MSP inlets: arg is # of inlets and is REQUIRED! 
										// use 0 if you don't need inlets
		outlet_new(x, "signal"); 		// signal outlet (note "signal" rather than NULL)
		x->offset = 0.0;
	}
	return (x);
}


// NOT CALLED!, we use dsp_free for a generic free function
void simplemsp_free(t_simplemsp *x) 
{
	;
}


//***********************************************************************************************

void simplemsp_assist(t_simplemsp *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET) { //inlet
		sprintf(s, "I am inlet %ld", a);
	} 
	else {	// outlet
		sprintf(s, "I am outlet %ld", a); 			
	}
}


void simplemsp_float(t_simplemsp *x, double f)
{
	x->offset = f;
}


//***********************************************************************************************

// this function is called when the DAC is enabled, and "registers" a function for the signal chain in Max 5 and earlier.
// In this case we register the 32-bit, "simplemsp_perform" method.
void simplemsp_dsp(t_simplemsp *x, t_signal **sp, short *count)
{
	post("my sample rate is: %f", sp[0]->s_sr);
	
	// dsp_add
	// 1: (t_perfroutine p) perform method
	// 2: (long argc) number of args to your perform method
	// 3...: argc additional arguments, all must be sizeof(pointer) or long
	// these can be whatever, so you might want to include your object pointer in there
	// so that you have access to the info, if you need it.
	dsp_add(simplemsp_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}


// this is the Max 6 version of the dsp method -- it registers a function for the signal chain in Max 6,
// which operates on 64-bit audio signals.
void simplemsp_dsp64(t_simplemsp *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
	post("my sample rate is: %f", samplerate);
	
	// instead of calling dsp_add(), we send the "dsp_add64" message to the object representing the dsp chain
	// the dsp_add64 arguments are:
	// 1: the dsp64 object passed-in by the calling function
	// 2: a pointer to your object
	// 3: a pointer to your 64-bit perform method
	// 4: flags to alter how the signal chain handles your object -- just pass 0
	// 5: a generic pointer that you can use to pass any additional data to your perform method
	
	object_method(dsp64, gensym("dsp_add64"), x, simplemsp_perform64, 0, NULL);
}


//***********************************************************************************************

// this is the 32-bit perform method for Max 5 and earlier
t_int *simplemsp_perform(t_int *w)
{
	// DO NOT CALL post IN HERE, but you can call defer_low (not defer)
	
	// args are in a vector, sized as specified in simplemsp_dsp method
	// w[0] contains &simplemsp_perform, so we start at w[1]
	t_simplemsp *x = (t_simplemsp *)(w[1]);
	t_float *inL = (t_float *)(w[2]);
	t_float *outL = (t_float *)(w[3]);
	int n = (int)w[4];
	
	// this perform method simply copies the input to the output, offsetting the value
	while (n--)
		*outL++ = *inL++ + x->offset;
		
	// you have to return the NEXT pointer in the array OR MAX WILL CRASH
	return w + 5;
}


// this is 64-bit perform method for Max 6
void simplemsp_perform64(t_simplemsp *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
	t_double *inL = ins[0];		// we get audio for each inlet of the object from the **ins argument
	t_double *outL = outs[0];	// we get audio for each outlet of the object from the **outs argument
	int n = sampleframes;
	
	// this perform method simply copies the input to the output, offsetting the value
	while (n--)
		*outL++ = *inL++ + x->offset;
}

