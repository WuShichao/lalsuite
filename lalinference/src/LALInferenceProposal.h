/*
 *  LALInferenceProposal.h:  Bayesian Followup, jump proposals.
 *
 *  Copyright (C) 2011 Ilya Mandel, Vivien Raymond, Christian Roever,
 *  Marc van der Sluys, John Veitch, Will M. Farr
 *
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
#ifndef LALInferenceProposal_h
#define LALInferenceProposal_h

#include <lal/LALInference.h>
#include <lal/LALInferenceClusteredKDE.h>


/**
 * \defgroup LALInferenceProposal_h Header LALInferenceProposal.h
 * \ingroup lalinference_general
 *
 * \author Ilya Mandel, Vivien Raymond, Christian Roever, Marc van der Sluys, John Veitch, and Will M. Farr.
 * \date 2011
 *
 * \brief Jump proposals for exploring the GW signal parameter space.
 *
 * For exploring the parameter space of GW signals, it is convenient
 * to use many different types of jumps.  For example, we supply in
 * this module proposals that change one parameter at a time, based on
 * a gaussian distribution centered at the current value with a fixed
 * width; proposals that change a combination of parameters along a
 * one-dimensional curve in parameter space; proposals that change a
 * combination of parameters along a sub-manifold of parameter space;
 * and proposals that change all parameters at once, attempting to
 * generate a jump along the directions in parameter space
 * corresponding to eigenvectors of the covariance matrix; etc.
 *
 * Good jump proposals involve a combination of moves, or
 * sub-proposals, chosen with various weights.  Having such a combined
 * jump proposal can cause difficulties with standard Metropolis
 * sampling in an MCMC, depending on how the combined proposal is
 * implemented.  The reason involves the ratio of forward to backward
 * jump probabilities that is used in the acceptance probability for a
 * jump in Metropolis sampling.  Many proposals are symmetric, having
 * equal forward and backward jump probabilities, but not all.
 *
 * If the combined proposal is implemented as a random choice among a
 * list of sub-proposals, then the proper jump probability is a
 * combination of the jump probabilities from all sub-propsals.  That
 * is, to properly compute the jump probability for such a combined
 * proposal, every sub-proposal---whether it individually is symmetric
 * or not---must be queried after each jump for its individual jump
 * probability, and these must be combined into the correct jump
 * probability for the overall proposal.  The situation becomes even
 * more complex when the sub-proposals act on sub-manifolds of
 * parameter space of differing dimensions, because then the
 * individual proposal jump probabilities involve varying numbers of
 * delta-functions that act to restrict the support of the jump
 * probability to the proper sub-manifold.  Ugh.
 *
 * We avoid this problem by implementing our combined proposal as a
 * cyclic sequence of sub-proposals.  At each MCMC step, only one of
 * the sub-proposals is used, and therefore only this proposal's jump
 * probability is relevant to the computation of the forward and
 * backward jump probabilities.  At the next step, the next proposal
 * in the cycle is called, etc.  The LALInferenceCyclicProposal()
 * function implements this procedure.  To add a number of copies of a
 * proposal function to the end of the cycle, use
 * LALInferenceAddProposalToCycle(); once all desired sub-proposals
 * are added to the cycle, call LALInferenceRandomizeProposalCycle()
 * in order to ensure that the ordering of the sub-proposals is
 * random.
 */
/*@{*/

#define MAX_STRLEN 512
#define ADAPTSUFFIX "adapt_sigma"
#define ACCEPTSUFFIX "accepted"
#define PROPOSEDSUFFIX "proposed"

extern const char *const cycleArrayName;
extern const char *const cycleArrayLengthName;
extern const char *const cycleArrayCounterName;


