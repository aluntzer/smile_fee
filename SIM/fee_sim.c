/**
 * @file   fee_sim.c
 * @author Armin Luntzer (armin.luntzer@univie.ac.at),
 * @date   2022
 *
 * @copyright GPLv2
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * @brief SMILE FEE simulator
 *
 * NOTE: I don't know the actual orientation of the CCDs coordinate system,
 *	 so the frames could be flipped upside down. This may however not be of
 *	 any consequence.
 *
 */

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fee_sim.h>
#include <smile_fee.h>
#include <smile_fee_cfg.h>
#include <smile_fee_ctrl.h>
#include <smile_fee_rmap.h>

#include <byteorder.h>

#include <sys/time.h>
#include <fitsio.h>


static uint16_t *CCD2E;
static uint16_t *CCD2F;
static uint16_t *CCD4E;
static uint16_t *CCD4F;
static uint16_t *RDO;

static uint16_t frame_cntr;

struct fee_data_payload {
	struct fee_data_pkt *pkt;
	size_t data_len_max;
};






/* begin ccd sim TODO: separate */

#include <math.h>

/* values values from "The SMILE Soft X-ray Imager (SXI) CCD design and
 * development" (Soman et al) */
/* nominal photon energy range */
#define SWCX_PHOT_EV_MIN	 200.0
#define SWCX_PHOT_EV_MAX	2000.0
/* pixel responsitivity in µV/electron */
#define CDD_RESP_uV_e		7.0
/* the gain equivalent, currently in discrepancy to value above*/
#define CCD_ADC_GAIN		40.

/* next few characteristics are just guesses based on a
 * typical CMOS ADC 5V input range and the responsitivity above
 * full well capacity in e-, */
#define CCD_N_FWC		714e3
/* dark signal, e-/pix/s, also CCD270-ish */
#define CCD_DARK		0.5
/* dark signal non-uniformity, as above */
#define CCD_DAR_NONUNI		0.05
/* readout noise e- rms */
#define CCD_NOISE		20.

/* number of electrons generated per 1 eV,
 * we will assume linear behaviour */
#define e_PER_eV		(55.0/200.0)
/* ccd thickness in µm, we us that for our cosmics */
#define CCD_THICKNESS_um	16.0
/* a ccd side length in mm */
#define CCD_SIDE_mm		81.8
/* ccd is partially covered for readout, this is the height
 * of the imaging area in mm */
#define CCD_IMG_HEIGHT_mm	68.24
/* the number of ccd pixels for a given row or column */
#define CCD_PIX_PER_AX		4510
/* the side dimensions of a pixel in µmn */
#define PIXEL_LEN_um		((81.8 * 1000.) / CCD_PIX_PER_AX)

/* we have 16 bit ADC res(allegedly) */
#define PIX_SATURATION		((1 << 16) - 1)


/* some values from SMILE SXI CCD Testing and Calibration Event
 * Detection Methodology TN 1.2 (Soman et al)
 */
/* the following integrated event rates are in counts/CCD/s for the illuminated
 * section of a CCD;
 *
 * note that we could add sigmas for the particular rates, but for now we'll
 * just use these as the mean with a sigma of 1
 */
/* solar wind exchange x-ray event rate for low and high solar activity */
#define SWCX_CCD_RATE_MIN	5.134
#define SWCX_CCD_RATE_MAX	82.150
/* soft x-ray thermal galacitc background event rate */
#define SXRB_CCD_RATE		15.403
/* (solar?) particle background event rate */
#define PB_CCD_RATE_MIN		0.627
#define PB_CCD_RATE_MAX		1.255
/* mean point source rate over FOV */
#define PS_CCD_RATE		0.657
/* 4pi cosmic ray flux (in particles per CCD as well) */
#define COSMIC_FLUX		24.61


/* we assume solar particle energies of 1 keV to 100 keV
 * taken from
 * Long-Term Fluences of Energetic Particles in the Heliosphere (Mewaldt et al)
 *
 * NOTE: the values for (what I assume) refer to solar wind contribution
 * (PB_CCD_RATE_...) appear a little low, on the other hand, their
 * direction more or less correspond to the plane fo the CCDs, since
 * we're looking mostly in a direction which is perpendicular-ish to the Sun.
 */
#define SOLAR_PARTICLE_EV_MIN		1e03
#define SOLAR_PARTICLE_EV_MAX		1e05

/* we assume cosmic particle energies of 10 MeV to 10 GeV */
#define COSMIC_PARTICLE_EV_MIN		1e07
#define COSMIC_PARTICLE_EV_MAX		1e11
/* energy distribution per magnitute over min (i.e. x=0, 1, 2 ...) */
#define PARTICLE_DROPOFF	1.0		/* extra drop */
#define PARTICLE_RATE_DROP(x)	powf(10., -(x) * PARTICLE_DROPOFF + 1.)
/**
 * the particle energy loss follows a type of Landau distribution,
 * but appears to be relatively uniform regardless of the particle energy
 * 0.23 keV/µm of silicone appears to be a sensible value for our purposes, see
 * doi 10.1088/1748-0221/6/06/p06013
 *
 * given our ccd thickness, this would amount to a deposition on the order
 * of ~10k e- per pixel, which is only a few % of its FWC, which really
 * isn't all that much
 */
#define PARTICLE_ENERGY_LOSS_PER_UM_EV 230.

/* restrict the incident angle towards the CCD plane of the solar wind */
#define SOLAR_WIND_EL_ANGLE_MAX	(10. / 180. * M_PI)
/* restrict the azimuth angle in the CCD plane of the solar wind */
#define SOLAR_WIND_AZ_ANGLE_MAX	(30. / 180. * M_PI)


/* other sim config */
/* maximum random scattering angle (degrees) for particle deflection simulation;
 * the higher this value, the less the total probability of a deflected
 * particle trail occuring in the CCD image
 */
#define RUTHERFORD_SCATTER_ANGLE_MAX	90
/* solar activity selector, range 0-1 */
#define SOLAR_ACT		1.0
/* number of pre-computed dark samples for faster simulation */
#define DARK_SAMPLES		128
/* simulate dark current (0/1) */
#define CFG_SIM_DARK		0
/* number of pre-computed readout noise samples for faster simulation */
#define RD_NOISE_SAMPLES	128

/* probability of charge transfer inefficiency occuring in a column */
#define CTI_PROB		0.1
/* bleed rate percentage if a CTI happens */
#define CTI_BLEED		0.1
/* probability of multi-pixel hits; set for testing; reasonable value: 0.002 */
#define MULTIPIX_HIT_PROB	0.5



static void save_fits(const char *name, uint16_t *buf, long rows, long cols)
{
	long naxes[3];
	long n_elem;
	int status = 0;
	fitsfile *ff;
	long fpixel[3] = {1, 1, 1};



	naxes[0] = cols;
	naxes[1] = rows;
	naxes[2] = 1;
	n_elem = naxes[0] * naxes[1] * naxes[2];

	if (fits_create_file(&ff, name, &status)) {
		printf("error %d\n", status);
		exit(-1);
	}

	if (fits_create_img(ff, USHORT_IMG, 3, naxes, &status)) {
		printf("error %d\n", status);
		exit(-1);
	}

	if (fits_write_pix(ff, TUSHORT, fpixel, n_elem, buf, &status)) {
		printf("error %d (%d)\n",status, __LINE__);
		exit(-1);
	}

	if (fits_close_file(ff, &status)) {
		printf("error %d (%d)\n",status, __LINE__);
		exit(-1);
	}
}


/**
 * @brief get a random number between 0 and 1 following a logarithmic
 * distribution
 * @note the base resolution is fixed to 1/1000
 */
static float sim_rand_log(void)
{
#define LOG_RAND_RES 1000
	float u;
	const float res = 1.0 / LOG_RAND_RES;

	u = 1.0 - res * (rand() % LOG_RAND_RES);

	return log(u) / log(res);
}



/**
 * @brief a box-muller gaussian-like distribution
 */

static float sim_rand_gauss(void)
{
	static int gen;

	static float u, v;


	gen = !gen;

	if (!gen)
		return sqrtf(- 2.0 * logf(u)) * cosf(2.0 * M_PI * v);

	u = (rand() + 1.0) / (RAND_MAX + 2.0);
	v =  rand()        / (RAND_MAX + 1.0);

	return sqrtf(-2.0 * logf(u)) * sinf(2.0 * M_PI * v);
}


static uint16_t ccd_sim_get_swcx_ray(void)
{
	float p;

	/* we assume the incident x-ray energy is uniformly distributed */
	p = fmodf(rand(), (SWCX_PHOT_EV_MAX + 1 - SWCX_PHOT_EV_MIN) * 1000.) * 0.001;
        p += SWCX_PHOT_EV_MIN;
	p *= e_PER_eV;
	p *= CDD_RESP_uV_e;	/* scale to voltage-equivalent */

	if (p > PIX_SATURATION)
		return PIX_SATURATION;
	else
		return (uint16_t) p;
}


