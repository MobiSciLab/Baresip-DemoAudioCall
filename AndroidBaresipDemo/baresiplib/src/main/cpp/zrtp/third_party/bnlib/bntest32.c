/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * Test driver for low-level bignum library (32-bit version).
 * This access the low-level library directly.  It is NOT an example of
 * how to program with the library normally!  By accessing the library
 * at a low level, it is possible to exercise the smallest components
 * and thus localize bugs more accurately.  This is especially useful
 * when writing assembly-language primitives.
 *
 * This also does timing tests on modular exponentiation.  Modular
 * exponentiation is so computationally expensive that the fact that this
 * code omits one level of interface glue has no perceptible effect on
 * the results.
 */
#include "zrtp.h"

#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H 0
#endif
#if HAVE_CONFIG_H
#include "bnconfig.h"
#endif

#define _ZTU_ "bntest"

/*
 * Some compilers complain about #if FOO if FOO isn't defined,
 * so do the ANSI-mandated thing explicitly...
 */
#ifndef NO_STDLIB_H
#define NO_STDLIB_H 0
#endif
#ifndef NO_STRING_H
#define NO_STRING_H 0
#endif
#ifndef HAVE_STRINGS_H
#define HAVE_STRINGS_H 0
#endif

#include <stdio.h>

#if !NO_STDLIB_H
#include <stdlib.h>	/* For strtol */
#else
long strtol(const char *, char **, int);
#endif

#if !NO_STRING_H
#include <string.h>	/* For memcpy */
#elif HAVE_STRINGS_H
#include <strings.h>
#endif

#include "lbn32.h"
#include "kludge.h"

#if BNYIELD
int (*bnYield)(void) = 0;
#endif

/* Work with up to 2048-bit numbers */
#define MAXBITS 3072
#define SIZE (MAXBITS/32 + 1)

/* Additive congruential random number generator, x[i] = x[i-24] + x[i-55] */
static BNWORD32 randp[55];
static BNWORD32 *randp1 = randp, *randp2 = randp+24;

static BNWORD32
rand32(void)
{
    if (++randp2 == randp+55) {
	randp2 = randp;
	randp1++;
    } else if (++randp1 == randp+55) {
	randp1 = randp;
    }

    return  *randp1 += *randp2;
}

/*
 * CRC-3_2: x^3_2+x^26+x^23+x^22+x^1_6+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1
 *
 * The additive congruential RNG is seeded with a single integer,
 * which is shuffled with a CRC polynomial to generate the initial
 * table values.  The Polynomial is the same size as the words being
 * used.
 *
 * Thus, in the various versions of this library, we actually use this
 * polynomial as-is, this polynomial mod x^17, and this polynomial with
 * the leading coefficient deleted and replaced with x^6_4.  As-is,
 * it's irreducible, so it has a long period.  Modulo x^17, it factors as
 * (x^4+x^3+x^2+x+1) * (x^12+x^11+x^8+x^7+x^6+x^5+x^4+x^3+1),
 * which still has a large enough period (4095) for the use it's put to.
 * With the leading coefficient moved up, it factors as
 * (x^50+x^49+x^48+x^47+x^46+x^43+x^41+x^40+x^38+x^37+x^36+x^35+x^34+x^33+
 *  x^31+x^30+x^29+x^28+x^27+x^25+x^23+x^18+x^1_6+x^15+x^14+x^13+x^11+x^9+
 *  x^8+x^7+x^6+x^5+x^3+x^2+1)*(x^11+x^10+x^9+x^5+x^4+x^3+1)*(x^3+x+1),
 * which definitely has a long enough period to serve for initialization.
 * 
 * The effort put into this PRNG is kind of unwarranted given the trivial
 * use it's being put to, but oh, well.  It does have the nice advantage
 * of producing numbers that are portable between platforms, so if there's
 * a problem with one platform, you can compare all the intermediate
 * results with another platform.
 */
#define POLY (BNWORD32)0x04c11db7

static void
srand32(BNWORD32 seed)
{
    int i, j;

    for (i = 0; i < 55; i++) {
	for (j = 0; j < 32; j++)
	    if (seed >> (32-1))
		seed = (seed << 1) ^ POLY;
	    else
		seed <<= 1;
	randp[i] = seed;
    }
    for (i = 0; i < 3*55; i ++)
	rand32();
}

static void
randnum(BNWORD32 *num, unsigned len)
{
    while (len--)
	BIGLITTLE(*--num,*num++) = rand32();
}

