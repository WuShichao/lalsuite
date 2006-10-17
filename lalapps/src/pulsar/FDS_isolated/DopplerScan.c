/*
 * Copyright (C) 2004, 2005, 2006 Reinhard Prix
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with with program; see the file COPYING. If not, write to the 
 *  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *  MA  02111-1307  USA
 */

/**
 * \author Reinhard Prix
 * \date 2004, 2005
 * \file
 * \brief Functions for generating search-grids for CFS.
 */

/*---------- INCLUDES ----------*/
#include <math.h>

#include <lal/LALStdlib.h>
#include <lal/DetectorSite.h>
#include <lal/StackMetric.h>
#include <lal/AVFactories.h>
#include <lal/LALError.h>
#include <lal/LALXMGRInterface.h>
#include <lal/StringInput.h>
#include <lal/ConfigFile.h>
#include <lal/Velocity.h>

#include <lal/LogPrintf.h>

#include "FlatPulsarMetric.h"

#include "DopplerScan.h"



/*---------- DEFINES ----------*/
#define MIN(x,y) (x < y ? x : y)
#define MAX(x,y) (x > y ? x : y)

#define TRUE (1==1)
#define FALSE (1==0)

/* Metric indexing scheme: if g_ij for i<=j: index = i + j*(j+1)/2 */
/* the variable-order of PulsarMetric is {f, alpha, delta, f1, f2, ... } */

#define INDEX_f0_f0	PMETRIC_INDEX(0,0)
#define INDEX_f0_A  	PMETRIC_INDEX(0,1)
#define INDEX_f0_D  	PMETRIC_INDEX(0,2)
#define INDEX_f0_f1 	PMETRIC_INDEX(0,3)

#define INDEX_A_A	PMETRIC_INDEX(1,1)
#define INDEX_D_D	PMETRIC_INDEX(2,2)
#define INDEX_A_D	PMETRIC_INDEX(1,2)
#define INDEX_A_f1	PMETRIC_INDEX(1,3)

#define INDEX_D_f1	PMETRIC_INDEX(2,3)

#define INDEX_f1_f1	PMETRIC_INDEX(3,3)

/* stay away ~ 1e-2 from all boundaries to avoid roundoff-problems leading to discrete 
 * differences on different platforms. This is particularly important for E@H-validation.
 */
#define DELTA_0     "-1.56)"
#define DELTA_1     " 1.56)"
#define ALPHA_0     "(1.0e-2, "
#define ALPHA_1     "(6.27, "

#define SKYREGION_ALLSKY  ALPHA_0 DELTA_0 "," ALPHA_1 DELTA_0 "," ALPHA_1 DELTA_1 "," ALPHA_0 DELTA_1

/* the amount by which we push-in the encosing rectangle to avoid spurious polygon-clipping
 * of boundary-points due to numerical fluctuations in TwoDMesh(). 
 */
#define EPS4	1e-6

NRCSID( DOPPLERSCANC, "$Id$" );

/*---------- internal types ----------*/
/** TwoDMesh() can have either of two preferred directions of meshing: */
enum {
  ORDER_ALPHA_DELTA,
  ORDER_DELTA_ALPHA
};

static int meshOrder = ORDER_DELTA_ALPHA;

typedef REAL4	meshREAL;
typedef TwoDMeshNode meshNODE;
typedef TwoDMeshParamStruc meshPARAMS;

/** ----- internal [opaque] type to store the state of a FULL multidimensional grid-scan ----- */
struct tagDopplerFullScanState {
  INT2 state;  			/**< idle, ready or finished */

  /* ----- the following are used to emulate foliated grids sky x Freq x f1dot ... */
  DopplerSkyScanState skyScan;	/**< keep track of sky-grid stepping */

  PulsarSpinRange spinRange;	/**< spin-range to search */
  PulsarSpins dfkdot;		/**< fixed stepsizes in spins */
  UINT4 spinCounters[PULSAR_MAX_SPINS]; /**< index of current spin-values being searched */

  /* ----- future extensions for real multidim-grids  ----- */
 
} /* struct DopplerFullScanState */;


/*---------- empty initializers ---------- */
/* some empty structs for initializations */
static const meshPARAMS empty_meshpar;

static const PtoleMetricIn empty_metricpar;
static const MetricParamStruc empty_MetricParamStruc;
static const PulsarTimesParamStruc empty_PulsarTimesParamStruc;

const DopplerSkyGrid empty_DopplerSkyGrid;
const DopplerSkyScanState empty_DopplerSkyScanState;
const DopplerSkyScanInit empty_DopplerSkyScanInit;
const DopplerRegion empty_DopplerRegion;
const PulsarDopplerParams empty_PulsarDopplerParams;

const DopplerFullScanState empty_DopplerFullScanState;

/*---------- Global variables ----------*/
extern INT4 lalDebugLevel;

/*---------- internal prototypes ----------*/
void getRange( LALStatus *, meshREAL y[2], meshREAL x, void *params );
void getMetric( LALStatus *, meshREAL g[3], meshREAL skypos[2], void *params );
REAL8 getDopplermax(EphemerisData *edat);

void ConvertTwoDMesh2SkyGrid ( LALStatus *, DopplerSkyGrid **grid, const meshNODE *mesh2d, const SkyRegion *region );

BOOLEAN pointInPolygon ( const SkyPosition *point, const SkyRegion *polygon );

void gridFlipOrder ( meshNODE *grid );

void buildFlatSkyGrid (LALStatus *, DopplerSkyGrid **skygrid, const SkyRegion *region, REAL8 dAlpha, REAL8 dDelta);
void buildIsotropicSkyGrid (LALStatus *, DopplerSkyGrid **skygrid, const SkyRegion *skyRegion, REAL8 dAlpha, REAL8 dDelta);
void buildMetricSkyGrid (LALStatus *, DopplerSkyGrid **skygrid, SkyRegion *skyRegion,  const DopplerSkyScanInit *init);
void loadSkyGridFile (LALStatus *, DopplerSkyGrid **skygrid, const CHAR *fname);

void plotSkyGrid (LALStatus *, DopplerSkyGrid *skygrid, const SkyRegion *region, const DopplerSkyScanInit *init);
void freeSkyGrid(DopplerSkyGrid *skygrid);
void getGridSpacings(LALStatus *, PulsarDopplerParams *spacings, PulsarDopplerParams gridpoint, const DopplerSkyScanInit *params);

void printFrequencyShifts(LALStatus *, const DopplerSkyScanState *skyScan, const DopplerSkyScanInit *init);

const char *va(const char *format, ...);	/* little var-arg string helper function */

/*==================== FUNCTION DEFINITIONS ====================*/

/** Set up a full multi-dimensional grid-scan. 
 * Currently this only emulates a 'foliated' grid-scan with sky x Freq x f1dot ... , but
 * keeps all details within the DopplerScan module for future extension to real multidimensional
 * grids. Use 'NextDopplerPos()' to step through this grid.
 */
void 
InitDopplerFullScan(LALStatus *status, 
		    DopplerFullScanState *scan,			/**< [out] initialized Doppler scan state */
		    const DetectorStateSeries *detStates, 	/**< [in] used for list of integration timestamps and detector-info */
		    const DopplerFullScanInit *init		/**< [in] initialization parameters */
		    )
{
  DopplerSkyScanInit skyScanInit = empty_DopplerSkyScanInit;
  LIGOTimeGPS startTime, endTime;
  REAL8 duration;
  UINT4 i;

  INITSTATUS( status, "InitDopplerFullScan", DOPPLERSCANC );
  ATTATCHSTATUSPTR (status); 

  ASSERT ( scan, status, DOPPLERSCANH_EINPUT, DOPPLERSCANH_MSGEINPUT );
  ASSERT ( detStates, status, DOPPLERSCANH_EINPUT, DOPPLERSCANH_MSGEINPUT );
  ASSERT ( init, status, DOPPLERSCANH_EINPUT, DOPPLERSCANH_MSGEINPUT );

  startTime = detStates->data[0].tGPS;
  endTime = detStates->data[detStates->length].tGPS;
  duration = XLALGPSDiff ( &endTime, &startTime );

  /* set up FullScanState */
  (*scan) = empty_DopplerFullScanState;

  /* ===== now we just emulate the foliate sky x Freq x f1dot scanning ===== */
  /* prepare initialization of DopplerSkyScanner to step through paramter space */
  skyScanInit.dAlpha = init->spacings.Alpha;
  skyScanInit.dDelta = init->spacings.Delta;
  skyScanInit.gridType = init->gridType;
  skyScanInit.metricType = init->metricType;
  skyScanInit.metricMismatch = init->metricMismatch;
  skyScanInit.projectMetric = TRUE;
  skyScanInit.obsBegin = startTime;
  skyScanInit.obsDuration = duration;

  skyScanInit.Detector = &(detStates->detector);
  skyScanInit.ephemeris = init->ephemeris;		/* used only by Ephemeris-based metric */
  skyScanInit.skyGridFile = init->skyGridFile;
  skyScanInit.skyRegionString = init->searchRegion.skyRegionString;
  skyScanInit.Freq = init->searchRegion.fkdot[0] + init->searchRegion.fkdotBand[0];

  LogPrintf (LOG_DEBUG, "Setting up template sky-grid ... ");
  TRY ( InitDopplerSkyScan ( status->statusPtr, &(scan->skyScan), &skyScanInit), status); 
  LogPrintfVerbatim (LOG_DEBUG, "done.\n");

  scan->spinRange.epoch = init->searchRegion.epoch;
  memcpy ( scan->spinRange.fkdot, init->searchRegion.fkdot, sizeof(PulsarSpins) );
  memcpy ( scan->spinRange.fkdotBand, init->searchRegion.fkdotBand, sizeof(PulsarSpins) );

  scan->dfkdot[0] = scan->skyScan.dFreq;
  scan->dfkdot[1] = scan->skyScan.df1dot;

  /* override with user-settings if given */
  for (i=0; i < PULSAR_MAX_SPINS; i ++ )
    if ( init->spacings.fkdot[i] )
      scan->dfkdot[i] = init->spacings.fkdot[i];

  /* set counters to start */
  memset ( scan->spinCounters, 0, sizeof(scan->spinCounters) );

  /* we're ready */
  scan->state = STATE_READY;


  DETATCHSTATUSPTR (status);
  RETURN( status );

} /* InitDopplerFullScan() */