static void ccd_sim_add_swcx(uint16_t *frame, uint16_t tint_ms)
{
	size_t n = FEE_CCD_IMG_SEC_ROWS * FEE_CCD_IMG_SEC_COLS;
	const float sigma = 1.0;
	float amp;
	float ray;
	float tint = (float) tint_ms / 1000.0;

	size_t i;
	size_t x, y;
	size_t pix;


	/* our event amplitude is the integration time for the configured
	 * event rates
	 */
	amp = tint * (SWCX_CCD_RATE_MIN + SOLAR_ACT * (SWCX_CCD_RATE_MAX - SWCX_CCD_RATE_MIN));

	/* use the square of the amplitude to scale the noise */
	amp = amp + sqrtf(amp) * sigma * sim_rand_gauss();

	printf("SWCX: %d rays produced\n", (int) amp/2);

	for (i = 0; i < ((size_t) amp / 2); i++) {

		ray = ccd_sim_get_swcx_ray();
		pix = rand() % (n + 1);

		/* trigger on random occurence (retval != 0) */
		if (rand() % ((int) (1. / MULTIPIX_HIT_PROB))) {
			frame[pix] += ray;
		} else {
			float fray = (float) ray;
			x = pix % FEE_CCD_IMG_SEC_COLS;
			y = (pix - x) / FEE_CCD_IMG_SEC_COLS;

			/* XXX meh... this function needs cleanup
			 * what's going on here: distribute hit power
			 * to adjacent pixels
			 */
			while (fray > 0.0) {
				int yy = 1 - rand() % 2;
				int xx = 1 - rand() % 2;
				ssize_t pp = (yy + y) * FEE_CCD_IMG_SEC_COLS + (xx + x);
				float bleedoff = ((float) (rand() % 100)) * 0.01 * fray;

				/* out of bounds? */
				if (pp < 0 || pp > (ssize_t) n)
					continue;

				/* make sure to reasonably abort the loop */
				if (bleedoff < 0.05 * (float) ray)
					bleedoff = fray;

				frame[pp] += bleedoff;
				fray -= bleedoff;
			}
		}
	}
}



static void ccd_sim_add_sxrb(uint16_t *frame, uint16_t tint_ms)
{
	size_t n = FEE_CCD_IMG_SEC_ROWS * FEE_CCD_IMG_SEC_COLS;
	const float sigma = 1.0;
	float amp;
	float tint = (float) tint_ms / 1000.0;

	size_t i;

	/* XXX add configurable CTI effect */

	/* our event amplitude is the integration time for the configured
	 * event rates
	 */
	amp = tint * SXRB_CCD_RATE;

	/* use the square of the amplitude to scale the noise */
	amp = amp + sqrtf(amp) * sigma * sim_rand_gauss();
	/* we use the same energies as SWCX */
	for (i = 0; i < ((size_t) amp / 2); i++)
		frame[rand() % (n + 1)] += ccd_sim_get_swcx_ray();
}


/**
 * @brief get a particle within the given energy range distribution
 */

static float ccd_sim_get_cosmic_particle(void)
{
	const float pmin = log10f(COSMIC_PARTICLE_EV_MIN);
	const float pmax = log10f(COSMIC_PARTICLE_EV_MAX);

	float r, p;

	/* get a energy range exponent */
	r = fmodf(rand(), (pmax + 1. - pmin) * 1000.) * 0.001;
	/* distribute to (logarithmic) particle rate */
	r = PARTICLE_RATE_DROP(r);

	/* get energy of the particle */
	p = COSMIC_PARTICLE_EV_MIN + (COSMIC_PARTICLE_EV_MAX - COSMIC_PARTICLE_EV_MIN) * r;
	p *= e_PER_eV;

	return p;
}


/**
 * @brief get a solar particle within the given energy range distribution
 */

static float ccd_sim_get_solar_particle(void)
{
	const float pmin = log10f(SOLAR_PARTICLE_EV_MIN);
	const float pmax = log10f(SOLAR_PARTICLE_EV_MAX);

	float r, p;

	/* get a energy range exponent */
	r = fmodf(rand(), (pmax + 1. - pmin) * 1000.) * 0.001;

	/* we assume equal probability for solar wind components */

	/* get energy of the particle */
	p = SOLAR_PARTICLE_EV_MIN + (SOLAR_PARTICLE_EV_MAX - SOLAR_PARTICLE_EV_MIN) * r;
	p *= e_PER_eV;

	return p;
}


/**
 * @brief get the scattering fraction of protons on Si atoms for
 *	  a CCD pixel
 *
 * @param p_eV the energy of the particle
 * @param theta the (minimum) scattering angle for the fraction scattered
 *
 * @note Rutherford scattering requires the target to only be a few µm thick
 *	 and preferably uses alpha particles as projectiles.
 *	 We're only interested in an approximate behaviour, so we can
 *	 easily get away with any deviations.
 */


static float ccd_sim_get_scatter_fraction(float p_eV, float theta)
{
	float t, r;
	float sigma;
	float f;

	/* we select between hydrogen and helium cores (~8%) */
	const float zp = (float) ((rand() % 100) <= 8 ? 2 : 1);
	const float Z  = 14.;				/* atomic number of Si */
	const float A  = 2.* Z;				/* mass number of  Si */
	const float rho= 2.33 * 1000.;			/* density of Si (kg/m^3) */
	const float Zp = powf(zp, 2.) / 4.;		/* projectile charge factor */
	const float k  = 8.9875517923e9;		/* Coloumb's constant */
	const float e  = -1.602176634e-19;		/* electron charge */
	const float eV = -e;				/* 1 eV in J */
	const float NA = 6.02214076e23;			/* Avogadro's number */
	const float L  = CCD_THICKNESS_um * 1e-6;	/* target thickness */


	t = k * pow(e, 2.) / (p_eV * eV);
	r = (1. + cosf(theta)) / (1. - cosf(theta));
	sigma = M_PI * Zp * pow(Z, 2.) * pow(t, 2.) * r;

	f = (NA * L * rho * sigma) / (A * 1e-3);

	if (f > 1.0)
		return 1.0;

	return f;
}

/**
 * @brief create particle traces in the CCD
 *
 * @param tint_ms the integration time in ms
 * @param solar 0 = cosmics, 1 = solar
 *
 */