static void
bnprint32(BNWORD32 const *num, unsigned len)
{
    BIGLITTLE(num -= len, num += len);

    while (len--)
	ZRTP_LOG(3, (_ZTU_, "%0*lX", 32/4, (unsigned long)BIGLITTLE(*num++,*--num)));
}

static void
bnput32(char const *prompt, BNWORD32 const *num, unsigned len)
{
    fputs(prompt, stdout);
    bnprint32(num, len);
    putchar('\n');
}

/*
 * One of our tests uses a known prime.  The following selections were
 * taken from the tables at the end of Hans Reisel's "Prime Numbers and
 * Computer Methods for Factorization", second edition - an excellent book.
 * (ISBN 0-8176-3743-5 ISBN 3-7643-3743-5)
 */
#if 0
/* P31=1839605 17620282 38179967 87333633 from the factors of 3^256+2^256 */
static unsigned char const prime[] = {
	0x17,0x38,0x15,0xBC,0x8B,0xBB,0xE9,0xEF,0x01,0xA9,0xFD,0x3A,0x01
};
#elif 0
/* P48=40554942 04557502 46193993 36199835 4279613_2 73199617 from the same */
static unsigned char const prime[] = {
	0x47,0x09,0x77,0x07,0xCF,0xFD,0xE1,0x54,0x3E,0x24,
	0xF7,0xF1,0x7A,0x3E,0x91,0x51,0xCC,0xC7,0xD4,0x01
};
#elif 0
/*
 * P75 = 450 55287640 97906895 47687014 5808213_2
 *  05219565 99525911 39967964 66003_258 91979521
 * from the factors of 4^128+3+128
 * (The "026" and "062" are to prevent a Bad String from appearing here.)
 */
static unsigned char const prime[] = {
	0xFF,0x00,0xFF,0x00,0xFF,0x01,0x06,0x4F,0xF8,0xED,
	0xA3,0x37,0x23,0x2A,0x04,0xEA,0xF9,0x5F,0x30,0x4C,
	0xAE,0xCD, 026,0x4E, 062,0x10,0x04,0x7D,0x0D,0x79,
	0x01
};
#else
/*
 * P75 = 664 85659796 45277755 9123_2190 67300940
 *  51844953 78793489 59444670 35675855 57440257
 * from the factors of 5^128+4^128
 * (The "026" is to prevent a Bad String from appearing here.)
 */
static unsigned char const prime[] = {
	0x01,0x78,0x4B,0xA5,0xD3,0x30,0x03,0xEB,0x73,0xE6,
	0x0F,0x4E,0x31,0x7D,0xBC,0xE2,0xA0,0xD4, 026,0x3F,
	0x3C,0xEA,0x1B,0x44,0xAD,0x39,0xE7,0xE5,0xAD,0x19,
	0x67,0x01
};
#endif

static int
usage(char const *name)
{
    ZRTP_LOG(3, (_ZTU_, "Usage: %s [modbits [expbits [expbits2]]"
"With no arguments, just runs test suite.  If modbits is given, runs\n"
"quick validation test, then runs timing tests of modular exponentiation.\n"
"If expbits is given, it is used as an exponent size, otherwise it defaults\n"
"to the same as modbits.  If expbits2 is given it is used as the second\n"
"exponent size in the double-exponentiation tests, otherwise it defaults\n"
"to the same as expbits.  All are limited to %u bits.\n",
	    name, (unsigned)MAXBITS));
    return 1;
}