/* Proposal Names */
extern const char *const nullProposalName;
extern const char *const singleAdaptProposalName;
extern const char *const singleProposalName;
extern const char *const orbitalPhaseJumpName;
extern const char *const covarianceEigenvectorJumpName;
extern const char *const skyLocWanderJumpName;
extern const char *const differentialEvolutionFullName;
extern const char *const differentialEvolutionIntrinsicName;
extern const char *const differentialEvolutionExtrinsicName;
extern const char *const ensembleStretchFullName;
extern const char *const ensembleStretchIntrinsicName;
extern const char *const ensembleStretchExtrinsicName;
extern const char *const drawApproxPriorName;
extern const char *const drawFlatPriorName;
extern const char *const skyReflectDetPlaneName;
extern const char *const skyRingProposalName;
extern const char *const PSDFitJumpName;
extern const char *const polarizationPhaseJumpName;
extern const char *const polarizationCorrPhaseJumpName;
extern const char *const extrinsicParamProposalName;
extern const char *const frequencyBinJumpName;
extern const char *const GlitchMorletJumpName;
extern const char *const GlitchMorletReverseJumpName;
extern const char *const ensembleWalkFullName;
extern const char *const ensembleWalkIntrinsicName;
extern const char *const ensembleWalkExtrinsicName;
extern const char *const clusteredKDEProposalName;

/** Creates a new proposal object from the given func pointer and name
 */

LALInferenceProposal *LALInferenceInitProposal(LALInferenceProposalFunction func, const char *name);


/** Use a default flag and a check of the command-line to set proposal flags in proposal args */
void LALInferenceRegisterProposal(LALInferenceVariables *propArgs, const char *name, INT4 *flag, ProcessParamsTable *command_line);

/**
 * Adds \a weight copies of the proposal \a prop to the end of the
 * proposal cycle.
 *
 * After adding all desired sub-proposals, call
 * LALInferenceRandomizeProposalCycle() to randomize the order of
 * sub-proposal application.
 */
void LALInferenceAddProposalToCycle(LALInferenceProposalCycle *cycle, LALInferenceProposal *prop, INT4 weight);

/** Randomizes the order of the proposals in the proposal cycle. */
void LALInferenceRandomizeProposalCycle(LALInferenceProposalCycle *cycle, gsl_rng *rng);

/** Create a new proposal cycle */
LALInferenceProposalCycle* LALInferenceInitProposalCycle(void);

/** Go through all logic for deciding proposals to use */
LALInferenceVariables *LALInferenceParseProposalArgs(LALInferenceRunState *runState);

/** Proposes a jump from the next proposal in the proposal cycle.*/
REAL8 LALInferenceCyclicProposal(LALInferenceThreadState *thread,
                                 LALInferenceVariables *currentParams,
                                 LALInferenceVariables *proposedParams);

/** Completely remove the current proposal cycle, freeing the associated memory. */
void LALInferenceDeleteProposalCycle(LALInferenceProposalCycle *cycle);

/**
 * A reasonable default proposal.  Uses adaptation if the --adapt
 * command-line flag active.
 */
LALInferenceProposalCycle* LALInferenceSetupDefaultInspiralProposalCycle(LALInferenceVariables *propArgs);

/**
 * Non-adaptive, sigle-variable update proposal with reasonable
 * widths in each dimension.
 */
REAL8 LALInferenceSingleProposal(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);

/**
 * Like LALInferenceSingleProposal() but will use adaptation if the
 * --adapt command-line flag given.
 */
REAL8 LALInferenceSingleAdaptProposal(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);

/** Polarization-phase exact degeneracy. */
REAL8 LALInferencePolarizationPhaseJump(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);

/** Polarization-phase correlation jump */
REAL8 LALInferenceCorrPolarizationPhaseJump(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);

/** Choose a random covariance matrix eigenvector to jump along. */
REAL8 LALInferenceCovarianceEigenvectorJump(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);

/** Jump around by 0.01 radians in angle on the sky */
REAL8 LALInferenceSkyLocWanderJump(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);

/** Differential evolution, on all non-fixed, non-output parameters. */
REAL8 LALInferenceDifferentialEvolutionFull(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);

/**
 * Perform differential evolution on the parameters of the given
 * names (the names array should be terminated by a NULL pointer).
 * If names == NULL, then perform a
 * LALInferenceDifferentialEvolutionFull() step.
 */
REAL8 LALInferenceDifferentialEvolutionNames(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams, const char *names[]);

/** Perform differential evolution on only the intrinsic parameters. */
REAL8 LALInferenceDifferentialEvolutionIntrinsic(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);

/**
 * Perform a differential evolution step on only the extrinsic
 * parameters.
 */
