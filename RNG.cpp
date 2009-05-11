#include "RNG.h"
#include <math.h>
#include <exception>

RNG::RNG(int sd, Type t) : t(t)
{
    iv = 0;
    reseed(sd);
}

RNG::~RNG()
{
    delete [] iv; // may be NULL if Ran0
    iv = 0;
    iy = 0;
}

int RNG::nextBadState() const
{
    struct BadStateException : public std::exception
    {
        const char *what() const throw() 
          { return "RNG type is supported for this operation"; }
    };
    throw BadStateException(); 
    return 0; // not reached
}

void RNG::reseed(int seed)
{
    if (seed < 0) seed = -seed; // force positive seed
    if (!seed) seed = 1; // force nonzero seed;
    originalSeed = s = seed;
    ct = 0;
    delete [] iv;
    iv = 0;
    iy = 0;
    iset = false;
}

double RNG::range(double p1, double p2)
{
    if (t != Gasdev) {
        static const double EPS = 1.2e-7; // minimum threshold for 0
        static const double RNMX = (1.0-EPS); // theshold for 1
        double r = next();
        if (r <= EPS) r = 0.;
        else if (r >= RNMX) r = 1.0; // to force non-inclusivity
        // normal case, p1 is min and p2 is max
        return (r/RNMX)*(p2-p1) + p1;
    }
    // else .. Gasdev case p1 means u (mean) and p2 is stddev (scale)
    return next()*p2 + p1;
}

double RNG::gasdev()
{
    double fac,rsq,v1,v2;

    //if (!iv || s < 0) /* if not seeded */ iset = false;
    if (!iset) {
        do {
            v1 = 2.0*ran1f()-1.0;
            v2 = 2.0*ran1f()-1.0;
            rsq = v1*v1 + v2*v2;
        } while (rsq >= 1.0 || rsq == 0.0);
        fac = sqrt(-2.0*log(rsq)/rsq);
        gset = v1*fac; // cache gset
        iset = true;
        return v2*fac;
    } 
    // else used cached gset
    iset = false;
    return gset;
}


#ifdef TEST_RNG

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
int main(int argc, char *argv[])
{
    int s, t;
    unsigned n;
    std::string str("");
    if (argc > 1) str += argv[1];
    if (argc > 2) str += std::string(" ") + argv[2];
    if (argc > 3) str += std::string(" ") + argv[3];
    std::istringstream is(str);
    if (argc != 4
        || (argc == 4 && ( !(is >> s) || !(is >> t) || !(is >> n) ) || t < 0 || t > 2 ) ) {
        std::cerr << "Please pass a seed and a number 0, 1, 2 for Ran0, Ran1, Gasdev, and a count\n";
        return 1;
    }
    std::cerr << "Seed: " << s << ", type = " << t << "\nGenerating " << n << " random numbers:\n";
    RNG rg(s,static_cast<RNG::Type>(t));
    for (unsigned i = 0; i < n; ++i) {
        std::cout << std::fixed << std::setprecision(20) << /*rg.range(0.,1.)*/ rg() << "\n";
        if (!(i % 1000000)) 
            std::cerr <<  "(Count now: " << i << ")\n";
    }
    return 0;
}

#endif