static void ccd_sim_add_particles(uint16_t *frame, uint16_t tint_ms, int solar)
{
	size_t n = FEE_CCD_IMG_SEC_ROWS * FEE_CCD_IMG_SEC_COLS;
	const float sigma = 1.0;
	float amp;
	float tint = (float) tint_ms / 1000.0;

	size_t i;

	float x, y;
	float p_ev;
	float phi, theta;
	float d, r;
	float dx, dy;
	float d_ev;


	float *ccd;

	float deflection_angle;
	unsigned int deflection_rate;


	ccd = (float *) calloc(sizeof(float), n);
	if (!ccd) {
		perror("malloc");
		exit(-1);
	}

	/* our event amplitude is the integration time for the configured
	 * event rates
	 */
	if (solar)
		amp = tint * (PB_CCD_RATE_MIN + (PB_CCD_RATE_MAX - PB_CCD_RATE_MIN) * SOLAR_ACT);
	else
		amp = tint * COSMIC_FLUX;

	/* use the square of the amplitude to scale the noise */
	amp = amp + sqrtf(amp) * sigma * sim_rand_gauss();

	for (i = 0; i < ((size_t) amp / 2); i++) {


		/* initial particle energy */
		if (solar)
			p_ev = ccd_sim_get_solar_particle();
		else
			p_ev = ccd_sim_get_cosmic_particle();



		/* pixel of particle entry into CCD */
		x = (float) (rand() % (FEE_CCD_IMG_SEC_COLS + 1));
		y = (float) (rand() % (FEE_CCD_IMG_SEC_ROWS + 1));


		/* angle from CCD plane */
		if (solar) /* shallow */
			phi = fmod(rand(), SOLAR_WIND_EL_ANGLE_MAX * 1000.) * 0.001;
		else
			phi = fmod(rand(), M_PI_2 * 1000. ) * 0.001;

		/* direction within plane */
		if (solar) /* just one side */
			theta = 0.5 * SOLAR_WIND_AZ_ANGLE_MAX - fmod(rand(),  SOLAR_WIND_AZ_ANGLE_MAX * 1000.) * 0.001;
		else	/* anywhere */
			theta = M_PI - fmod(rand(), 2. * M_PI * 1000.) * 0.001;


restart:
		/* max distance traveled through ccd */
		d = CCD_THICKNESS_um / tanf(phi);

		/* get a random deflection angle */
#if 1
		/* log distribution */
		deflection_angle = sim_rand_log() * (RUTHERFORD_SCATTER_ANGLE_MAX / 180. * M_PI);
#else
		/* uniform */
		deflection_angle = ((float) (1 + rand() % RUTHERFORD_SCATTER_ANGLE_MAX )) / 180. * M_PI;
#endif
		/* our rate for rand() */
		deflection_rate =  (unsigned int) (1.0 / ccd_sim_get_scatter_fraction(p_ev, deflection_angle));
#if 0
		printf("deflection rate %u angle %g frac: %g ev %g\n", deflection_rate, deflection_angle / M_PI * 180., ccd_sim_get_scatter_fraction(p_ev, deflection_angle), p_ev);
#endif
		/* step size in x and y direction, we compute
		 * one pixel at a time and assume they are all cubes,
		 * so the max diagonal is sqrt(2) * thickness for the
		 * absorption
		 */
		dx = (PIXEL_LEN_um * M_SQRT2) * sinf(theta);
		dy = (PIXEL_LEN_um * M_SQRT2) * cosf(theta);
		/* max distance per pixel in vertical direction */
		r  = (CCD_THICKNESS_um * M_SQRT2) * cosf(phi);

		/* scale energy loss by longest distance travelelled
		 * within a pixel and the material-specific loss
		*/
		if (fabsf(dx) > fabsf(dy))
			d_ev = fabsf(dx);
		else
			d_ev = fabsf(dy);

		/* in case vertical travel component is the longest */
		if (fabsf(d_ev) < fabsf(r))
			d_ev = fabsf(r);

		/* scale to material */
		d_ev *= PARTICLE_ENERGY_LOSS_PER_UM_EV;

#if 0
		printf("energy: %g, x: %g y: %g, phi %g, theta %g streak %g, dx %g, dy %g, r %g, d_ev keV %g\n",
		       p_ev, x, y, phi * 180. / M_PI, theta * 180. / M_PI, d, dx, dy, r, d_ev/1000);
#endif

		/* XXX scale back to integer CCD pixels (not pretty..) */
		dx /= PIXEL_LEN_um * M_SQRT2;
		dy /= PIXEL_LEN_um * M_SQRT2;


		while (1) {

			size_t pix;

			if (x <= 0)
				break;
			if (y <= 0)
				break;
			if (x > FEE_CCD_IMG_SEC_COLS)
				break;
			if (y > FEE_CCD_IMG_SEC_ROWS)
				break;
			if (d < 0.)
				break;
			/* could set configurable cutoff here */
			if (p_ev < 0.)
				break;

			pix = (size_t) y * FEE_CCD_IMG_SEC_COLS + (size_t) x;
			ccd[pix] += d_ev * e_PER_eV * CDD_RESP_uV_e; // * (p_ev/p0) * logf(d/d0);

			x += dx;
			y += dy;

			p_ev -= d_ev;
			d -= r;

			if (rand() % (deflection_rate + 1) == 0) {
				float ratio = ((float) (rand(), 50)) * 0.01;
				float sign = (rand() & 0x1 ? -1.0 : 1.0);

				/* deflect the sucker */
				theta += sign * deflection_angle * ratio;
				sign = (rand() & 0x1 ? -1.0 : 1.0);
				phi   += sign * deflection_angle * (1. - ratio);
				goto restart;
			}

		}

	}

	for (i = 0; i < n; i++) {

		float tot = frame[i] + ccd[i];

		if (tot < (float) PIX_SATURATION)
			frame[i] = (uint16_t) tot;
		else /* else saturate, TODO: bleed charges (maybe) */
			frame[i] = PIX_SATURATION;
	}


	free(ccd);
}



/**
 * we don't really need a dark sim, the effective amplitude variation is way
 * to low to be significant
 */
__attribute__((unused))
static void ccd_sim_add_dark(uint16_t tint_ms)
{
	float *noisearr;
	size_t n = FEE_CCD_IMG_SEC_ROWS * FEE_CCD_IMG_SEC_COLS;
	float amp;
	float tint = (float) tint_ms / 1000.0;

	size_t i;



	/* total average accumulated dark current amplitude */
	amp = tint * CCD_DARK;

	/* use the square of the amplitude to scale the noise */
	amp = amp + sqrtf(amp);

	noisearr = (float *) calloc(sizeof(float), DARK_SAMPLES);
	if (!noisearr) {
		perror("malloc");
		exit(-1);
	}

	for (i = 0; i < DARK_SAMPLES; i++) {
		noisearr[i] =  amp + fmodf(sim_rand_gauss(), CCD_DAR_NONUNI * 1000.) * 0.001;
		noisearr[i]*= CDD_RESP_uV_e;	/* scale to voltage-equivalent */
	}


	/* fill CCDs */
	for (i = 0; i < n; i++) {
		CCD2E[i] += noisearr[rand() % (DARK_SAMPLES + 1)];
		CCD2F[i] += noisearr[rand() % (DARK_SAMPLES + 1)];
		CCD4E[i] += noisearr[rand() % (DARK_SAMPLES + 1)];
		CCD4F[i] += noisearr[rand() % (DARK_SAMPLES + 1)];
	}

	free(noisearr);
}


static void ccd_sim_add_rd_noise(uint16_t *ccd, size_t n)
{
	float *noisearr;
	const float sigma = 1.0;
	float amp;

	size_t i;

	struct timeval t0, t;
	double elapsed_time;

	gettimeofday(&t0, NULL);
	/* total average accumulated dark current amplitude */
	amp = CCD_NOISE;

	noisearr = (float *) calloc(sizeof(float), RD_NOISE_SAMPLES);
	if (!noisearr) {
		perror("malloc");
		exit(-1);
	}

	for (i = 0; i < RD_NOISE_SAMPLES; i++) {
		/* use the square of the amplitude to scale the noise */
		noisearr[i] =  amp + sqrtf(amp) * sigma * sim_rand_gauss();
		noisearr[i]*= CDD_RESP_uV_e;	/* scale to voltage-equivalent */
	}


	/* add noise */
	for (i = 0; i < n; i++)
		ccd[i] += noisearr[rand() % (RD_NOISE_SAMPLES + 1)];

	free(noisearr);

	/* time elapsed in ms */
	gettimeofday(&t, NULL);
	elapsed_time  = (t.tv_sec  - t0.tv_sec)  * 1000.0;
	elapsed_time += (t.tv_usec - t0.tv_usec) / 1000.0;
	printf("readout noise in %g ms\n", elapsed_time);
}




static void ccd_sim_clear(void)
{
	size_t n = FEE_CCD_IMG_SEC_ROWS * FEE_CCD_IMG_SEC_COLS * sizeof(uint16_t);

	memset(CCD2E, 0, n);
	memset(CCD2F, 0, n);
	memset(CCD4E, 0, n);
	memset(CCD4F, 0, n);
}

static void ccd_sim_refresh(void)
{
	struct timeval t0, t;
	double elapsed_time;

	uint16_t tint_ms;



	gettimeofday(&t0, NULL);

	ccd_sim_clear();

	tint_ms = smile_fee_get_int_period();

	if (CFG_SIM_DARK)
		ccd_sim_add_dark(tint_ms);

	ccd_sim_add_swcx(CCD2E, tint_ms);
	ccd_sim_add_swcx(CCD2F, tint_ms);
	ccd_sim_add_swcx(CCD4E, tint_ms);
	ccd_sim_add_swcx(CCD4F, tint_ms);

	ccd_sim_add_sxrb(CCD2E, tint_ms);
	ccd_sim_add_sxrb(CCD2F, tint_ms);
	ccd_sim_add_sxrb(CCD4E, tint_ms);
	ccd_sim_add_sxrb(CCD4F, tint_ms);

	/* solar and cosmic particles */
	ccd_sim_add_particles(CCD2E, tint_ms, 0);
	ccd_sim_add_particles(CCD2E, tint_ms, 1);
	ccd_sim_add_particles(CCD2F, tint_ms, 0);
	ccd_sim_add_particles(CCD2F, tint_ms, 1);
	ccd_sim_add_particles(CCD4E, tint_ms, 0);
	ccd_sim_add_particles(CCD4E, tint_ms, 1);
	ccd_sim_add_particles(CCD4F, tint_ms, 0);
	ccd_sim_add_particles(CCD4F, tint_ms, 1);

	/* time elapsed in ms */
	gettimeofday(&t, NULL);
	elapsed_time  = (t.tv_sec  - t0.tv_sec)  * 1000.0;
	elapsed_time += (t.tv_usec - t0.tv_usec) / 1000.0;
	printf("ccd refresh in %g ms\n", elapsed_time);

}
/* end ccd sim */







static void fee_sim_destroy_hk_data_payload(struct fee_hk_data_payload *hk)
{
	free(hk);
}

/**
 * @brief allocate and fill HK payload data
 *
 * @note the retured structure is either NULL or
 *	 sizeof(struct fee_hk_data_payload) long when valid
 */