REAL8 LALInferenceDifferentialEvolutionExtrinsic(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);

/**
 * Draws from an approximation to the true prior.  Flat in all
 * variables except for: Mc^(-11/6), flat in cos(co-latitudes), flat
 * in sin(dec), dist^2.
 * WARNING: This seems to break detailed balance for the LALInferenceProposalTest
 */
REAL8 LALInferenceDrawApproxPrior(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);


/**
 * Draws from a flat prior for all variables where are flat prior is specified. Variables
 * that do not have a flat prior are not moved.
 */
REAL8 LALInferenceDrawFlatPrior(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);

/**
 * Reflects the sky location through the plane formed by three
 * detectors.  Should only be used when there are exactly three
 * different locations for detectors.
 */
REAL8 LALInferenceSkyReflectDetPlane(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);

REAL8 LALInferenceSkyRingProposal(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);;

/* Noise model proposals. */
REAL8 LALInferenceGlitchMorletProposal(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);
REAL8 LALInferenceGlitchMorletReverseJump(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);
REAL8 LALInferencePSDFitJump(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);

/**
 * Proposal for the extrinsic parameters. Uses the sky reflection for 3
 * independent detector locations, then computes the corresponding values
 * of polarisation, inclination and distance for the proposed sky location.
 * See Vivien's thesis for the details of the equations implemented.
 */
REAL8 LALInferenceExtrinsicParamProposal(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);

/**
 * Proposal to jump in frequency by one frequency bin. The frequency bin size \c df must be
 * given as a (fixed) variable in the \c proposedParams. The frequency parameter is
 * presumed to have the variable name \c f0.
 */
REAL8 LALInferenceFrequencyBinJump(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);

/** Proposal which draws a sample from the distance likelihood function
 * Requires the currentParams to have optimal_snr and matched_filter_snr.
 */
REAL8 LALInferenceDistanceLikelihoodProposal(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);

/* Zero out proposal statistics */
void LALInferenceZeroProposalStats(LALInferenceProposalCycle *cycle);

/** Function for updating proposal acceptance rates if tracking. */
void LALInferenceTrackProposalAcceptance(LALInferenceThreadState *thread);

/** Helper function to update the adaptive steps after each jump. Set accepted=1 if accepted, 0 otherwise */
void LALInferenceUpdateAdaptiveJumps(LALInferenceThreadState *thread, REAL8 targetAcceptance);

/** Struct to hold clustered-KDE estimates */
typedef struct
tagLALInferenceClusteredKDEProposal
{
    char name[VARNAME_MAX];
    LALInferenceKmeans *kmeans;
    REAL8 weight;
    INT4 dimension;
    LALInferenceVariables *params;
    struct tagLALInferenceClusteredKDEProposal *next;
} LALInferenceClusteredKDE;

/* Setup all clustered-KDE proposals with samples read from file. */
//void LALInferenceSetupClusteredKDEProposalsFromFile(LALInferenceThreadState *thread);

/* Setup all clustered-KDE proposals with samples read from file. */
void LALInferenceSetupClusteredKDEProposalsFromASCII(LALInferenceThreadState *thread, FILE *input, INT4 burnin, REAL8 weight, INT4 ptmcmc);

/* Add a KDE proposal to the KDE proposal set. */
void LALInferenceAddClusteredKDEProposalToSet(LALInferenceVariables *propArgs, LALInferenceClusteredKDE *kde);

/* Destroy an existing clustered-KDE proposal. */
void LALInferenceDestroyClusteredKDEProposal(LALInferenceClusteredKDE *proposal);

/* Setup a clustered-KDE proposal from the differential evolution buffer. */
void LALInferenceSetupClusteredKDEProposalFromDEBuffer(LALInferenceThreadState *thread);

/* Setup a clustered-KDE proposal from the parameters in a run. */
void LALInferenceSetupClusteredKDEProposalFromRun(LALInferenceThreadState *thread, REAL8 *samples, INT4 size, INT4 cyclic_reflective, INT4 ntrials);