/** Function to step through the full template grid point by point. 
 * Normal return = 0, errors return -1, end of scan is signalled by return = 1
 */
int
XLALNextDopplerPos(PulsarDopplerParams *pos, DopplerFullScanState *scan)
{ 
  PulsarDopplerParams dopplerpos = empty_PulsarDopplerParams;
  SkyPosition skypos;
  PulsarSpins fkdotStep;
  UINT4 i;

  /* This traps coding errors in the calling routine. */
  if ( pos == NULL || scan == NULL )
    {
      xlalErrno = XLAL_EINVAL;
      return -1;
    }

  memset ( fkdotStep, 0, sizeof( fkdotStep ) );

  switch( scan->state )
    {
    case STATE_IDLE:
    case STATE_FINISHED:
      xlalErrno = XLAL_EFAILED;
      LogPrintf (LOG_CRITICAL, "DopplerScan 'scan' not in ready state\n");
      return -1;	/* scan is not ready to proceed */
      break;

    case STATE_READY:  
      /* get next template, using the ordering: { df3dot , df2dot, df1dot, df0dot, sky[Alpha,Delta] } */
      scan->spinCounters[3] ++;
      fkdotStep[3] = scan->spinCounters[3] * scan->dfkdot[3];
      if ( fkdotStep[3] > scan->spinRange.fkdotBand[3] )
	{
	  scan->spinCounters[3] = 0;
	  fkdotStep[3] = 0;
	  scan->spinCounters[2] ++;
	  fkdotStep[2] = scan->spinCounters[2] * scan->dfkdot[2];
	  if ( fkdotStep[2] > scan->spinRange.fkdotBand[2] )
	    {
	      scan->spinCounters[2] = 0;
	      fkdotStep[2] = 0;
	      scan->spinCounters[1] ++;
	      fkdotStep[1] = scan->spinCounters[1] * scan->dfkdot[1];
		if ( fkdotStep[1] > scan->spinRange.fkdotBand[1] )
		  {
		    scan->spinCounters[1] = 0;
		    fkdotStep[1] = 0;
		    scan->spinCounters[0] ++;
		    fkdotStep[0] = scan->spinCounters[0] * scan->dfkdot[0];
		    if ( fkdotStep[0] > scan->spinRange.fkdotBand[0] )
		    {
		      scan->spinCounters[0] = 0;
		      fkdotStep[0] = 0;
		      if ( XLALNextDopplerSkyPos(&dopplerpos, &scan->skyScan ) != 0 ) {
			LogPrintf (LOG_CRITICAL, "sky-stepping with NextDopplerSkyPos() failed!\n");
			return -1;
		      }
		      if (scan->skyScan.state == STATE_FINISHED) /* scanned all sky-positions yet? */
			{
			  scan->state = STATE_FINISHED;
			  return 1;
			} /* sky-scan finished ? */
		      else
			{
			  /* normalize skyposition: correctly map into [0,2pi]x[-pi/2,pi/2] */
			  skypos.longitude = dopplerpos.Alpha;
			  skypos.latitude = dopplerpos.Delta;
			  skypos.system = COORDINATESYSTEM_EQUATORIAL;
			  XLALNormalizeSkyPosition ( &skypos );
			} /* [ Alpha, Delta ] */
		    } /* f0dot */
		  } /* f1dot */
	    } /* f2dot */
	} /* f3dot */
      
      /* return the new Doppler-position */
      pos->Alpha = skypos.longitude;
      pos->Delta = skypos.latitude;
      for (i=0; i < PULSAR_MAX_SPINS; i ++ )
	pos->fkdot[i] = scan->spinRange.fkdot[i] + fkdotStep[i];
      
      break;
  
    } /* switch scan->state */

  return 0;
  
} /* XLALNextDopplerPos() */


/* ============================== SKYGRID-ONLY scanning functions ============================== */
/** Initialize the Doppler sky-scanner 
 */
void
InitDopplerSkyScan( LALStatus *status, 
		    DopplerSkyScanState *skyScan, 		/**< [out] the initialized scan-structure */
		    const DopplerSkyScanInit *init)	/**< [in] init-params */
{
  DopplerSkyGrid *node; 

  INITSTATUS( status, "InitDopplerSkyInit", DOPPLERSCANC );
  ATTATCHSTATUSPTR (status); 

  /* This traps coding errors in the calling routine. */
  ASSERT ( skyScan != NULL, status, DOPPLERSCANH_ENONULL, DOPPLERSCANH_MSGENONULL );  
  ASSERT ( skyScan->state == STATE_IDLE, status, DOPPLERSCANH_EINPUT, DOPPLERSCANH_MSGEINPUT );
  ASSERT ( init->gridType < GRID_LAST, status, DOPPLERSCANH_EINPUT, DOPPLERSCANH_MSGEINPUT );
  
  /* trap some abnormal input */
  if ( (init->gridType != GRID_FILE) && (init->gridType != GRID_METRIC_SKYFILE) && (init->skyRegionString == NULL) ) 
    {
      LogPrintf (LOG_CRITICAL, "ERROR: No sky-region was specified!\n\n");
      ABORT (status,  DOPPLERSCANH_ENULL ,  DOPPLERSCANH_MSGENULL );
    } 
  if ( (init->gridType == GRID_FILE) && (init->skyGridFile == NULL) )
    {
      LogPrintf (LOG_CRITICAL, "ERROR: no skyGridFile has been specified!\n\n");
      ABORT (status,  DOPPLERSCANH_ENULL ,  DOPPLERSCANH_MSGENULL );
    }

  /* general initializations */
  skyScan->skyGrid = NULL;  
  skyScan->skyNode = NULL;
  
  if ( (init->gridType != GRID_FILE) && ( init->gridType != GRID_METRIC_SKYFILE) ) 
    {
      TRY (ParseSkyRegionString(status->statusPtr, &(skyScan->skyRegion), init->skyRegionString), status);

      if (skyScan->skyRegion.numVertices == 2){ /* anomaly! Allowed are either 1 or >= 3 */
	ABORT (status, DOPPLERSCANH_E2DSKY, DOPPLERSCANH_MSGE2DSKY);
      }
    } /* if gridType != GRID_FILE || GRID_METRIC_SKYFILE */
 
  switch (init->gridType)
    {
    case GRID_FLAT:		/* flat-grid: constant dAlpha, dDelta */
      TRY ( buildFlatSkyGrid( status->statusPtr, &(skyScan->skyGrid), &(skyScan->skyRegion), init->dAlpha, init->dDelta), status);
      break;

    case GRID_ISOTROPIC: 	/* variant of manual stepping: try to produce an isotropic mesh */
      TRY ( buildIsotropicSkyGrid( status->statusPtr, &(skyScan->skyGrid), &(skyScan->skyRegion), init->dAlpha, init->dDelta), status);
      break;

    case GRID_METRIC:
      TRY ( buildMetricSkyGrid (status->statusPtr, &(skyScan->skyGrid), &(skyScan->skyRegion), init), status);
      break;

    case GRID_METRIC_SKYFILE:
    case GRID_FILE:
      TRY ( loadSkyGridFile (status->statusPtr, &(skyScan->skyGrid), init->skyGridFile), status);
      break;

    default:
      LogPrintf (LOG_CRITICAL, "Unknown grid-type `%d`\n\n", init->gridType);
      ABORT ( status, DOPPLERSCANH_EMETRICTYPE, DOPPLERSCANH_MSGEMETRICTYPE);
      break;

    } /* switch (metric) */

  /* NOTE: we want to make sure we return at least one grid-point: 
   * so check if we got one, and if not, we return the
   * first point of the skyRegion-polygon as a grid-point
   */
  if (skyScan->skyGrid == NULL)
    {
      skyScan->skyGrid = LALCalloc (1, sizeof(DopplerSkyGrid));
      if (skyScan->skyGrid == NULL) {
	ABORT (status, DOPPLERSCANH_EMEM, DOPPLERSCANH_MSGEMEM);
      }
      skyScan->skyGrid->Alpha = skyScan->skyRegion.vertices[0].longitude;
      skyScan->skyGrid->Delta = skyScan->skyRegion.vertices[0].latitude;
      
    } /* no points found inside of sky-region */

  /* initialize skygrid-pointer to first node in list */
  skyScan->skyNode = skyScan->skyGrid; 	

  /* count number of nodes in our sky-grid */
  skyScan->numSkyGridPoints = 0;
  node = skyScan->skyGrid;
  while (node)
    {
      skyScan->numSkyGridPoints ++;
      node = node->next;
    }

  if (lalDebugLevel >= 4)
    {
      LogPrintf (LOG_NORMAL, "DEBUG: plotting sky-grid into file 'mesh_debug.agr' ...");
      TRY( plotSkyGrid (status->statusPtr, skyScan->skyGrid, &(skyScan->skyRegion), init), status);
      LogPrintfVerbatim (LOG_NORMAL, " done.\n");
    }

  /* ----------
   * determine spacings in frequency and spindowns
   * NOTE: this is only meaningful if these spacings
   * are sufficiently independent of the phase-parameters
   * ----------*/
  {
    PulsarDopplerParams gridpoint = empty_PulsarDopplerParams;
    PulsarDopplerParams gridSpacings = empty_PulsarDopplerParams;

    gridpoint.Alpha = skyScan->skyGrid->Alpha;
    gridpoint.Delta = skyScan->skyGrid->Delta;
    gridpoint.fkdot[0] = init->Freq;
    gridpoint.fkdot[1] = 0;
    gridpoint.fkdot[2] = 0;
    gridpoint.fkdot[3] = 0;

    TRY ( getGridSpacings( status->statusPtr, &gridSpacings, gridpoint, init), status);


    LogPrintf (LOG_DETAIL, "'theoretical' spacings in frequency and spindown: \n");
    LogPrintf (LOG_DETAIL, "dFreq = %g, df1dot = %g, df2dot = %g, df3dot = %g\n", 
	       gridSpacings.fkdot[0], gridSpacings.fkdot[1], gridSpacings.fkdot[2], gridSpacings.fkdot[3]);

    skyScan->dFreq  = gridSpacings.fkdot[0];
    skyScan->df1dot = gridSpacings.fkdot[1];
  }
  
  skyScan->state = STATE_READY;

  /* clean up */
  DETATCHSTATUSPTR (status);
  RETURN( status );

} /* InitDopplerSkyScan() */