static struct fee_hk_data_payload *fee_sim_create_hk_data_payload(void)
{
	struct fee_hk_data_payload *hk;


	hk = (struct fee_hk_data_payload * ) malloc(sizeof(struct fee_hk_data_payload));
	if (!hk) {
		perror("malloc");
		exit(-1);
	}

	/* XXX set actual HK data here once we get the contents/order info */

	return hk;
}


/**
 * @brief increment current frame counter
 */

static void fee_increment_frame_cntr(void)
{
	frame_cntr++;
}


/**
 * @brief get the current frame counter
 */

static uint16_t fee_get_frame_cntr(void)
{
	return frame_cntr;
}


/**
 * @brief get a SpW-style time code, increments one tick for every call
 */

static uint8_t fee_get_spw_time_code(void)
{
	static uint8_t tc;

	return (tc++ & 0x3f);
}


/**
 * @brief set the logical destination address
 */

static void fee_sim_hdr_set_logical_addr(struct fee_data_hdr *hdr, uint8_t addr)
{
	hdr->logical_addr = addr;
}


/**
 * @brief set the protocol id
 */

static void fee_sim_hdr_set_protocol_id(struct fee_data_hdr *hdr, uint8_t id)
{
	hdr->proto_id = id;
}


/**
 * @brief set the packet type
 */

static void fee_sim_hdr_set_pkt_type(struct fee_data_hdr *hdr, uint8_t type)
{
	hdr->type.pkt_type = type & 0x3;
}

/**
 * @brief set the CCD side
 */

static void fee_sim_hdr_set_ccd_side(struct fee_data_hdr *hdr, uint8_t side)
{
	hdr->type.ccd_side = side & 0x3;
}

/**
 * @brief set the CCD id
 */

static void fee_sim_hdr_set_ccd_id(struct fee_data_hdr *hdr, uint8_t id)
{
	hdr->type.ccd_id = id & 0x1;
}

/**
 * @brief set the FEE mode
 */

static void fee_sim_hdr_set_fee_mode(struct fee_data_hdr *hdr, uint8_t mode)
{
	hdr->type.fee_mode = mode & 0xF;
}


/**
 * @brief set the last packet marker
 */

static void fee_sim_hdr_set_last_pkt(struct fee_data_hdr *hdr, uint8_t last)
{
	hdr->type.last_pkt = last & 0x1;
}


/**
 * @brief set the current frame counter
 */

static void fee_sim_hdr_set_frame_cntr(struct fee_data_hdr *hdr, uint16_t cntr)
{
	hdr->frame_cntr = cntr;
}


/**
 * @brief increment the sequence counter
 */

static void fee_sim_hdr_inc_seq_cntr(struct fee_data_hdr *hdr)
{
	hdr->seq_cntr++;
}

/**
 * @brief set the payload data size
 * @note a proper value is the user's responsibility
 */

static void fee_sim_hdr_set_data_len(struct fee_data_hdr *hdr, uint16_t len)
{
	hdr->data_len = len;
}


/**
 * @brief swap header bytes to correct endianess for transfer
 *
 * @note do this just before the transfer, swap back for manipulation
 *	 afterwards!
 */

static void fee_sim_hdr_cpu_to_tgt(struct fee_data_hdr *hdr)
{
	/* swap type field to correct byte order */
	hdr->data_len = __be16_to_cpu(hdr->data_len);
	hdr->fee_pkt_type = __be16_to_cpu(hdr->fee_pkt_type);
	hdr->frame_cntr   = __be16_to_cpu(hdr->frame_cntr);
	hdr->seq_cntr     = __be16_to_cpu(hdr->seq_cntr);
}

/**
 * @brief swap header bytes to correct endianess for use with local cpu
 *
 * @note before the transfer, swap back to target endianess!
 */

static void fee_sim_hdr_tgt_to_cpu(struct fee_data_hdr *hdr)
{
	/* swap type field to correct byte order */
	hdr->data_len	  = __cpu_to_be16(hdr->data_len);
	hdr->fee_pkt_type = __cpu_to_be16(hdr->fee_pkt_type);
	hdr->frame_cntr   = __cpu_to_be16(hdr->frame_cntr);
	hdr->seq_cntr     = __cpu_to_be16(hdr->seq_cntr);
}


/**
 * @brief destroy a payload data packet
 */

static void fee_sim_destroy_data_payload(struct fee_data_payload *pld)
{
	if (!pld)
		return;

	if (pld->pkt)
		free(pld->pkt);

	free(pld);
}


/**
 * @brief create a payload data packet according to the current FEE configuration
 */

static struct fee_data_payload *fee_sim_create_data_payload(void)
{
	size_t tx_size;
	size_t pkt_size_max;

	struct fee_data_payload *pld;



	tx_size = smile_fee_get_packet_size() - sizeof(struct fee_data_hdr);

	if (tx_size & 0x3) {
		printf("Warning, configured payload size must be a multiple of "
		       "4 according to SMILE-MSSL-PL-Register_map_v0.20, "
		       "clamping to next lower bound\n");

		tx_size &= ~0x3;
	}

	/* update and verify pkt_size_max */
	pkt_size_max =  sizeof(struct fee_data_hdr) + tx_size;
	if (pkt_size_max < (sizeof(struct fee_data_hdr) + 1)) {
		printf("Configured packet size must be at least header size "
		       "+ 1 or we won't be able to transfer anything\n");
		return NULL;
	}

	/* allocate payload */
	pld = (struct fee_data_payload * ) calloc(1, sizeof(struct fee_data_payload));
	if (!pld) {
		perror("malloc");
		exit(-1);
	}

	/* allocate the packet structure we use for the transfer of this frame */
	pld->pkt = (struct fee_data_pkt * ) calloc(1, pkt_size_max);
	if (!pld->pkt) {
		free(pld);
		perror("malloc");
		exit(-1);
	}

	pld->data_len_max = tx_size;

	return pld;
}


/**
 * @brief send a data packet
 */

static void fee_sim_send_data_payload(struct sim_net_cfg *cfg,
				      struct fee_data_payload *pld)
{
	size_t n;

	n = pld->pkt->hdr.data_len + sizeof(struct fee_data_pkt);

	/* send */
	fee_sim_hdr_cpu_to_tgt(&pld->pkt->hdr);
	fee_send_non_rmap(cfg, (uint8_t *) pld->pkt, n);
	fee_sim_hdr_tgt_to_cpu(&pld->pkt->hdr);
}



/**
 * @brief send buffer in chunks according to the configuration
 */

static void fee_sim_tx_payload_data(struct sim_net_cfg *cfg,
				      struct fee_data_payload *pld,
				      uint8_t *buf, size_t n)
{
	size_t i = 0;


	fee_sim_hdr_set_last_pkt(&pld->pkt->hdr, 0);

	fee_sim_hdr_set_data_len(&pld->pkt->hdr, pld->data_len_max);

	while (n) {

		if (n > pld->data_len_max) {
			memcpy(pld->pkt->data, &buf[i], pld->data_len_max);
			i += pld->data_len_max;
			n -= pld->data_len_max;
		} else {

			memcpy(pld->pkt->data, &buf[i], n);
			fee_sim_hdr_set_data_len(&pld->pkt->hdr, n);
			fee_sim_hdr_set_last_pkt(&pld->pkt->hdr, 1);
			n = 0;
		}
		/* send */
		fee_sim_send_data_payload(cfg, pld);
		fee_sim_hdr_inc_seq_cntr(&pld->pkt->hdr);
	}

}


