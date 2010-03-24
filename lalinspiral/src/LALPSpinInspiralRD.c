/*
*  Copyright (C) 2009 Riccardo Sturani, based on LALEOBWaveform.c by
*  Stas Babak, David Churches, Duncan Brown, David Chin, Jolien Creighton,
*  B.S. Sathyaprakash, Craig Robinson , Thomas Cokelaer, Evan Ochsner
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

/****  <lalVerbatim file="LALPSpinInspiralRDCV">
 * $Id$
 **** </lalVerbatim> */

/****  <lalLaTeX>
 *
 * \subsection{Module \texttt{LALPSpinInspiralRD.c},
 * \texttt{LALPSpinInspiralTemplates} and \texttt{LALPSpinInspiralForInjection}} 
 * \label{ss:LALPSpinInspiralRD.c}
 *
 * Module to generate generic spinning binaries waveforms complete with ring-down 
 * 
 * \subsubsection*{Prototypes}
 * \vspace{0.1in}
 * \input{LALPSpinInspiralRDCP}
 * \idx{\verb&LALPSpinInspiralRD()&}
 * \begin{itemize}
 * \item {\tt signalvec:} Output containing the inspiral waveform.
 * \item {\tt params:} Input containing binary chirp parameters.
 * \end{itemize}
 *
 * \input{LALPSpinInspiralRDTemplatesCP}
 * \idx{\verb&LALPSpinInspiralRDTemplates()&}
 * \begin{itemize}
 * \item {\tt signalvec1:} Output containing the $+$ inspiral waveform.
 * \item {\tt signalvec2:} Output containing the $\times$ inspiral waveform.
 * \item {\tt params:} Input containing binary chirp parameters.
 * \end{itemize}
 *
 * \input{LALPSpinInspiralRDInjectionCP}
 * \idx{\verb&LALPSpinInspiralRDInjection()&}
 * \begin{itemize}
 * \item {\tt signalvec:} Output containing the inspiral waveform.
 * \item {\tt params:} Input containing binary chirp parameters.
 * \end{itemize}
 *
 * \subsubsection*{Description}
 * This codes provide complete waveforms for generically spinning binary systems.
 * In order to construct the waveforms three phases are joined together: 
 * an initial inspiral phase, a phenomenological phase encompassing the description
 * of the merger and the ring-down of the final black hole.
 * During the inspiral phase the system is evolved according to the standard
 * PN formulas, valid up to 3.5PN for the orbital motion, 
 * to 2.5PN level for spin-orbital momentum and to 2PN for spin-spin contributions
 * to the orbital phase.
 * Then a phenomenological phase is added during which the frequency of the 
 * waveform has a pole-like behaviour. The stitching is performed in order to 
 * ensure continuity of the phase, the frequency and its first and second 
 * derivatives. Finally a ring-down phase is attached.
 * 
 * \subsubsection*{Algorithm}
 *
 * \subsubsection*{Uses}
 * \begin{verbatim}
 * LALPSpinInspiralRDderivatives
 * LALInspiralSetup
 * LALInspiralChooseModel
 * LALRungeKutta4
 * \end{verbatim}
 *
 * \subsubsection*{Notes}
 * 
 * \vfill{\footnotesize\input{LALPSpinInspiralRDCV}}
 * 
 **** </lalLaTeX>  */


#include <lal/Units.h>
#include <lal/LALInspiral.h>
#include <lal/SeqFactories.h>
#include <lal/NRWaveInject.h>

NRCSID (LALPSPININSPIRALRDMC, "$Id$");


/*
 *
 * private structure with PN parameters
 *
 */

typedef struct LALPSpinInspiralRDstructparams {
  REAL8 eta;                  ///< symmetric mass ratio
  REAL8 dm;
  REAL8 m1m2;                 ///< \f$m_1/m_2\f$
  REAL8 m2m1;                 ///< \f$m_2/m_1\f$
  REAL8 m2m;
  REAL8 m1m;
  REAL8 wdotorb[8];           ///< \f$ \f$
  REAL8 wdotorblog;
  REAL8 wdotspin15S1LNh;
  REAL8 wdotspin15S2LNh;
  REAL8 wdotspin20S1S2;
  REAL8 wdotspin20S1S1;      ///< Coeff. of the \f$s_1s_1\f$ cntrb. to \f$\dot\omega\f$
  REAL8 wdotspin20S1S2LNh;
  REAL8 wdotspin25S1LNh;
  REAL8 wdotspin25S2LNh;     ///< Coeff. of the \f$s_2\cdot \hat L_N\f$ cntrb. to \f$\dot\omega\f$
  REAL8 S1dot15;
  REAL8 S2dot15;
  REAL8 Sdot20;
  REAL8 S1dot25;
  REAL8 S2dot25;
  REAL8 LNhdot15;
  REAL8 LNhdot20;
  REAL8 epnorb[4];
  REAL8 epnspin15S1dotLNh;
  REAL8 epnspin15S2dotLNh;
  REAL8 epnspin20S1S2;
  REAL8 epnspin20S1S2dotLNh;
  REAL8 epnspin20S1S1;
  REAL8 epnspin20S1S1dotLNh;
  REAL8 epnspin20S2S2;
  REAL8 epnspin20S2S2dotLNh;
  REAL8 epnspin25S1dotLNh;
  REAL8 epnspin25S2dotLNh;

} LALPSpinInspiralRDparams;

/* 
 *
 * function to set derivatives: values and mparams input, dvalues output
 *
 */