/** Destroy the DopplerSkyScanState structure  
 */
void
FreeDopplerSkyScan (LALStatus *status, DopplerSkyScanState *skyScan)
{ 

  INITSTATUS( status, "FreeDopplerSkyScan", DOPPLERSCANC);
  ATTATCHSTATUSPTR (status);

  /* This traps coding errors in the calling routine. */
  ASSERT( skyScan != NULL, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL );
  
  if ( skyScan->state == STATE_IDLE )
    {
      LALError (status, "\nTrying to free an uninitialized DopplerSkyScan.\n");
      ABORT (status, DOPPLERSCANH_ENOTREADY, DOPPLERSCANH_MSGENOTREADY);
    }
  else if ( skyScan->state != STATE_FINISHED )
    LALWarning (status, "freeing unfinished DopplerSkyScan.");

  freeSkyGrid (skyScan->skyGrid);

  skyScan->skyGrid = skyScan->skyNode = NULL;

  if (skyScan->skyRegion.vertices)
    LALFree (skyScan->skyRegion.vertices);
  skyScan->skyRegion.vertices = NULL;
    
  skyScan->state = STATE_IDLE;

  DETATCHSTATUSPTR (status);
  RETURN( status );

} /* FreeDopplerSkyScan() */

/*----------------------------------------------------------------------
 *  helper-function: free the linked list containing the grid 
 *----------------------------------------------------------------------*/
void
freeSkyGrid (DopplerSkyGrid *skygrid)
{
  DopplerSkyGrid *node, *next;

  if ( skygrid == NULL)
    return;

  node = skygrid;

  while (node)
    {
      next = node->next;
      LALFree (node);
      node = next;
    } /* while node */

  return;
} /* freeSkyGrid() */

/** NextDopplerSkyPos(): step through sky-grid 
 * return 0 = OK, -1 = ERROR 
 */
int 
XLALNextDopplerSkyPos( PulsarDopplerParams *pos, DopplerSkyScanState *skyScan)
{ 

  /* This traps coding errors in the calling routine. */
  if ( pos == NULL || skyScan == NULL )
    {
      xlalErrno = XLAL_EINVAL;
      return -1;
    }

  switch( skyScan->state )
    {
    case STATE_IDLE:
    case STATE_FINISHED:
      xlalErrno = XLAL_EFAILED;
      return -1;
      break;

    case STATE_READY:  
      if (skyScan->skyNode == NULL) 	/* we're done */
	skyScan->state = STATE_FINISHED;
      else
	{
	  pos->Alpha = skyScan->skyNode->Alpha;
	  pos->Delta = skyScan->skyNode->Delta;

	  skyScan->skyNode = skyScan->skyNode->next;  /* step forward */
	}
      break;

    } /* switch skyScan->stat */

  return 0;

} /* XLALNextDopplerSkyPos() */


/* **********************************************************************
   The following 2 helper-functions for TwoDMesh() have been adapted 
   from Ben's PtoleMeshTest.

   NOTE: Currently we are very expicitly restricted to 2D searches!! 
   FIXME: generalize to N-dimensional parameter-searches
********************************************************************** */

/** This is the parameter range function as required by TwoDMesh(). 
 *
 * NOTE: for the moment we only provide a trival range as defined by the  
 * rectangular parameter-area [ a1, a2 ] x [ d1, d2 ]
 * 
 * In order to avoid later cliping of boundary-points only due to numerical 
 * fluctuations, we push the enclosing rectangle boundaries inside
 * by about EPS~1e-6 [as REAL4-arithmetic is used in TwoDMesh()].
 *
 */
void getRange( LALStatus *status, meshREAL y[2], meshREAL x, void *params )
{
  SkyRegion *region = (SkyRegion*)params;
  meshREAL nix;

  /* Set up shop. */
  INITSTATUS( status, "getRange", DOPPLERSCANC );
  /*   ATTATCHSTATUSPTR( status ); */

  nix = x;	/* avoid compiler warning */
  
  /* for now: we return the fixed y-range, indendent of x */
  if (meshOrder == ORDER_ALPHA_DELTA)
    {
      y[0] = region->lowerLeft.latitude + EPS4;
      y[1] = region->upperRight.latitude - EPS4;
    }
  else
    {
      y[0] = region->lowerLeft.longitude + EPS4;
      y[1] = region->upperRight.longitude - EPS4;
    }


  /* Clean up and leave. */
  /*   DETATCHSTATUSPTR( status ); */

  RETURN( status );
} /* getRange() */


/* ----------------------------------------------------------------------
 * This is a wrapper for the metric function as required by TwoDMesh. 
 *
 *
 * NOTE: this will be called by TwoDMesh(), therefore
 * skypos is in internalOrder, which is not necessarily ORDER_ALPHA_DELTA!!
 * 
 * NOTE2: this is only using the 2D projected sky-metric !
 *
 *----------------------------------------------------------------------*/
void getMetric( LALStatus *status, meshREAL g[3], meshREAL skypos[2], void *params )
{
  REAL8Vector   *metric = NULL;  /* for output of metric */
  DopplerSkyScanInit *par = (DopplerSkyScanInit*) params;
  PtoleMetricIn metricpar = empty_metricpar;

  /* Set up shop. */
  INITSTATUS( status, "getMetric", DOPPLERSCANC );
  ATTATCHSTATUSPTR( status );

  /* set up the metric parameters proper (using PtoleMetricIn as container-type) */
  metricpar.metricType = par->metricType;
  metricpar.position.system = COORDINATESYSTEM_EQUATORIAL;
  /* theoretically and empirically it seems that the spindowns 
   * do not influence the sky-metric to a good approximation,
   * at least for 'physical' values of spindown. 
   * We take this 0 therefore
   */
  metricpar.spindown = NULL;	

  metricpar.epoch = par->obsBegin;
  metricpar.duration = par->obsDuration;
  metricpar.maxFreq = par->Freq;
  metricpar.site = par->Detector;
  metricpar.ephemeris = par->ephemeris;

  /* Call the metric function. (Ptole or Coherent, which is handled by wrapper) */
  if (meshOrder == ORDER_ALPHA_DELTA)
    {
      metricpar.position.longitude = skypos[0];
      metricpar.position.latitude =  skypos[1];
    }
  else
    {
      metricpar.position.longitude = skypos[1];
      metricpar.position.latitude =  skypos[0];
    }

  /* before we call the metric: make sure the sky-position  is "normalized" */
  TRY ( LALNormalizeSkyPosition(status->statusPtr, &(metricpar.position), &(metricpar.position)), status);

  TRY ( LALPulsarMetric( status->statusPtr, &metric, &metricpar), status);

  /* project metric if requested */
  if (par->projectMetric)
    {
      LALProjectMetric( status->statusPtr, metric, 0 );

      BEGINFAIL( status )
	TRY( LALDDestroyVector( status->statusPtr, &metric ), status );
      ENDFAIL( status );
    }

  /* Translate output. Careful about the coordinate-order here!! */
  if (meshOrder == ORDER_ALPHA_DELTA)
    {
      g[0] = metric->data[INDEX_A_A]; /* gxx */
      g[1] = metric->data[INDEX_D_D]; /* gyy */
    }
  else
    {
      g[0] = metric->data[INDEX_D_D]; /* gxx */
      g[1] = metric->data[INDEX_A_A]; /* gyy */
    }

  g[2] = metric->data[INDEX_A_D]; /* g12: 1 + 2*(2+1)/2 = 4; */

  /* check positivity */
  if ( lalDebugLevel ) 
    {
      REAL8 det = g[0] * g[1] - g[2]*g[2];
      if ( (g[0] <=0) || ( g[1] <= 0 ) || ( det <= 0 ) ) 
	{
	  LogPrintf (LOG_CRITICAL, "Negative sky-metric found!\n");
	  LogPrintf (LOG_CRITICAL, "Skypos = [%16g, %16g],\n\n"
		     "metric = [ %16g, %16g ;\n"
		     "           %16g, %16g ],\n\n"
		     "det = %16g\n\n",
		     metricpar.position.longitude, metricpar.position.latitude,
		     metric->data[INDEX_A_A], metric->data[INDEX_A_D],
		     metric->data[INDEX_A_D], metric->data[INDEX_D_D],
		     det);
	  ABORT ( status, DOPPLERSCANH_ENEGMETRIC, DOPPLERSCANH_MSGENEGMETRIC);
	} /* if negative metric */
    } /* if lalDebugLevel() */


  /* Clean up and leave. */
  TRY( LALDDestroyVector( status->statusPtr, &metric ), status );

  DETATCHSTATUSPTR( status );
  RETURN( status );
} /* getMetric() */