/* for libzrtp support */
int
bntest_main(int argc, char **argv)
{
    unsigned i, j, k, l, m;
    int z;
    BNWORD32 t, carry, borrow;
    BNWORD32 a[SIZE], b[SIZE], c[SIZE], d[SIZE];
    BNWORD32 e[SIZE], f[SIZE];
    static BNWORD32 entries[sizeof(prime)*2][(sizeof(prime)-1)/(32/8)+1];
    BNWORD32 *array[sizeof(prime)*2];
    unsigned long modbits = 0, expbits = 0, expbits2 = 0;
    char *p;
#define A BIGLITTLE((a+SIZE),a)
#define B BIGLITTLE((b+SIZE),b)
#define C BIGLITTLE((c+SIZE),c)
#define D BIGLITTLE((d+SIZE),d)
#define E BIGLITTLE((e+SIZE),e)
#define F BIGLITTLE((f+SIZE),f)
    static unsigned const smallprimes[] = {
	2, 3, 5, 7, 11, 13, 17, 19, 23, 27, 29, 31, 37, 41, 43
    };
	
    /* Set up array for precomputed modexp */
    for (i = 0; i < sizeof(array)/sizeof(*array); i++)
	array[i] = entries[i] BIG(+ SIZE);

    srand32(1);

    puts(BIGLITTLE("Big-endian machine","Little-endian machine"));

    if (argc >= 2) {
	modbits = strtoul(argv[1], &p, 0);
	if (!modbits || *p) {
		ZRTP_LOG(1, (_ZTU_, "Invalid modbits: %s", argv[1]));
		return usage(argv[0]);
	}
    }
    if (argc >= 3) {
	expbits = strtoul(argv[2], &p, 0);
	if (!expbits || *p) {
		ZRTP_LOG(1, (_ZTU_, "Invalid expbits: %s", argv[2]));
		return usage(argv[0]);
	}
	expbits2 = expbits;
    }
    if (argc >= 4) {
	expbits2 = strtoul(argv[3], &p, 0);
	if (!expbits2 || *p) {
		ZRTP_LOG(1, (_ZTU_, "Invalid expbits2: %s", argv[3]));
		return usage(argv[0]);
	}
    }
    if (argc >= 5) {
	ZRTP_LOG(1, (_ZTU_, "Too many arguments: %s", argv[4]));
	return usage(argv[0]);
    }
    
/* B is a nice not-so-little prime */
    lbnInsertBigBytes_32(B, prime, 0, sizeof(prime));
    ((unsigned char *)c)[0] = 0;
    lbnInsertBigBytes_32(B, (unsigned char *)c, sizeof(prime), 1);
    lbnExtractBigBytes_32(B, (unsigned char *)c, 0, sizeof(prime)+1);
    i = (sizeof(prime)-1)/(32/8)+1;        /* Size of array in words */
    if (((unsigned char *)c)[0] ||
	memcmp(prime, (unsigned char *)c+1, sizeof(prime)) != 0)
    {
	ZRTP_LOG(3, (_ZTU_, "Input != output!:   "));
	for (k = 0; k < sizeof(prime); k++)
	    ZRTP_LOG(3, (_ZTU_, "%02X ", prime[k]));
	putchar('\n');
	for (k = 0; k < sizeof(prime)+1; k++)
	    ZRTP_LOG(3, (_ZTU_, "%02X ", ((unsigned char *)c)[k]));
	putchar('\n');
	bnput32("p = ", B, i);

    }

    /* Timing test code - only if requested on the command line */
    if (modbits) {
#if CLOCK_AVAIL
	timetype start, stop;
	unsigned long cursec, expsec, twoexpsec, dblexpsec;
	unsigned curms, expms, twoexpms, dblexpms;

	expsec = twoexpsec = dblexpsec = 0;
	expms = twoexpms = dblexpms = 0;
#endif

	lbnCopy_32(C,B,i);
	lbnSub1_32(C,i,1);        /* C is exponent: p-1 */

	puts("Testing modexp with a known prime.  "
	     "All results should be 1.");
	bnput32("p   = ", B, i);
	bnput32("p-1 = ", C, i);
	z = lbnTwoExpMod_32(A, C, i, B, i);
	if (z < 0)
	    goto nomem;
	bnput32("2^(p-1) mod p = ", A, i);
	for (j = 0; j < 10; j++) {
	    randnum(A,i);
	    (void)lbnDiv_32(D,A,i,B,i);

	    bnput32("a = ", A, i);
	    z = lbnExpMod_32(D, A, i, C, i, B, i);
	    if (z < 0)
		goto nomem;
	    bnput32("a^(p-1) mod p = ", D, i);
		
	    z = lbnBasePrecompBegin_32(array, (sizeof(prime)*8+4)/5, 5,
				       A, i, B, i);
	    if (z < 0)
		goto nomem;
	    BIGLITTLE(D[-1],D[0]) = -1;
	    z = lbnBasePrecompExp_32(D, (BNWORD32 const * const *)array,
			   	     5, C, i, B, i);
	    if (z < 0)
		goto nomem;
	    bnput32("a^(p-1) mod p = ", D, i);
		
	    for (k = 0; k < 5; k++) {
		randnum(E,i);
		bnput32("e = ", E, i);
		z = lbnExpMod_32(D, A, i, E, i, B, i);
		if (z < 0)
		    goto nomem;
		bnput32("a^e mod p = ", D, i);
		z = lbnBasePrecompExp_32(D, (BNWORD32 const * const *)array,
					 5, E, i, B, i);
		if (z < 0)
		    goto nomem;
		bnput32("a^e mod p = ", D, i);
	    }	
	}

	ZRTP_LOG(3, (_ZTU_, "\n"
	       "Timing exponentiations modulo a %d-bit modulus, i.e.\n"
	       "2^<%d> mod <%d> bits, <%d>^<%d> mod <%d> bits and\n"
	       "<%d>^<%d> * <%d>^<%d> mod <%d> bits",
	       (int)modbits, (int)expbits, (int)modbits,
	       (int)modbits, (int)expbits, (int)modbits,
	       (int)modbits, (int)expbits, (int)modbits, (int)expbits2,
	       (int)modbits));

	i = ((int)modbits-1)/32+1;
	k = ((int)expbits-1)/32+1;
	l = ((int)expbits2-1)/32+1;
	for (j = 0; j < 25; j++) {
	    randnum(A,i);        /* Base */
	    randnum(B,k);        /* Exponent */
	    randnum(C,i);        /* Modulus */
	    randnum(D,i);        /* Base2 */
	    randnum(E,l);        /* Exponent */
	    /* Clip bases and mod to appropriate number of bits */
	    t = ((BNWORD32)2<<((modbits-1)%32)) - 1;
	    *(BIGLITTLE(A-i,A+i-1)) &= t;
	    *(BIGLITTLE(C-i,C+i-1)) &= t;
	    *(BIGLITTLE(D-i,D+i-1)) &= t;
	    /* Make modulus large (msbit set) and odd (lsbit set) */
	    *(BIGLITTLE(C-i,C+i-1)) |= (t >> 1) + 1;
	    BIGLITTLE(C[-1],C[0]) |= 1;

	    /* Clip exponent to appropriate number of bits */
	    t = ((BNWORD32)2<<((expbits-1)%32)) - 1;
	    *(BIGLITTLE(B-k,B+k-1)) &= t;
	    /* Make exponent large (msbit set) */
	    *(BIGLITTLE(B-k,B+k-1)) |= (t >> 1) + 1;
	    /* The same for exponent 2 */
	    t = ((BNWORD32)2<<((expbits2-1)%32)) - 1;
	    *(BIGLITTLE(E-l,E+l-1)) &= t;
	    *(BIGLITTLE(E-l,E+l-1)) |= (t >> 1) + 1;

	    m = lbnBits_32(A, i);
	    if (m > (unsigned)modbits) {
		bnput32("a = ", a, i);
		ZRTP_LOG(3, (_ZTU_, "%u bits, should be <= %d", m, (int)modbits));
	    }
	    m = lbnBits_32(B, k);
	    if (m != (unsigned)expbits) {
		bnput32("b = ", b, i);
		ZRTP_LOG(3, (_ZTU_, "%u bits, should be %d", m, (int)expbits));
	    }
	    m = lbnBits_32(C, i);
	    if (m != (unsigned)modbits) {
		bnput32("c = ", c, k);
		ZRTP_LOG(3, (_ZTU_, "%u bits, should be %d", m, (int)modbits));
	    }
	    m = lbnBits_32(D, i);
	    if (m > (unsigned)modbits) {
		bnput32("d = ", d, i);
		ZRTP_LOG(3, (_ZTU_, "%u bits, should be <= %d", m, (int)modbits));
	    }
	    m = lbnBits_32(E, l);
	    if (m != (unsigned)expbits2) {
		bnput32("e = ", e, i);
		ZRTP_LOG(3, (_ZTU_, "%u bits, should be %d", m, (int)expbits2));
	    }
#if CLOCK_AVAIL
		gettime(&start);
#endif
	    z = lbnTwoExpMod_32(A, B, k, C, i);
	    if (z < 0)
		goto nomem;
#if CLOCK_AVAIL
	    gettime(&stop);
	    subtime(stop, start);
	    twoexpsec += cursec = sec(stop);
	    twoexpms += curms = msec(stop);

	    ZRTP_LOG(3, (_ZTU_, "2^<%d>:%4lu.%03u   ", (int)expbits, cursec, curms));
#else
		ZRTP_LOG(3, (_ZTU_, "<%d>^<%d>    ", (int)modbits, (int)expbits));
#endif
	    fflush(stdout);

#if CLOCK_AVAIL
	    gettime(&start);
#endif
	    z = lbnExpMod_32(A, A, i, B, k, C, i);
	    if (z < 0)
		goto nomem;
#if CLOCK_AVAIL
	    gettime(&stop);
	    subtime(stop, start);
	    expsec += cursec = sec(stop);
	    expms += curms = msec(stop);
	    ZRTP_LOG(3, (_ZTU_, "<%d>^<%d>:%4lu.%03u   ",(int)modbits, (int)expbits, cursec, curms));
	    fflush(stdout);

	    gettime(&start);
	    z = lbnDoubleExpMod_32(D, A, i, B, k, D, i, E, l,C,i);
	    if (z < 0)
		goto nomem;
	    gettime(&stop);
	    subtime(stop, start);
	    dblexpsec += cursec = sec(stop);
	    dblexpms += curms = msec(stop);
	    ZRTP_LOG(3, (_ZTU_, "<%d>^<%d>*<%d>^<%d>:%4lu.%03u",
		   (int)modbits, (int)expbits,
		   (int)modbits, (int)expbits2,
		   cursec, curms));
#else
		ZRTP_LOG(3, (_ZTU_, "<%d>^<%d>*<%d>^<%d>",
			(int)modbits, (int)expbits,
			(int)modbits, (int)expbits2));
#endif
	}
#if CLOCK_AVAIL
	twoexpms += (twoexpsec % j) * 1000;
	ZRTP_LOG(3, (_ZTU_, "2^<%d> mod <%d> bits AVERAGE: %4lu.%03u s",
	       (int)expbits, (int)modbits, twoexpsec/j, twoexpms/j));
	expms += (expsec % j) * 1000;
	ZRTP_LOG(3, (_ZTU_, "<%d>^<%d> mod <%d> bits AVERAGE: %4lu.%03u s",
	       (int)modbits, (int)expbits, (int)modbits, expsec/j, expms/j));
	dblexpms += (dblexpsec % j) * 1000;
	ZRTP_LOG(3, (_ZTU_, "<%d>^<%d> * <%d>^<%d> mod <%d> bits AVERAGE:"
	       " %4lu.%03u s",
	       (int)modbits, (int)expbits, (int)modbits, 
	       (int)expbits2,
	       (int)modbits, dblexpsec/j, dblexpms/j));

	putchar('\n');
#endif
    }

    puts("Beginning 1000 interations of sanity checking.\n"
	 "Any output indicates a bug.  No output is very strong\n"
	 "evidence that all the important low-level bignum routines\n"
	 "are working properly.\n");

    /*
     * If you change this loop to have an iteration 0, all results
     * are primted on that iteration.  Useful to see what's going
     * on in case of major wierdness, but it produces a *lot* of
     * output.
     */
#if (ZRTP_PLATFORM == ZP_WINCE) || (ZRTP_PLATFORM == ZP_SYMBIAN)
	for (j = 1; j <= 20; j++) {
#else
    for (j = 1; j <= 1000; j++) {
#endif
/* Do the tests for lots of different number sizes. */
	for (i = 1; i <= SIZE/2; i++) {
	    /* Make a random number i words long */
	    do {
		randnum(A,i);
	    } while (lbnNorm_32(A,i) < i);

	    /* Checl lbnCmp - does a == a? */
	    if (lbnCmp_32(A,A,i) || !j) {
		bnput32("a = ", A, i);
		ZRTP_LOG(3, (_ZTU_, "(a <=> a) = %d", lbnCmp_32(A,A,i)));
	    }

	    memcpy(c, a, sizeof(a));

	    /* Check that the difference, after copy, is good. */
	    if (lbnCmp_32(A,C,i) || !j) {
		bnput32("a = ", A, i);
		bnput32("c = ", C, i);
		ZRTP_LOG(3, (_ZTU_, "(a <=> c) = %d", lbnCmp_32(A,C,i)));
	    }

	    /* Generate a non-zero random t */
	    do {
		t = rand32();
	    } while (!t);

	    /*
	     * Add t to A.  Check that:
	     * - lbnCmp works in both directions, and
	     * - A + t is greater than A.  If there was a carry,
	     *   the result, less the carry, should be *less*
	     *   than A.
	     */
	    carry = lbnAdd1_32(A,i,t);
	    if (lbnCmp_32(A,C,i) + lbnCmp_32(C,A,i) != 0 ||
		lbnCmp_32(A,C,i) != (carry ? -1 : 1) || !j)
	    {
		bnput32("c       = ", C, i);
		ZRTP_LOG(3, (_ZTU_, "t = %lX", (unsigned long)t));
		bnput32("a = c+t = ", A, i);
		ZRTP_LOG(3, (_ZTU_, "carry = %lX", (unsigned long)carry));
		ZRTP_LOG(3, (_ZTU_, "(a <=> c) = %d", lbnCmp_32(A,C,i)));
		ZRTP_LOG(3, (_ZTU_, "(c <=> a) = %d", lbnCmp_32(C,A,i)));
	    }

	    /* Subtract t again */
	    memcpy(d, a, sizeof(a));
	    borrow = lbnSub1_32(A,i,t);

	    if (carry != borrow || lbnCmp_32(A,C,i) || !j) {
		bnput32("a = ", C, i);
		ZRTP_LOG(3, (_ZTU_, "t = %lX", (unsigned long)t));
		lbnAdd1_32(A,i,t);
		bnput32("a += t = ", A, i);
		ZRTP_LOG(3, (_ZTU_, "Carry = %lX", (unsigned long)carry));
		lbnSub1_32(A,i,t);
		bnput32("a -= t = ", A, i);
		ZRTP_LOG(3, (_ZTU_, "Borrow = %lX", (unsigned long)borrow));
		ZRTP_LOG(3, (_ZTU_, "(a <=> c) = %d", lbnCmp_32(A,C,i)));
	    }

	    /* Generate a random B */
	    do {
		randnum(B,i);
	    } while (lbnNorm_32(B,i) < i);

	    carry = lbnAddN_32(A,B,i);
	    memcpy(d, a, sizeof(a));
	    borrow = lbnSubN_32(A,B,i);

	    if (carry != borrow || lbnCmp_32(A,C,i) || !j) {
		bnput32("a = ", C, i);
		bnput32("b = ", B, i);
		bnput32("a += b = ", D, i);
		ZRTP_LOG(3, (_ZTU_, "Carry = %lX", (unsigned long)carry));
		bnput32("a -= b = ", A, i);
		ZRTP_LOG(3, (_ZTU_, "Borrow = %lX", (unsigned long)borrow));
		ZRTP_LOG(3, (_ZTU_, "(a <=> c) = %d", lbnCmp_32(A,C,i)));
	    }

	    /* D = B * t */
	    lbnMulN1_32(D, B, i, t);
	    memcpy(e, d, sizeof(e));
	    /* D = A + B * t, "carry" is overflow */
	    borrow = *(BIGLITTLE(D-i-1,D+i)) += lbnAddN_32(D,A,i);

	    carry = lbnMulAdd1_32(A, B, i, t);

	    /* Did MulAdd get the same answer as mul then add? */
	    if (carry != borrow || lbnCmp_32(A, D, i) || !j) {
		bnput32("a = ", C, i);
		bnput32("b = ", B, i);
		ZRTP_LOG(3, (_ZTU_, "t = %lX", (unsigned long)t));
		bnput32("e = b * t = ", E, i+1);
		bnput32("    a + e = ", D, i+1);
		bnput32("a + b * t = ", A, i);
		ZRTP_LOG(3, (_ZTU_, "carry = %lX", (unsigned long)carry));
	    }

	    memcpy(d, a, sizeof(a));
	    borrow = lbnMulSub1_32(A, B, i, t);

	    /* Did MulSub perform the inverse of MulAdd */
	    if (carry != borrow || lbnCmp_32(A,C,i) || !j) {
		bnput32("       a = ", C, i);
		bnput32("       b = ", B, i);
		bnput32("a += b*t = ", D, i);
		ZRTP_LOG(3, (_ZTU_, "Carry = %lX", (unsigned long)carry));
		bnput32("a -= b*t = ", A, i);
		ZRTP_LOG(3, (_ZTU_, "Borrow = %lX", (unsigned long)borrow));
		ZRTP_LOG(3, (_ZTU_, "(a <=> c) = %d", lbnCmp_32(A,C,i)));
		bnput32("b*t = ", E, i+1);
	    }
	    /* At this point we're done with t, so it's scratch */
#if 0
/* Extra debug code */
	    lbnMulN1_32(C, A, i, BIGLITTLE(B[-1],B[0]));
	    bnput32("a * b[0] = ", C, i+1);
	    for (k = 1; k < i; k++) {
		carry = lbnMulAdd1_32(BIGLITTLE(C-k,C+k), A, i, 
				      *(BIGLITTLE(B-1-k,B+k)));
		*(BIGLITTLE(C-i-k,C+i+k)) = carry;
		bnput32("a * b[x] = ", C, i+k+1);
	    }

	    lbnMulN1_32(D, B, i, BIGLITTLE(A[-1],A[0]));
	    bnput32("b * a[0] = ", D, i+1);
	    for (k = 1; k < i; k++) {
		carry = lbnMulAdd1_32(BIGLITTLE(D-k,D+k), B, i, 
				      *(BIGLITTLE(A-1-k,A+k)));
		*(BIGLITTLE(D-i-k,D+i+k)) = carry;
		bnput32("b * a[x] = ", D, i+k+1);
	    }
#endif
	    /* Does Mul work both ways symmetrically */
	    lbnMul_32(C,A,i,B,i);
	    lbnMul_32(D,B,i,A,i);
	    if (lbnCmp_32(C,D,i+i) || !j) {
		bnput32("a = ", A, i);
		bnput32("b = ", B, i);
		bnput32("a * b = ", C, i+i);
		bnput32("b * a = ", D, i+i);
		ZRTP_LOG(3, (_ZTU_, "(a*b <=> b*a) = %d", lbnCmp_32(C,D,i+i)));
	    }
	    /* Check multiplication modulo some small things */
	    /* 30030 = 2*3*5*11*13 */
	    k = lbnModQ_32(C, i+i, 30030);
	    for (l = 0;
		 l < sizeof(smallprimes)/sizeof(*smallprimes);
		 l++)
	    {
		m = smallprimes[l];
		t = lbnModQ_32(C, i+i, m);
		carry = lbnModQ_32(A, i, m);
		borrow = lbnModQ_32(B, i, m);
		if (t != (carry * borrow) % m) {
		    bnput32("a = ", A, i);
		    ZRTP_LOG(3, (_ZTU_, "a mod %u = %u", m, (unsigned)carry));
		    bnput32("b = ", B, i);
		    ZRTP_LOG(3, (_ZTU_, "b mod %u = %u", m, (unsigned)borrow));
		    bnput32("a*b = ", C, i+i);
		    ZRTP_LOG(3, (_ZTU_, "a*b mod %u = %u", m, (unsigned)t));
		    ZRTP_LOG(3, (_ZTU_, "expected %u", (unsigned)((carry*borrow)%m)));
		}
				/* Verify that (C % 30030) % m == C % m */
		if (m <= 13 && t != k % m) {
		    ZRTP_LOG(3, (_ZTU_, "c mod 30030 = %u mod %u= %u", k, m, k%m));
		    ZRTP_LOG(3, (_ZTU_, "c mod %u = %u", m, (unsigned)t));
		}
	    }

	    /* Generate an F less than A and B */
	    do {
		randnum(F,i);
	    } while (lbnCmp_32(F,A,i) >= 0 ||
		     lbnCmp_32(F,B,i) >= 0);

	    /* Add F to D (remember, D = A*B) */
	    lbnAdd1_32(BIGLITTLE(D-i,D+i), i, lbnAddN_32(D, F, i));
	    memcpy(c, d, sizeof(d));

	    /*
	     * Divide by A and check that quotient and remainder
	     * match (remainder should be F, quotient should be B)
	     */
	    t = lbnDiv_32(E,C,i+i,A,i);
	    if (t || lbnCmp_32(E,B,i) || lbnCmp_32(C, F, i) || !j) {
		bnput32("a = ", A, i);
		bnput32("b = ", B, i);
		bnput32("f = ", F, i);
		bnput32("a * b + f = ", D, i+i);
		ZRTP_LOG(3, (_ZTU_, "qhigh = %lX", (unsigned long)t));
		bnput32("(a*b+f) / a = ", E, i);
		bnput32("(a*b+f) % a = ", C, i);
	    }

	    memcpy(c, d, sizeof(d));

	    /* Divide by B and check similarly */
	    t = lbnDiv_32(E,C,i+i,B,i);
	    if (lbnCmp_32(E,A,i) || lbnCmp_32(C, F, i) || !j) {
		bnput32("a = ", A, i);
		bnput32("b = ", B, i);
		bnput32("f = ", F, i);
		bnput32("a * b + f = ", D, i+i);
		ZRTP_LOG(3, (_ZTU_, "qhigh = %lX", (unsigned long)t));
		bnput32("(a*b+f) / b = ", E, i);
		bnput32("(a*b+f) % b = ", C, i);
	    }

	    /* Check that A*A == A^2 */
	    lbnMul_32(C,A,i,A,i);
	    lbnSquare_32(D,A,i);
	    if (lbnCmp_32(C,D,i+i) || !j) {
		bnput32("a*a = ", C, i+i);
		bnput32("a^2 = ", D, i+i);
		ZRTP_LOG(3, (_ZTU_, "(a * a == a^2) = %d", lbnCmp_32(C,D,i+i)));
	    }

	    /* Compute a GCD */
	    lbnCopy_32(C,A,i);
	    lbnCopy_32(D,B,i);
	    z = lbnGcd_32(C, i, D, i, &k);
	    if (z < 0)
		goto nomem;
	    /* z = 1 if GCD in D; z = 0 if GCD in C */

	    /* Approximate check that the GCD came out right */
	    for (l = 0;
		 l < sizeof(smallprimes)/sizeof(*smallprimes);
		 l++)
	    {
		m = smallprimes[l];
		t = lbnModQ_32(z ? D : C, k, m);
		carry = lbnModQ_32(A, i, m);
		borrow = lbnModQ_32(B, i, m);
		if (!t != (!carry && !borrow)) {
		    bnput32("a = ", A, i);
		    ZRTP_LOG(3, (_ZTU_, "a mod %u = %u", m, (unsigned)carry));
		    bnput32("b = ", B, i);
		    ZRTP_LOG(3, (_ZTU_, "b mod %u = %u", m, (unsigned)borrow));
		    bnput32("gcd(a,b) = ", z ? D : C, k);
		    ZRTP_LOG(3, (_ZTU_, "gcd(a,b) mod %u = %u", m, (unsigned)t));
		}
	    }


	    /*
	     * Do some Montgomery operations
	     * Start with A > B, and also place a copy of B into C.
	     * Then make A odd so it can be a Montgomery modulus.
	     */
	    if (lbnCmp_32(A, B, i) < 0) {
		memcpy(c, a, sizeof(c));
		memcpy(a, b, sizeof(a));
		memcpy(b, c, sizeof(b));
	    } else {
		memcpy(c, b, sizeof(c));
	    }
	    BIGLITTLE(A[-1],A[0]) |= 1;
			
	    /* Convert to and from */
	    lbnToMont_32(B, i, A, i);
	    lbnFromMont_32(B, A, i);
	    if (lbnCmp_32(B, C, i)) {
		memcpy(b, c, sizeof(c));
		bnput32("mod = ", A, i);
		bnput32("input = ", B, i);
		lbnToMont_32(B, i, A, i);
		bnput32("mont = ", B, i);
		lbnFromMont_32(B, A, i);
		bnput32("output = ", B, i);
	    }
	    /* E = B^5 (mod A), no Montgomery ops */
	    lbnSquare_32(E, B, i);
	    (void)lbnDiv_32(BIGLITTLE(E-i,E+i),E,i+i,A,i);
	    lbnSquare_32(D, E, i);
	    (void)lbnDiv_32(BIGLITTLE(D-i,D+i),D,i+i,A,i);
	    lbnMul_32(E, D, i, B, i);
	    (void)lbnDiv_32(BIGLITTLE(E-i,E+i),E,i+i,A,i);

	    /* D = B^5, using ExpMod */
	    BIGLITTLE(F[-1],F[0]) = 5;
	    z = lbnExpMod_32(D, B, i, F, 1, A, i);
	    if (z < 0)
		goto nomem;
	    if (lbnCmp_32(D, E, i)  || !j) {
		bnput32("mod = ", A, i);
		bnput32("input = ", B, i);
		bnput32("input^5 = ", E, i);
		bnput32("input^5 = ", D, i);
		ZRTP_LOG(3, (_ZTU_, "a>b (x <=> y) = %d", lbnCmp_32(D,E,i)));
	    }
	    /* TODO: Test lbnTwoExpMod, lbnDoubleExpMod */
	} /* for (i) */
	ZRTP_LOG(3, (_ZTU_, "\r%d ", j));
	fflush(stdout);
    } /* for (j) */
    ZRTP_LOG(3, (_ZTU_, "%d iterations of up to %d 32-bit words completed.", j-1, i-1));
    return 0;
nomem:
    ZRTP_LOG(3, (_ZTU_, "Out of memory"));
    return 1;
}
