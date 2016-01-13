#include "xinclude.h"
#include "xtypes.h"
#include "xdiff.h"

/*
 * Output a single diff hunk.
 */
int xdl_do_lazy_diff(mmfile_t *mf1, mmfile_t *mf2,
		     xpparam_t const *xpp, xdfenv_t *xe)
{
	long i;

	if (xdl_prepare_env(mf1, mf2, xpp, xe) < 0)
		return -1;

	for (i = 0; i < xe->xdf1.nrec; i++)
		xe->xdf1.rchg[i] = 1;
	for (i = 0; i < xe->xdf2.nrec; i++)
		xe->xdf2.rchg[i] = 1;

	i = 0;
	while (i < xe->xdf1.nrec && i < xe->xdf2.nrec) {
		xrecord_t *r1 = xe->xdf1.recs[i];
		xrecord_t *r2 = xe->xdf2.recs[i];

		if (!r1 || !r2 ||
		    r1->ha != r2->ha ||
		    r1->size != r2->size ||
		    memcmp(r1->ptr, r2->ptr, r1->size))
			break;

		xe->xdf1.rchg[i] = 0;
		xe->xdf2.rchg[i] = 0;
		i++;
	}

	i = 1;
	while (0 <= xe->xdf1.nrec - i && 0 <= xe->xdf2.nrec - i) {
		xrecord_t *r1 = xe->xdf1.recs[xe->xdf1.nrec - i];
		xrecord_t *r2 = xe->xdf2.recs[xe->xdf2.nrec - i];

		if (!r1 || !r2 ||
		    r1->ha != r2->ha ||
		    r1->size != r2->size ||
		    memcmp(r1->ptr, r2->ptr, r1->size))
			break;

		xe->xdf1.rchg[xe->xdf1.nrec - i] = 0;
		xe->xdf2.rchg[xe->xdf2.nrec - i] = 0;
		i++;
	}

	return 0;
}