/*----------------------------------------------------------------------
 * Debug helper for mesh and metric stuff
 *----------------------------------------------------------------------*/
#define SPOKES 60  /* spokes for ellipse-plots */
#define NUM_SPINDOWN 0       /* Number of spindown parameters */

void 
plotSkyGrid (LALStatus *status, 
	     DopplerSkyGrid *skyGrid, 
	     const SkyRegion *region, 
	     const DopplerSkyScanInit *init)
{
  FILE *fp = NULL;	/* output xmgrace-file */
  FILE *fp1 = NULL;	/* output pure data */
  DopplerSkyGrid *node;
  REAL8 Alpha, Delta;
  UINT4 set, i;

  const CHAR *xmgrHeader = 
    "@version 50103\n"
    "@title \"Sky-grid\"\n"
    "@world xmin -0.1\n"
    "@world xmax 6.4\n"
    "@world ymin -1.58\n"
    "@world ymax 1.58\n"
    "@xaxis label \"Alpha\"\n"
    "@yaxis label \"Delta\"\n";

  /* Set up shop. */
  INITSTATUS( status, "plotSkyGrid", DOPPLERSCANC );
  ATTATCHSTATUSPTR( status );

  fp = fopen ("mesh_debug.agr", "w");
  fp1 = fopen ("mesh_debug.dat", "w");

  if( !fp ) {
    ABORT ( status, DOPPLERSCANH_ESYS, DOPPLERSCANH_MSGESYS );
  }
  
  fprintf (fp, xmgrHeader);

  set = 0;

  /* Plot boundary. (if given) */
  if (region->vertices)
    {
      fprintf( fp, "@target s%d\n@type xy\n", set );

      for( i = 0; i < region->numVertices; i++ )
	{
	  fprintf( fp, "%e %e\n", region->vertices[i].longitude, region->vertices[i].latitude );
	  fprintf( fp1,"%e %e\n", region->vertices[i].longitude, region->vertices[i].latitude );
	}
      /* close contour */
      fprintf (fp, "%e %e\n", region->vertices[0].longitude, region->vertices[0].latitude ); 
      fprintf (fp1, "%e %e\n\n", region->vertices[0].longitude, region->vertices[0].latitude ); 

      set ++;
    }
      
  /* Plot mesh points. */
  fprintf( fp, "@s%d symbol 9\n@s%d symbol size 0.33\n", set, set );
  fprintf( fp, "@s%d line type 0\n", set );
  fprintf( fp, "@target s%d\n@type xy\n", set );

  for( node = skyGrid; node; node = node->next )
  {
    fprintf( fp, "%e %e\n", node->Alpha, node->Delta );
    fprintf( fp1, "%e %e\n", node->Alpha, node->Delta );
  }
  fprintf( fp1, "\n\n");

  /* if metric is given: plot ellipses */
  if ( (lalDebugLevel >=5) && ( init->metricType > LAL_PMETRIC_NONE) && (init->metricType < LAL_PMETRIC_LAST) )
    {
      REAL8Vector  *metric = NULL;   /* Parameter-space metric: for plotting ellipses */
      MetricEllipse ellipse;
      REAL8 mismatch = init->metricMismatch;
      PtoleMetricIn metricPar;

      set++;

      /* set up the metric parameters common to all sky-points */
      metricPar.position.system = COORDINATESYSTEM_EQUATORIAL;
      metricPar.spindown = LALCalloc ( 1, sizeof(REAL4Vector) );
      metricPar.spindown->length=0;
      metricPar.spindown->data=NULL;
      metricPar.epoch = init->obsBegin;
      metricPar.duration = init->obsDuration;
      metricPar.maxFreq = init->Freq;
      metricPar.site = init->Detector;
      metricPar.ephemeris = init->ephemeris;
      metricPar.metricType = init->metricType;
      
      node = skyGrid;
      while (node)
	{
	  Alpha =  node->Alpha;
	  Delta =  node->Delta;

	  /* Get the metric at this skypos */
	  /* only need the update the position, the other
	   * parameter have been set already ! */
	  metricPar.position.longitude = Alpha;
	  metricPar.position.latitude  = Delta;

	  /* make sure we "normalize" point before calling metric */
	  TRY( LALNormalizeSkyPosition(status->statusPtr, &(metricPar.position), 
				       &(metricPar.position) ), status);

	  TRY( LALPulsarMetric( status->statusPtr, &metric, &metricPar), status);

	  if ( init->projectMetric ) {
	    TRY (LALProjectMetric( status->statusPtr, metric, 0 ), status);
	  }

	  TRY (getMetricEllipse(status->statusPtr, &ellipse, mismatch, metric, 1), status);

	  /*
	  printf ("Alpha=%f Delta=%f\ngaa=%f gdd=%f gad=%f\n", Alpha, Delta, gaa, gdd, gad);
	  printf ("smaj = %f, smin = %f angle=%f\n", smaj, smin, angle);
	  */
	  set ++;
	  /* Print set header. */
	  fprintf( fp, "@target G0.S%d\n@type xy\n", set);
	  fprintf( fp, "@s%d color (0,0,0)\n", set );
	  
	  /* Loop around patch ellipse. */
	  for (i=0; i<=SPOKES; i++) {
	    float c, r, b, x, y;
	    
	    c = LAL_TWOPI*i/SPOKES;
	    x = ellipse.smajor * cos(c);
	    y = ellipse.sminor * sin(c);
	    r = sqrt( x*x + y*y );
	    b = atan2 ( y, x );
	    fprintf( fp, "%e %e\n", 
		     Alpha + r*cos(ellipse.angle+b), Delta + r*sin(ellipse.angle+b) );

	    fprintf( fp1, "%e %e\n", 
		     Alpha + r*cos(ellipse.angle+b), Delta + r*sin(ellipse.angle+b) );
	  }
	  fprintf ( fp1, "\n\n");

	  node = node -> next;

	  TRY( LALDDestroyVector( status->statusPtr, &metric ), status );
	  metric = NULL;
	  
	} /* while node */
      
      LALFree (metricPar.spindown);

    } /* if plotEllipses */
      
  fclose(fp);

  DETATCHSTATUSPTR( status );
  RETURN (status);

} /* plotSkyGrid */

/** Function for checking if a given point lies inside or outside a given
 *  polygon, which is specified by a list of points in a SkyPositionVector.
 *
 * \par Note1: 
 * 	The list of polygon-points must not close on itself, the last point
 * 	is automatically assumed to be connected to the first 
 * 
 * \par Alorithm: 
 *     Count the number of intersections of rays emanating to the right
 *     from the point with the lines of the polygon: even=> outside, odd=> inside
 *
 * \par Note2: 
 *     we try to get this algorith to count all boundary-points as 'inside'
 *     we do this by counting intersection to the left _AND_ to the right
 *     and consider the point inside if either of those says its inside...
 *
 * \return : TRUE or FALSE
 *----------------------------------------------------------------------*/
BOOLEAN
pointInPolygon ( const SkyPosition *point, const SkyRegion *polygon )
{
  UINT4 i;
  UINT4 N;
  UINT4 insideLeft, insideRight;
  BOOLEAN inside = 0;
  SkyPosition *vertex;
  REAL8 xinter, v1x, v1y, v2x, v2y, px, py;

  if (!point || !polygon || !polygon->vertices || (polygon->numVertices < 3) )
    return 0;

  vertex = polygon->vertices;
  N = polygon->numVertices; 	/* num of vertices = num of edges */

  insideLeft = insideRight = 0;

  px = point->longitude;
  py = point->latitude;

  for (i=0; i < N; i++)
    {
      v1x = vertex[i].longitude;
      v1y = vertex[i].latitude;
      v2x = vertex[(i+1) % N].longitude;
      v2y = vertex[(i+1) % N].latitude;

      /* pre-select candidate edges */
      if ( (py <  MIN(v1y,  v2y)) || (py >=  MAX(v1y, v2y) ) || (v1y == v2y) )
	continue;

      /* now calculate the actual intersection point of the horizontal ray with the edge in question*/
      xinter = v1x + (py - v1y) * (v2x - v1x) / (v2y - v1y);

      if (xinter > px)	      /* intersection lies to the right of point */
	insideLeft ++;

      if (xinter < px)       /* intersection lies to the left of point */
	insideRight ++;

    } /* for sides of polygon */

  inside = ( ((insideLeft %2) == 1) || (insideRight %2) == 1);
  return inside;
  
} /* pointInPolygon() */

/*----------------------------------------------------------------------
 * Translate a TwoDMesh into a DopplerSkyGrid using a SkyRegion for clipping
 * 
 * NOTE: the returned grid will be NULL if there are no points inside the sky-region
 *----------------------------------------------------------------------*/
void 
ConvertTwoDMesh2SkyGrid ( LALStatus *status, 
		       DopplerSkyGrid **skyGrid, 	/* output: a dopperScan-grid */
		       const meshNODE *mesh2d, 		/* input: a 2Dmesh */
		       const SkyRegion *region )   	/* a sky-region for clipping */
{
  const meshNODE *meshpoint;
  DopplerSkyGrid head = empty_DopplerSkyGrid;
  DopplerSkyGrid *node = NULL;
  SkyPosition point;

  INITSTATUS( status, "ConvertTwoDMesh2SkyGrid", DOPPLERSCANC );

  ASSERT ( *skyGrid == NULL, status, DOPPLERSCANH_ENONULL, DOPPLERSCANH_MSGENONULL );
  ASSERT (region, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL);
  ASSERT (mesh2d, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL);

  meshpoint = mesh2d;	/* this is the 2-d mesh from LALTwoDMesh() */

  node = &head;		/* this is our Doppler-grid (empty for now) */
  
  while (meshpoint)
    {
      if (meshOrder == ORDER_ALPHA_DELTA)
	{
	  point.longitude = meshpoint->x;
	  point.latitude = meshpoint->y;
	}
      else
	{
	  point.longitude = meshpoint->y;
	  point.latitude = meshpoint->x;
	}
      
      if (pointInPolygon (&point, region) )
	{
	  /* prepare a new node for this point */
	  if ( (node->next = (DopplerSkyGrid*) LALCalloc (1, sizeof(DopplerSkyGrid) )) == NULL) {
	    ABORT ( status, DOPPLERSCANH_EMEM, DOPPLERSCANH_MSGEMEM );
	  }
	  node = node->next;

	  node->Alpha = point.longitude;
	  node->Delta = point.latitude;

	} /* if point in polygon */
      else
	LogPrintf (LOG_DEBUG, "Point [%g, %g] has been discarded by polygon-clipping!\n",
		   point.longitude, point.latitude );

      meshpoint = meshpoint->next;

    } /* while meshpoint */

  
  *skyGrid = head.next;		/* return the final skygrid (excluding static head) */

  RETURN (status);

} /* ConvertTwoDMesh2SkyGrid() */