void LALPSpinInspiralRDderivatives(REAL8Vector *values, REAL8Vector *dvalues, void *mparams ) {

  REAL8 Phi;                    ///< half of the main GW phase, this is \f$Phi\f$ of eq.3.11 of arXiv:0810.5336
  REAL8 omega;                  ///< time-derivative of the orbital phase
  REAL8 omega2;                 ///< omega squared
  REAL8 LNhx,LNhy,LNhz;         ///< orbital angolar momentum unit vector
  REAL8 S1x,S1y,S1z;            ///< dimension-less spin variable S/M^2
  REAL8 S2x,S2y,S2z;
  REAL8 alphadotcosi;           ///< alpha is the right ascension of L, i(iota) the angle between L and J 
  REAL8 LNhS1,LNhS2;            ///< scalar products
  REAL8 domega;                 ///< derivative of omega
  REAL8 dLNhx,dLNhy,dLNhz;      ///< derivatives of \f$\hat L_N\f$ components
  REAL8 dS1x,dS1y,dS1z;         ///< derivative of \f$S_i\f$
  REAL8 dS2x,dS2y,dS2z;
  REAL8 S1S2,S1S1,S2S2;         ///< Scalar products
  REAL8 energy;
  
  LALPSpinInspiralRDparams *params = (LALPSpinInspiralRDparams*)mparams;

  REAL8 v, v2, v3, v4, v5, v6, v7, v11;                                        ///< support variables
  REAL8 tmpx,tmpy,tmpz,cross1x,cross1y,cross1z,cross2x,cross2y,cross2z,LNhxy;  ///< support variables

  /* --- computation start here --- */
  Phi   = values->data[0];
  omega = values->data[1];

  if (omega < 0.0){
    fprintf(stderr, "WARNING: Omega has become -ve, this should lead to nan's \n");
  }

  LNhx  = values->data[2];
  LNhy  = values->data[3];
  LNhz  = values->data[4];
  
  S1x   = values->data[5];
  S1y   = values->data[6];
  S1z   = values->data[7];
  
  S2x   = values->data[8];
  S2y   = values->data[9];
  S2z   = values->data[10];

  v = pow(omega, 1.0/3.0);
  v2 = v * v;
  v3 = v2 * v;
  v4 = v2 * v2;
  v5 = v3 * v2;
  v6 = v4 * v2;
  v7 = v5 * v2;
  v11= v7 * v4;

  // Omega derivative without spin effects up to 3.5 PN
  // this formula does not include the 1.5PN shift mentioned in arXiv:0810.5336, five lines below (3.11)
  domega =
    params->wdotorb[0]
    + v * (params->wdotorb[1]
	   + v * ( params->wdotorb[2]
		   + v * ( params->wdotorb[3]
			   + v * ( params->wdotorb[4]
				   + v * ( params->wdotorb[5]
					   + v * ( params->wdotorb[6] +  params->wdotorblog *  log(omega)
						   + v * params->wdotorb[7] ) ) ) ) ) );

  // energy=-eta/2 * v^2 * [1-(9+\eta)/12 v^2 +...] up to 3PN without spin effects
  energy = ( 1. + v2 * ( params->epnorb[1] +
			 v2 * ( params->epnorb[2] + 
				v2 * params->epnorb[3] ) ) );

  // Adding spin effects to omega and energy
  // L dot S1,2
  LNhS1 = (LNhx*S1x + LNhy*S1y + LNhz*S1z);
  LNhS2 = (LNhx*S2x + LNhy*S2y + LNhz*S2z);

  // wdotspin15SiLNh = -1/12 (...)
  domega+= v3 * ( params->wdotspin15S1LNh * LNhS1 + params->wdotspin15S2LNh * LNhS2 );      // see e.g. Buonanno et al. gr-qc/0211087
  energy+= v3 * ( params->epnspin15S1dotLNh * LNhS1 + params->epnspin15S2dotLNh * LNhS2 );  // see e.g. Blanchet et al. gr-qc/0605140

  // wdotspin20S1S1 = -1/48 eta
  S1S1= (S1x*S1x + S1y*S1y + S1z*S1z);
  S2S2= (S2x*S2x + S2y*S2y + S2z*S2z);
  S1S2 = (S1x*S2x + S1y*S2y + S1z*S2z);
  domega+= params->wdotspin20S1S2 * v4 * ( 247.0 * S1S2 - 721.0 * LNhS1 * LNhS2 );                // see e.g. Buonanno et al. arXiv:0810.5336
  domega+= params->wdotspin20S1S1 * v4 * (719.*( LNhS1*LNhS1 + LNhS2*LNhS2 ) - 233.*(S1S1+S2S2)); // see Racine et al. arXiv:0812.4413

  energy+= v4 * ( params->epnspin20S1S2*S1S2 + params->epnspin20S1S2dotLNh * LNhS1 * LNhS2);      // see e.g. Buonanno et al. as above
  energy+= v4 * ( params->epnspin20S1S1*S1S1 + params->epnspin20S2S2*S2S2 + 
  	  params->epnspin20S1S1dotLNh * LNhS1*LNhS1 + params->epnspin20S2S2 * LNhS2*LNhS2 );      // see Racine et al. as above
  
  // wdotspin25SiLNh = see below
  domega+= v5 * ( params->wdotspin25S1LNh * LNhS1 + params->wdotspin25S2LNh * LNhS2);       //see (8.3) of Blanchet et al.  
  energy+= v5 * ( params->epnspin25S1dotLNh * LNhS1 + params->epnspin25S2dotLNh * LNhS2 );  //see (7.9) of Blanchet et al.

  // Setting the right pre-factor 
  omega2 = omega * omega;
  domega *= 96./5. * params->eta * v5 * omega2;

  energy *= params->epnorb[0] * v2;

  /*Derivative of the angular momentum and spins*/

  /* dS1, 1.5PN*/
  /* S1dot15= (4+3m2/m1)/2 * eta*/

  cross1x = (LNhy*S1z - LNhz*S1y);
  cross1y = (LNhz*S1x - LNhx*S1z);
  cross1z = (LNhx*S1y - LNhy*S1x);

  cross2x = ( LNhy*S2z - LNhz*S2y );
  cross2y = ( LNhz*S2x - LNhx*S2z );
  cross2z = ( LNhx*S2y - LNhy*S2x ); 
  
  dS1x = params->S1dot15 * v5 * cross1x;
  dS1y = params->S1dot15 * v5 * cross1y;
  dS1z = params->S1dot15 * v5 * cross1z;
  
  /* dS1, 2PN*/
  /* Sdot20= 0.5*/
  tmpx = S1z*S2y - S1y*S2z;
  tmpy = S1x*S2z - S1z*S2x;
  tmpz = S1y*S2x - S1x*S2y;

  // S1S2 contribution
  dS1x += params->Sdot20 * omega2 * (tmpx - 3. * LNhS2 * cross1x);
  dS1y += params->Sdot20 * omega2 * (tmpy - 3. * LNhS2 * cross1y);
  dS1z += params->Sdot20 * omega2 * (tmpz - 3. * LNhS2 * cross1z);
  // S1S1 contribution
  dS1x -= 3. * params->Sdot20 * omega2 * LNhS1 * cross1x * (1. + params->m2m1) * params->m2m;
  dS1y -= 3. * params->Sdot20 * omega2 * LNhS1 * cross1y * (1. + params->m2m1) * params->m2m;
  dS1z -= 3. * params->Sdot20 * omega2 * LNhS1 * cross1z * (1. + params->m2m1) * params->m2m;

  // dS1, 2.5PN, eq. 7.8 of Blanchet et al. gr-qc/0605140
  // S1dot25= 9/8-eta/2.+eta+mparams->eta*29./24.+mparams->m1m2*(-9./8.+5./4.*mparams->eta)
  dS1x += params->S1dot25 * v7 * cross1x;
  dS1y += params->S1dot25 * v7 * cross1y;
  dS1z += params->S1dot25 * v7 * cross1z;
  
  /* dS2, 1.5PN*/
  /*Attenzione all'errore qui!*/
  dS2x = params->S2dot15 * v5 * cross2x;
  dS2y = params->S2dot15 * v5 * cross2y;
  dS2z = params->S2dot15 * v5 * cross2z;

  /* dS2, 2PN*/
  dS2x += params->Sdot20 * omega2 * (-tmpx - 3.0 * LNhS1 * cross2x);
  dS2y += params->Sdot20 * omega2 * (-tmpy - 3.0 * LNhS1 * cross2y);
  dS2z += params->Sdot20 * omega2 * (-tmpz - 3.0 * LNhS1 * cross2z);
  // S2S2 contribution
  dS2x -= 3.0 * params->Sdot20 * omega2 * ( LNhS2 * cross2x * params->m1m2 + 0.*LNhS1 * cross1x *params->m2m1/(params->m1m2+1.));
  dS2y -= 3.0 * params->Sdot20 * omega2 * ( LNhS2 * cross2y * params->m1m2 + 0.*LNhS1 * cross1y *params->m2m1/(params->m1m2+1.));
  dS2z -= 3.0 * params->Sdot20 * omega2 * ( LNhS2 * cross2z * params->m1m2 + 0.*LNhS1 * cross1z *params->m2m1/(params->m1m2+1.));

  // dS2, 2.5PN, eq. 7.8 of Blanchet et al. gr-qc/0605140
  dS2x += params->S2dot25 * v7 * cross2x;
  dS2y += params->S2dot25 * v7 * cross2y;
  dS2z += params->S2dot25 * v7 * cross2z;

  dLNhx = -( dS1x + dS2x ) * v / params->eta;
  dLNhy = -( dS1y + dS2y ) * v / params->eta;
  dLNhz = -( dS1z + dS2z ) * v / params->eta;

  /* dphi*/
  LNhxy=sqrt( LNhx*LNhx + LNhy+LNhy );

  if (LNhxy > 0.0) alphadotcosi = LNhz * (LNhx*dLNhy - LNhy*dLNhx) / (LNhx*LNhx + LNhy*LNhy);
  else alphadotcosi = 0.;

  /* dvalues->data[0] is the phase derivative*/
  /* omega is the derivative of the orbital phase omega \neq dvalues->data[0]*/

  dvalues->data[0] = omega - alphadotcosi;
  dvalues->data[1] = domega;

  dvalues->data[2] = dLNhx;
  dvalues->data[3] = dLNhy;
  dvalues->data[4] = dLNhz;

  dvalues->data[5] = dS1x;
  dvalues->data[6] = dS1y;
  dvalues->data[7] = dS1z;
  
  dvalues->data[8] = dS2x;
  dvalues->data[9] = dS2y;
  dvalues->data[10]= dS2z;

  dvalues->data[11]= 0.;

  // energy value is stored
  values->data[11] = energy;
  
} /* end of LALPSpinInspiralRDderivatives*/


/*  <lalVerbatim file="LALPSpinInspiralRDCP"> */
void
LALPSpinInspiralRD (
   LALStatus        *status,
   REAL4Vector      *signalvec,
   InspiralTemplate *params
   )
{ /* </lalVerbatim> */

   UINT4 count;
   InspiralInit paramsInit;
   INITSTATUS(status, "LALPSpinInspiralRD", LALPSPININSPIRALRDMC);
   ATTATCHSTATUSPTR(status);

   ASSERT(signalvec,  status,
	LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL);
   ASSERT(signalvec->data,  status,
   	LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL);
   ASSERT(params,  status,
   	LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL);
   ASSERT(params->nStartPad >= 0, status,
   	LALINSPIRALH_ESIZE, LALINSPIRALH_MSGESIZE);
   ASSERT(params->nEndPad >= 0, status,
   	LALINSPIRALH_ESIZE, LALINSPIRALH_MSGESIZE);
   ASSERT(params->fLower > 0, status,
   	LALINSPIRALH_ESIZE, LALINSPIRALH_MSGESIZE);
   ASSERT(params->tSampling > 0, status,
   	LALINSPIRALH_ESIZE, LALINSPIRALH_MSGESIZE);
   ASSERT(params->totalMass > 0., status,
   	LALINSPIRALH_ESIZE, LALINSPIRALH_MSGESIZE);

   LALInspiralSetup (status->statusPtr, &(paramsInit.ak), params);
   CHECKSTATUSPTR(status);
   LALInspiralChooseModel(status->statusPtr, &(paramsInit.func), &(paramsInit.ak), params);
   CHECKSTATUSPTR(status);

   memset(signalvec->data, 0, signalvec->length * sizeof( REAL4 ));
   /* Call the engine function */
   LALPSpinInspiralRDEngine(status->statusPtr, signalvec, NULL,NULL, NULL, NULL, NULL, &count, params, &paramsInit);
   CHECKSTATUSPTR( status );

   DETATCHSTATUSPTR(status);
   RETURN(status);
}


NRCSID (LALPSPININSPIRALRDTEMPLATESMC,"$Id$");