static void fee_sim_exec_ff_mode(struct sim_net_cfg *cfg, uint8_t fee_mode)
{
	struct fee_hk_data_payload *hk;
	struct fee_data_payload *pld;

	uint8_t id;

	uint16_t *E, *F;
	uint16_t *frame = NULL;

	size_t i;
	size_t n ;



	if (smile_fee_get_ccd_readout(1)) {
		E = CCD2E;
		F = CCD2F;
		id = FEE_CCD_ID_2;
	} else if (smile_fee_get_ccd_readout(2)) {
		E = CCD4E;
		F = CCD4F;
		id = FEE_CCD_ID_4;
	} else {
		return;
	}

	/* we transfer BOTH sides */
	n = 2 * FEE_CCD_IMG_SEC_ROWS * FEE_CCD_IMG_SEC_COLS * sizeof(uint16_t);
	frame = malloc(n);
	if (!frame) {
		perror("malloc");
		exit(-1);
	}

	/* as per MSSL-IF-115 MSSL-SMILE-SXI-IRD-0001 Draft A0.14, full frame
	 * transfer pixels are ordered in pairs of FxEx
	 */
	for (i = 0; i < FEE_CCD_IMG_SEC_ROWS * FEE_CCD_IMG_SEC_COLS; i++) {
		frame[2 * i]     = F[i];
		frame[2 * i + 1] = E[i];
	}


	pld = fee_sim_create_data_payload();

	/* required once */
	fee_sim_hdr_set_logical_addr(&pld->pkt->hdr, DPU_LOGICAL_ADDRESS);
	fee_sim_hdr_set_protocol_id(&pld->pkt->hdr, FEE_DATA_PROTOCOL);
	fee_sim_hdr_set_frame_cntr(&pld->pkt->hdr, fee_get_frame_cntr());

	/* in FF mode, first packet in sequence is SUGGESTED to be HK,
	 * given the sequence number in Table 8-13 of
	 * MSSL-SMILE-SXI-IRD-0001 Draft A0.14
	 */
	hk = fee_sim_create_hk_data_payload();

	if (hk) {
		fee_sim_hdr_set_pkt_type(&pld->pkt->hdr, FEE_PKT_TYPE_HK);
		fee_sim_tx_payload_data(cfg, pld, (uint8_t *) hk,
					sizeof(struct fee_hk_data_payload));
		fee_sim_destroy_hk_data_payload(hk);
	}

	fee_sim_hdr_set_pkt_type(&pld->pkt->hdr, FEE_PKT_TYPE_DATA);
	/* CCD side is unused in this mode */
	fee_sim_hdr_set_ccd_id(&pld->pkt->hdr,   id);
	fee_sim_hdr_set_fee_mode(&pld->pkt->hdr, fee_mode);
	fee_sim_tx_payload_data(cfg, pld, (uint8_t *) frame, n);


	fee_sim_destroy_data_payload(pld);

	free(frame);
}



/**
 * @brief execute a frame transfer block
 *
 * @param cfg	a struct sim_net_cfg;
 *
 * @param E2	E-side CCD2 buffer or NULL if unused
 * @param F2	F-side CCD2 buffer or NULL if unused
 * @param E3	E-side CCD2 buffer or NULL if unused
 * @param F4	F-side CCD4 buffer or NULL if unused
 *
 * @param n the lenght of the frame data buffer; this applies to all buffers
 *
 * this executes a nominal transfer sequence as per
 * MSSL-SMILE-SXI-IRD-0001 draft A0.14 Table 8-12
 *
 */

static void fee_sim_frame_transfer(struct sim_net_cfg *cfg,  uint8_t fee_mode,
				   uint8_t *E2, uint8_t *F2, uint8_t *E4, uint8_t *F4,
				   size_t n)
{
	struct fee_hk_data_payload *hk;
	struct fee_data_payload *pld;



	switch (fee_mode) {
	case FEE_MODE_ID_FTP:
	case FEE_MODE_ID_FT:
		break;
	default:
		printf("Only FT type transfers are supported by this function\n");
		return;
	}

	pld = fee_sim_create_data_payload();

	/* update once per CDD readout cycl */
	fee_increment_frame_cntr();

	/* required once */
	fee_sim_hdr_set_logical_addr(&pld->pkt->hdr, DPU_LOGICAL_ADDRESS);
	fee_sim_hdr_set_protocol_id(&pld->pkt->hdr, FEE_DATA_PROTOCOL);
	fee_sim_hdr_set_frame_cntr(&pld->pkt->hdr, fee_get_frame_cntr());

	/* in FT mode, first packet in sequence is HK */
	hk = fee_sim_create_hk_data_payload();

	if (hk) {
		fee_sim_hdr_set_pkt_type(&pld->pkt->hdr, FEE_PKT_TYPE_HK);
		fee_sim_tx_payload_data(cfg, pld, (uint8_t *) hk,
					sizeof(struct fee_hk_data_payload));
		fee_sim_destroy_hk_data_payload(hk);
	}

	/* E2 next (if set) */
	if (E2) {
		fee_sim_hdr_set_pkt_type(&pld->pkt->hdr, FEE_PKT_TYPE_DATA);
		fee_sim_hdr_set_ccd_side(&pld->pkt->hdr, FEE_CCD_SIDE_E);
		fee_sim_hdr_set_ccd_id(&pld->pkt->hdr,   FEE_CCD_ID_2);
		fee_sim_hdr_set_fee_mode(&pld->pkt->hdr, fee_mode);
		fee_sim_tx_payload_data(cfg, pld, E2, n);
	}

	/* F2 next (if set) */
	if (F2) {
		fee_sim_hdr_set_pkt_type(&pld->pkt->hdr, FEE_PKT_TYPE_DATA);
		fee_sim_hdr_set_ccd_side(&pld->pkt->hdr, FEE_CCD_SIDE_F);
		fee_sim_hdr_set_ccd_id(&pld->pkt->hdr,   FEE_CCD_ID_2);
		fee_sim_hdr_set_fee_mode(&pld->pkt->hdr, fee_mode);
		fee_sim_tx_payload_data(cfg, pld, F2, n);
	}

	/* E4 next (if set) */
	if (E4) {
		fee_sim_hdr_set_pkt_type(&pld->pkt->hdr, FEE_PKT_TYPE_DATA);
		fee_sim_hdr_set_ccd_side(&pld->pkt->hdr, FEE_CCD_SIDE_E);
		fee_sim_hdr_set_ccd_id(&pld->pkt->hdr,   FEE_CCD_ID_4);
		fee_sim_hdr_set_fee_mode(&pld->pkt->hdr, fee_mode);
		fee_sim_tx_payload_data(cfg, pld, E4, n);
	}

	/* F4 next (if set) */
	if (F4) {
		fee_sim_hdr_set_pkt_type(&pld->pkt->hdr, FEE_PKT_TYPE_DATA);
		fee_sim_hdr_set_ccd_side(&pld->pkt->hdr, FEE_CCD_SIDE_F);
		fee_sim_hdr_set_ccd_id(&pld->pkt->hdr,   FEE_CCD_ID_4);
		fee_sim_hdr_set_fee_mode(&pld->pkt->hdr, fee_mode);
		fee_sim_tx_payload_data(cfg, pld, F4, n);
	}


	fee_sim_destroy_data_payload(pld);
}


/**
 * @brief extract ccd data for frame transfer mode
 */

static uint16_t *fee_sim_get_ft_data(uint16_t *ccd, size_t rows, size_t cols,
				     size_t bins)
{
	size_t i, j;
	size_t x, y;
	size_t rw, cl;

	uint16_t *buf;
	uint16_t *acc;

	struct timeval t0, t;
	double elapsed_time;

	/* we need a cleared buffer to accumulate the bins */
	buf = calloc(sizeof(uint16_t), rows * cols);
	if (!buf) {
		perror("malloc");
		exit(-1);
	}

	if (bins == 1) {
		memcpy(buf, ccd, rows * cols * sizeof(uint16_t));
		return buf;
	}

	/* our accumulator */
	acc = malloc(FEE_CCD_IMG_SEC_COLS * sizeof(uint16_t));
	if (!acc) {
		perror("malloc");
		exit(-1);
	}

	gettimeofday(&t0, NULL);

	/* the binned data contain overscan in the real FEE, i.e. the "edge"
	 * pixels contain CCD bias values, but we just ignore those
	 * and round down to the next integer
	 * note that we keep the nominal size, we just don't fill the
	 * out-of-bounds samples with values
	 */
	rw = FEE_CCD_IMG_SEC_ROWS / bins;
	cl = FEE_CCD_IMG_SEC_COLS / bins;


	/* rebinned rows */
	for (y = 0; y < rw; y++) {

		/* clear line accumulator */
		memset(acc, 0x0, FEE_CCD_IMG_SEC_COLS * sizeof(uint16_t));

		/* accumulate the data lines in "blocks" of bins */
		for (i = 0; i < bins; i++) {

			/* the offset into the buffer of the start of the
			 * next row; note that the original number of rows
			 * is required, otherwise the data would be skewed
			 */
			size_t y0 = (y * bins + i) * FEE_CCD_IMG_SEC_COLS;

			for (j = 0; j < FEE_CCD_IMG_SEC_COLS; j++)
				acc[j] += ccd[y0 + j];

		}

		/* accumulate blocks of bins into columns */
		for (x = 0; x < cl; x++) {

			for (i = 0; i < bins; i++)
				buf[y * cols + x] += acc[x * bins + i];

		}

	}

	/* time in ms  */
	gettimeofday(&t, NULL);
	elapsed_time  = (t.tv_sec  - t0.tv_sec)  * 1000.0;
	elapsed_time += (t.tv_usec - t0.tv_usec) / 1000.0;
	printf("rebinned in %g ms\n", elapsed_time);

	free(acc);

	return buf;
}


/**
 * comparsison function for qsort
 */

int compar(const void *a, const void *b)
{
	return (*(uint16_t *) a - *(uint16_t *) b);
}

/**
 * find the median (via qusort)
 */