/*----------------------------------------------------------------------
 *
 * make a "flat" grid, i.e. a grid with fixed mesh-sizes dAlpha, dDelta
 *
 *----------------------------------------------------------------------*/
void
buildFlatSkyGrid (LALStatus *status, 
	       DopplerSkyGrid **skyGrid, 
	       const SkyRegion *skyRegion, 
	       REAL8 dAlpha, 
	       REAL8 dDelta)
{
  SkyPosition thisPoint;
  DopplerSkyGrid head = empty_DopplerSkyGrid;  /* empty head to start grid-list */
  DopplerSkyGrid *node = NULL;

  INITSTATUS( status, "buildFlatSkyGrid", DOPPLERSCANC );

  ASSERT ( skyGrid, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL);
  ASSERT ( skyRegion, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL);
  ASSERT ( (dAlpha > 0) && (dDelta > 0), status, DOPPLERSCANH_EINPUT, DOPPLERSCANH_MSGEINPUT);
  ASSERT ( *skyGrid == NULL, status, DOPPLERSCANH_ENONULL, DOPPLERSCANH_MSGENONULL);

  thisPoint = skyRegion->lowerLeft;	/* start from lower-left corner */

  node = &head;

  while (1)
    {
      if (pointInPolygon (&thisPoint, skyRegion) )
	{
	  /* prepare this node */
	  node->next = LALCalloc (1, sizeof(DopplerSkyGrid));
	  if (node->next == NULL) {
	    freeSkyGrid ( head.next );
	    ABORT (status, DOPPLERSCANH_EMEM, DOPPLERSCANH_MSGEMEM);
	  }
	  node = node->next;
	  
	  node->Alpha = thisPoint.longitude;
	  node->Delta = thisPoint.latitude;
	} /* if pointInPolygon() */
	  
      thisPoint.latitude += dDelta;
	  
      if (thisPoint.latitude > skyRegion->upperRight.latitude)
	{
	  thisPoint.latitude = skyRegion->lowerLeft.latitude;
	  thisPoint.longitude += dAlpha;
	} 
      
      /* this it the break-condition: are we done yet? */
      if (thisPoint.longitude >= skyRegion->upperRight.longitude + dAlpha)
	break;
	  
    } /* while(1) */

  *skyGrid = head.next;	/* return final grid-list */

  
  RETURN (status);

} /* buildFlatSkyGrid */


/*----------------------------------------------------------------------
 *
 * (approx.) isotropic grid with cells of fixed solid-angle dAlpha x dDelta
 *
 *----------------------------------------------------------------------*/
void
buildIsotropicSkyGrid (LALStatus *status, DopplerSkyGrid **skyGrid, const SkyRegion *skyRegion, REAL8 dAlpha, REAL8 dDelta)
{
  SkyPosition thisPoint;
  DopplerSkyGrid head = empty_DopplerSkyGrid;  /* empty head to start grid-list */
  DopplerSkyGrid *node = NULL;
  REAL8 step_Alpha, step_Delta, cos_Delta;

  INITSTATUS( status, "buildIsotropicSkyGrid", DOPPLERSCANC );

  ASSERT ( skyGrid, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL);
  ASSERT ( skyRegion, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL);
  ASSERT ( *skyGrid == NULL, status, DOPPLERSCANH_ENONULL, DOPPLERSCANH_MSGENONULL);

  thisPoint = skyRegion->lowerLeft;	/* start from lower-left corner */

  step_Delta = dDelta;	/* Delta step-size is fixed */
  cos_Delta = fabs(cos (thisPoint.latitude));

  node = &head;		/* start our grid with an empty head */

  while (1)
    {
      if (pointInPolygon ( &thisPoint, skyRegion ) ) 
	{
	  /* prepare this node */
	  node->next = LALCalloc (1, sizeof(DopplerSkyGrid));
	  if (node->next == NULL) {
	    freeSkyGrid ( head.next );
	    ABORT (status, DOPPLERSCANH_EMEM, DOPPLERSCANH_MSGEMEM);
	  }
	  node = node->next;
	      
	  node->Alpha = thisPoint.longitude;
	  node->Delta = thisPoint.latitude;
	} /* if pointInPolygon() */
      
      step_Alpha = dAlpha / cos_Delta;	/* Alpha stepsize depends on Delta */

      thisPoint.longitude += step_Alpha;
      if (thisPoint.longitude > skyRegion->upperRight.longitude)
	{
	  thisPoint.longitude = skyRegion->lowerLeft.longitude;
	  thisPoint.latitude += step_Delta;	
	  cos_Delta = fabs (cos (thisPoint.latitude));
	} 

      /* this it the break-condition: are we done yet? */
      if (thisPoint.latitude > skyRegion->upperRight.latitude)
	break;
	  
    } /* while(1) */

  *skyGrid = head.next;	/* set result: could be NULL! */

  RETURN (status);

} /* buildIsotropicSkyGrid() */

/** Build the skygrid using a specified metric.
 *
 * \note: first we cover the enclosing rectange of the skyRegion
 * using the metric-covering code, then we clip out the actual 
 * polygon-skyRegion using pointInPolygon().
 * 
 * In order for this not to clip boundary-points only due to numerical 
 * fluctuations, we push the enclosing rectangle boundaries inside
 * by about EPS~1e-6 [as REAL4-arithmetic is used in TwoDMesh()].
 *
 */
void
buildMetricSkyGrid (LALStatus *status, 
		    DopplerSkyGrid **skyGrid, 
		    SkyRegion *skyRegion,  
		    const DopplerSkyScanInit *init)
{
  SkyPosition thisPoint;
  meshNODE *mesh2d = NULL;
  meshPARAMS meshpar = empty_meshpar;
  PtoleMetricIn metricpar = empty_metricpar;
  DopplerSkyScanInit params = (*init);

  INITSTATUS( status, "buildMetricSkyGrid", DOPPLERSCANC );
  ATTATCHSTATUSPTR (status);

  ASSERT ( skyGrid, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL);
  ASSERT ( skyRegion, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL);
  ASSERT ( *skyGrid == NULL, status, DOPPLERSCANH_ENONULL, DOPPLERSCANH_MSGENONULL);
  ASSERT ( (init->metricType > LAL_PMETRIC_NONE) && (init->metricType < LAL_PMETRIC_LAST), 
	   status, DOPPLERSCANH_EINPUT, DOPPLERSCANH_MSGEINPUT);

  thisPoint = skyRegion->lowerLeft;	/* start from lower-left corner */
  
  /* some general mesh-settings are needed in metric case */
  meshpar.getRange = getRange;
      
  if (meshOrder == ORDER_ALPHA_DELTA)
    {
      meshpar.domain[0] = skyRegion->lowerLeft.longitude + EPS4;
      meshpar.domain[1] = skyRegion->upperRight.longitude - EPS4;
    }
  else
    {
      meshpar.domain[0] = skyRegion->lowerLeft.latitude + EPS4;
      meshpar.domain[1] = skyRegion->upperRight.latitude - EPS4;
    }
  
  meshpar.rangeParams = (void*) skyRegion;

  /* Prepare call of TwoDMesh(): the mesh-parameters */
  meshpar.mThresh = init->metricMismatch;
  meshpar.nIn = 1e8;  	/* maximum nodes in mesh */ /*  FIXME: hardcoded */
      
  /* helper-function: range and metric */
  meshpar.getMetric = getMetric;
  /* and its parameters: simply pass the whole DopplerSkyScanInit struct, which
   * contains all the required information 
   */
  meshpar.metricParams = (void *) (&params);

  /* finally: create the mesh! (ONLY 2D for now!) */
  TRY( LALCreateTwoDMesh( status->statusPtr, &mesh2d, &meshpar ), status);

  if (metricpar.spindown) {
    /* FIXME: this is currently broken in LAL, as length=0 is not allowed */
    /*    TRY (LALSDestroyVector ( status->statusPtr, &(skyScan->MetricPar.spindown) ), status); */
    LALFree (metricpar.spindown);
    metricpar.spindown = NULL;
  }

  /* convert this 2D-mesh into our skygrid-structure, including clipping to the skyRegion */
  if ( mesh2d ) 
    ConvertTwoDMesh2SkyGrid ( status->statusPtr, skyGrid, mesh2d, skyRegion );

  /* get rid of 2D-mesh */
  TRY ( LALDestroyTwoDMesh ( status->statusPtr,  &mesh2d, 0), status);

  DETATCHSTATUSPTR (status);
  RETURN (status);

} /* buildMetricSkyGrid() */


/*----------------------------------------------------------------------
 *
 * load skygrid from a file
 *
 *----------------------------------------------------------------------*/