/*  <lalVerbatim file="LALPSpinInspiralRDTemplatesCP"> */
void
LALPSpinInspiralRDTemplates (
   LALStatus        *status,
   REAL4Vector      *signalvec1,
   REAL4Vector      *signalvec2,
   InspiralTemplate *params
   )
{ /* </lalVerbatim> */

   UINT4 count;

   InspiralInit paramsInit;

   INITSTATUS(status, "LALPSpinInspiralRDTemplates", LALPSPININSPIRALRDTEMPLATESMC);
   ATTATCHSTATUSPTR(status);

   ASSERT(signalvec1,  status,
	LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL);
   ASSERT(signalvec1->data,  status,
   	LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL);
   ASSERT(signalvec2,  status,
	LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL);
   ASSERT(signalvec2->data,  status,
   	LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL);
   ASSERT(params,  status,
   	LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL);
   ASSERT(params->nStartPad >= 0, status,
   	LALINSPIRALH_ESIZE, LALINSPIRALH_MSGESIZE);
   ASSERT(params->nEndPad >= 0, status,
   	LALINSPIRALH_ESIZE, LALINSPIRALH_MSGESIZE);
   ASSERT(params->fLower > 0, status,
   	LALINSPIRALH_ESIZE, LALINSPIRALH_MSGESIZE);
   ASSERT(params->tSampling > 0, status,
   	LALINSPIRALH_ESIZE, LALINSPIRALH_MSGESIZE);
   ASSERT(params->totalMass > 0., status,
   	LALINSPIRALH_ESIZE, LALINSPIRALH_MSGESIZE);

   LALInspiralSetup (status->statusPtr, &(paramsInit.ak), params);
   CHECKSTATUSPTR(status);
   LALInspiralChooseModel(status->statusPtr, &(paramsInit.func), &(paramsInit.ak), params);
   CHECKSTATUSPTR(status);
 
   LALInspiralInit(status->statusPtr, params, &paramsInit);

   memset(signalvec1->data, 0, signalvec1->length * sizeof( REAL4 ));
   memset(signalvec2->data, 0, signalvec2->length * sizeof( REAL4 ));

   LALPSpinInspiralRDEngine(status->statusPtr, signalvec1, signalvec2, NULL, NULL, NULL, NULL, &count, params, &paramsInit);

   RETURN(status);
}


NRCSID (LALPSPININSPIRALRDINJECTIONC,"$Id$");

/*  <lalVerbatim file="LALPSpinInspiralRDInjectionCP"> */
void
LALPSpinInspiralRDForInjection (
			     LALStatus        *status,
			     CoherentGW       *waveform,
			     InspiralTemplate *params,
			     PPNParamStruc    *ppnParams
			    )
{
  /* </lalVerbatim> */
  UINT4 count,i;

  REAL4Vector *hh=NULL;/* pointers to generated amplitude  data */
  REAL4Vector *ff=NULL ;/* pointers to generated  frequency data */
  REAL8Vector *phi=NULL;/* pointer to generated phase data */
  REAL4Vector *alpha=NULL;/* pointer to generated phase data */

  InspiralInit paramsInit;
  UINT4 nbins;


  INITSTATUS(status, "LALPSpinInspiralRDInjection", LALPSPININSPIRALRDINJECTIONC);
  ATTATCHSTATUSPTR(status);

  /* Make sure parameter and waveform structures exist. */
  ASSERT( params, status, LALINSPIRALH_ENULL,  LALINSPIRALH_MSGENULL );
  ASSERT(waveform, status, LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL);
  /* Make sure waveform fields don't exist. */
  ASSERT( !( waveform->a ), status,
  	LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL );
  ASSERT( !( waveform->f ), status,
  	LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL );
  ASSERT( !( waveform->phi ), status,
  	LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL );
  ASSERT( !( waveform->shift ), status,
  	LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL );
  ASSERT( !( waveform->h ), status,
  	LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL );

  /* Compute some parameters*/
  LALInspiralInit(status->statusPtr, params, &paramsInit);
  CHECKSTATUSPTR(status);
  if (paramsInit.nbins==0)
    {
      DETATCHSTATUSPTR(status);
      RETURN (status);
    }

  nbins=2*paramsInit.nbins;

  /* Now we can allocate memory and vector for coherentGW structure*/
  ff   = XLALCreateREAL4Vector(   nbins);
  //LALSCreateVector(status->statusPtr, &ff, nbins);
  //CHECKSTATUSPTR(status);
  hh   = XLALCreateREAL4Vector( 2*nbins);
  //LALSCreateVector(status->statusPtr, &hh, 2*nbins);
  //CHECKSTATUSPTR(status);
  phi = XLALCreateREAL8Vector(  nbins);
  //LALDCreateVector(status->statusPtr, &phi, nbins);
  //CHECKSTATUSPTR(status);
  alpha = XLALCreateREAL4Vector( nbins);
  //LALSCreateVector(status->statusPtr, &alpha, nbins);
  //CHECKSTATUSPTR(status);

  /* By default the waveform is empty */
  memset(ff->data, 0, nbins * sizeof(REAL4));
  memset(hh->data, 0, 2 * nbins * sizeof(REAL4));
  memset(phi->data, 0, nbins * sizeof(REAL8));
  memset(alpha->data, 0, nbins * sizeof(REAL4));

  /* Call the engine function */
  //params->fCutoff     = ppnParams->fStop;
  LALPSpinInspiralRDEngine(status->statusPtr, NULL, NULL, hh, ff, phi, alpha,&count, params, &paramsInit);

  BEGINFAIL( status )
  {
     XLALDestroyREAL4Vector(ff);
     XLALDestroyREAL4Vector(hh);
     XLALDestroyREAL8Vector(phi);
     XLALDestroyREAL4Vector(alpha);
  }
  ENDFAIL( status );

  /* Check an empty waveform hasn't been returned */
  for (i = 0; i < phi->length; i++)
  {
    if (phi->data[i] != 0.0) break;
    if (i == phi->length - 1)
    {
      XLALDestroyREAL4Vector(ff);
      XLALDestroyREAL4Vector(hh);
      XLALDestroyREAL8Vector(phi);
      XLALDestroyREAL4Vector(alpha);
    }
  }

  /* Allocate the waveform structures. */
  if ( ( waveform->h = (REAL4TimeVectorSeries *)
	 LALMalloc( sizeof(REAL4TimeVectorSeries) ) ) == NULL ) {
    ABORT( status, LALINSPIRALH_EMEM,
	   LALINSPIRALH_MSGEMEM );
  }
  memset( waveform->h, 0, sizeof(REAL4TimeVectorSeries) );

  if ( ( waveform->f = (REAL4TimeSeries *)
	 LALMalloc( sizeof(REAL4TimeSeries) ) ) == NULL ) {
    LALFree( waveform->h ); waveform->a = NULL;
    ABORT( status, LALINSPIRALH_EMEM,
	   LALINSPIRALH_MSGEMEM );
  }
  memset( waveform->f, 0, sizeof(REAL4TimeSeries) );

  if ( ( waveform->phi = (REAL8TimeSeries *)
	 LALMalloc( sizeof(REAL8TimeSeries) ) ) == NULL ) {
    LALFree( waveform->h ); waveform->h = NULL;
    LALFree( waveform->f ); waveform->f = NULL;
    ABORT( status, LALINSPIRALH_EMEM,
	   LALINSPIRALH_MSGEMEM );
  }
  memset( waveform->phi, 0, sizeof(REAL8TimeSeries) );

  if ( ( waveform->shift = (REAL4TimeSeries *)
	 LALMalloc( sizeof(REAL4TimeSeries) ) ) == NULL ) {
    LALFree( waveform->h ); waveform->h = NULL;
    LALFree( waveform->f ); waveform->f = NULL;
    LALFree( waveform->phi ); waveform->phi = NULL;
    ABORT( status, LALINSPIRALH_EMEM,
	   LALINSPIRALH_MSGEMEM );
  }
  memset( waveform->shift, 0, sizeof(REAL4TimeSeries) );

  waveform->h->data=XLALCreateREAL4VectorSequence(count,2);
  waveform->f->data=XLALCreateREAL4Vector(count);
  waveform->phi->data=XLALCreateREAL8Vector(count);
  waveform->shift->data=XLALCreateREAL4Vector(count);

  memcpy(waveform->f->data->data ,     ff->data ,    count*(sizeof(REAL4)));
  memcpy(waveform->h->data->data ,     hh->data ,  2*count*(sizeof(REAL4)));
  memcpy(waveform->phi->data->data ,   phi->data ,   count*(sizeof(REAL8)));
  memcpy(waveform->shift->data->data , alpha->data , count*(sizeof(REAL4)));

  waveform->h->deltaT = waveform->f->deltaT = waveform->phi->deltaT = waveform->shift->deltaT
    = 1./params->tSampling;

  waveform->h->sampleUnits = lalStrainUnit;
  waveform->f->sampleUnits = lalHertzUnit;
  waveform->phi->sampleUnits = lalDimensionlessUnit;
  waveform->shift->sampleUnits 	= lalDimensionlessUnit;

  waveform->position = ppnParams->position;
  waveform->psi = ppnParams->psi;

  snprintf( waveform->h->name,     LALNameLength, "PSpinInspiralRD amplitudes" );
  snprintf( waveform->f->name,     LALNameLength, "PSpinInspiralRD frequency" );
  snprintf( waveform->phi->name,   LALNameLength, "PSpinInspiralRD phase" );
  snprintf( waveform->shift->name, LALNameLength, "PSpinInspiralRD alpha" );

    /* --- fill some output ---*/
  ppnParams->tc     = (double)(count-1) / params->tSampling ;
  ppnParams->length = count;
  ppnParams->dfdt   = ((REAL4)(waveform->f->data->data[count-1]
			       - waveform->f->data->data[count-2]))
    * ppnParams->deltaT;
  ppnParams->fStop  = params->fFinal;
  ppnParams->termCode        = GENERATEPPNINSPIRALH_EFSTOP;
  ppnParams->termDescription = GENERATEPPNINSPIRALH_MSGEFSTOP;
  
  ppnParams->fStart   = ppnParams->fStartIn;
  
  /* --- free memory --- */

  XLALDestroyREAL4Vector(ff);
  XLALDestroyREAL4Vector(hh);
  XLALDestroyREAL8Vector(phi);
  XLALDestroyREAL4Vector(alpha);

  DETATCHSTATUSPTR(status);
  RETURN(status);

} /* End LALPSpinInspiralRDForInjection */