/* A proposal based on the clustered kernal density estimate of a set of samples. */
REAL8 LALInferenceClusteredKDEProposal(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams);
REAL8 LALInferenceStoredClusteredKDEProposal(LALInferenceThreadState *thread, LALInferenceVariables *currentParams, LALInferenceVariables *proposedParams, REAL8 *propDensity);

/* Initialize a clustered-KDE proposal. */
void LALInferenceInitClusteredKDEProposal(LALInferenceThreadState *thread, LALInferenceClusteredKDE *kde, REAL8 *array, INT4 nSamps, LALInferenceVariables *params, const char *name, REAL8 weight, LALInferenceKmeans* (*cluster_method)(gsl_matrix*, INT4, gsl_rng*), INT4 cyclic_reflective, INT4 ntrials);

/* Dump clustered KDE information to file. */
void LALInferenceDumpClusteredKDE(LALInferenceClusteredKDE *kde, char *outp_name, REAL8 *array);

/* Dump draws from a KDE to file. */
void LALInferenceDumpClusteredKDEDraws(LALInferenceClusteredKDE *kde, char *outp_name, INT4 nSamps);

/* Compute the maximum ACL from the differential evolution buffer. */
void LALInferenceComputeMaxAutoCorrLenFromDE(LALInferenceThreadState *thread, INT4* maxACL);

/* Compute the maximum single-parameter autocorrelation length. */
REAL8 LALInferenceComputeMaxAutoCorrLen(REAL8 *array, INT4 nPoints, INT4 nPar);

/* Update the estimatate of the autocorrelation length. */
void LALInferenceUpdateMaxAutoCorrLen(LALInferenceThreadState *thread);

/* Determine the effective sample size based on the DE buffer. */
INT4 LALInferenceComputeEffectiveSampleSize(LALInferenceThreadState *thread);

/** Helper function to setup the adaptive step proposals before the run */
void LALInferenceSetupAdaptiveProposals(LALInferenceVariables *propArgs, LALInferenceVariables *params);

/** Setup glitch-related stuff */
void LALInferenceSetupGlitchProposal(LALInferenceIFOData *data, LALInferenceVariables *propArgs);

/** Output proposal tracking header to file *fp */
int LALInferencePrintProposalTrackingHeader(FILE *fp,LALInferenceVariables *params);

/** Output proposal tracking information to file *fp */
void LALInferencePrintProposalTracking(FILE *fp, LALInferenceProposalCycle *cycle, LALInferenceVariables *theta, LALInferenceVariables *theta_prime, REAL8 logPropRatio, INT4 accepted);

/** Ensemble stretch moves - see http://dx.doi.org/10.2140/camcos.2010.5.65 */
REAL8 LALInferenceEnsembleStretchFull(LALInferenceThreadState *thread, LALInferenceVariables *cp, LALInferenceVariables *proposedParams);
REAL8 LALInferenceEnsembleStretchIntrinsic(LALInferenceThreadState *thread, LALInferenceVariables *cp, LALInferenceVariables *pp);
REAL8 LALInferenceEnsembleStretchExtrinsic(LALInferenceThreadState *thread, LALInferenceVariables *cp, LALInferenceVariables *pp);
REAL8 LALInferenceEnsembleStretchNames(LALInferenceThreadState *thread, LALInferenceVariables *cpi, LALInferenceVariables *ppi, const char **names);


REAL8 LALInferenceEnsembleWalkFull(LALInferenceThreadState *thread, LALInferenceVariables *cp, LALInferenceVariables *proposedParams);
REAL8 LALInferenceEnsembleWalkIntrinsic(LALInferenceThreadState *thread, LALInferenceVariables *cp, LALInferenceVariables *pp);
REAL8 LALInferenceEnsembleWalkExtrinsic(LALInferenceThreadState *thread, LALInferenceVariables *cp, LALInferenceVariables *pp);
REAL8 LALInferenceEnsembleWalkNames(LALInferenceThreadState *thread, LALInferenceVariables *cpi, LALInferenceVariables *ppi, const char **names);

/** Proposes jumps in the spline calibration parameters, if present. */
REAL8 LALInferenceSplineCalibrationProposal(LALInferenceThreadState *thread, LALInferenceVariables *cp, LALInferenceVariables *pp);
/*@}*/

#endif