void
loadSkyGridFile (LALStatus *status, DopplerSkyGrid **skyGrid, const CHAR *fname)
{
  LALParsedDataFile *data = NULL;
  DopplerSkyGrid *node, head = empty_DopplerSkyGrid;
  UINT4 i;

  INITSTATUS( status, "loadSkyGridFile", DOPPLERSCANC );
  ATTATCHSTATUSPTR (status);

  ASSERT ( skyGrid, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL);
  ASSERT ( *skyGrid == NULL, status, DOPPLERSCANH_ENONULL, DOPPLERSCANH_MSGENONULL);
  ASSERT ( fname, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL);

  TRY (LALParseDataFile (status->statusPtr, &data, fname), status);

  /* parse this list of lines into a sky-grid */
  node = &head;	/* head will remain empty! */
  for (i=0; i < data->lines->nTokens; i++)
    {
      /* prepare next list-entry */
      if ( (node->next = LALCalloc (1, sizeof (DopplerSkyGrid))) == NULL)
	{
	  freeSkyGrid (head.next);
	  ABORT (status, DOPPLERSCANH_EMEM, DOPPLERSCANH_MSGEMEM);
	}

      node = node->next;

      if ( 2 != sscanf( data->lines->tokens[i], "%" LAL_REAL8_FORMAT "%" LAL_REAL8_FORMAT, &(node->Alpha), &(node->Delta)) )
	{
	  LogPrintf (LOG_CRITICAL,"ERROR: could not parse line %d in skyGrid-file '%s'\n\n", i, fname);
	  freeSkyGrid (head.next);
	  ABORT (status, DOPPLERSCANH_EINPUT, DOPPLERSCANH_MSGEINPUT);
	}

    } /* for i < nLines */

  TRY ( LALDestroyParsedDataFile (status->statusPtr, &data), status);

  *skyGrid = head.next;	/* pass result (without head!) */

  DETATCHSTATUSPTR (status);
  RETURN (status);
  
} /* loadSkyGridFile() */

/** Write the given sky-grid to a file.
 * Possibly including some comments containing the parameters of the grid (?).
 *
 */
void
writeSkyGridFile (LALStatus *status, 
		  const DopplerSkyGrid *skyGrid, 
		  const CHAR *fname )
{
  FILE *fp;
  const DopplerSkyGrid *node;

  INITSTATUS( status, "writeSkyGridFile", DOPPLERSCANC );
  ATTATCHSTATUSPTR (status);

  ASSERT ( skyGrid, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL);
  ASSERT ( fname, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL);

  if ( (fp = fopen(fname, "w")) == NULL )
    {
      LogPrintf (LOG_CRITICAL, "ERROR: could not open %s for writing!\n", fname);
      ABORT (status, DOPPLERSCANH_ESYS, DOPPLERSCANH_MSGESYS);
    }

  node = skyGrid;
  while (node)
    {
      fprintf (fp, "%g %g \n", node->Alpha, node->Delta);
      node = node->next;
    }
  
  fclose (fp);

  DETATCHSTATUSPTR (status);
  RETURN (status);

} /* writeSkyGridFile() */

/*----------------------------------------------------------------------
 * 
 * write predicted frequency-shift of Fmax as function of sky-position
 *
 *----------------------------------------------------------------------*/
void
printFrequencyShifts ( LALStatus *status, const DopplerSkyScanState *skyScan, const DopplerSkyScanInit *init)
{
  FILE *fp;
  const CHAR *fname = "dFreq.pred";
  const DopplerSkyGrid *node = NULL;

  REAL8 v[3], a[3];
  REAL8 np[3], n[3];
  REAL8 fact, kappa0;
  REAL8 Alpha, Delta, f0;
  REAL8* vel;
  REAL8* acc;
  REAL8 t0e;        /*time since first entry in Earth ephem. table */  
  INT4 ientryE;      /*entry in look-up table closest to current time, tGPS */

  REAL8 tinitE;           /*time (GPS) of first entry in Earth ephem table*/
  REAL8 tdiffE;           /*current time tGPS minus time of nearest entry
                             in Earth ephem look-up table */
  REAL8 tgps[2];
  EphemerisData *edat = init->ephemeris;
  UINT4 j;
  REAL8 corr1, accN, accDot[3];
  REAL8 Tobs, dT;
  REAL8 V0[3], V1[3], V2[3];

  INITSTATUS( status, "printFrequencyShifts", DOPPLERSCANC );
  ATTATCHSTATUSPTR (status);

  if ( (fp = fopen(fname, "w")) == NULL) {
    LogPrintf (LOG_CRITICAL, "ERROR: could not open %s for writing!\n", fname);
    ABORT (status, DOPPLERSCANH_ESYS, DOPPLERSCANH_MSGESYS);
  }

  /* get detector velocity */
  Tobs = init->obsDuration;

  tgps[0] = (REAL8)(init->obsBegin.gpsSeconds);
  tgps[1] = (REAL8)(init->obsBegin.gpsNanoSeconds);

  tinitE = edat->ephemE[0].gps;
  dT = edat->dtEtable;
  t0e = tgps[0] - tinitE;
  ientryE = floor((t0e/dT) +0.5e0);  /*finding Earth table entry closest to arrival time*/

  /* tdiff is arrival time minus closest Earth table entry; tdiff can be pos. or neg. */
  tdiffE = t0e - edat->dtEtable*ientryE + tgps[1]*1.e-9; 


  vel = edat->ephemE[ientryE].vel;
  acc = edat->ephemE[ientryE].acc; 

  /* interpolate v, a to the actual start-time */
  for (j = 0; j < 3; j++)
    {
      accDot[j] = (edat->ephemE[ientryE+1].acc[j] - edat->ephemE[ientryE].acc[j]) / dT;
      v[j] = vel[j] + acc[j]*tdiffE + 0.5 * accDot[j] * tdiffE * tdiffE;
      a[j] = acc[j] + accDot[j] * tdiffE;
    }


  /* calculate successively higher order-expressions for V */
  for ( j=0; j < 3; j++)
    {
      V0[j] = v[j];			/* 0th order */
      V1[j] = v[j] + 0.5 * a[j]*Tobs;	/* 1st order */
      V2[j] = v[j] + 0.5 * a[j]*Tobs + (2.0/5.0)*accDot[j] * Tobs * Tobs; /* 2nd order */
    }

  LALPrintError ("dT = %f, tdiffE = %f, Tobs = %f\n", dT, tdiffE, Tobs);
  LALPrintError (" vel =  [ %g, %g, %g ]\n", vel[0], vel[1], vel[2]);
  LALPrintError (" acc =  [ %g, %g, %g ]\n", acc[0], acc[1], acc[2]);
  LALPrintError (" accDot =  [ %g, %g, %g ]\n\n", accDot[0], accDot[1], accDot[2]);

  LALPrintError (" v =  [ %g, %g, %g ]\n", v[0], v[1], v[2]);
  LALPrintError (" a =  [ %g, %g, %g ]\n", a[0], a[1], a[2]);

  LALPrintError ("\nVelocity-expression in circle-equation: \n");
  LALPrintError (" V0 = [ %g, %g, %g ]\n", V0[0], V0[1], V0[2] );
  LALPrintError (" V1 = [ %g, %g, %g ]\n", V1[0], V1[1], V1[2] );
  LALPrintError (" V2 = [ %g, %g, %g ]\n", V2[0], V2[1], V2[2] );

  node = skyScan->skyGrid;

  /* signal params */
  Alpha = 0.8;
  Delta = 1.0;
  f0 = 101.0;

  n[0] = cos(Delta) * cos(Alpha);
  n[1] = cos(Delta) * sin(Alpha);
  n[2] = sin(Delta);
  
  kappa0 = (1.0 + n[0]*v[0] + n[1]*v[1] + n[2]*v[2] );

  accN = (acc[0]*n[0] + acc[1]*n[1] + acc[2]*n[2]);
  corr1 = (1.0/60.0)* (accN * accN) * Tobs * Tobs / kappa0;

  while (node)
    {
      np[0] = cos(node->Delta) * cos(node->Alpha);
      np[1] = cos(node->Delta) * sin(node->Alpha);
      np[2] = sin(node->Delta);

      fact = 1.0 / (1.0 + np[0]*v[0] + np[1]*v[1] + np[2]*v[2] );
      
      fprintf (fp, "%.7f %.7f %.7f\n", node->Alpha, node->Delta, fact);

      node = node->next;
    } /* while grid-points */
  


  fclose (fp);
  
  DETATCHSTATUSPTR (status);
  RETURN (status);

} /* printFrequencyShifts() */


/** some temp test-code outputting the maximal possible dopper-shift
 * |vE| + |vS| over the ephemeris */
REAL8 
getDopplermax(EphemerisData *edat)
{
#define mymax(a,b) ((a) > (b) ? (a) : (b))
  UINT4 i;
  REAL8 maxvE, maxvS;
  REAL8 *vel, beta;
      
  maxvE = 0;
  for (i=0; i < (UINT4)edat->nentriesE; i++)
    {
      vel = edat->ephemE[i].vel;
      beta = sqrt( vel[0]*vel[0] + vel[1]*vel[1] + vel[2]*vel[2] );
      maxvE = mymax( maxvE, beta );
    }
  
  maxvS = 0;	
  for (i=0; i < (UINT4)edat->nentriesS; i++)
    {
      vel = edat->ephemS[i].vel;
      beta = sqrt( vel[0]*vel[0] + vel[1]*vel[1] + vel[2]*vel[2] );
      maxvS = mymax( maxvS, beta );
    }
  
  LALPrintError ("Maximal Doppler-shift to be expected from ephemeris: %e", maxvE + maxvS );

  return (maxvE + maxvS);

} /* getDopplermax() */


/**
 * parse a skyRegion-string into a SkyRegion structure: the expected 
 * string-format is
 *   " (ra1, dec1), (ra2, dec2), (ra3, dec3), ... "
 *
 * One special string "allsky" is current understood ==> replace by a 
 * sky-region covering the whole sky.
 *
 */