uint16_t median(uint16_t *base, size_t nmemb)
{
	uint16_t med;


	qsort(base, nmemb, sizeof(uint16_t), compar);

	/* we use an approximated mean for even nmemb */
	if (nmemb % 2)
		med = base[nmemb / 2];
	else
		med = (base[nmemb / 2] + base[(nmemb / 2) + 1]) / 2;

	return med;
}


/**
 * find the median of the pixels marked "blue" in the outer ring to determine
 * thelocal background as per TN "SMILE SXI CCD Testing and Calibration
 * Event Detection Methodology" issue 2 rev 0, section "Processes in the
 * Event Detection Unit"
 */

static uint16_t fee_sim_get_local_background(struct fee_event_detection *pkt)
{
	size_t cnt = 0;
	uint16_t medpix[11];


	medpix[cnt++] = pkt->pix[0];
	medpix[cnt++] = pkt->pix[1];
	medpix[cnt++] = pkt->pix[2];
	medpix[cnt++] = pkt->pix[3];
	medpix[cnt++] = pkt->pix[4];
	medpix[cnt++] = pkt->pix[5];
	medpix[cnt++] = pkt->pix[9];
	medpix[cnt++] = pkt->pix[10];
	medpix[cnt++] = pkt->pix[15];
	medpix[cnt++] = pkt->pix[20];
	medpix[cnt++] = pkt->pix[21];


	return median(medpix, cnt);
}


/*
 * 9 Event detection algorithm
 *
 *
 * The EDU algorithm will be applied on a pixel-by-pixel basis, inspecting the pixels along a row. 5 rows
 * must be observed at a time. Each pixel is inspected and if the following is true, the pixel is identified
 * as an ‘event’:
 * 1. The pixel contains more signal than ‘single pixel threshold’ + local signal background
 * 2. The pixel is a local maximum with more signal when compared individually to its surrounding
 * 8 nearest neighbours
 *
 *
 * (from: SMILE SXI CCD Testing and Calibration Event Detection Methodology issue 2 rev 0)
 *
 */

static int fee_sim_check_event_pixel(struct sim_net_cfg *cfg,
				     struct fee_event_detection *pkt, uint16_t *frame,
				     size_t idx, size_t cols, uint16_t threshold)
{
	uint16_t pix;

	ssize_t i, j;

	size_t cnt = 0;
	size_t ev  = 0;


	/* central pixel */
	pix = frame[idx];


	/* collect event area into packet */
	for (i = 2; i >= -2; i--) {
		for (j = -2; j <= 2; j++)
			pkt->pix[cnt++] = frame[idx + i * cols + j];

	}

	/* below threshold, no point in doing other checks */
	if ( pix < threshold + fee_sim_get_local_background(pkt))
		return 0;


	/* since this is a ring of only 8 pixels, we unroll this here
	 * to avoid dealing with nasty indexing
	 */

	if (pkt->pix[ 6] < pix) ev++;
	if (pkt->pix[ 7] < pix) ev++;
	if (pkt->pix[ 8] < pix) ev++;
	if (pkt->pix[11] < pix) ev++;
	if (pkt->pix[13] < pix) ev++;
	if (pkt->pix[16] < pix) ev++;
	if (pkt->pix[17] < pix) ev++;
	if (pkt->pix[18] < pix) ev++;

	if (ev != 8)	/* no event */
		return 0;

	/* swap ED data endianess */
	pkt->row = cpu_to_be16(pkt->row);
	pkt->col = cpu_to_be16(pkt->col);

	for (i = 0; i < FEE_EV_DET_PIXELS; i++)
		pkt->pix[i] = cpu_to_be16(pkt->pix[i]);

	/* send event */
	fee_sim_hdr_cpu_to_tgt(&pkt->hdr);
	fee_send_non_rmap(cfg, (uint8_t *) pkt, sizeof(struct fee_event_detection));
	fee_sim_hdr_tgt_to_cpu(&pkt->hdr);
	fee_sim_hdr_inc_seq_cntr(&pkt->hdr);

	return 1;

}


/**
 * @brief run event detection on a frame
 */

static size_t fee_sim_detect_events(struct sim_net_cfg *cfg,
				    struct fee_event_detection *pkt,
				    uint16_t *frame, size_t rows, size_t cols,
				    uint16_t threshold)
{
	size_t ev_cnt = 0;
	size_t r, c;

	struct timeval t0, t;
	double elapsed_time;


	if (!frame) /* unused sides are NULL */
		return 0;

	/* detect only if event detection is set */
	if (!smile_fee_get_event_detection())
		return 0;

	gettimeofday(&t0, NULL);

	/* the event detection window is a fixed 5x5 imagette with the
	 * "event" pixel being in the centre, hence we have to shrink
	 * the frame by a fixed 2 pixels on each side
	 */

	for (r = 2; r < (rows - 2); r++) {

		size_t c0 = r * cols;

		for (c = 2; c < (cols - 2); c++) {

			size_t idx = c0 + c;

			pkt->row = r;
			pkt->col = c;

#if 0	/* debug */
			if (fee_sim_check_event_pixel(cfg, pkt, frame, idx, cols, threshold))
					printf("event at %ld %ld, value %d\n", r, c, frame[idx]);
#else
			if (fee_sim_check_event_pixel(cfg, pkt, frame, idx, cols, threshold))
				ev_cnt++;
#endif
		}
	}

	/* time in ms  */
	gettimeofday(&t, NULL);
	elapsed_time  = (t.tv_sec  - t0.tv_sec)  * 1000.0;
	elapsed_time += (t.tv_usec - t0.tv_usec) / 1000.0;
	printf("event detection in %g ms\n", elapsed_time);
#if 1
	printf("SWCX event candidates detected %ld\n", ev_cnt);

#endif
	return ev_cnt;
}

