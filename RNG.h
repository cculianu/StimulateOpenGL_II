#ifndef RNG_H
#define RNG_H

/** \brief A class to generate random numbers.  

    Can pick an algorithm & seed
    at c'tor time and can change seed at runtime.

    Currently supports ran0, ran1, and gasdev generators from numerical
    recipes.

    I wanted to encapsulate random number generation in a class to
    keep different plugins and sets of experiment variables isolated
    from one another, so that they each have their own random state
    and seed.  This makes reproducing experiments clearer and easier,
    hopefully. -Calin */
class RNG
{
public:
    enum Type {
        Ran0, /**< Numerical recipes ran0() algorithm.  
                   Returns a uniformly distributed random number in range
                   (0, 1.00 (0 non-inclusive to 1 non-inclusive). */
        Ran1, /**< Numerical recipes ran1() algorithm.  
                   Returns a uniformly distributed random number in range
                   (0, 1.0) (0 non-inclusive to 1 non-inclusive). 
                   Uses a 32-element Bayes-Durham shuffle box to reduce bit 
                   correlation.   */
//         SFMT, /**< Fastest RNG in the world!
//                    Returns a uniformly distributed random number
//                    from (0.0, 1.0).
//                    Note only one of these generators may be active
//                    at once per application due to implementation limitations.*/
        Gasdev/**< Similar to numerical recipes gasdev() function. Returns
                   a normally distributed random number with mean 0.0 and 
                   variance 1.0 */
    };

    RNG(int seed = 1, Type = Ran0);
    ~RNG();

    /// Identify what type of generator this is.
    Type type() const { return t; }

    /// Returns the original seed
    int seed() const { return originalSeed; }

    /// Returns the current seed
    int currentSeed() const { return s; }

    /// Returns the number of times this generator generated a number since it was seeded. 
    unsigned count() const { return ct; }

    /** Generate the next random number.  Synonym of operator().

    Generation is as follows:

    If type() == Ran0 
            Numerical recipes ran0() algorithm.  
            Returns a uniformly distributed random number in range
            (0, 1.0) (0 non-inclusive to 1 non-inclusive). 
   
    If type() == Ran01
            Numerical recipes ran1() algorithm.  
            Returns a uniformly distributed random number in range
            (0, 1.0) (0 non-inclusive to 1 non-inclusive). 
   
    If type() == Gasdev
             Similar to numerical recipes gasdev() function. Returns
             a normally distributed random number with mean 0.0 and 
             variance 1.0                                               */
    inline double next() {
        switch (t) {
        case Ran0: ++ct; return ran0f();
        case Ran1: ++ct; return ran1f();
        case Gasdev: ++ct; return gasdev();
        default: break;
        }
        return nextBadState(); // for now..
    }

    inline int nexti() {
        switch (t) {
        case Ran0: ++ct; return ran0i();
        case Ran1: ++ct; return ran1i();
        default: break;
        }
        return nextBadState(); // for now..
    }

    /// resets count and sets the seed to seed.
    void reseed(int seed);

    /// calls next()
    double operator()() { return next(); }


    /** Convenience function.

        For Ran0 and Ran1 generators:

        This uses next() to return a random number in range [p1, p2).
        Note how this range differs in how it is inclusive from the
        next() function.  (It is inclusive of p1 and not inclusive of
        p2).  [Undefined if p1 > p2.]

        Note this is slightly slower than calling next() because it scales
        the results of next() to modify the range of the output.

        The way this function is inclusive is more useful for some
        programmers since traditionally programmers like to start at min 
        (inclusive) and go to max non-inclusively.
        
        For Gasdev generators:

        Uses next() to return a random number with mean p1 and stddev p2.   */
    double range(double p1 = 0., double p2 = 1.);

private:
    int originalSeed; ///< the original seed
    int s; ///< current seed (state of generator is basically s,ct for Ran1/Gasdev or just s for Ran0)
    unsigned ct; ///< count
    Type t;
    /// variables used for Gasdev
    double gset; 
    bool iset; 
    int iy;
    int *iv;
    inline int ran0i();
    inline double ran0f();
    inline int ran1i();
    inline double ran1f();
    double gasdev();
    int nextBadState() const;
};

inline double RNG::ran1f() 
{
    static const int IA = 16807;
    static const int IM = 2147483647;
    static const double AM = (1.0/IM);
    static const int IQ = 127773;
    static const int IR = 2836;
    static const int NTAB = 32;
    static const int NDIV (1+(IM-1)/NTAB);
    static const double EPS = 1.2e-7;
    static const double RNMX = (1.0-EPS);
    int j;
    int k;
    double ans;

    if (!iv) {
        iv = new int[NTAB];
        if (s == 0) s = 1; // disallow 0 s
        if (s < 0) s = -s; // force positive s
    }
    if (s <= 0 || !iy) {
        //if (-s < 1) s = 1; // force s of 1 if negative s
        //else s = -s; // force s positive
        for (j=NTAB+7;j>=0;j--) {
            k=(s)/IQ;
            s=IA*(s-k*IQ)-IR*k;
            if (s < 0) s += IM;
            if (j < NTAB) iv[j] = s;
        }
        iy=iv[0];
    }
    k=(s)/IQ;
    s=IA*(s-k*IQ)-IR*k;
    if (s < 0) s += IM;
    j=iy/NDIV;
    iy=iv[j];
    iv[j] = s;
    if ((ans = AM*iy) > RNMX) ans = RNMX;
    return ans;
}

inline int RNG::ran1i() 
{
    static const int IA = 16807;
    static const int IM = 2147483647;
    static const int IQ = 127773;
    static const int IR = 2836;
    static const int NTAB = 32;
    static const int NDIV (1+(IM-1)/NTAB);
    int j;
    int k;
    int ans;

    if (!iv) {
        iv = new int[NTAB];
        if (s == 0) s = 1; // disallow 0 s
        if (s < 0) s = -s; // force positive s
    }
    if (s <= 0 || !iy) {
        //if (-s < 1) s = 1; // force s of 1 if negative s
        //else s = -s; // force s positive
        for (j=NTAB+7;j>=0;j--) {
            k=(s)/IQ;
            s=IA*(s-k*IQ)-IR*k;
            if (s < 0) s += IM;
            if (j < NTAB) iv[j] = s;
        }
        iy=iv[0];
    }
    k=(s)/IQ;
    s = IA*(s-k*IQ)-IR*k;
    if (s < 0) s += IM;
    j = iy/NDIV;
    iy=iv[j];
    iv[j] = s;
    ans = iy;
    if (ans < 0) ans += IM;
    return ans;
}

inline double RNG::ran0f()
{
    static const int IA = 16807;
    static const int IM = 2147483647;
    static const double AM = (1.0/IM);
    static const int IQ = 127773;
    static const int IR = 2836;
    static const int MASK = 123459876;
    int k;
    double ans;
    
    s ^= MASK;
    k = (s)/IQ;
    s = IA*(s-k*IQ)-IR*k;
    if (s < 0) s += IM;
    ans = AM * (s);
    s ^= MASK;
    return ans;
}

inline int RNG::ran0i()
{
    static const int IA = 16807;
    static const int IM = 2147483647;
    static const int IQ = 127773;
    static const int IR = 2836;
    static const int MASK = 123459876;
    int k;
    int ans;
    
    s ^= MASK;
    k = (s)/IQ;
    s = IA*(s-k*IQ)-IR*k;
    if (s < 0) s += IM;
    ans = s;
    s ^= MASK;
    return ans;
}

#endif

