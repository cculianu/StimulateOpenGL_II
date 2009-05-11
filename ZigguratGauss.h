#ifndef ZigguratGauss_H
#define ZigguratGauss_H
#include <math.h>
#ifdef _MSC_VER
/* Microsoft compilers don't come with stdint.h so we have to emulate it */
typedef unsigned int uint32_t;
#else
#include <stdint.h>
#endif
/** \brief A namespace for Gaussian distribution generation using the ziggurat
    method.

    Function that generates gaussian distributed numbers with mean 0
    and parmeterized sigma.  It requires a source of entropy, so
    the generate() function is a template taking a functor or function
    returning uint32_t. 

    Code based on code by Jochen Voss
    Copyright (C) 2005  Jochen Voss.

    Code stolen from gauss.c available at http://seehuhn.de/pages/ziggurat#GSL
       
    License: GPL 
*/
namespace ZigguratGauss
{
    extern const double ytab[];
    extern const uint32_t ktab[];
    extern const double wtab[];
    /* position of right-most step */
    static const double PARAM_R = 3.44428647676;

    /** GenFunctorT f shuold be a function returning unsigned from 0->UINT_MAX 
        or a functor implementing: unsigned operator()(void); */
    template <typename GenFunctorT> 
    double generate(GenFunctorT & genFunctor, double sigma)
    {
        uint32_t  U, sign, i, j;
        double  x, y;

        while (1) {
            U = genFunctor();
            i = U & 0x0000007F;		/* 7 bit to choose the step */
            sign = U & 0x00000080;	/* 1 bit for the sign */
            j = U>>8;			/* 24 bit for the x-value */
            x = j*wtab[i];

            if (j < ktab[i])  break;

            if (i<127) {
                double  y0, y1;
                y0 = ytab[i];
                y1 = ytab[i+1];
                y = y1+(y0-y1)*genFunctor();
            } else {
                x = PARAM_R - log(1.0-genFunctor())/PARAM_R;
                y = exp(-PARAM_R*(x-0.5*PARAM_R))*genFunctor();
            }
            if (y < exp(-0.5*x*x))  break;
        }
        return  sign ? sigma*x : -sigma*x;        
    }
};

#endif