static void fee_sim_exec_ft_mode(struct sim_net_cfg *cfg)
{
	uint16_t *E2 = NULL;
	uint16_t *F2 = NULL;
	uint16_t *E4 = NULL;
	uint16_t *F4 = NULL;

	size_t rows, cols, bins;
	uint16_t readout;

	size_t E_ev_cnt = smile_fee_get_event_pkt_limit();
	size_t F_ev_cnt = smile_fee_get_event_pkt_limit();

	struct fee_event_detection ev_pkt = {0};


	ccd_sim_refresh();	/* XXX not here, just testing */
	switch (smile_fee_get_ccd_mode2_config()) {

	case FEE_MODE2_NOBIN:
		rows = FEE_CCD_IMG_SEC_ROWS;
		cols = FEE_CCD_IMG_SEC_COLS;
		bins = 1;
		break;

	case FEE_MODE2_BIN6:
		rows = FEE_EDU_FRAME_6x6_ROWS;
		cols = FEE_EDU_FRAME_6x6_COLS;
		bins = 6;
		break;

	case FEE_MODE2_BIN24:
		/* values are guessed base on returned data from FEE, the
		 * actual size is not mentioned in the IRD
		 * it is unclear whether this mode is ever going to be used
		 */
		rows = FEE_EDU_FRAME_24x24_ROWS;
	        cols = FEE_EDU_FRAME_24x24_COLS;
		bins = 24;
		break;
	default:
		printf("Unknown binning mode specified\n");
		return;
	}


	/* prep EV packet structure */
	fee_sim_hdr_set_logical_addr(&ev_pkt.hdr, DPU_LOGICAL_ADDRESS);
	fee_sim_hdr_set_protocol_id(&ev_pkt.hdr, FEE_DATA_PROTOCOL);
	fee_sim_hdr_set_pkt_type(&ev_pkt.hdr, FEE_PKT_TYPE_EV_DET);
	fee_sim_hdr_set_frame_cntr(&ev_pkt.hdr, fee_get_frame_cntr());
	fee_sim_hdr_set_data_len(&ev_pkt.hdr, FEE_EV_DATA_LEN);



	readout = smile_fee_get_readout_node_sel();


	if (readout & FEE_READOUT_NODE_E2) {
		E2 = fee_sim_get_ft_data(CCD2E, rows, cols, bins);
		ccd_sim_add_rd_noise(E2, rows * cols);

		/* event detection is only done for 6x binning */
		if (smile_fee_get_ccd_mode2_config() == FEE_MODE2_BIN6) {
			fee_sim_hdr_set_ccd_side(&ev_pkt.hdr, FEE_CCD_SIDE_E);
			fee_sim_hdr_set_ccd_id(&ev_pkt.hdr,   FEE_CCD_ID_2);
			fee_sim_detect_events(cfg, &ev_pkt, E2, rows, cols, smile_fee_get_ccd2_e_pix_threshold());
		}

	}

	if (readout & FEE_READOUT_NODE_F2) {
		F2 = fee_sim_get_ft_data(CCD2F, rows, cols, bins);
		ccd_sim_add_rd_noise(F2, rows * cols);

		/* event detection is only done for 6x binning */
		if (smile_fee_get_ccd_mode2_config() == FEE_MODE2_BIN6) {
			fee_sim_hdr_set_ccd_side(&ev_pkt.hdr, FEE_CCD_SIDE_F);
			fee_sim_hdr_set_ccd_id(&ev_pkt.hdr,   FEE_CCD_ID_2);
			fee_sim_detect_events(cfg, &ev_pkt, F2, rows, cols, smile_fee_get_ccd4_e_pix_threshold());
		}
	}

	if (readout & FEE_READOUT_NODE_E4) {
		E4 = fee_sim_get_ft_data(CCD4E, rows, cols, bins);
		ccd_sim_add_rd_noise(E4, rows * cols);

		/* event detection is only done for 6x binning */
		if (smile_fee_get_ccd_mode2_config() == FEE_MODE2_BIN6) {
			fee_sim_hdr_set_ccd_side(&ev_pkt.hdr, FEE_CCD_SIDE_E);
			fee_sim_hdr_set_ccd_id(&ev_pkt.hdr,   FEE_CCD_ID_4);
			fee_sim_detect_events(cfg, &ev_pkt, E4, rows, cols, smile_fee_get_ccd2_f_pix_threshold());
		}
	}

	if (readout & FEE_READOUT_NODE_F4) {
		F4 = fee_sim_get_ft_data(CCD4F, rows, cols, bins);
		ccd_sim_add_rd_noise(F4, rows * cols);

		/* event detection is only done for 6x binning */
		if (smile_fee_get_ccd_mode2_config() == FEE_MODE2_BIN6) {
			fee_sim_hdr_set_ccd_side(&ev_pkt.hdr, FEE_CCD_SIDE_F);
			fee_sim_hdr_set_ccd_id(&ev_pkt.hdr,   FEE_CCD_ID_4);
			fee_sim_detect_events(cfg, &ev_pkt, F4, rows, cols, smile_fee_get_ccd4_f_pix_threshold());
		}
	}

	/* no FT if event detection is enabled */
	if (!smile_fee_get_event_detection()) {
		if (smile_fee_get_digitise_en()) {
			/* transfer only when digitise is enabled */
			fee_sim_frame_transfer(cfg, FEE_MODE_ID_FTP,
					       (uint8_t *) E2, (uint8_t *) F2,
					       (uint8_t *) E4, (uint8_t *) F4,
					       sizeof(uint16_t) * rows * cols);
		}
	}

	/* TODO: if event detection is active, send a last_packet
	 * header to indicate end of frame when either the number of remaining
	 * events to send is 0 (NOTE: this is a per-CCD limit!) OR the last frame is fully processed
	 */

#if 1
	/* for testing */
	ccd_sim_add_rd_noise(CCD2E, FEE_CCD_IMG_SEC_ROWS * FEE_CCD_IMG_SEC_COLS);
	save_fits("!CCD2E.fits", CCD2E, FEE_CCD_IMG_SEC_ROWS, FEE_CCD_IMG_SEC_COLS);
	save_fits("!E2.fits", E2, rows, cols);
	ccd_sim_add_rd_noise(CCD2F, FEE_CCD_IMG_SEC_ROWS * FEE_CCD_IMG_SEC_COLS);
	save_fits("!CCD2F.fits", CCD2F, FEE_CCD_IMG_SEC_ROWS, FEE_CCD_IMG_SEC_COLS);
	save_fits("!F2.fits", F2, rows, cols);
#endif


#if 1

	/* for testing: dump raw interleaved E/F data */

	if (smile_fee_get_ccd_mode2_config() == FEE_MODE2_BIN6) {
		if (readout == 0xF) {
			FILE *fd;

			uint16_t *px_inter;
			size_t i;

			px_inter = malloc(2*sizeof(uint16_t) * rows * cols);


			for (i = 0; i < rows*cols; i++) {
			       px_inter[2*i]   = (E4[i]);
			       px_inter[2*i+1] = (E2[i]);
			}

			fd = fopen("e_raw.dat", "w");
			fwrite((void *)px_inter, 2 * rows * cols, 2, fd);
			fclose(fd);


			for (i = 0; i < rows*cols; i++) {
			       px_inter[2*i]   = (F4[i]);
			       px_inter[2*i+1] = (F2[i]);
			}

			fd = fopen("f_raw.dat", "w");
			fwrite((void *)px_inter, 2 * rows * cols, 2, fd);
			fclose(fd);


			free(px_inter);

		}
	}


#endif

	free(E2);
	free(F2);
	free(E4);
	free(F4);
}



/**
 * @brief extract interleaved image data from SRAM
 *
 * @note the node value must be unambiguous (i.e. just one bit set, not the
 *	whole field)
 */

static uint16_t *fee_sim_get_sram_data(uint16_t node)
{
	size_t i;

	uint16_t *buf;
	uint16_t *E_buf;
	uint16_t *F_buf;

	const size_t rows = FEE_EDU_FRAME_6x6_ROWS;
	const size_t cols = FEE_EDU_FRAME_6x6_COLS;


	buf = malloc(sizeof(uint16_t) * rows * cols);
	if (!buf) {
		perror("malloc");
		exit(-1);
	}

	/* We always re-read the SRAM mirror buffers. The ccd4 and ccd2
	 * E and F side data are stored as interleaved 16 bit samples
	 */

	E_buf = malloc(sizeof(uint16_t) * rows * cols * 2);
	if (!E_buf) {
		perror("malloc");
		exit(-1);
	}


	F_buf = malloc(sizeof(uint16_t) * rows * cols * 2);
	if (!F_buf) {
		perror("malloc");
		exit(-1);
	}

	/* the SRAM areas for the pixel data appear slightly oversized. let's
	 * assume the actual samples for 6x6 bin mode start at the beginning
	 * and the remainder is just spare for margin...
	 */
	smile_fee_read_sram_16(E_buf, FEE_SRAM_SIDE_E_START, 2 * rows * cols);
	smile_fee_read_sram_16(F_buf, FEE_SRAM_SIDE_F_START, 2 * rows * cols);


	/* CCD4 pixels start at index 0 */
	if (node == FEE_READOUT_NODE_E4) {
		for (i = 0; i < rows * cols; i++)
			buf[i] = E_buf[2 * i];
	}

	/* CCD2 pixels start at index 1 */
	if (node == FEE_READOUT_NODE_E2) {
		for (i = 0; i < rows * cols; i++)
			buf[i] = E_buf[2 * i + 1];
	}

	/* CCD4 pixels start at index 0 */
	if (node == FEE_READOUT_NODE_F4) {
		for (i = 0; i < rows * cols; i++)
			buf[i] = F_buf[2 * i];
	}

	/* CCD2 pixels start at index 1 */
	if (node == FEE_READOUT_NODE_F2) {
		for (i = 0; i < rows * cols; i++)
			buf[i] = F_buf[2 * i + 1];
	}


	free(E_buf);
	free(F_buf);

	return buf;
}





static void fee_sim_exec_evsim_mode(struct sim_net_cfg *cfg)
{
	uint16_t *E2 = NULL;
	uint16_t *F2 = NULL;
	uint16_t *E4 = NULL;
	uint16_t *F4 = NULL;

	uint16_t readout;

	size_t E_ev_cnt = smile_fee_get_event_pkt_limit();
	size_t F_ev_cnt = smile_fee_get_event_pkt_limit();

	struct fee_event_detection ev_pkt = {0};

	const size_t rows = FEE_EDU_FRAME_6x6_ROWS;
	const size_t cols = FEE_EDU_FRAME_6x6_COLS;


	/* prep EV packet structure */
	fee_sim_hdr_set_logical_addr(&ev_pkt.hdr, DPU_LOGICAL_ADDRESS);
	fee_sim_hdr_set_protocol_id(&ev_pkt.hdr, FEE_DATA_PROTOCOL);
	fee_sim_hdr_set_pkt_type(&ev_pkt.hdr, FEE_PKT_TYPE_EV_DET);
	fee_sim_hdr_set_frame_cntr(&ev_pkt.hdr, fee_get_frame_cntr());
	fee_sim_hdr_set_data_len(&ev_pkt.hdr, FEE_EV_DATA_LEN);



	readout = smile_fee_get_readout_node_sel();


	if (readout & FEE_READOUT_NODE_E2) {
		E2 = fee_sim_get_sram_data(FEE_READOUT_NODE_E2);

		fee_sim_hdr_set_ccd_side(&ev_pkt.hdr, FEE_CCD_SIDE_E);
		fee_sim_hdr_set_ccd_id(&ev_pkt.hdr,   FEE_CCD_ID_2);
		fee_sim_detect_events(cfg, &ev_pkt, E2, rows, cols, smile_fee_get_ccd2_e_pix_threshold());

	}

	if (readout & FEE_READOUT_NODE_F2) {
		F2 = fee_sim_get_sram_data(FEE_READOUT_NODE_F2);

		fee_sim_hdr_set_ccd_side(&ev_pkt.hdr, FEE_CCD_SIDE_F);
		fee_sim_hdr_set_ccd_id(&ev_pkt.hdr,   FEE_CCD_ID_2);
		fee_sim_detect_events(cfg, &ev_pkt, F2, rows, cols, smile_fee_get_ccd4_e_pix_threshold());
	}

	if (readout & FEE_READOUT_NODE_E4) {
		E4 = fee_sim_get_sram_data(FEE_READOUT_NODE_E4);

		fee_sim_hdr_set_ccd_side(&ev_pkt.hdr, FEE_CCD_SIDE_E);
		fee_sim_hdr_set_ccd_id(&ev_pkt.hdr,   FEE_CCD_ID_4);
		fee_sim_detect_events(cfg, &ev_pkt, E4, rows, cols, smile_fee_get_ccd2_f_pix_threshold());
	}

	if (readout & FEE_READOUT_NODE_F4) {
		F4 = fee_sim_get_sram_data(FEE_READOUT_NODE_F4);

		fee_sim_hdr_set_ccd_side(&ev_pkt.hdr, FEE_CCD_SIDE_F);
		fee_sim_hdr_set_ccd_id(&ev_pkt.hdr,   FEE_CCD_ID_4);
		fee_sim_detect_events(cfg, &ev_pkt, F4, rows, cols, smile_fee_get_ccd4_f_pix_threshold());
	}


	/* TODO: if event detection is active, send a last_packet
	 * header to indicate end of frame when either the number of remaining
	 * events to send is 0 (NOTE: this is a per-CCD limit!) OR the last frame is fully processed
	 */


	free(E2);
	free(F2);
	free(E4);
	free(F4);
}