void
ParseSkyRegionString (LALStatus *status, SkyRegion *region, const CHAR *input)
{ 
  const CHAR *pos;
  const CHAR *skyRegion = NULL;
  CHAR buf[100];
  UINT4 i;

  INITSTATUS( status, "ParseSkyRegionString", DOPPLERSCANC );
  ATTATCHSTATUSPTR (status);

  ASSERT (region != NULL, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL);
  ASSERT (region->vertices == NULL, status, DOPPLERSCANH_ENONULL,  DOPPLERSCANH_MSGENONULL);
  ASSERT (input != NULL, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL);

  region->numVertices = 0;
  region->lowerLeft.longitude = LAL_TWOPI;
  region->lowerLeft.latitude = LAL_PI;
  region->upperRight.longitude = 0;
  region->upperRight.latitude = 0;
  region->lowerLeft.system = region->upperRight.system = COORDINATESYSTEM_EQUATORIAL;

  /* ----- first check if special skyRegion string was specified: */
  strncpy (buf, input, 99);
  buf[99] = 0;
  TRY ( LALLowerCaseString (status->statusPtr, buf), status);
  /* check if "allsky" was given: replace input by allsky-skyRegion */
  if ( !strcmp( buf, "allsky" ) )	/* All-sky search */
    skyRegion = SKYREGION_ALLSKY;
  else
    skyRegion = input;


  /* count number of entries (by # of opening parantheses) */
  pos = skyRegion;
  while ( (pos = strchr (pos, '(')) != NULL )
    {
      region->numVertices ++;
      pos ++;
    }

  if (region->numVertices == 0) {
    LogPrintf (LOG_CRITICAL, "Failed to parse sky-region: `%s`\n", skyRegion);
    ABORT (status, DOPPLERSCANH_ESKYREGION, DOPPLERSCANH_MSGESKYREGION);
  }
    
  
  /* allocate list of vertices */
  if ( (region->vertices = LALMalloc (region->numVertices * sizeof (SkyPosition))) == NULL) {
    ABORT (status, DOPPLERSCANH_EMEM, DOPPLERSCANH_MSGEMEM);
  }

  region->lowerLeft.longitude = LAL_TWOPI;
  region->lowerLeft.latitude  = LAL_PI/2.0;

  region->upperRight.longitude = 0;
  region->upperRight.latitude  = -LAL_PI/2;


  /* and parse list of vertices from input-string */
  pos = skyRegion;
  for (i = 0; i < region->numVertices; i++)
    {
      if ( sscanf (pos, "(%" LAL_REAL8_FORMAT ", %" LAL_REAL8_FORMAT ")", 
		   &(region->vertices[i].longitude), &(region->vertices[i].latitude) ) != 2) 
	{
	  ABORT (status, DOPPLERSCANH_ESKYREGION, DOPPLERSCANH_MSGESKYREGION);
	}

      /* keep track of min's and max's to get the bounding square */
      region->lowerLeft.longitude=MIN(region->lowerLeft.longitude,region->vertices[i].longitude);
      region->lowerLeft.latitude =MIN(region->lowerLeft.latitude, region->vertices[i].latitude);

      region->upperRight.longitude = MAX( region->upperRight.longitude, 
					  region->vertices[i].longitude);
      region->upperRight.latitude  = MAX( region->upperRight.latitude, 
					  region->vertices[i].latitude);

      pos = strchr (pos + 1, '(');

    } /* for numVertices */

  DETATCHSTATUSPTR(status);
  RETURN (status);

} /* ParseSkyRegionString() */

/*----------------------------------------------------------------------*/
/**
 * parse a 'classical' sky-square (Alpha, Delta, AlphaBand, DeltaBand) into a 
 * "SkyRegion"-string of the form '(a1,d1), (a2,d2),...'
 */
void
SkySquare2String (LALStatus *status,
		  CHAR **string,	/**< OUT: skyRegion string */
		  REAL8 Alpha,		/**< longitude of first point */
		  REAL8 Delta,		/**< latitude of first point */
		  REAL8 AlphaBand,	/**< longitude-interval */
		  REAL8 DeltaBand)	/**< latitude-interval */
{
  REAL8 Da, Dd;
  BOOLEAN onePoint, region2D;
  CHAR *ret, *buf;

  INITSTATUS( status, "SkySquare2String", DOPPLERSCANC );

  /* consistency check either one single point or a real 2D region! */
  onePoint = (AlphaBand == 0) && (DeltaBand == 0);
  region2D = (AlphaBand != 0) && (DeltaBand != 0);

  ASSERT ( onePoint || region2D, status, DOPPLERSCANH_EINPUT, DOPPLERSCANH_MSGEINPUT );

  Da = AlphaBand;
  Dd = DeltaBand;

  /* get enough memory for max 4 points... */
  if ( (buf = LALMalloc (1024)) == NULL ) {
    ABORT (status, DOPPLERSCANH_EMEM, DOPPLERSCANH_MSGEMEM);
  }

  if ( onePoint )  
    sprintf (buf, "(%.16g, %.16g)", Alpha, Delta);
  else                            /* or a 2D rectangle */
    sprintf (buf, 
	     "(%.16g, %.16g), (%.16g, %.16g), (%.16g, %.16g), (%.16g, %.16g)", 
	     Alpha, Delta, 
	     Alpha + Da, Delta, 
	     Alpha + Da, Delta + Dd,
	     Alpha, Delta + Dd );
 
  /* make tight-fitting string */
  if ( (ret = LALMalloc( strlen(buf) + 1 )) == NULL) {
    LALFree (buf);
    ABORT (status, DOPPLERSCANH_EMEM, DOPPLERSCANH_MSGEMEM);
  }

  strcpy ( ret, buf );
  LALFree (buf);

  *string = ret;

  RETURN (status);

} /* SkySquare2String() */

/*----------------------------------------------------------------------*/    
/**
 * determine the 'canonical' stepsizes in all parameter-space directions,
 * either from the metric (if --gridType==GRID_METRIC) or using {dAlpha,dDelta} 
 * and rough guesses dfkdot=1/T^{k+1} otherwise
 * 
 * In the metric case, the metric is evaluated at the given gridpoint.
 * 
 * NOTE: currently only 1 spindown is supported!
 */
void
getGridSpacings( LALStatus *status, 
		 PulsarDopplerParams *spacings,		/**< OUT: grid-spacings in gridpoint */
		 PulsarDopplerParams gridpoint,		/**< IN: gridpoint to get spacings for*/
		 const DopplerSkyScanInit *params)		/**< IN: Doppler-scan parameters */
{
  REAL8Vector *metric = NULL;
  REAL8 g_f0_f0 = 0, gamma_f1_f1 = 0, gamma_a_a, gamma_d_d;
  PtoleMetricIn metricpar = empty_metricpar;

  INITSTATUS( status, "getGridSpacings", DOPPLERSCANC );
  ATTATCHSTATUSPTR (status); 

  ASSERT ( params != NULL, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL );  
  ASSERT ( spacings != NULL, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL );  

  if ( (params->gridType == GRID_METRIC) || (params->gridType == GRID_METRIC_SKYFILE) )	/* use the metric to fix f0/fdot stepsizes */
    {
      /* setup metric parameters */
      metricpar.position.system = COORDINATESYSTEM_EQUATORIAL;
      metricpar.position.longitude = gridpoint.Alpha;
      metricpar.position.latitude = gridpoint.Delta;

      TRY ( LALSCreateVector (status->statusPtr, &(metricpar.spindown), 1), status);
      /* !!NOTE!!: in the metric-codes, the spindown f1 is defined as 
       * f1 = f1dot / Freq, while here f1dot = d Freq/dt 
       */
      metricpar.spindown->data[0] = gridpoint.fkdot[1] / gridpoint.fkdot[0];
      
      metricpar.epoch = params->obsBegin;
      metricpar.duration = params->obsDuration;
      metricpar.maxFreq = gridpoint.fkdot[0];
      metricpar.site = params->Detector;
      metricpar.ephemeris = params->ephemeris;	/* needed for ephemeris-metrics */
      metricpar.metricType = params->metricType;

      TRY ( LALNormalizeSkyPosition(status->statusPtr, &(metricpar.position), &(metricpar.position)), status);

      TRY ( LALPulsarMetric (status->statusPtr, &metric, &metricpar), status);
      TRY ( LALSDestroyVector(status->statusPtr, &(metricpar.spindown)), status);

      g_f0_f0 = metric->data[INDEX_f0_f0];

      /* NOTE: for simplicity we use params->mismatch, instead of mismatch/D 
       * where D is the number of parameter-space dimensions
       * as in the case of spindown this would require adapting the 
       * sky-mismatch too.
       * Instead, the user has to adapt 'mismatch' corresponding to 
       * the number of dimensions D in the parameter-space considered.
       */

      spacings->fkdot[0] = 2.0 * sqrt ( params->metricMismatch / g_f0_f0 );

      if ( params->projectMetric ) {
	TRY ( LALProjectMetric( status->statusPtr, metric, 0 ), status);
      }
      if ( lalDebugLevel >= 3 ) 
	{
	  LALPrintError ("\ngetGridSpacing(): using the %s metric\n", 
			 params->projectMetric ? "projected" : "unprojected");
	  LALPrintError (" %g \n", g_f0_f0);
	  LALPrintError (" %g  %g\n", metric->data[INDEX_f0_A], metric->data[INDEX_A_A]);
	  LALPrintError (" %g  %g  %g\n", 
			 metric->data[INDEX_f0_D], metric->data[INDEX_A_D], metric->data[INDEX_D_D]);
	  LALPrintError (" %g  %g  %g  %g\n\n",
			 metric->data[INDEX_f0_f1], metric->data[INDEX_A_f1], metric->data[INDEX_D_f1],
			 metric->data[INDEX_f1_f1]);
	}

      
      gamma_f1_f1 = metric->data[INDEX_f1_f1];
      spacings->fkdot[1] = 2.0 * gridpoint.fkdot[0] * sqrt( params->metricMismatch / gamma_f1_f1 );

      gamma_a_a = metric->data[INDEX_A_A];
      gamma_d_d = metric->data[INDEX_D_D];

      spacings->Alpha = 2.0 * sqrt ( params->metricMismatch / gamma_a_a );
      spacings->Delta = 2.0 * sqrt ( params->metricMismatch / gamma_d_d );

      TRY( LALDDestroyVector (status->statusPtr, &metric), status);
      metric = NULL;
    }
  else	/* no metric: use 'naive' value of 1/(2*T^k) [previous default in CFS] */
    {
      spacings->Alpha = params->dAlpha;	/* dummy */
      spacings->Delta = params->dDelta;
      spacings->fkdot[0] = 1.0 / (2.0 * params->obsDuration);
      spacings->fkdot[1] = 1.0 / (2.0 * params->obsDuration * params->obsDuration);
    }

  DETATCHSTATUSPTR(status);
  RETURN(status);

} /* getGridSpacings() */