/*
 *
 * Main function
 *
 */

/*  <lalVerbatim file="LALPSpinInspiralRDForInjectionCP"> */
void LALPSpinInspiralRDEngine (
                LALStatus        *status,			      
                REAL4Vector      *signalvec1,
                REAL4Vector      *signalvec2,
                REAL4Vector      *hh,
                REAL4Vector      *ff,
                REAL8Vector      *phi,
                REAL4Vector      *shift,
                UINT4            *countback,
                InspiralTemplate *params,
		InspiralInit     *paramsInit
                )
     /* </lalVerbatim> */

{

  /* declare model parameters*/
  LALPSpinInspiralRDparams PSIRDparameters;
  
  /* declare code parameters and variables*/
  INT4		nn = 11+1;              ///< number of dynamical variables, the extra one is the energy
  UINT4 	count,apcount;          ///< integration steps performed 
  UINT4		maxlength;              ///< maximum signal vector length
  UINT4 	length;                 ///< signal vector length
  UINT4		i,j,k,l;                ///< counters          
  UINT4         subsampling=1;          ///< multiplies the rate           
  //  UINT4         pad=1;

  rk4In 	in4;                    ///< used to setup the Runge-Kutta integration
  rk4GSLIntegrator *integrator;

  expnCoeffs 	ak;                     ///<Coefficients in a generic PN expansion (E, flux...)

  REAL8Vector 	dummy, values, dvalues, newvalues, yt, dym, dyt;
  ///< support variables

  REAL8 	lengths;                ///<length in seconds
  REAL4         v2=0.;
  REAL8 	m;                      ///< Total mass in SI units
  REAL8 	t;                      ///< time (units of total mass)
  REAL8 	unitHz;
  REAL8  	dt;
  REAL8 LNhztol = 1.0e-8;

  /* declare initial values of dynamical variables */
  REAL8 initPhi,initomega,initv;                      
  REAL8 initLNh[3],LNmag;
  REAL8 iJ[3],iJmod,initJh[3];
  REAL8 iS1[3],initS1[3];
  REAL8 iS2[3],initS2[3];
  REAL8 ry[3][3],rz[3][3];
  REAL8 thetaJ,phiJ;

  REAL8 ci,si,s2i,c2i2,s2i2,c4i2,s4i2;
  REAL8 amp22,amp20;
  /* Useful variables to define GW multipolar modes*/

  REAL4Vector  *h22, *h21, *h20, *sig1, *sig2, *hap, *fap, *shift22;
  REAL8Vector  *phap;
  /* support variables*/

  /* declare dynamical variables*/
  REAL8 Phi, omega, LNhx, LNhy, LNhz, S1x, S1y, S1z, S2x, S2y, S2z;
  REAL8 dLNhzold,dLNhx,dLNhy,dLNhz=0.;
  REAL8 Psi=0.;
  REAL8 Psiold;
  REAL8 omegaold;
  REAL8 omegadot=0.;
  REAL8 omegadotold;
  REAL8 omegaddot;
  REAL8 alpha=0.;
  REAL8 alphaold;
  REAL8 alphadot=0.;
  REAL8 alphadotold;
  REAL8 alphaddot=0.;
  REAL8 alphaddotold;
  REAL8 alphadddot=0.;
  REAL8 cialphadot,cialphadotd,cialphadotdd;

  REAL4 mchi1,mchi2;
  REAL4 cosa1,cosa2,cosg;

  REAL8 energy=0.;
  REAL8 enold;
  REAL8 v2old;

  LALPSpinInspiralRDparams *mparams;

  CHAR message[256];

  /* used in the phen-phase attach procedure*/
  REAL8           tAs,om1,om0;
  REAL8           omrac,trac;
  REAL8           t0,Psi0;
  
  /* used in the ring-down attach procedure*/
  REAL8           omegamatch,omegaRD=0.;
  COMPLEX8Vector  *modefreqs;
  INT4            xlalStatus22,xlalStatus21,xlalStatus20;
  UINT4           nmodes;
  COMPLEX16       MultSphHarm2P2;     ///< Spin-weighted spherical harmonics 2,2
  COMPLEX16       MultSphHarm2M2;     ///< Spin-weighted spherical harmonics 2,-2
  COMPLEX16       MultSphHarm2P1;     ///< Spin-weighted spherical harmonics 2,1
  COMPLEX16       MultSphHarm2M1;     ///< Spin-weighted spherical harmonics 2,-1
  COMPLEX16       MultSphHarm20;      ///< Spin-weighted spherical harmonics 2,0
  REAL4           x1, x2;
  REAL8           y_1, y_2, z1, z2;   ///< spherical harmonics needed to compute (h+,hx) from hlm

  REAL4 inc, phiangle;

  const REAL4 fracRD=0.8;

  /* switch to keep track of matching of the linear frequency growth phase*/
  INT4 rett=1;

  INT4 count0;

  INITSTATUS(status, "LALPSpinInspiralRD", LALPSPININSPIRALRDMC);
  ATTATCHSTATUSPTR(status);
  
  /* set parameters from InspiralTemplate structure*/

  /* Make sure parameter and waveform structures exist. */
  ASSERT(params, status, LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL);
  /*===================*/
  
  /* Compute some parameters*/

  if (paramsInit->nbins==0)
    {
      DETATCHSTATUSPTR(status);
      RETURN (status);
    }
  ak   = paramsInit->ak;
  
  mparams = &PSIRDparameters;

  /* set units*/
  m = params->totalMass * LAL_MTSUN_SI;
  unitHz = params->totalMass * LAL_MTSUN_SI * (REAL8)LAL_PI;
  /*    tSampling is in Hz, so dt is in seconds*/
  params->tSampling *= subsampling;
  dt = 1.0/params->tSampling;

  /* -- length in seconds from Newtonian formula; */
  lengths = (5.0/256.0)/LAL_PI * pow(LAL_PI * params->chirpMass * LAL_MTSUN_SI * params->fLower,-5.0/3.0) / params->fLower;

  /*
    REAL4 length;
    length = ceil(log10(lengths/dt)/log10(2.0));
    length = pow(2,length);
    length *= 2;
  */

  /* set initial values of dynamical variables*/
  initPhi = params->inclination;
  initomega = params->fLower * unitHz;
  initv = pow( initomega, oneby3 );

  /* Here we use the following convention:
     the coordinates of the spin vectors params->spin1,2 are given 
     in the reference frame where the initial orbital angular momentum
     is along the z axis.
     The modulus of the initial angular momentum is fixed by m1,m2 and 
     initial frequency.
     I (Riccardo) then perform two rotations to bring the initial 
     total angular momentum J along the new z axis. 
     Then orbitTheta0 and orbitPhi0 will be automatically fixed, no matter 
     which values are passed by the param structure. */
  
  LNmag= params->eta * params->totalMass * params->totalMass / initv;

  mchi1=sqrt( params->spin1[0]*params->spin1[0] + params->spin1[1]*params->spin1[1] + params->spin1[2]*params->spin1[2] );
  mchi2=sqrt( params->spin2[0]*params->spin2[0] + params->spin2[1]*params->spin2[1] + params->spin2[2]*params->spin2[2] );

  cosa1 = params->spin1[2]/mchi1;
  cosa2 = params->spin2[2]/mchi2;
  cosg  = (params->spin1[0]*params->spin2[0] + params->spin1[1]*params->spin2[1] + params->spin1[2]*params->spin2[2])/ mchi1/mchi2;

  for (j=0;j<3;j++) {
    iS1[j] = params->spin1[j] * params->mass1 * params->mass1;
    iS2[j] = params->spin2[j] * params->mass2 * params->mass2;
    iJ[j] = iS1[j] + iS2[j];
  }
  iJ[2] += LNmag;

  iJmod = sqrt ( iJ[0]*iJ[0] + iJ[1]*iJ[1] + iJ[2]*iJ[2] );

  for (j=0;j<3;j++) {
    initJh[j] = iJ[j]/iJmod;
    initLNh[j]=0.;
    initS1[j]=0.;
    initS2[j]=0.;
  }

  if ((initJh[0]==0.)&&(initJh[1]==0.)) {
    phiJ=0.;
    thetaJ=0.;
  }
  else {
    phiJ=atan2(initJh[1],initJh[0]);
  }    

  rz[0][0]=cos(phiJ)  ; rz[0][1]=sin(phiJ); rz[0][2]=0.;
  rz[1][0]=-sin(phiJ) ; rz[1][1]=cos(phiJ); rz[1][2]=0.;
  rz[2][0]=0.         ; rz[2][1]=0.       ; rz[2][2]=1.;

  thetaJ=acos(initJh[2]);
  ry[0][0]=cos(thetaJ); ry[0][1]=0        ; ry[0][2]=-sin(thetaJ);
  ry[1][0]=0.         ; ry[1][1]=1.       ; ry[1][2]=0.;
  ry[2][0]=sin(thetaJ); ry[2][1]=0.       ; ry[2][2]=cos(thetaJ);
  
  for (j=0;j<3;j++) {
    
    for (k=0;k<3;k++) {
      
      initLNh[j] += ry[j][k] * rz[k][2];
      
      for (l=0;l<3;l++) {
	initS1[j] += ry[j][k] * rz[k][l] * iS1[l];
	initS2[j] += ry[j][k] * rz[k][l] * iS2[l];
      }
    }
    
    initS1[j] /= (params->totalMass * params->totalMass);
    initS2[j] /= (params->totalMass * params->totalMass);
  }
  
  /* In the convention of arXiv:0810.5336 we have 
     iota_0=InspiralTemplate.orbitTheta0
     alpha_0=InspiralTemplate.orbitPhi0 
     theta=InspiralTemplate.inclination=SimInspiralTable.inclination 
     InspiralTemplate.polarization=SimInspiralTable.polarizationAngle=0.*/
  
  length=0;
  if (signalvec1) length = signalvec1->length; else if (ff) length = ff->length;

  dummy.length = nn * 6;
  
  values.length = dvalues.length = newvalues.length = yt.length = dym.length = dyt.length = nn;
  
  if (!(dummy.data = (REAL8 * ) LALMalloc(sizeof(REAL8) * nn * 6))) {
    ABORT(status, LALINSPIRALH_EMEM, LALINSPIRALH_MSGEMEM);
  }
  
  values.data 		= &dummy.data[0];
  dvalues.data 		= &dummy.data[nn];
  newvalues.data 	= &dummy.data[2*nn];
  yt.data 		= &dummy.data[3*nn];
  dym.data 		= &dummy.data[4*nn];
  dyt.data 		= &dummy.data[5*nn];
  
  /* setup coefficients for PN equations*/
  mparams->m2m1 = params->mass2/params->mass1;
  mparams->m1m2 = params->mass1/params->mass2;
  mparams->m1m  = params->mass1/params->totalMass;
  mparams->m2m  = params->mass1/params->totalMass;
  mparams->dm   = (params->mass1-params->mass2)/params->totalMass;

  /* params->eta might have been set up before but just for safety, we
   * recompute it here below.*/
  params->eta =  (params->mass1 * params->mass2)/
    (params->mass1 + params->mass2)/(params->mass1 + params->mass2);
  mparams->eta = params->eta;

  for (j = LAL_PNORDER_NEWTONIAN; j <= params->order; j++) {
    mparams->wdotorb[j] = ak.ST[j];
  }
  mparams->wdotorblog=0.;
  for (j = params->order +1; j < 8; j++){
    mparams->wdotorb[j] = 0.;
  }
  if ((params->order)>=6) {
    mparams->wdotorblog=ak.ST[7];
    if ((params->order)==7) mparams->wdotorb[7]=ak.ST[8];
  }

  mparams->epnorb[0]=ak.ETaN;

  switch (params->order){
  case     LAL_PNORDER_NEWTONIAN:
  case     LAL_PNORDER_HALF:
    break;
  case     LAL_PNORDER_ONE:
    mparams->epnorb[1]     = ak.ETa1;
    break;
  case     LAL_PNORDER_ONE_POINT_FIVE:

    mparams->epnorb[1]     = ak.ETa1;
    mparams->epnspin15S1dotLNh = 8./3.+2.*mparams->m2m1;
    mparams->epnspin15S2dotLNh = 8./3.+2.*mparams->m1m2;

    mparams->wdotspin15S1LNh  = -( 113.0 + 75.0 * mparams->m2m1 ) / 12.0;
    mparams->wdotspin15S2LNh  = -( 113.0 + 75.0 * mparams->m1m2 ) / 12.0;

    mparams->LNhdot15      = 0.5;

    mparams->S1dot15 	   = (4.0 + 3.0 * mparams->m2m1) / 2.0 * mparams->eta;
    mparams->S2dot15 	   = (4.0 + 3.0 * mparams->m1m2) / 2.0 * mparams->eta;

    mparams->wdotspin15S1LNh  = -( 113.0 + 75.0 * mparams->m2m1 ) / 12.0;
    mparams->wdotspin15S2LNh  = -( 113.0 + 75.0 * mparams->m1m2 ) / 12.0;
    mparams->LNhdot15 	= 0.5;
    mparams->S1dot15 	= (4.0 + 3.0 * mparams->m2m1) / 2.0 * mparams->eta;
    mparams->S2dot15 	= (4.0 + 3.0 * mparams->m1m2) / 2.0 * mparams->eta;

    break;
  case     LAL_PNORDER_TWO:

    mparams->epnorb[1]         = ak.ETa1;
    mparams->epnspin15S1dotLNh = 8./3.+2.*mparams->m2m1;
    mparams->epnspin15S2dotLNh = 8./3.+2.*mparams->m1m2;
    mparams->epnorb[2]         = ak.ETa2;

    mparams->wdotspin15S1LNh   = -( 113.0 + 75.0 * mparams->m2m1 ) / 12.0;
    mparams->wdotspin15S2LNh   = -( 113.0 + 75.0 * mparams->m1m2 ) / 12.0;
    mparams->wdotspin20S1S2    = -(1.0/48.0) / mparams->eta ;
    mparams->wdotspin20S1S1    = 1./96.;

    mparams->LNhdot15 	       = 0.5;
    mparams->LNhdot20          = -1.5 / mparams->eta;

    mparams->S1dot15 	       = (4.0 + 3.0 * mparams->m2m1) / 2.0 * mparams->eta;
    mparams->S2dot15 	       = (4.0 + 3.0 * mparams->m1m2) / 2.0 * mparams->eta;
    mparams->Sdot20 	       = 0.5 ;
    mparams->S1dot25 	       = 0.5625 + 1.25*mparams->eta - mparams->eta*mparams->eta/24. + mparams->dm*(-0.5625+0.625*mparams->eta);
    mparams->S2dot25 	       = 0.5625 + 1.25*mparams->eta - mparams->eta*mparams->eta/24. - mparams->dm*(-0.5625+0.625*mparams->eta);

    break;
  case     LAL_PNORDER_TWO_POINT_FIVE:

    mparams->epnorb[1]         = ak.ETa1;
    mparams->epnspin15S1dotLNh = 8./3.+2.*mparams->m2m1;
    mparams->epnspin15S2dotLNh = 8./3.+2.*mparams->m1m2;
    mparams->epnorb[2]         = ak.ETa2;
    mparams->epnspin25S1dotLNh = 8. - 31./9.*mparams->eta + (3.-10./3.*mparams->eta)*mparams->m2m1;
    mparams->epnspin25S1dotLNh = 8. - 31./9.*mparams->eta + (3.-10./3.*mparams->eta)*mparams->m1m2;

    mparams->wdotspin15S1LNh   = -( 113.0 + 75.0 * mparams->m2m1 ) / 12.0;
    mparams->wdotspin15S2LNh   = -( 113.0 + 75.0 * mparams->m1m2 ) / 12.0;
    mparams->wdotspin20S1S2    = -(1.0/48.0) / mparams->eta ;
    mparams->wdotspin20S1S1    = 1./96.;
    mparams->wdotspin25S1LNh   = -26135./1008. + 6385./168.* mparams->eta + (-473/84.+1231./56.*mparams->eta)*mparams->m1m2;
    mparams->wdotspin25S2LNh   = -26135./1008. + 6385./168.* mparams->eta + (-473/84.+1231./56.*mparams->eta)*mparams->m2m1;

    mparams->LNhdot15 	       = 0.5;
    mparams->LNhdot20 	       = -1.5 / mparams->eta;

    mparams->S1dot15 	       = (4.0 + 3.0 * mparams->m2m1) / 2.0 * mparams->eta;
    mparams->S2dot15 	       = (4.0 + 3.0 * mparams->m1m2) / 2.0 * mparams->eta;
    mparams->Sdot20            = 0.5;

    break;
  case     LAL_PNORDER_THREE:
  case     LAL_PNORDER_THREE_POINT_FIVE:

    mparams->epnorb[1]           = ak.ETa1;
    mparams->epnspin15S1dotLNh   = 8./3.+2.*mparams->m2m1;
    mparams->epnspin15S2dotLNh   = 8./3.+2.*mparams->m1m2;
    mparams->epnorb[2]           = ak.ETa2;
    mparams->epnspin20S1S2       = 1./mparams->eta;
    mparams->epnspin20S1S2dotLNh = -3./mparams->eta;
    mparams->epnspin20S1S1       = (1.+mparams->m2m1)*(1.+mparams->m2m1)/2.;
    mparams->epnspin20S2S2       = (1.+mparams->m1m2)*(1.+mparams->m1m2)/2.;
    mparams->epnspin20S1S1dotLNh = -3.*(1.+mparams->m2m1)*(1.+mparams->m2m1)/2.;
    mparams->epnspin20S2S2dotLNh = -3.*(1.+mparams->m1m2)*(1.+mparams->m1m2)/2.;
    mparams->epnspin25S1dotLNh   = 8. - 31./9.*mparams->eta + (3.-10./3.*mparams->eta)*mparams->m2m1;
    mparams->epnspin25S2dotLNh   = 8. - 31./9.*mparams->eta + (3.-10./3.*mparams->eta)*mparams->m1m2;
    mparams->epnorb[3]           = ak.ETa3;

    mparams->wdotspin15S1LNh     = -( 113.0 + 75.0 * mparams->m2m1 ) / 12.0;
    mparams->wdotspin15S2LNh     = -( 113.0 + 75.0 * mparams->m1m2 ) / 12.0;
    mparams->wdotspin20S1S2      = -(1.0/48.0) / mparams->eta ;
    mparams->wdotspin20S1S1      = 1./96.;
    mparams->wdotspin25S1LNh     = -26135./1008. + 6385./168.* mparams->eta + (-473/84.+1231./56.*mparams->eta)*mparams->m1m2;
    mparams->wdotspin25S2LNh     = -26135./1008. + 6385./168.* mparams->eta + (-473/84.+1231./56.*mparams->eta)*mparams->m2m1;

    mparams->S1dot15 	         = (4.0 + 3.0 * mparams->m2m1) / 2.0 * mparams->eta;
    mparams->S2dot15 	         = (4.0 + 3.0 * mparams->m1m2) / 2.0 * mparams->eta;
    mparams->Sdot20              = 0.5;

    mparams->S1dot25 	       = 0.5625 + 1.25*mparams->eta - mparams->eta*mparams->eta/24. + mparams->dm*(-0.5625+0.625*mparams->eta);
    mparams->S2dot25 	       = 0.5625 + 1.25*mparams->eta - mparams->eta*mparams->eta/24. - mparams->dm*(-0.5625+0.625*mparams->eta);
    break;
  case     LAL_PNORDER_PSEUDO_FOUR:
    fprintf(stderr,"*** PSIRD: ERROR spin Taylor approximant not available at p4PN order for SpinTaylor approximant\n");
    break;
  case     LAL_PNORDER_NUM_ORDER:
    fprintf(stderr,"*** PSIRD: ERROR NUM_ORDER not a valid PN order\n");
  }

  /* setup initial conditions for dynamical variables*/

  Phi  = initPhi;
  omega = initomega;

  LNhx = initLNh[0];
  LNhy = initLNh[1];
  LNhz = initLNh[2];

  S1x = initS1[0];
  S1y = initS1[1];
  S1z = initS1[2];

  S2x = initS2[0];
  S2y = initS2[1];
  S2z = initS2[2];

  /* copy everything in the "values" structure*/

  values.data[0] = Phi;
  values.data[1] = omega;

  values.data[2] = LNhx;
  values.data[3] = LNhy;
  values.data[4] = LNhz;

  values.data[5] = S1x;
  values.data[6] = S1y;
  values.data[7] = S1z;

  values.data[8] = S2x;
  values.data[9] = S2y;
  values.data[10]= S2z;

  in4.function 	= LALPSpinInspiralRDderivatives;
  in4.y 	= &values;
  in4.dydx 	= &dvalues;
  in4.h 	= dt/m;
  in4.n 	= nn;
  in4.yt 	= &yt;
  in4.dym 	= &dym;
  in4.dyt 	= &dyt;

  /* Allocate memory for temporary arrays */
  h22  = XLALCreateREAL4Vector ( length*2 );
  h21  = XLALCreateREAL4Vector ( length*2 );
  h20  = XLALCreateREAL4Vector ( length*2 );
  sig1 = XLALCreateREAL4Vector ( length);
  sig2 = XLALCreateREAL4Vector ( length);
  hap  = XLALCreateREAL4Vector ( length*2);
  fap  = XLALCreateREAL4Vector ( length );
  phap = XLALCreateREAL8Vector ( length );
  shift22 = XLALCreateREAL4Vector ( length );
  
  if ( !h22 || !h21 ||!h20 || !sig1 || !sig2 || !fap || !phap || !shift22 || !hap )
    {
      if ( h22 ) XLALDestroyREAL4Vector( h22 );
      if ( h21 ) XLALDestroyREAL4Vector( h21 );
      if ( h20 ) XLALDestroyREAL4Vector( h20 );
      if ( sig1 ) XLALDestroyREAL4Vector( sig1 );
      if ( sig2 ) XLALDestroyREAL4Vector( sig2 );
      if ( fap ) XLALDestroyREAL4Vector( fap );
      if ( hap ) XLALDestroyREAL4Vector( hap );
      if ( phap ) XLALDestroyREAL8Vector( phap );
      if ( shift22 ) XLALDestroyREAL4Vector( shift22 );
      
      LALFree( dummy.data );
      ABORT( status, LALINSPIRALH_EMEM, LALINSPIRALH_MSGEMEM );
    }
  
  memset(h22->data, 0, h22->length * sizeof( REAL4 ));
  memset(h21->data, 0, h21->length * sizeof( REAL4 ));
  memset(h20->data, 0, h20->length * sizeof( REAL4 ));
  memset(sig1->data, 0, sig1->length * sizeof( REAL4 ));
  memset(sig2->data, 0, sig2->length * sizeof( REAL4 ));
  memset(hap->data, 0, hap->length * sizeof( REAL4 ));
  memset(fap->data, 0, fap->length * sizeof( REAL4 ));
  memset(phap->data, 0, phap->length * sizeof( REAL8 ));
  memset(shift22->data, 0, shift22->length * sizeof( REAL4 ));
  
  xlalErrno = 0;
  /* Initialize GSL integrator */
  if (!(integrator = XLALRungeKutta4Init(nn, &in4)))
    {
      INT4 errNum = XLALClearErrno();
      LALFree(dummy.data);
      
      if (errNum == XLAL_ENOMEM)
	ABORT(status, LALINSPIRALH_EMEM, LALINSPIRALH_MSGEMEM);
      else
	ABORTXLAL( status );
    }
  
  /* main integration loop*/
  
  t = 0.0;
  count = 0;

  alpha=atan2(LNhy,LNhx);

  LALPSpinInspiralRDderivatives(&values,&dvalues,(void*)mparams);

  /*Initialization of support variables*/
  cialphadot=0.;
  alphadot=0.;
  alphaddot=0.;
  alphadddot=0.;
  omrac=0.;

  /* Injection: hh,ff; template: signalvec1,2*/

  if ( hh || signalvec2)
    params->nStartPad = 0; /* must be zero for templates and injection */
  
  /*Analytical formula for omega_match, it can be controlled by fCutoff
    by (un)commenting the relative lines. It refers to the values of spins at
    omega M = 4.33e-2*/
  omegamatch = 0.05480;
  //omegamatch = params->fCutoff * unitHz;

  /* The number of Ring Down modes is set here, it cannot exceed 3*/
  nmodes = 1;

  /* For RD, check that the 220 QNM freq. is less than the Nyquist freq. */
  /* Get QNM frequencies */
  modefreqs = XLALCreateCOMPLEX8Vector( nmodes );

  xlalStatus22 = XLALPSpinGenerateQNMFreq( modefreqs, params, energy, 2, 2, nmodes, LNhx, LNhy, LNhz);
  if ( xlalStatus22 != XLAL_SUCCESS )
    {
      XLALDestroyCOMPLEX8Vector( modefreqs );
      ABORTXLAL( status );
    }  
  omegaRD=modefreqs->data[0].re * unitHz / LAL_PI / 2.;    
  /* If Nyquist freq. <  220 QNM freq., do not attach RD */
  /* Note that we cancelled a factor of 2 occuring on both sides */
  if ( params->tSampling < fracRD * modefreqs->data[0].re / LAL_PI ) {
    XLALDestroyCOMPLEX8Vector( modefreqs ); 
    fprintf(stderr, "Estimated ringdown freq larger than Nyquist freq. "
	    "Increase sample rate or consider using SpinTaylor approximant.\n" );
    LALError(status->statusPtr, message);
    ABORT( status, LALINSPIRALH_ECHOICE, LALINSPIRALH_MSGECHOICE); 
  }
  if (signalvec1) maxlength=signalvec1->length; 
  else {
    if (ff) maxlength=ff->length;
    else maxlength=0;
  }

  v2=pow(omega,2./3.);
  
  params->ampOrder=LAL_PNORDER_NEWTONIAN;

  do {

    if (count >= maxlength)
      {
        XLALRungeKutta4Free( integrator );
	LALFree(dummy.data);
	ABORT(status, LALINSPIRALH_ESIZE, LALINSPIRALH_MSGESIZE);
      }

    Psiold      = Psi;
    omegaold    = omega;
    omegadotold = omegadot;
    alphaold    = alpha;    
    alphadotold = alphadot;
    alphaddotold = alphaddot;
    enold       = energy;

    Psi = Phi - 2. * omega * log(omega);
    
    amp22 = -2.0 * params->mu * v2 * LAL_MRSUN_SI/(params->distance) * sqrt( 16.*LAL_PI/5.);

    amp20 = amp22 * sqrt(3/2.);

    ci=(LNhz);
    s2i=1.-ci*ci;
    si=sqrt(s2i);
    c2i2 = (1. + ci)/2.;
    s2i2 = (1. - ci)/2.;
    c4i2 = c2i2*c2i2;
    s4i2=s2i2*s2i2;

    h22->data[2*count] = (REAL4)(amp22 * ( cos(2.*(Psi-alpha))*c4i2 + cos(2.*(Psi+alpha)) *s4i2 ) );
    h22->data[2*count+1] = (REAL4)(amp22 * (sin(2.*(Psi-alpha))*c4i2 - sin(2.*(Psi+alpha)) * s4i2 ) );

    h21->data[2*count] = (REAL4) (amp22 * si * ( sin(2.*Psi-alpha)*s2i2 - sin(2.*Psi+alpha)*c2i2 ) );
    h21->data[2*count+1] = (REAL4)(-amp22 * si * ( cos(2.*Psi-alpha)*s2i2 + cos(2.*Psi+alpha)*c2i2 ) );

    h20->data[2*count] = (REAL4)(amp20 * s2i * cos( 2.* Psi));
    h20->data[2*count+1] = 0.;
        
    fap->data[count] =  (REAL4)( omega - ci*alphadot );
    phap->data[count] =  (REAL8)( Psi );
    
    shift22->data[count] = alpha;

    in4.x = t/m;
    LALRungeKutta4(status->statusPtr, &newvalues, integrator, (void*)mparams);
    CHECKSTATUSPTR(status);

    /* updating values of dynamical variables*/

    Phi  = values.data[0] = newvalues.data[0];

    omega = values.data[1] = newvalues.data[1];
    
    LNhx  = values.data[2] = newvalues.data[2];
    LNhy  = values.data[3] = newvalues.data[3];
    LNhz  = values.data[4] = newvalues.data[4];
    
    S1x   = values.data[5] = newvalues.data[5];
    S1y   = values.data[6] = newvalues.data[6];
    S1z   = values.data[7] = newvalues.data[7];
    
    S2x   = values.data[8] = newvalues.data[8];
    S2y   = values.data[9] = newvalues.data[9];
    S2z   = values.data[10]= newvalues.data[10];
    
    dLNhzold=dLNhz;

    LALPSpinInspiralRDderivatives(&values,&dvalues,(void*)mparams);

    dLNhx=dvalues.data[2];
    dLNhy=dvalues.data[3];
    dLNhz=dvalues.data[4];

    alpha = atan2(LNhy,LNhx);
    if ((LNhx*LNhx+LNhy*LNhy)>0.)
      alphadot  = (LNhx*dLNhy - LNhy*dLNhx) / (LNhx*LNhx + LNhy*LNhy);
    else 
      alphadot = 0.;

    if ((ci*ci*alphadot*alphadot)>0.1*(omega*omega)) fprintf(stderr," ** LALSTIRD WARNING**:  cosi=%11.3e  adot=%11.3e  > w=%11.3e\n",ci,alphadot,omega);

    alphaddot = (alphadot-alphadotold)/dt*m;
    cialphadotd=dLNhz*alphadot+LNhz*alphaddot;
    alphadddot = (alphaddot-alphaddotold)/dt*m;
    cialphadotdd=(dLNhz-dLNhzold)/dt*m*alphadot+2.*dLNhz*alphaddot+LNhz*alphadddot;

    energy = values.data[11];
    v2=pow(omega,2./3.);

    omegadot = dvalues.data[1];
    omegaddot= (omegadot-omegadotold)/dt*m;

    t = (++count - params->nStartPad) * dt;

    /*
    if ((count%100)==0) {
      fprintf(stdout,"S1=(%8.3f,%8.3f,%8.3f)  %10.5f",S1x,S1y,S1z,sqrt(S1x*S1x+S1y*S1y+S1z*S1z));
      fprintf(stdout," S2=(%8.3f,%8.3f,%8.3f)  %10.5f\n",S2x,S2y,S2z,sqrt(S2x*S2x+S2y*S2y+S2z*S2z));
    }
    */

    //adjourn ommatch
    omegamatch= 0.0548 - 5.63e-03*(S1z+S2z) + 2.16e-3*(S1x*S2x+S1y*S2y) + 1.36e-2*(S1x*S1x+S1y*S1y+S2x*S2x+S2y*S2y) - 0.81e-3*(S1z*S1z+S2z*S2z);

  }

 /* Test that omega/unitHz < NYQUIST */
  
  while( energy < enold && omega > omegaold && omega/unitHz < params->tSampling && !(isnan(omega)) && count < maxlength && omega < omegamatch);

  //  fprintf(stdout,"ommatch=%11.3e\n",omegamatch);
  //fprintf(stdout,"S1=(%8.3f,%8.3f,%8.3f)  %10.5f",S1x,S1y,S1z,sqrt(S1x*S1x+S1y*S1y+S1z*S1z));
  //fprintf(stdout,"   S2=(%8.3f,%8.3f,%8.3f)  %10.5f\n",S2x,S2y,S2z,sqrt(S2x*S2x+S2y*S2y+S2z*S2z));

  tAs=t+2.*(omegadot-cialphadotd)/(omegaddot-cialphadotdd)*m;
  om1=(omegadot-cialphadotd)*tAs*pow((1.-t/tAs),2.)/m;
  om0=(omega-cialphadot)-om1/(1.-t/tAs);

  for (i=2*count;i<2*length;i++) {
    h22->data[i]=0.;
    h20->data[i]=0.;
  }

 /* if code stopped since evolving quantities became nan write an error message */
  if (isnan(omega)){
    fprintf(stderr,
	    "** LALSTIRD WARNING **: evolving quantities have become nan. "
	    "m1: %e, "
	    "m2: %e, "
	    "spin1x: %e, "
	    "spin1y: %e, "
	    "spin1z: %e, "
	    "spin2x: %e, "
	    "spin2y: %e, "
	    "spin2z: %e, "
	    "inclination: %e\n ",
	    params->mass1, params->mass2,
	    params->spin1[0], params->spin1[1], params->spin1[2],
	    params->spin2[0], params->spin2[1], params->spin2[2],
	    params->inclination);
    rett=0;
    omega=omegaold;
    energy=enold;
    fprintf(stderr,"en=%11.3e  om=%11.3e\n",energy,omega);
  }
  /* if code stopped due to co-ord singularity write an error message */
  else if ((LNhx*LNhx + LNhy*LNhy + LNhz*LNhz) < (1.0 - LNhztol)){
    fprintf( stderr,
	     "WARNING: Injection terminated, co-ord singularity. "
	     "m1: %e, "
	     "m2: %e, "
	     "spin1x: %e, "
	     "spin1y: %e, "
	     "spin1z: %e, "
	     "spin2x: %e, "
	     "spin2y: %e, "
	     "spin2z: %e, "
	     "inclination: %e\n ",
	     params->mass1, params->mass2,
	     params->spin1[0], params->spin1[1], params->spin1[2],
	     params->spin2[0], params->spin2[1], params->spin2[2],
	     params->inclination);
    rett=0;
    omega=omegaold;
    energy=enold;
  }
  else if ( omega > omegamatch)  fprintf(stdout, "** LALPSpinInspiralRD.c ** STOP: omega > omegamatch= %10.4e\n",omegamatch);
  else if ( energy > enold) {
    fprintf(stderr, "** LALPSpinInspiralRD ** STOP: energy increases %11.3e > %11.3e\n",energy,enold);
    omega=omegaold;
    energy=enold;
    rett=0;
  }

  params->vFinal = pow(omega,oneby3);
  if (signalvec1 && !signalvec2) params->tC = t;
  if (signalvec1 || signalvec2){
    params->fFinal = omega/(LAL_PI*m);
  }
  else if (hh){
    params->fFinal =fap->data[count-1];
  }

  t0=t-dt;
  Psi0=Psi+tAs*om1*log(1.-t0/tAs)/m;

  count0=count;

  /* Get QNM frequencies */
  modefreqs = XLALCreateCOMPLEX8Vector( nmodes );

  xlalStatus22 = XLALPSpinGenerateQNMFreq( modefreqs, params, energy, 2, 2, nmodes, LNhx, LNhy, LNhz );
  if ( xlalStatus22 != XLAL_SUCCESS )
    {
      XLALDestroyCOMPLEX8Vector( modefreqs );
      ABORTXLAL( status );
    }
  
  omegaRD=modefreqs->data[0].re * unitHz / LAL_PI /2.;
  
  if (rett==1) {

    //fprintf(stdout,"Inserisco la crescita rettilinea da count=%d fino a w=%14.6e\n",count,omegaRD);
    //fprintf("m=%14.6e  t=%14.6e %14.6e  tAs=%14.6e  %14.6e\n",m,t,t/m,tAs,tAs/m);
    //fprintf(stdout,"Psi=%14.6e  alpha=%14.6e\n",Psi,alpha);
    //fprintf(stdout,"om0=%14.6e  om1=%14.6e  %14.6e\n",om0,om1,(omegaddot+alphadddot)*tAs*tAs*pow((1.-t/tAs),3.)/m/m/2.);

    trac=tAs*(1.-1.5*om1/(omegaRD-om0));
    omrac=4./27.*pow(omegaRD-om0,3.)/om1/om1;
 
    do {
      
      omega = om1/(1.-t/tAs)+om0;
      fap->data[count] =  omega;
      Psiold=Psi;
      Psi = Psi0-tAs*om1*log(1.-t/tAs)/m+om0*(t-t0)/m;      
      v2old=v2;
      v2=pow(omega,2./3.);
      amp22*=v2/v2old;

      h22->data[2*count] = (REAL4)(amp22 * ( cos(2.*(Psi-alpha))*c4i2 + cos(2.*(Psi+alpha)) *s4i2 ) );
      h22->data[2*count+1] = (REAL4)(amp22 * (sin(2.*(Psi-alpha))*c4i2 - sin(2.*(Psi+alpha)) * s4i2 ) );

      h21->data[2*count] = (REAL4) (amp22 * si * ( sin(2.*Psi-alpha)*s2i2 - sin(2.*Psi+alpha)*c2i2 ) );
      h21->data[2*count+1] = (REAL4)(-amp22 * si * ( cos(2.*Psi-alpha)*s2i2 + cos(2.*Psi+alpha)*c2i2 ) );
      
      h20->data[2*count] = (REAL4)(amp20 * s2i * cos( 2.* Psi));
      h20->data[2*count+1] = 0.;
            
      phap->data[count] =  (REAL8)( Psi );
      
      shift22->data[count] = alpha;
      
      count++;
      t+=dt;    
  
    } while ((omega < fracRD*omegaRD)&&( t < trac));
     
  }

  else fprintf(stderr,"** PSIRD: No phen part added\n");

  /* Smoothing the matching with the ring down*/
  count0=count;

  t0=t-dt;
  Psi0=Psi-omegaRD*t0/m-tAs*omrac*pow((1.-t0/tAs),3.)/3./m;

  do {
    
    omegaold=omega;
    omega=omegaRD - omrac * (1.-t/tAs)*(1.-t/tAs);
    
    fap->data[count] =  omega;
    Psiold=Psi;
    Psi = omegaRD*t/m + tAs*omrac*pow((1.-t/tAs),3.)/3./m + Psi0;
      
    v2old=v2;
    v2=pow(omega,2./3.);
    amp22*=sqrt(v2old/v2);
           
    h22->data[2*count] = (REAL4)(amp22 * ( cos(2.*(Psi-alpha))*c4i2 + cos(2.*(Psi+alpha)) *s4i2 ) );
    h22->data[2*count+1] = (REAL4)(amp22 * (sin(2.*(Psi-alpha))*c4i2 - sin(2.*(Psi+alpha)) * s4i2 ) );
    
    h21->data[2*count] = (REAL4) (amp22 * si * ( sin(2.*Psi-alpha)*s2i2 - sin(2.*Psi+alpha)*c2i2 ) );
    h21->data[2*count+1] = (REAL4)(-amp22 * si * ( cos(2.*Psi-alpha)*s2i2 + cos(2.*Psi+alpha)*c2i2 ) );
    
    h20->data[2*count] = (REAL4)(amp20 * s2i * cos( 2.* Psi));
    h20->data[2*count+1] = 0.;
    
    phap->data[count] =  (REAL8)( Psi );
        
    shift22->data[count] = alpha;
    
    count++;
    t+=dt;

  } while ((omega < fracRD*omegaRD) && (omegaold < omega) );

  if (omegaold>omega) count--;
  *countback=count;

  /*Now ringdown is inserted*/

  XLALRungeKutta4Free( integrator );
  LALFree(dummy.data);
  
  /*--------------------------------------------------------------
   * Attach the ringdown waveform to the end of inspiral if
   * the approximant is SpinTayloRD
   -------------------------------------------------------------*/

  //fprintf(stderr,"** LALPSpinInspiralRD: count prima attachRD %d\n",*countback);

  apcount=count;
  xlalStatus20 = XLALPSpinInspiralAttachRingdownWave( h20 , params , energy, &apcount, nmodes , 2 , 0 , LNhx, LNhy, LNhz );
  if (xlalStatus20 != XLAL_SUCCESS )	XLALDestroyREAL4Vector( h20 );
  else {
    for (i=2*apcount;i<2*length;i++) h20->data[i]=0.;
    *countback= apcount;
  }

  apcount= count;
  xlalStatus21 = XLALPSpinInspiralAttachRingdownWave( h21 , params , energy, &apcount, nmodes , 2 , 1 , LNhx, LNhy, LNhz );
  if (xlalStatus21 != XLAL_SUCCESS )	XLALDestroyREAL4Vector( h21 );
  else {
    for (i=2*apcount;i<2*length;i++) h21->data[i]=0.;
    if (apcount > *countback) *countback = apcount;
  }

  apcount= count;
  xlalStatus22 = XLALPSpinInspiralAttachRingdownWave( h22 , params , energy, &apcount, nmodes , 2 , 2 , LNhx, LNhy, LNhz);
  if (xlalStatus22 != XLAL_SUCCESS ) XLALDestroyREAL4Vector( h22 );
  else {
    for (i=2*apcount;i<2*length;i++) h22->data[i]=0.;
    if ( apcount > *countback ) *countback= apcount;
  }

  //fprintf(stderr,"** LALPSpinInsiralRD: countback dopo attacco=%d\n",*countback);
  
  if ((xlalStatus22!= XLAL_SUCCESS) && (xlalStatus20!=XLAL_SUCCESS)) {
    XLALDestroyREAL4Vector( fap );
    XLALDestroyREAL8Vector( phap );
    XLALDestroyREAL4Vector( shift22 );
    ABORTXLAL( status );
  }

  /*-------------------------------------------------------------------
   * Compute the spherical harmonics required for constructing (h+,hx).
   -------------------------------------------------------------------*/

  inc = params->orbitTheta0;
  phiangle = params->orbitPhi0;

  /* -----------------------------------------------------------------
   * Attaching the (2,2), (2,1) and (2,0) Spherical Harmonic
   *----------------------------------------*/

   xlalStatus22 = XLALSphHarm( &MultSphHarm2P2, 2, 2, inc , phiangle );
   if (xlalStatus22 != XLAL_SUCCESS )
   {
     XLALDestroyREAL4Vector( h22 );
     XLALDestroyREAL4Vector( fap );
     XLALDestroyREAL8Vector( phap );
     XLALDestroyREAL4Vector( shift22 );
     ABORTXLAL( status );
   }

   xlalStatus22 = XLALSphHarm( &MultSphHarm2M2, 2, -2, inc, phiangle );

   if (xlalStatus22 != XLAL_SUCCESS )
   {
     XLALDestroyREAL4Vector( h22 );
     XLALDestroyREAL4Vector( fap );
     XLALDestroyREAL8Vector( phap );
     XLALDestroyREAL4Vector( shift22 );
     ABORTXLAL( status );
   }

   y_1 =   MultSphHarm2P2.re + MultSphHarm2M2.re;
   y_2 = - MultSphHarm2P2.im + MultSphHarm2M2.im;
   z1  = - MultSphHarm2P2.im - MultSphHarm2M2.im;
   z2  = - MultSphHarm2P2.re + MultSphHarm2M2.re;
   
   //fprintf(stderr," ** PSIRD WARNING ** : Sph.Harm. Re2p2=%11.3e  %11.3e\n",(y_1-z2)/2.,(y_1+z2)/2.);
   //fprintf(stderr," ** PSIRD WARNING ** : Sph.Harm. Im2p2=%11.3e  %11.3e\n",-(y_2+z1)/2.,(y_2-z1)/2.);

   /* Next, compute h+ and hx from h22, h22*, Y22, Y2-2 */
   for ( i = 0; i < length; i++)
   {
     fap->data[i] /= unitHz;
     x1 = h22->data[2*i];
     x2 = h22->data[2*i+1];
     sig1->data[i] = (x1 * y_1) + (x2 * y_2);
     /* Check the sign of sig2 */ 
     sig2->data[i] = (x1 * z1)  + (x2 * z2);
   }

   xlalStatus21 = XLALSphHarm( &MultSphHarm2P1, 2, 1, inc , phiangle );
   if (xlalStatus21 != XLAL_SUCCESS )
     {
       XLALDestroyREAL4Vector( h21 );
       ABORTXLAL( status );
     }
   
   xlalStatus21 = XLALSphHarm( &MultSphHarm2M1, 2, -1, inc , phiangle );
   if (xlalStatus21 != XLAL_SUCCESS )
     {
       XLALDestroyREAL4Vector( h21 );
       ABORTXLAL( status );
     }
   
   y_1 =   MultSphHarm2P1.re + MultSphHarm2M1.re;
   y_2 = - MultSphHarm2P1.im + MultSphHarm2M1.im;
   z1  = - MultSphHarm2P1.im - MultSphHarm2M1.im;
   z2  = - MultSphHarm2P1.re + MultSphHarm2M1.re;

   //fprintf(stderr," ** PSIRD WARNING ** : Sph.Harm. Re2p1=%11.3e  %11.3e\n",(y_1-z2)/2.,(y_1+z2)/2.);
   //fprintf(stderr," ** PSIRD WARNING ** : Sph.Harm. Im2p1=%11.3e  %11.3e\n",-(y_2+z1)/2.,(y_2-z1)/2.);

   for ( i = 0; i < length; i++)
   {
     x1 = h21->data[2*i];
     x2 = h21->data[2*i+1];
     // Uncomment this to add the 21 mode
     sig1->data[i]+= (x1 * y_1) + (x2 * y_2);
     sig2->data[i]+= (x1 * z1)  + (x2 * z2);
   }

   xlalStatus20 = XLALSphHarm( &MultSphHarm20, 2, 0, inc , phiangle );
   if (xlalStatus20 != XLAL_SUCCESS )
     {
       XLALDestroyREAL4Vector( h20 );
       ABORTXLAL( status );
     }
   
   y_1 = 2.*MultSphHarm20.re;
   z1  = -2.*MultSphHarm2P1.im;

   //fprintf(stderr," ** PSIRD WARNING ** : Sph.Harm. Re20=%11.3e  Im20=%11.3e\n",y_1/2.,z1/2.);

   for ( i = 0; i < length; i++)
   {
     x1 = h21->data[2*i];
     x2 = h21->data[2*i+1];
     // Uncomment this to add the 20 mode
     /*sig1->data[i]+ = x1 * y_1;
       sig2->data[i]+ = x1 * z1;*/

   }

   /*------------------------------------------------------
    * If required by the user copy other data sets to the
    * relevant arrays
    ------------------------------------------------------*/
   if (hh)
   {
     for(i = 0; i < length; i++)
     {
       j = 2*i;
       k = j+1;
       hap->data[j] = sig1->data[i];
       hap->data[k] = sig2->data[i];
     }
   }

   if (signalvec1) memcpy(signalvec1->data , sig1->data,    length * (sizeof(REAL4)));
   if (signalvec2) memcpy(signalvec2->data , sig2->data,    length * (sizeof(REAL4)));
   if (hh)         memcpy(hh->data         , hap->data,   2*length * (sizeof(REAL4))); 
   if (ff)         memcpy(ff->data         , fap->data,     length * (sizeof(REAL4)));
   if (phi)        memcpy(phi->data        , phap->data,    length * (sizeof(REAL8)));
   if (shift)      memcpy(shift->data      , shift22->data, length * (sizeof(REAL4)));

   /* Clean up */
   XLALDestroyREAL4Vector ( h22 );
   XLALDestroyREAL4Vector ( shift22 );
   XLALDestroyREAL4Vector ( fap );
   XLALDestroyREAL8Vector ( phap );
   XLALDestroyREAL4Vector ( sig1 );
   XLALDestroyREAL4Vector ( sig2 );
   XLALDestroyREAL4Vector ( h20 );
   XLALDestroyREAL4Vector ( h21 );
   XLALDestroyREAL4Vector ( hap );
   
   DETATCHSTATUSPTR(status);
   RETURN(status);

  /*End*/
  
}