/**
 * @brief generate a pattern for frame transfer pattern mode
 */

static struct fee_pattern *fee_sim_gen_ft_pat(uint8_t ccd_side, uint8_t ccd_id,
					      size_t rows, size_t cols)
{
	size_t i, j;

	struct fee_pattern pix;
	struct fee_pattern *buf;


	buf = malloc(sizeof(struct fee_pattern) * rows * cols);
	if (!buf) {
		perror("malloc");
		exit(-1);
	}

	pix.side = ccd_side;
	pix.ccd  = ccd_id;
	pix.time_code = fee_get_spw_time_code() & 0x7;

	for (i = 0; i < rows; i++) {

		pix.row = i & 0x1F;

		for (j = 0; j < cols; j++) {

			pix.col = j & 0x1F;

#if 0
			buf[i * cols + j].field = __cpu_to_be16(pix.field);
#else
			buf[i * cols + j].field = pix.field;
#endif
		}
	}

	return buf;
}



static void fee_sim_exec_ft_pat_mode(struct sim_net_cfg *cfg)
{
	struct fee_pattern *E2 = NULL;
	struct fee_pattern *F2 = NULL;
	struct fee_pattern *E4 = NULL;
	struct fee_pattern *F4 = NULL;

	size_t rows, cols;
	uint16_t readout;


	switch (smile_fee_get_ccd_mode2_config()) {

	case FEE_MODE2_NOBIN:
		rows = FEE_CCD_IMG_SEC_ROWS;
		cols = FEE_CCD_IMG_SEC_COLS;
		break;

	case FEE_MODE2_BIN6:
		rows = FEE_EDU_FRAME_6x6_ROWS;
		cols = FEE_EDU_FRAME_6x6_COLS;
		break;

	case FEE_MODE2_BIN24:
		/* values are guessed base on returned data from FEE, the
		 * actual size is not mentioned in the IRD
		 * it is unclear whether this mode is ever going to be used
		 */
		rows = FEE_EDU_FRAME_24x24_ROWS;
		cols = FEE_EDU_FRAME_24x24_COLS;
		break;
	default:
		printf("Unknown binning mode specified\n");
		return;
	}

	readout = smile_fee_get_readout_node_sel();


	if (readout & FEE_READOUT_NODE_E2)
		E2 = fee_sim_gen_ft_pat(FEE_CCD_SIDE_E, FEE_CCD_ID_2, rows, cols);

	if (readout & FEE_READOUT_NODE_F2)
		F2 = fee_sim_gen_ft_pat(FEE_CCD_SIDE_F, FEE_CCD_ID_2, rows, cols);

	if (readout & FEE_READOUT_NODE_E4)
		E4 = fee_sim_gen_ft_pat(FEE_CCD_SIDE_E, FEE_CCD_ID_4, rows, cols);

	if (readout & FEE_READOUT_NODE_F4)
		F4 = fee_sim_gen_ft_pat(FEE_CCD_SIDE_F, FEE_CCD_ID_4, rows, cols);


	/* transfer only when digitise is enabled */
	if (smile_fee_get_digitise_en()) {
		fee_sim_frame_transfer(cfg, FEE_MODE_ID_FTP,
				       (uint8_t *) E2, (uint8_t *) F2,
				       (uint8_t *) E4, (uint8_t *) F4,
				       sizeof(struct fee_pattern) * rows * cols);
	}

	free(E2);
	free(F2);
	free(E4);
	free(F4);
}


/**
 * @brief main simulator execution
 */

static void fee_sim_exec(struct sim_net_cfg *cfg)
{
	uint8_t mode;


	mode = smile_fee_get_ccd_mode_config();

	switch (mode) {

	case FEE_MODE_ID_ON:
		/* the thing is switched on */
		printf("We're switched on, cool!\n");
		break;
	case FEE_MODE_ID_FTP:
		/* frame transfer pattern */
		printf("Frame Transfer Pattern Mode\n");
		fee_sim_exec_ft_pat_mode(cfg);
		break;
	case FEE_MODE_ID_STBY:
		/* stand-by-mode */
		printf("We're in stand-by, no idea what that does\n");
		break;
	case FEE_MODE_ID_FT:
		/* frame transfer */
		printf("Frame Transfer Mode\n");
		fee_sim_exec_ft_mode(cfg);
		break;
	case FEE_MODE_ID_FF:
		/* full frame */
		printf("Full Frame Mode\n");
		fee_sim_exec_ff_mode(cfg, FEE_MODE_ID_FF);
		break;
	case FEE_CMD__ID_IMM_ON:
		/* immediate on-mode, this is a command, not a mode */
		printf("Mode %d not implemented\n", mode);
		break;

	case FEE_MODE_ID_EVSIM:
		printf("Event detection simulation\n");
		fee_sim_exec_evsim_mode(cfg);
		break;
	case FEE_MODE_ID_PTP1:
		/* parallel trap pump mode 1 */
		printf("Mode %d not implemented\n", mode);
		break;
	case FEE_MODE_ID_PTP2:
		/* parallel trap pump mode 2 */
		printf("Mode %d not implemented\n", mode);
		break;
	case FEE_MODE_ID_STP1:
		/* serial trap pump mode 1 */
		printf("Mode %d not implemented\n", mode);
		break;
	case FEE_MODE_ID_STP2:
		/* serial trap pump mode 2 */
		printf("Mode %d not implemented\n", mode);
		break;
	default:
		printf("Unknown mode %d, ignoring\n", mode);
		break;
	}
}


/**
 * @brief sim main loop
 */

void fee_sim_main(struct sim_net_cfg *cfg)
{
	/* we have 2 ccds, each with 2 sides (E&F) */
	const size_t img_side_pix = FEE_CCD_IMG_SEC_ROWS * FEE_CCD_IMG_SEC_COLS;

	CCD2E = (uint16_t *) malloc(img_side_pix * sizeof(uint16_t));
	if (!CCD2E) {
		perror("malloc");
		exit(-1);
	}

	CCD2F = (uint16_t *) malloc(img_side_pix * sizeof(uint16_t));
	if (!CCD2F) {
		perror("malloc");
		exit(-1);
	}

	CCD4E = (uint16_t *) malloc(img_side_pix * sizeof(uint16_t));
	if (!CCD4E) {
		perror("malloc");
		exit(-1);
	}

	CCD4F = (uint16_t *) malloc(img_side_pix * sizeof(uint16_t));
	if (!CCD4F) {
		perror("malloc");
		exit(-1);
	}

	/* we need only a single readout area for simulation */
	RDO = (uint16_t *) malloc(img_side_pix * sizeof(uint16_t));
	if (!RDO) {
		perror("malloc");
		exit(-1);
	}


	/* simulator main loop */
	while (1) {

		if (!smile_fee_get_execute_op()) {
			/* poll every 1 ms */
			usleep(1000);
			continue;
		}

		printf("EXECUTE OP!\n");
		fee_sim_exec(cfg);

		smile_fee_set_execute_op(0);
		printf("OP complete\n");
	}

	free(CCD2E);
	free(CCD2F);
	free(CCD4E);
	free(CCD4F);
	free(RDO);
}