/*----------------------------------------------------------------------*/
/** Determine a (randomized) cubic DopplerRegion around a search-point 
 *  with (roughly) the given number of grid-points in each non-projected 
 *  dimension.
 * 
 *  Motivation: mainly useful for MC tests of the search-grid. 
 *              For this we need to simulate a 'small' search-grid around 
 *              the signal-location.
 *
 *  This function tries to estimate a region in parameter-space 
 *  with roughly the given number of grid-points in each non-projected dimension. 
 *
 *  NOTE: if the frequency has been projected, we need to search
 *       the *whole* possible Doppler-range of frequencies, in which the 
 *       signal can show up. This range is bounded by (from circle-equation)
 *       |FreqBand/Freq| < beta_orb Delta_n, where beta_orb = V_orb/c ~1e-4,
 *       and Delta_n = |\vec{n} - \vec{n}_sig| can be estimated from the
 *       metric sky-ellipses: Delta_n ~ smajor of the sky-ellipse
 * 
 *  The region will be randomized wrt the central point within one
 *  grid-spacing in order to avoid systematic effects in MC simulations.
 *
 *  PointsPerDim == 0: trivial search-region consisting just of the signal-location
 * 			( no randomization! )
 *  PointsPerDim == 1: DopplerRegion is only one point (randomized within one cell)
 */
void
getMCDopplerCube (LALStatus *status, 
		  DopplerRegion *cube, 		/**< OUT: 'cube' around signal-position */
		  PulsarDopplerParams signal, 	/**< signal-position: approximate cube-center */
		  UINT4 PointsPerDim,		/**< desired number of grid-points per dim. */
		  const DopplerSkyScanInit *params)/**< search+metric parameters */
{
  PulsarDopplerParams spacings = empty_PulsarDopplerParams;
  REAL8 Alpha, Delta, Freq, f1dot;
  REAL8 dAlpha, dDelta, dFreq, df1dot;
  REAL8 AlphaBand, DeltaBand, FreqBand, f1dotBand;
  REAL8 numSteps;

  INITSTATUS( status, "getMCDopplerCube", DOPPLERSCANC );
  ATTATCHSTATUSPTR (status); 

  ASSERT ( params != NULL, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL );  
  ASSERT ( cube != NULL, status, DOPPLERSCANH_ENULL, DOPPLERSCANH_MSGENULL );  

  /* get the grid-spacings at the signal-location */
  TRY ( getGridSpacings(status->statusPtr, &spacings, signal, params), status);

  dAlpha = spacings.Alpha;
  dDelta = spacings.Delta;
  dFreq  = spacings.fkdot[0];
  df1dot = spacings.fkdot[1];

  numSteps = PointsPerDim;
  if ( PointsPerDim )
    numSteps -= 1.0e-4;	/* trick: make sure we get the right number of points */

  /* figure out corresponding Bands in each dimension */
  AlphaBand = (dAlpha * numSteps);
  DeltaBand = (dDelta * numSteps);
  f1dotBand = (df1dot * numSteps);

  FreqBand  = (dFreq  * numSteps);	/* 'canonical' value if not projecting */

  /* 
   * if projected sky-metric is used, we need to estimate
   * the maximal Delta_n now, so that we can get a reasonable 
   * bound on the required Frequency-band (the whole Doppler-window
   * just gets too large for longer observation times... 
   */
  if ( (PointsPerDim > 0) && params->projectMetric )
    { /* choose large-enough FreqBand if projecting */
      REAL8 DopplerFreqBand;
      REAL8 fB = FreqBand;
      PtoleMetricIn metricpar = empty_metricpar;/* we need to know metric for this..:( */
      MetricEllipse ellipse;
      REAL8Vector *metric = NULL;

      /* setup metric parameters */
      metricpar.position.system = COORDINATESYSTEM_EQUATORIAL;
      metricpar.position.longitude = signal.Alpha;
      metricpar.position.latitude = signal.Delta;
      TRY ( LALSCreateVector (status->statusPtr, &(metricpar.spindown), 1), status);
      metricpar.spindown->data[0] = signal.fkdot[1] / signal.fkdot[0];
      metricpar.epoch = params->obsBegin;
      metricpar.duration = params->obsDuration;
      metricpar.maxFreq = signal.fkdot[0];
      metricpar.site = params->Detector;
      metricpar.ephemeris = params->ephemeris;
      metricpar.metricType = params->metricType;
      TRY ( LALPulsarMetric(status->statusPtr, &metric, &metricpar), status);
      TRY ( LALSDestroyVector(status->statusPtr, &(metricpar.spindown)), status);
      TRY ( LALProjectMetric( status->statusPtr, metric, 0 ), status);

      TRY ( getMetricEllipse(status->statusPtr, &ellipse, params->metricMismatch, metric, 1), status);
      TRY ( LALDDestroyVector(status->statusPtr, &metric), status);

      /* now we can estimate the Doppler-Band on f: |dFreq| < Freq * 1e-4 * smajor */
      DopplerFreqBand = 2.0 * signal.fkdot[0] * 1.0e-4 * ellipse.smajor;

      LogPrintf (LOG_DEBUG, 
		 "Using projected sky-metric: canonical FreqBand would be %g,"
		 "but Doppler-FreqBand = %g\n", fB, DopplerFreqBand);

      FreqBand = MAX( fB, DopplerFreqBand );	/* pick larger one */
	
    } /* if project metric */

  /* set center-point to signal-location */
  Alpha = signal.Alpha - 0.5 * AlphaBand;
  Delta = signal.Delta - 0.5 * DeltaBand;
  Freq  = signal.fkdot[0] - 0.5 * FreqBand;
  f1dot = signal.fkdot[1] - 0.5 * f1dotBand;

  /* randomize center-point within one grid-cell *
   * (we assume seed has been set elsewhere) */
  if ( PointsPerDim > 0 )
    {
#define randShift() (1.0 * rand()/RAND_MAX)
      Alpha += dAlpha * randShift();
      Delta += dDelta * randShift();
      Freq  += dFreq  * randShift();
      f1dot += df1dot * randShift();
    }

  /* convert sky-square into a skyRegionString */
  TRY ( SkySquare2String (status->statusPtr, &(cube->skyRegionString),  Alpha, Delta, AlphaBand, DeltaBand), status);
  cube->fkdot[0] = Freq;
  cube->fkdotBand[0] = FreqBand;
  cube->fkdot[1] = f1dot;
  cube->fkdotBand[1] = f1dotBand;

  DETATCHSTATUSPTR(status);
  RETURN(status);

} /* getMCDopplerCube() */

/*----------------------------------------------------------------------*/
/** get "metric-ellipse" for given metric.
 * \note This function uses only 2 dimensions starting from dim0 of the given metric!
 */
void
getMetricEllipse(LALStatus *status, 
		 MetricEllipse *ellipse, 
		 REAL8 mismatch, 
		 const REAL8Vector *metric,
		 UINT4 dim0)
{
	
  REAL8 gaa, gad, gdd;
  REAL8 smin, smaj, angle;
  UINT4 dim;
  INITSTATUS( status, "getMetricEllipse", DOPPLERSCANC );

  ASSERT ( metric, status, DOPPLERSCANH_ENULL ,  DOPPLERSCANH_MSGENULL );
  dim = dim0 + 2;
  ASSERT ( metric->length >= dim*(dim+1)/2, status, DOPPLERSCANH_EINPUT, DOPPLERSCANH_MSGEINPUT);

  gaa = metric->data[ PMETRIC_INDEX(dim0 + 0, dim0 + 0) ];
  gad = metric->data[ PMETRIC_INDEX(dim0 + 0, dim0 + 1) ];
  gdd = metric->data[ PMETRIC_INDEX(dim0 + 1, dim0 + 1) ];
	  
  /* Semiminor axis from larger eigenvalue of metric. */
  smin = gaa+gdd + sqrt( pow(gaa-gdd,2) + pow(2*gad,2) );
  smin = sqrt(2.0* mismatch/smin);

  /* Semimajor axis from smaller eigenvalue of metric. */
  smaj = gaa+gdd - sqrt( pow(gaa-gdd,2) + pow(2*gad,2) );
  smaj = sqrt(2.0* mismatch /smaj);
	  
  /* Angle of semimajor axis with "horizontal" (equator). */
  angle = atan2( gad, mismatch /smaj/smaj-gdd );
  if (angle <= -LAL_PI_2) angle += LAL_PI;
  if (angle > LAL_PI_2) angle -= LAL_PI;

  ellipse->smajor = smaj;
  ellipse->sminor = smin;
  ellipse->angle = angle;

  RETURN(status);

} /* getMetricEllipse() */

